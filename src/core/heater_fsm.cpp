/**
 * @file heater_fsm.cpp
 * @brief Machine d'état du chauffage avec protections de sécurité
 *
 * Corrections de sécurité appliquées :
 *   C1 — Arrêt forcé si erreur capteur critique (5 min sans lecture)
 *   C2 — Queue FreeRTOS pour les événements du callback WiFi (plus de race condition)
 *   C4 — Température max de sécurité absolue (TEMP_SAFETY_MAX = 40°C)
 *   I5 — Perte connexion pendant HEATING → LOCKED (pas IDLE)
 *
 * Les événements ESP-NOW arrivent via une queue FreeRTOS (écrite dans le
 * callback WiFi, lue dans heater_fsm_update() du contexte loop()).
 * Plus aucune variable partagée n'est modifiée directement depuis le callback.
 */

#include "heater_fsm.h"
#include "relay_reconcile.h"
#include "../comm/relay_link.h"
#include "../comm/protocol.h"
#include "../hal/sensor.h"
#include "../config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/* État courant — modifié UNIQUEMENT depuis heater_fsm_update() (contexte loop) */
static HeaterState current_state = HEATER_IDLE;
static bool connection_alert = false;

/* Cause de l'arrêt de sécurité en attente d'acquittement par l'UI.
 * Typée (et non booléenne) pour que l'écran d'alerte puisse afficher
 * un message explicite : capteur HS (C1) ou température max (C4). */
static HeaterSafetyReason safety_reason = HEATER_SAFETY_NONE;

/*
 * Queue FreeRTOS pour recevoir les événements du callback WiFi
 * de manière thread-safe. Capacité 8 messages (largement suffisant).
 */
static QueueHandle_t event_queue = NULL;
constexpr int EVENT_QUEUE_SIZE = 8;

/**
 * Callback appelé par relay_link depuis le contexte WiFi.
 * Au lieu de modifier current_state directement (race condition !),
 * on pousse l'événement dans une queue FreeRTOS.
 */
static void on_relay_event(uint8_t msg_type)
{
    if (event_queue) {
        /* xQueueSendFromISR serait nécessaire dans un vrai ISR,
         * mais le callback ESP-NOW est dans une tâche FreeRTOS,
         * donc xQueueSend est correct ici. */
        xQueueSend(event_queue, &msg_type, 0);
    }
}

/**
 * Traite un événement reçu du relais (appelé depuis loop() uniquement).
 * Gère les transitions d'état de manière sûre.
 */
/* Instant d'entrée dans l'état LOCKED : arme le filet de sécurité
 * HEATER_LOCKED_FAILSAFE_MS (voir heater_fsm_update). Sans lui, un
 * ACK_UNLOCKED perdu (radio) ou jamais émis (relais déjà OFF au moment
 * du HEAT_OFF → verrou non armé côté relais, constaté au banc de test
 * du 27/07/2026) laisserait le contrôleur bloqué en LOCKED à vie. */
static unsigned long locked_since_ms = 0;

/** Passe en LOCKED en armant le filet de sécurité. */
static void enter_locked(const char *reason)
{
    current_state   = HEATER_LOCKED;
    locked_since_ms = millis();
    Serial.println(reason);
}

static void process_event(uint8_t msg_type)
{
    switch (msg_type) {
        case ACK_ON:
            if (current_state == HEATER_IDLE) {
                current_state = HEATER_HEATING;
                Serial.println("[HEATER] IDLE -> HEATING");
            }
            break;

        case ACK_OFF:
            if (current_state == HEATER_HEATING) {
                enter_locked("[HEATER] HEATING -> LOCKED (verrou 3 min)");
            }
            break;

        case ACK_LOCKED:
            if (current_state != HEATER_LOCKED) {
                enter_locked("[HEATER] -> LOCKED");
            }
            break;

        case ACK_UNLOCKED:
            if (current_state == HEATER_LOCKED) {
                current_state = HEATER_IDLE;
                Serial.println("[HEATER] LOCKED -> IDLE (verrou expire)");
            }
            break;

        default:
            break;
    }
}

void heater_fsm_init()
{
    current_state = HEATER_IDLE;
    connection_alert = false;
    safety_reason = HEATER_SAFETY_NONE;

    /* Créer la queue FreeRTOS pour les événements */
    event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(uint8_t));
    if (!event_queue) {
        Serial.println("[HEATER] ERREUR : impossible de creer la queue");
    }

    /* Enregistrer le callback pour recevoir les ACK du relais */
    relay_link_set_event_callback(on_relay_event);

    Serial.println("[HEATER] FSM initialisee");
}

void heater_fsm_update()
{
    /* ===================================================
     * 1. Traiter tous les événements en attente dans la queue
     *    (écrits par le callback WiFi, lus ici dans loop)
     * =================================================== */
    uint8_t evt;
    while (xQueueReceive(event_queue, &evt, 0) == pdTRUE) {
        process_event(evt);
    }

    /* ===================================================
     * 1bis. Filet de sécurité de l'état LOCKED : si l'ACK_UNLOCKED
     * n'arrive jamais (perte radio, ou relais qui n'avait pas armé son
     * verrou car déjà éteint), revenir en IDLE après un délai supérieur
     * au verrou nominal du relais (3 min). Sans ce filet, le contrôleur
     * resterait verrouillé jusqu'au reboot.
     * =================================================== */
    if (current_state == HEATER_LOCKED
        && millis() - locked_since_ms > HEATER_LOCKED_FAILSAFE_MS) {
        current_state = HEATER_IDLE;
        Serial.println("[HEATER] LOCKED -> IDLE (filet de securite : "
                       "ACK_UNLOCKED jamais recu)");
    }

    /* ===================================================
     * 1ter. RÉCONCILIATION avec l'état réel du relais.
     *
     * Le relais rapporte son état dans chaque ACK_PONG : on s'aligne
     * sur la réalité au lieu de faire confiance à notre seule mémoire.
     * C'est ce qui rattrape un reboot de l'un ou l'autre appareil et
     * les ACK perdus (voir core/relay_reconcile.h pour les scénarios).
     * Placé AVANT les vérifications de sécurité pour qu'un état corrigé
     * soit pris en compte dès ce cycle.
     * =================================================== */
    switch (relay_reconcile_step(current_state == HEATER_HEATING,
                                 relay_get_reported_state())) {
        case RECONCILE_ADOPT_OFF:
            /* Le relais s'est arrêté sans qu'on le demande (son
             * watchdog, un reboot, un arrêt manuel au bouton). Adopter
             * la réalité et prévenir : l'utilisateur croit avoir du
             * chauffage. Retour en IDLE (et non LOCKED) — le relais est
             * ouvert depuis un moment, son verrou a couru en même temps. */
            current_state = HEATER_IDLE;
            safety_reason = HEATER_SAFETY_DESYNC;
            Serial.println("[HEATER] DESYNC : le relais est eteint alors que "
                           "nous chauffions -> IDLE + alerte");
            break;

        case RECONCILE_SEND_OFF:
            /* Le relais chauffe alors qu'aucune chauffe n'est en cours
             * de notre côté : typiquement notre propre reboot pendant
             * une chauffe. Ne JAMAIS laisser tourner un appareil à
             * combustion que nous ne régulons pas (nos pings satisfont
             * son watchdog : rien d'autre ne le couperait). */
            Serial.println("[HEATER] DESYNC : le relais chauffe hors de notre "
                           "controle -> arret force");
            heater_force_off();
            break;

        case RECONCILE_NONE:
        default:
            break;
    }

    /* ===================================================
     * 2. Vérifications de sécurité (uniquement si HEATING)
     * =================================================== */
    if (current_state != HEATER_HEATING) return;

    /* C1 — Erreur capteur critique : arrêt forcé du chauffage.
     * Si le capteur est en erreur depuis > 5 min, la température
     * affichée est gelée → le thermostat ne peut pas réagir.
     * On coupe ici, dans la couche core, pas dans l'UI. */
    if (sensor_is_critical_error()) {
        Serial.println("[HEATER] SECURITE : erreur capteur critique -> arret force");
        relay_send_heat_off();
        current_state = HEATER_IDLE;
        safety_reason = HEATER_SAFETY_SENSOR;
        return;
    }

    /* C4 — Température max de sécurité absolue.
     * Si la température dépasse TEMP_SAFETY_MAX (40°C),
     * on coupe le chauffage quel que soit le mode actif. */
    if (!sensor_is_error() && sensor_get_temperature() >= TEMP_SAFETY_MAX) {
        Serial.printf("[HEATER] SECURITE : temp %.1f >= %.1f -> arret force\n",
                      sensor_get_temperature(), TEMP_SAFETY_MAX);
        relay_send_heat_off();
        current_state = HEATER_IDLE;
        safety_reason = HEATER_SAFETY_OVERTEMP;
        return;
    }

    /* I5 — Perte de connexion pendant HEATING.
     * Le watchdog relais coupera après 3 min, donc le relais
     * sera en verrou. On passe en LOCKED (pas IDLE) pour
     * refléter l'état réel du relais. */
    if (!relay_is_connected()) {
        connection_alert = true;
        enter_locked("[HEATER] Perte connexion pendant HEATING -> LOCKED + alerte");
    }
}

bool heater_request_on()
{
    if (current_state != HEATER_IDLE) {
        Serial.printf("[HEATER] request_on refuse (etat: %d)\n", current_state);
        return false;
    }

    if (!relay_is_connected()) {
        Serial.println("[HEATER] request_on refuse (relais non connecte)");
        return false;
    }

    Serial.println("[HEATER] Envoi HEAT_ON...");
    return relay_send_heat_on();
}

bool heater_request_off()
{
    if (current_state != HEATER_HEATING) {
        Serial.printf("[HEATER] request_off refuse (etat: %d)\n", current_state);
        return false;
    }

    Serial.println("[HEATER] Envoi HEAT_OFF...");
    return relay_send_heat_off();
}

bool heater_force_off()
{
    /* Aucune condition sur current_state : c'est tout l'intérêt.
     * Voir heater_fsm.h pour le scénario de l'ACK_ON perdu. */
    if (!relay_is_connected()) {
        Serial.println("[HEATER] force_off impossible (relais non connecte)");
        return false;
    }

    Serial.println("[HEATER] Envoi HEAT_OFF (inconditionnel)...");
    return relay_send_heat_off();
}

HeaterState heater_get_state()
{
    return current_state;
}

bool heater_has_connection_alert()
{
    return connection_alert;
}

void heater_clear_connection_alert()
{
    connection_alert = false;
}

bool heater_has_sensor_safety_alert()
{
    return safety_reason != HEATER_SAFETY_NONE;
}

HeaterSafetyReason heater_get_safety_alert_reason()
{
    return safety_reason;
}

void heater_clear_sensor_safety_alert()
{
    safety_reason = HEATER_SAFETY_NONE;
}
