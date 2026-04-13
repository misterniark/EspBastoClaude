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
 * Structure du message ESP-NOW (1 octet)
 * ========================================== */
typedef struct {
    uint8_t type;       /* Code commande ou réponse */
} __attribute__((packed)) EspNowMessage;

#endif /* COMM_PROTOCOL_H */
