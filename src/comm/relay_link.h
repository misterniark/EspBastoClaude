/**
 * @file relay_link.h
 * @brief Liaison avec le module relais (découverte, ping, retry)
 *
 * Couche de communication haut niveau avec le relais.
 * Gère :
 * - La découverte automatique (broadcast → ACK_PONG → enregistrement MAC)
 * - La sauvegarde de l'adresse MAC en NVS
 * - L'envoi de commandes avec retry (3 tentatives, 500ms d'espacement)
 * - Le ping périodique (60s) et la détection de perte de connexion
 */

#ifndef COMM_RELAY_LINK_H
#define COMM_RELAY_LINK_H

#include <cstdint>

/**
 * Type du callback de notification pour les événements du relais.
 * Utilisé par heater_fsm pour réagir aux ACK.
 *
 * @param msg_type Code du message reçu (ACK_ON, ACK_OFF, etc.)
 */
typedef void (*relay_event_cb_t)(uint8_t msg_type);

/**
 * Initialise la liaison relais.
 * Tente de charger la MAC sauvegardée en NVS.
 * Enregistre le callback de réception ESP-NOW.
 */
void relay_link_init();

/**
 * Boucle de mise à jour à appeler dans loop().
 * Gère la découverte, le ping périodique et les retries.
 */
void relay_link_update();

/**
 * Enregistre un callback pour les événements du relais.
 * Utilisé par heater_fsm pour recevoir les ACK.
 */
void relay_link_set_event_callback(relay_event_cb_t cb);

/** Envoie CMD_HEAT_ON avec retry. */
bool relay_send_heat_on();

/** Envoie CMD_HEAT_OFF avec retry. */
bool relay_send_heat_off();

/** Retourne true si le relais est connecté (ping OK). */
bool relay_is_connected();

/** Retourne true si la découverte est en cours. */
bool relay_is_discovering();

/** Force une redécouverte (ex: si le relais a été remplacé). */
void relay_start_discovery();

#endif /* COMM_RELAY_LINK_H */
