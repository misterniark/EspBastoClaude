/**
 * @file heater_fsm.h
 * @brief Machine d'état du chauffage (IDLE / HEATING / LOCKED)
 *
 * Gère l'état global du chauffage indépendamment du mode sélectionné.
 * Le mode (A, B ou C) détermine QUAND envoyer les commandes ON/OFF,
 * la FSM gère COMMENT (envoi, ACK, transitions, verrouillage).
 *
 * Diagramme :
 *   IDLE ──(HEAT_ON + ACK_ON)──→ HEATING
 *   IDLE ──(HEAT_ON + ACK_LOCKED)──→ LOCKED
 *   HEATING ──(HEAT_OFF + ACK_OFF)──→ LOCKED
 *   HEATING ──(perte connexion)──→ LOCKED + alerte
 *   HEATING ──(erreur capteur 5min)──→ IDLE + arrêt forcé
 *   HEATING ──(temp >= 40°C)──→ IDLE + arrêt forcé
 *   LOCKED ──(ACK_UNLOCKED)──→ IDLE
 */

#ifndef CORE_HEATER_FSM_H
#define CORE_HEATER_FSM_H

/** États de la machine d'état du chauffage */
enum HeaterState {
    HEATER_IDLE,     /* Chauffage OFF, prêt à démarrer */
    HEATER_HEATING,  /* Chauffage ON, confirmé par ACK_ON */
    HEATER_LOCKED    /* Verrou anti-redémarrage actif (3 min côté relais) */
};

/**
 * Cause d'un arrêt de sécurité du chauffage.
 * Permet à l'UI d'afficher un message explicite selon la coupure
 * (C1 : capteur mort, C4 : surchauffe) au lieu d'un simple booléen.
 */
enum HeaterSafetyReason {
    HEATER_SAFETY_NONE,     /* Aucun arrêt de sécurité en attente d'acquittement */
    HEATER_SAFETY_SENSOR,   /* C1 — capteur en erreur critique (> 5 min) */
    HEATER_SAFETY_OVERTEMP  /* C4 — température >= TEMP_SAFETY_MAX */
};

/**
 * Initialise la machine d'état.
 * Enregistre le callback de réception des ACK depuis relay_link.
 */
void heater_fsm_init();

/**
 * Boucle de mise à jour à appeler dans loop().
 * Vérifie la perte de connexion pendant HEATING.
 */
void heater_fsm_update();

/**
 * Demande l'allumage du chauffage.
 * Envoie CMD_HEAT_ON via relay_link.
 * La transition vers HEATING se fait à réception de ACK_ON.
 *
 * @return true si la commande a pu être envoyée (relais connecté et pas LOCKED)
 */
bool heater_request_on();

/**
 * Demande l'arrêt du chauffage.
 * Envoie CMD_HEAT_OFF via relay_link.
 * La transition vers LOCKED se fait à réception de ACK_OFF.
 *
 * @return true si la commande a pu être envoyée
 */
bool heater_request_off();

/** Retourne l'état actuel de la machine d'état. */
HeaterState heater_get_state();

/**
 * Retourne true si une alerte de connexion perdue est active.
 * L'alerte est levée quand la connexion est rétablie.
 */
bool heater_has_connection_alert();

/** Acquitte l'alerte de connexion perdue. */
void heater_clear_connection_alert();

/**
 * Retourne true si un arrêt de sécurité a été déclenché
 * (erreur capteur critique ou température max dépassée).
 * Équivaut à heater_get_safety_alert_reason() != HEATER_SAFETY_NONE.
 */
bool heater_has_sensor_safety_alert();

/**
 * Retourne la cause de l'arrêt de sécurité en attente d'acquittement,
 * ou HEATER_SAFETY_NONE si aucun. L'UI s'en sert pour choisir le
 * message affiché (capteur HS vs température max).
 */
HeaterSafetyReason heater_get_safety_alert_reason();

/** Acquitte l'alerte de sécurité capteur/température. */
void heater_clear_sensor_safety_alert();

#endif /* CORE_HEATER_FSM_H */
