/**
 * @file espnow_manager.h
 * @brief Gestionnaire ESP-NOW bas niveau
 *
 * Initialise le WiFi en mode STA et ESP-NOW.
 * Gère l'envoi et la réception de messages via ESP-NOW.
 * Fournit un système de callback pour notifier les couches supérieures
 * (relay_link) des messages reçus.
 */

#ifndef COMM_ESPNOW_MANAGER_H
#define COMM_ESPNOW_MANAGER_H

#include "protocol.h"
#include <cstdint>

/* Taille d'une adresse MAC */
constexpr int MAC_ADDR_LEN = 6;

/* Adresse broadcast ESP-NOW */
extern const uint8_t BROADCAST_MAC[MAC_ADDR_LEN];

/**
 * Type du callback appelé à la réception d'un message ESP-NOW.
 *
 * @param mac_addr Adresse MAC de l'expéditeur (6 octets)
 * @param msg      Message reçu
 */
typedef void (*espnow_recv_cb_t)(const uint8_t *mac_addr, const EspNowMessage &msg);

/**
 * Initialise le WiFi (mode STA) et ESP-NOW.
 * Active le mode modem sleep pour l'économie d'énergie.
 * Affiche l'adresse MAC du contrôleur sur Serial.
 */
void espnow_init();

/**
 * Enregistre un callback pour la réception de messages.
 * Un seul callback est supporté (relay_link).
 */
void espnow_set_recv_callback(espnow_recv_cb_t cb);

/**
 * Ajoute un peer ESP-NOW.
 *
 * @param mac_addr Adresse MAC du peer (6 octets)
 * @param encrypt  true pour chiffrer les échanges avec ce peer
 *                 (PMK/LMK de config.h). OBLIGATOIREMENT false pour
 *                 l'adresse de diffusion : ESP-NOW interdit le
 *                 chiffrement en broadcast.
 *
 *                 Un peer déjà présent avec un mode de chiffrement
 *                 différent est recréé : c'est ce qui permet de passer
 *                 du clair (découverte) au chiffré (exploitation).
 * @return true si le peer a été ajouté avec succès
 */
bool espnow_add_peer(const uint8_t *mac_addr, bool encrypt = false);

/**
 * Supprime un peer ESP-NOW.
 *
 * @param mac_addr Adresse MAC du peer à supprimer
 */
void espnow_remove_peer(const uint8_t *mac_addr);

/**
 * Envoie un message ESP-NOW à une adresse MAC.
 *
 * @param mac_addr Adresse MAC du destinataire (6 octets)
 * @param msg      Message à envoyer
 * @return true si l'envoi a été accepté (pas de garantie de réception)
 */
bool espnow_send(const uint8_t *mac_addr, const EspNowMessage &msg);

#endif /* COMM_ESPNOW_MANAGER_H */
