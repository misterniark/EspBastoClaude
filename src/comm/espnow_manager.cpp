/**
 * @file espnow_manager.cpp
 * @brief Implémentation du gestionnaire ESP-NOW bas niveau
 *
 * Initialise le WiFi en mode STA sans connexion à un AP,
 * puis initialise ESP-NOW avec les callbacks d'envoi et de réception.
 * Le mode modem sleep est activé pour réduire la consommation.
 */

#include "espnow_manager.h"
#include "../config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/* Adresse MAC broadcast pour la découverte */
const uint8_t BROADCAST_MAC[MAC_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Callback utilisateur enregistré par relay_link */
static espnow_recv_cb_t user_recv_cb = nullptr;

/**
 * Callback interne appelé par ESP-NOW à chaque réception de message.
 * La signature utilise esp_now_recv_info (API IDF 5.x).
 * Vérifie la taille du message et transmet au callback utilisateur.
 */
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    const uint8_t *mac_addr = recv_info->src_addr;

    if (data_len != sizeof(EspNowMessage)) {
        Serial.printf("[ESPNOW] Message reçu de taille inattendue : %d octets\n", data_len);
        return;
    }

    EspNowMessage msg;
    memcpy(&msg, data, sizeof(EspNowMessage));

    Serial.printf("[ESPNOW] Message reçu : type=%d depuis %02X:%02X:%02X:%02X:%02X:%02X\n",
                  msg.type,
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    if (user_recv_cb) {
        user_recv_cb(mac_addr, msg);
    }
}

/**
 * Callback interne appelé par ESP-NOW après chaque tentative d'envoi.
 * La signature utilise wifi_tx_info_t (API IDF 5.x).
 */
static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW] Échec envoi");
    }
}

void espnow_init()
{
    /* Initialiser le WiFi en mode Station (sans se connecter à un AP) */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    /* Mode modem sleep conservateur (MIN_MODEM).
     * MAX_MODEM cause des pertes de paquets ESP-NOW en réception
     * (les ACK du relay sont manqués quand le modem dort).
     * MIN_MODEM garde le modem suffisamment actif pour recevoir les ACK.
     * L'économie d'énergie WiFi se fait via les autres optimisations
     * (LVGL stoppé en veille, capteur adaptatif, 10 FPS). */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    /* Afficher l'adresse MAC du contrôleur */
    Serial.printf("[ESPNOW] Adresse MAC contrôleur : %s\n", WiFi.macAddress().c_str());

    /* Initialiser ESP-NOW */
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Erreur : échec initialisation ESP-NOW");
        return;
    }

    /* Enregistrer les callbacks */
    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    Serial.println("[ESPNOW] Initialisé avec succès");
}

void espnow_set_recv_callback(espnow_recv_cb_t cb)
{
    user_recv_cb = cb;
}

bool espnow_add_peer(const uint8_t *mac_addr)
{
    /* Vérifier si le peer existe déjà */
    if (esp_now_is_peer_exist(mac_addr)) {
        return true;
    }

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac_addr, MAC_ADDR_LEN);
    peer_info.channel = 0;    /* Canal auto */
    peer_info.encrypt = false; /* Pas de chiffrement */

    esp_err_t result = esp_now_add_peer(&peer_info);
    if (result != ESP_OK) {
        Serial.printf("[ESPNOW] Erreur ajout peer : %d\n", result);
        return false;
    }

    Serial.printf("[ESPNOW] Peer ajouté : %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);
    return true;
}

void espnow_remove_peer(const uint8_t *mac_addr)
{
    if (esp_now_is_peer_exist(mac_addr)) {
        esp_now_del_peer(mac_addr);
    }
}

bool espnow_send(const uint8_t *mac_addr, const EspNowMessage &msg)
{
    esp_err_t result = esp_now_send(mac_addr, (const uint8_t *)&msg, sizeof(EspNowMessage));
    return (result == ESP_OK);
}
