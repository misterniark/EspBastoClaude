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

    xQueueSend(recv_queue, &rm, 0);
}

/**
 * Traite un message reçu du relais (appelé depuis relay_link_update uniquement).
 */
static void process_received(const ReceivedMsg &rm)
{
    switch (rm.msg_type) {
        case ACK_PONG:
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
            } else {
                ping_fail_count = 0;
                ping_pending = false;
            }
            break;

        case ACK_ON:
        case ACK_OFF:
        case ACK_LOCKED:
        case ACK_UNLOCKED:
            /* Transmettre à heater_fsm (via sa propre queue) */
            if (event_cb) {
                event_cb(rm.msg_type);
            }
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

/** Gère le ping périodique et la détection de perte de connexion. */
static void update_connected()
{
    unsigned long now = millis();

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

bool relay_send_heat_on()
{
    if (state != RL_CONNECTED) return false;
    EspNowMessage msg = { CMD_HEAT_ON };
    return espnow_send(relay_mac, msg);
}

bool relay_send_heat_off()
{
    if (state != RL_CONNECTED) return false;
    EspNowMessage msg = { CMD_HEAT_OFF };
    return espnow_send(relay_mac, msg);
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
