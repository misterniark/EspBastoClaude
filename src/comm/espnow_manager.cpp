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
#include <esp_arduino_version.h> /* ESP_ARDUINO_VERSION_* pour le shim ci-dessous */

/* Adresse MAC broadcast pour la découverte */
const uint8_t BROADCAST_MAC[MAC_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Callback utilisateur enregistré par relay_link */
static espnow_recv_cb_t user_recv_cb = nullptr;

/**
 * Traitement commun d'un message reçu, indépendant de la version d'API.
 * Vérifie la taille du message et transmet au callback utilisateur.
 */
static void handle_recv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    /* Tolérance de taille : 2 octets (protocole courant, avec l'octet
     * d'état du relais) ou 1 octet (relais non encore mis à jour —
     * l'état est alors simplement inconnu, pas de réconciliation). */
    if (data_len < ESPNOW_MSG_MIN_LEN || data_len > (int)sizeof(EspNowMessage)) {
        Serial.printf("[ESPNOW] Message reçu de taille inattendue : %d octets\n", data_len);
        return;
    }

    EspNowMessage msg = { 0, 0 };
    memcpy(&msg, data, data_len);
    /* Marqueur « pas de charge utile » pour un message legacy 1 octet */
    bool has_payload = (data_len >= (int)sizeof(EspNowMessage));
    if (!has_payload) {
        msg.payload = ESPNOW_PAYLOAD_ABSENT;
    }

    Serial.printf("[ESPNOW] Message reçu : type=%d depuis %02X:%02X:%02X:%02X:%02X:%02X\n",
                  msg.type,
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    if (user_recv_cb) {
        user_recv_cb(mac_addr, msg);
    }
}

/*
 * Shim de compatibilité : la signature des callbacks ESP-NOW a changé
 * entre les générations du core Arduino-ESP32 (le protocole radio, lui,
 * est identique — les deux cartes peuvent utiliser des cores différents) :
 *   - core 2.x (IDF 4.4) : adresse MAC source passée directement
 *   - core 3.0 à 3.2     : réception via esp_now_recv_info_t
 *   - core 3.3+          : idem + envoi via wifi_tx_info_t
 */
#if ESP_ARDUINO_VERSION_MAJOR >= 3

/** Réception (core 3.x) : l'adresse source est dans recv_info */
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    handle_recv(recv_info->src_addr, data, data_len);
}

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
/** Confirmation d'envoi (core 3.3+) */
static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW] Échec envoi");
    }
}
#else
/** Confirmation d'envoi (core 3.0 à 3.2) */
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW] Échec envoi");
    }
}
#endif

#else /* core 2.x (IDF 4.4) */

/** Réception (core 2.x) : l'adresse source est passée directement */
static void on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    handle_recv(mac_addr, data, data_len);
}

/** Confirmation d'envoi (core 2.x) */
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW] Échec envoi");
    }
}

#endif /* ESP_ARDUINO_VERSION_MAJOR */

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

    /* Clé primaire de chiffrement : elle protège les LMK des peers.
     * Doit être identique côté relais (voir config.h). */
    if (esp_now_set_pmk(ESPNOW_PMK) != ESP_OK) {
        Serial.println("[ESPNOW] ATTENTION : echec configuration de la PMK");
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

bool espnow_add_peer(const uint8_t *mac_addr, bool encrypt)
{
    /* Peer déjà présent : ne le recréer que si le mode de chiffrement
     * demandé diffère. C'est le passage clair → chiffré au moment de
     * l'appairage (et l'inverse si l'on retombe en découverte). */
    esp_now_peer_info_t existing = {};
    if (esp_now_get_peer(mac_addr, &existing) == ESP_OK) {
        if ((bool)existing.encrypt == encrypt) {
            return true;
        }
        esp_now_del_peer(mac_addr);
    }

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac_addr, MAC_ADDR_LEN);
    peer_info.channel = 0;    /* Canal auto */
    peer_info.encrypt = encrypt;
    if (encrypt) {
        memcpy(peer_info.lmk, ESPNOW_LMK, sizeof(peer_info.lmk));
    }

    esp_err_t result = esp_now_add_peer(&peer_info);
    if (result != ESP_OK) {
        Serial.printf("[ESPNOW] Erreur ajout peer : %d\n", result);
        return false;
    }

    Serial.printf("[ESPNOW] Peer ajouté (%s) : %02X:%02X:%02X:%02X:%02X:%02X\n",
                  encrypt ? "chiffre" : "clair",
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
