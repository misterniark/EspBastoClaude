/**
 * @file storage.h
 * @brief Stockage persistant des paramètres via NVS (Preferences)
 *
 * Sauvegarde et restaure les paramètres utilisateur :
 * - Setpoint (consigne de température)
 * - Hystérésis
 * - Durée minuteur
 * - Dernier mode utilisé
 * - Adresse MAC du relais (découverte automatique)
 *
 * Utilise le namespace NVS "espbasto".
 */

#ifndef CORE_STORAGE_H
#define CORE_STORAGE_H

#include <cstdint>

/**
 * Initialise le stockage NVS.
 * Ouvre le namespace "espbasto" en lecture/écriture.
 */
void storage_init();

/* --- Setpoint (consigne température) --- */
float storage_load_setpoint();
void  storage_save_setpoint(float value);

/* --- Hystérésis --- */
int  storage_load_hysteresis();
void storage_save_hysteresis(int value);

/* --- Durée minuteur (en minutes) --- */
int  storage_load_timer_min();
void storage_save_timer_min(int value);

/* --- Dernier mode utilisé (0=A, 1=B, 2=C) --- */
int  storage_load_last_mode();
void storage_save_last_mode(int mode);

/* --- Adresse MAC du relais (6 octets) --- */

/**
 * Charge l'adresse MAC du relais depuis NVS.
 *
 * @param mac_out Buffer de 6 octets pour stocker la MAC chargée
 * @return true si une MAC valide a été trouvée en NVS
 */
bool storage_load_relay_mac(uint8_t *mac_out);

/**
 * Sauvegarde l'adresse MAC du relais en NVS.
 *
 * @param mac Adresse MAC à sauvegarder (6 octets)
 */
void storage_save_relay_mac(const uint8_t *mac);

#endif /* CORE_STORAGE_H */
