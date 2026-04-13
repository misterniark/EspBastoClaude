/**
 * @file mode_thermostat.h
 * @brief Mode A — Thermostat avec hysterese
 *
 * Regulation de temperature par hysterese autour d'une consigne.
 * Exemple : consigne = 21°C, hysterese = 3°C
 *   - Allumage en dessous de 18°C (consigne - hysterese)
 *   - Extinction a partir de 21°C (consigne atteinte)
 *   - Zone morte entre 18°C et 21°C : etat inchange
 *
 * Le thermostat evalue la temperature toutes les 60 secondes
 * (THERMOSTAT_DECISION_INTERVAL_MS) et appelle heater_request_on()
 * ou heater_request_off() selon la decision.
 */

#ifndef CORE_MODE_THERMOSTAT_H
#define CORE_MODE_THERMOSTAT_H

/**
 * Demarre le mode thermostat avec les parametres donnes.
 * La premiere evaluation a lieu immediatement.
 *
 * @param setpoint   Temperature de consigne en °C
 * @param hysteresis Hysterese en °C (ecart sous la consigne pour allumer)
 */
void thermostat_start(float setpoint, int hysteresis);

/**
 * Arrete le mode thermostat.
 * N'envoie PAS de commande d'arret au chauffage : c'est au
 * code appelant de gerer l'arret si necessaire.
 */
void thermostat_stop();

/**
 * Mise a jour periodique du thermostat.
 * A appeler dans la boucle principale (loop).
 * Evalue la temperature toutes les THERMOSTAT_DECISION_INTERVAL_MS
 * et decide d'allumer ou d'eteindre le chauffage.
 */
void thermostat_update();

/** Retourne true si le mode thermostat est actif. */
bool thermostat_is_active();

/** Retourne la consigne actuelle en °C. */
float thermostat_get_setpoint();

/**
 * Modifie la consigne.
 * Prend effet au prochain cycle d'evaluation.
 *
 * @param setpoint Nouvelle consigne en °C
 */
void thermostat_set_setpoint(float setpoint);

/** Retourne la valeur d'hysterese actuelle en °C. */
int thermostat_get_hysteresis();

/**
 * Modifie l'hysterese.
 * Prend effet au prochain cycle d'evaluation.
 *
 * @param hysteresis Nouvelle valeur d'hysterese en °C
 */
void thermostat_set_hysteresis(int hysteresis);

#endif /* CORE_MODE_THERMOSTAT_H */
