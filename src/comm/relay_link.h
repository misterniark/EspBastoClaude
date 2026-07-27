/**
 * @file relay_link.h
 * @brief Liaison avec le module relais (découverte, ping, retry)
 *
 * Couche de communication haut niveau avec le relais.
 * Gère :
 * - La découverte automatique (broadcast → ACK_PONG → enregistrement MAC)
 * - La sauvegarde de l'adresse MAC en NVS
 * - Le ping périodique (60s) et la détection de perte de connexion
 * - Le suivi de l'état RÉEL du relais, rapporté dans chaque ACK_PONG
 *   (base de la réconciliation, voir core/relay_reconcile.h)
 * - La mise en sécurité au démarrage (CMD_HEAT_OFF à la première
 *   connexion) et la réémission du CMD_HEAT_OFF non confirmé
 */

#ifndef COMM_RELAY_LINK_H
#define COMM_RELAY_LINK_H

#include <cstdint>
#include "../core/relay_reconcile.h"

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

/** Envoie CMD_HEAT_ON. Invalide l'état rapporté (voir relay_reconcile.h). */
bool relay_send_heat_on();

/**
 * Envoie CMD_HEAT_OFF et arme la réémission applicative : sans ACK_OFF
 * sous ESPNOW_ACK_TIMEOUT_MS, la commande est renvoyée jusqu'à
 * ESPNOW_MAX_RETRIES fois. Seul l'arrêt est réémis (sens critique).
 */
bool relay_send_heat_off();

/**
 * Retourne le dernier état du relais rapporté par un ACK_PONG (ou par
 * un ACK direct), ou RELAY_REPORT_UNKNOWN si aucun rapport exploitable
 * n'est disponible — notamment juste après l'envoi d'une commande, ou
 * si le relais exécute un firmware antérieur au PONG porteur d'état.
 */
RelayReport relay_get_reported_state();

/** Retourne true si le relais est connecté (ping OK). */
bool relay_is_connected();

/** Retourne true si la découverte est en cours. */
bool relay_is_discovering();

/** Force une redécouverte (ex: si le relais a été remplacé). */
void relay_start_discovery();

#endif /* COMM_RELAY_LINK_H */
