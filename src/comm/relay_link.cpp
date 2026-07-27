/**
 * @file relay_link.cpp
 * @brief Liaison avec le module relais (découverte, ping, retry)
 *
 * Correction I1 : les messages ESP-NOW sont stockés dans une queue FreeRTOS
 * dans le callback WiFi, puis traités dans relay_link_update() (contexte loop).
 * Plus aucune variable d'état n'est modifiée directement dans le callback.
 */

#include "relay_link.h"
#include "espnow_manager.h"
#include "protocol.h"
#include "../core/storage.h"
#include "../core/relay_reconcile.h"
#include "../config.h"
#include <Arduino.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/* ==========================================
 * États internes
 * ========================================== */
enum RelayLinkState {
    RL_DISCOVERING,
    RL_CONNECTED
};

/* ==========================================
 * Structure pour la queue de messages reçus
 * ========================================== */
typedef struct {
    uint8_t mac[MAC_ADDR_LEN];
    uint8_t msg_type;
    uint8_t payload;   /* État du relais dans ACK_PONG (ou ABSENT) */
} ReceivedMsg;

/* ==========================================
 * Variables d'état (modifiées UNIQUEMENT depuis relay_link_update)
 * ========================================== */
static RelayLinkState state = RL_DISCOVERING;
static uint8_t relay_mac[MAC_ADDR_LEN] = {0};
static bool    has_relay_mac = false;
static relay_event_cb_t event_cb = nullptr;

/* Timing découverte */
static unsigned long last_discovery_ms = 0;
static int           discovery_unicast_tries = 0;

/* Timing ping */
static unsigned long last_ping_ms = 0;
static int           ping_fail_count = 0;
static bool          ping_pending = false;
static unsigned long ping_sent_ms = 0;

/* ==========================================
 * Réconciliation d'état (voir core/relay_reconcile.h)
 * ========================================== */

/* Dernier état rapporté par le relais, et son horodatage. Remis à
 * UNKNOWN à chaque commande envoyée : un rapport décrivant l'état
 * d'AVANT notre ordre ne doit jamais servir à le défaire. */
static RelayReport   reported_state    = RELAY_REPORT_UNKNOWN;
static unsigned long last_cmd_ms       = 0;

/* Mise en sécurité au boot : tant que ce drapeau est armé, la première
 * connexion établie déclenche un CMD_HEAT_OFF inconditionnel. Couvre le
 * cas où le contrôleur a redémarré pendant une chauffe : le relais est
 * peut-être encore fermé, et rien d'autre ne le couperait puisque nos
 * pings satisfont son watchdog. */
static bool boot_safety_pending = true;

/* ==========================================
 * Retry applicatif du CMD_HEAT_OFF
 *
 * Seul l'ARRÊT est réémis : c'est le sens critique pour la sécurité.
 * Un allumage perdu est rattrapé naturellement (le thermostat
 * redemande à chaque lecture), un arrêt perdu laisserait le Webasto
 * tourner sans surveillance.
 * ========================================== */
static bool          off_ack_pending = false;
static unsigned long off_sent_ms     = 0;
static int           off_retries     = 0;

/* Queue FreeRTOS pour recevoir les messages du callback WiFi */
static QueueHandle_t recv_queue = NULL;
constexpr int RECV_QUEUE_SIZE = 8;

/**
 * Callback ESP-NOW : pousse le message dans la queue (thread-safe).
 * Ne modifie AUCUNE variable d'état directement.
 */
static void on_message_received(const uint8_t *mac_addr, const EspNowMessage &msg)
{
    if (!recv_queue) return;

    ReceivedMsg rm;
    memcpy(rm.mac, mac_addr, MAC_ADDR_LEN);
    rm.msg_type = msg.type;
    rm.payload  = msg.payload;

    xQueueSend(recv_queue, &rm, 0);
}

/**
 * Enregistre l'état rapporté par un ACK_PONG, s'il est exploitable.
 *
 * Deux raisons de l'ignorer : le relais n'a pas envoyé l'octet d'état
 * (firmware antérieur), ou le rapport est trop proche de notre dernière
 * commande pour être postérieur à son traitement (voir
 * relay_report_usable).
 */
static void note_reported_state(uint8_t payload)
{
    if (payload != RELAY_PAYLOAD_OFF && payload != RELAY_PAYLOAD_ON) {
        return; /* Relais legacy : pas d'état dans le PONG */
    }
    if (!relay_report_usable(millis(), last_cmd_ms, RELAY_RECONCILE_GRACE_MS)) {
        return; /* Rapport potentiellement antérieur à notre commande */
    }
    reported_state = (payload == RELAY_PAYLOAD_ON) ? RELAY_REPORT_ON
                                                   : RELAY_REPORT_OFF;
}

/**
 * Traite un message reçu du relais (appelé depuis relay_link_update uniquement).
 */
static void process_received(const ReceivedMsg &rm)
{
    switch (rm.msg_type) {
        case ACK_PONG:
            note_reported_state(rm.payload);
            if (state == RL_DISCOVERING) {
                /* Sauvegarder la MAC du relais */
                memcpy(relay_mac, rm.mac, MAC_ADDR_LEN);
                has_relay_mac = true;

                espnow_remove_peer(BROADCAST_MAC);
                espnow_add_peer(relay_mac);

                storage_save_relay_mac(relay_mac);

                state = RL_CONNECTED;
                ping_fail_count = 0;
                last_ping_ms = millis();

                Serial.printf("[RELAY] Relais decouvert : %02X:%02X:%02X:%02X:%02X:%02X\n",
                              relay_mac[0], relay_mac[1], relay_mac[2],
                              relay_mac[3], relay_mac[4], relay_mac[5]);

                /* Mise en sécurité au boot : ce contrôleur vient de
                 * démarrer et ignore tout de ce qui se passait avant.
                 * Si le relais est resté fermé (reboot pendant une
                 * chauffe), personne ne le couperait — nos pings
                 * satisfont son watchdog. On ordonne donc l'arrêt une
                 * fois, sans condition. Sans effet s'il est déjà
                 * ouvert (le relais traite l'arrêt redondant). */
                if (boot_safety_pending) {
                    boot_safety_pending = false;
                    Serial.println("[RELAY] Mise en securite au demarrage : HEAT_OFF");
                    relay_send_heat_off();
                }
            } else {
                ping_fail_count = 0;
                ping_pending = false;
            }
            break;

        case ACK_ON:
            /* Confirmation directe : plus fiable et plus fraîche qu'un
             * futur PONG, on met l'état rapporté à jour tout de suite. */
            reported_state = RELAY_REPORT_ON;
            if (event_cb) event_cb(rm.msg_type);
            break;

        case ACK_OFF:
        case ACK_LOCKED:
            /* Le relais confirme qu'il est ouvert (ACK_OFF) ou qu'il
             * refuse d'allumer (ACK_LOCKED) : dans les deux cas le
             * chauffage est éteint, et notre demande d'arrêt — s'il y
             * en avait une en attente — a bien été reçue. */
            reported_state  = RELAY_REPORT_OFF;
            off_ack_pending = false;
            if (event_cb) event_cb(rm.msg_type);
            break;

        case ACK_UNLOCKED:
            if (event_cb) event_cb(rm.msg_type);
            break;

        default:
            Serial.printf("[RELAY] Message inconnu : type=%d\n", rm.msg_type);
            break;
    }
}

/** Gère la phase de découverte. */
static void update_discovery()
{
    unsigned long now = millis();
    if (now - last_discovery_ms < ESPNOW_DISCOVERY_INTERVAL_MS) return;
    last_discovery_ms = now;

    EspNowMessage msg = { CMD_PING };

    if (has_relay_mac && discovery_unicast_tries < ESPNOW_MAX_RETRIES) {
        discovery_unicast_tries++;
        espnow_add_peer(relay_mac);
        espnow_send(relay_mac, msg);
        Serial.printf("[RELAY] Tentative unicast %d/%d\n",
                      discovery_unicast_tries, ESPNOW_MAX_RETRIES);
    } else {
        if (has_relay_mac && discovery_unicast_tries >= ESPNOW_MAX_RETRIES) {
            espnow_remove_peer(relay_mac);
            has_relay_mac = false;
            Serial.println("[RELAY] Unicast echoue, passage en broadcast");
        }
        espnow_add_peer(BROADCAST_MAC);
        espnow_send(BROADCAST_MAC, msg);
    }
}

/**
 * Réémet le CMD_HEAT_OFF resté sans confirmation.
 * Voir la déclaration de off_ack_pending : seul l'arrêt est réémis.
 */
static void update_off_retry()
{
    if (!off_ack_pending) return;
    if (millis() - off_sent_ms < ESPNOW_ACK_TIMEOUT_MS) return;

    if (off_retries >= ESPNOW_MAX_RETRIES) {
        /* Abandon : ne pas boucler indéfiniment. La réconciliation par
         * PONG reprendra le relais si le chauffage est resté allumé. */
        off_ack_pending = false;
        Serial.printf("[RELAY] HEAT_OFF sans confirmation apres %d tentatives\n",
                      ESPNOW_MAX_RETRIES);
        return;
    }

    off_retries++;
    off_sent_ms = millis();
    last_cmd_ms = millis();
    Serial.printf("[RELAY] HEAT_OFF sans ACK, reemission %d/%d\n",
                  off_retries, ESPNOW_MAX_RETRIES);
    EspNowMessage msg = { CMD_HEAT_OFF, 0 };
    espnow_send(relay_mac, msg);
}

/** Gère le ping périodique et la détection de perte de connexion. */
static void update_connected()
{
    unsigned long now = millis();

    update_off_retry();

    /* Vérifier le timeout du ping en attente */
    if (ping_pending && (now - ping_sent_ms > ESPNOW_ACK_TIMEOUT_MS)) {
        ping_fail_count++;
        ping_pending = false;
        Serial.printf("[RELAY] Ping sans reponse (%d/%d)\n",
                      ping_fail_count, ESPNOW_MAX_PING_FAILS);

        if (ping_fail_count >= ESPNOW_MAX_PING_FAILS) {
            Serial.println("[RELAY] Connexion perdue !");
            state = RL_DISCOVERING;
            discovery_unicast_tries = 0;
            last_discovery_ms = 0;
            return;
        }
    }

    /* Envoyer un ping périodique */
    if (!ping_pending && (now - last_ping_ms >= ESPNOW_PING_INTERVAL_MS)) {
        EspNowMessage msg = { CMD_PING };
        espnow_send(relay_mac, msg);
        ping_pending = true;
        ping_sent_ms = now;
        last_ping_ms = now;
    }
}

void relay_link_init()
{
    /* Créer la queue de réception */
    recv_queue = xQueueCreate(RECV_QUEUE_SIZE, sizeof(ReceivedMsg));
    if (!recv_queue) {
        Serial.println("[RELAY] ERREUR : impossible de creer la queue");
    }

    espnow_set_recv_callback(on_message_received);

    if (storage_load_relay_mac(relay_mac)) {
        has_relay_mac = true;
        Serial.printf("[RELAY] MAC chargee depuis NVS : %02X:%02X:%02X:%02X:%02X:%02X\n",
                      relay_mac[0], relay_mac[1], relay_mac[2],
                      relay_mac[3], relay_mac[4], relay_mac[5]);
    } else {
        has_relay_mac = false;
        Serial.println("[RELAY] Pas de MAC sauvegardee, decouverte broadcast");
    }

    state = RL_DISCOVERING;
    discovery_unicast_tries = 0;
    last_discovery_ms = 0;

    /* Ce contrôleur vient de démarrer : il ignore si le relais est
     * resté fermé. La première connexion déclenchera un HEAT_OFF. */
    boot_safety_pending = true;
    reported_state      = RELAY_REPORT_UNKNOWN;
    off_ack_pending     = false;
}

void relay_link_update()
{
    /* D'abord, traiter tous les messages reçus de la queue */
    ReceivedMsg rm;
    while (xQueueReceive(recv_queue, &rm, 0) == pdTRUE) {
        process_received(rm);
    }

    /* Puis gérer l'état courant */
    switch (state) {
        case RL_DISCOVERING:
            update_discovery();
            break;
        case RL_CONNECTED:
            update_connected();
            break;
    }
}

void relay_link_set_event_callback(relay_event_cb_t cb)
{
    event_cb = cb;
}

/**
 * Marque l'envoi d'une commande : l'état rapporté devient inconnu
 * jusqu'au prochain rapport POSTÉRIEUR à cette commande. Sans cela, un
 * PONG parti avant que le relais ne traite notre ordre décrirait un
 * état périmé et la réconciliation déferait l'ordre qu'on vient de
 * donner (allumage annulé dans la seconde, par exemple).
 */
static void note_command_sent()
{
    last_cmd_ms    = millis();
    reported_state = RELAY_REPORT_UNKNOWN;
}

bool relay_send_heat_on()
{
    if (state != RL_CONNECTED) return false;
    note_command_sent();
    EspNowMessage msg = { CMD_HEAT_ON, 0 };
    return espnow_send(relay_mac, msg);
}

bool relay_send_heat_off()
{
    if (state != RL_CONNECTED) return false;
    note_command_sent();

    /* Armer le retry applicatif : un arrêt perdu laisserait le Webasto
     * tourner sans surveillance, on ne s'en remet pas au seul ESP-NOW. */
    off_ack_pending = true;
    off_sent_ms     = millis();
    off_retries     = 0;

    EspNowMessage msg = { CMD_HEAT_OFF, 0 };
    return espnow_send(relay_mac, msg);
}

RelayReport relay_get_reported_state()
{
    return reported_state;
}

bool relay_is_connected()
{
    return (state == RL_CONNECTED);
}

bool relay_is_discovering()
{
    return (state == RL_DISCOVERING);
}

void relay_start_discovery()
{
    if (has_relay_mac) {
        espnow_remove_peer(relay_mac);
    }
    has_relay_mac = false;
    memset(relay_mac, 0, MAC_ADDR_LEN);
    discovery_unicast_tries = 0;
    last_discovery_ms = 0;
    ping_fail_count = 0;
    ping_pending = false;
    state = RL_DISCOVERING;
    Serial.println("[RELAY] Redecouverte forcee");
}
