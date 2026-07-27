/**
 * @file protocol.h
 * @brief Définition du protocole de communication ESP-NOW
 *
 * Structure des messages et codes de commande échangés
 * entre le contrôleur et le module relais.
 * Les codes sont alignés avec specs_relay.md.
 */

#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <cstdint>

/* ==========================================
 * Codes de commande (Contrôleur → Relais)
 * ========================================== */
enum Command : uint8_t {
    CMD_HEAT_ON  = 1,   /* Activer le chauffage */
    CMD_HEAT_OFF = 2,   /* Désactiver le chauffage */
    CMD_PING     = 3    /* Vérifier la connexion / découverte */
};

/* ==========================================
 * Codes de réponse (Relais → Contrôleur)
 * ========================================== */
enum Response : uint8_t {
    ACK_ON       = 11,  /* Chauffage allumé confirmé */
    ACK_OFF      = 12,  /* Chauffage éteint confirmé */
    ACK_PONG     = 13,  /* Relais connecté (réponse au ping) */
    ACK_LOCKED   = 14,  /* Redémarrage bloqué (délai sécurité 3 min) */
    ACK_UNLOCKED = 15   /* Redémarrage autorisé (délai expiré) */
};

/* ==========================================
 * Charge utile (2e octet), portée par ACK_PONG
 *
 * Le PONG transporte l'état RÉEL du relais : c'est le mécanisme de
 * resynchronisation du système. Sans lui, un reboot de l'un des deux
 * appareils (microcoupure 12 V, banal en van) ou un simple ACK perdu
 * laissait le contrôleur afficher un état faux INDÉFINIMENT — chauffage
 * éteint alors qu'il se croit en chauffe, ou pire, Webasto allumé sans
 * régulation alors qu'il se croit à l'arrêt.
 * Ignoré pour les autres codes de réponse.
 * ========================================== */
enum RelayStatePayload : uint8_t {
    RELAY_PAYLOAD_OFF = 0,  /* Relais ouvert (chauffage éteint) */
    RELAY_PAYLOAD_ON  = 1   /* Relais fermé (chauffage allumé) */
};

/* ==========================================
 * Structure du message ESP-NOW (2 octets)
 *
 * La réception tolère 1 ou 2 octets : un relais non encore mis à jour
 * n'envoie que le code, l'état est alors simplement inconnu (aucune
 * réconciliation, comportement d'avant).
 * ========================================== */
typedef struct {
    uint8_t type;       /* Code commande ou réponse */
    uint8_t payload;    /* Charge utile (état relais dans ACK_PONG) */
} __attribute__((packed)) EspNowMessage;

/** Taille minimale acceptée en réception (message legacy 1 octet). */
constexpr int ESPNOW_MSG_MIN_LEN = 1;

/** Valeur substituée à la charge utile quand le message reçu n'en
 *  comporte pas (relais legacy) : distincte de OFF et de ON, pour ne
 *  jamais confondre « absent » avec « éteint ». */
constexpr uint8_t ESPNOW_PAYLOAD_ABSENT = 0xFF;

#endif /* COMM_PROTOCOL_H */
