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
static bool sensor_safety_alert = false;

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
                current_state = HEATER_LOCKED;
                Serial.println("[HEATER] HEATING -> LOCKED (verrou 3 min)");
            }
            break;

        case ACK_LOCKED:
            if (current_state != HEATER_LOCKED) {
                current_state = HEATER_LOCKED;
                Serial.println("[HEATER] -> LOCKED");
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
    sensor_safety_alert = false;

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
        sensor_safety_alert = true;
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
        sensor_safety_alert = true;
        return;
    }

    /* I5 — Perte de connexion pendant HEATING.
     * Le watchdog relais coupera après 3 min, donc le relais
     * sera en verrou. On passe en LOCKED (pas IDLE) pour
     * refléter l'état réel du relais. */
    if (!relay_is_connected()) {
        current_state = HEATER_LOCKED;
        connection_alert = true;
        Serial.println("[HEATER] Perte connexion pendant HEATING -> LOCKED + alerte");
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
    return sensor_safety_alert;
}

void heater_clear_sensor_safety_alert()
{
    sensor_safety_alert = false;
}
