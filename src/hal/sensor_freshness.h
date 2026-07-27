/**
 * @file sensor_freshness.h
 * @brief Logique pure de fraîcheur d'une lecture capteur
 *
 * Fonction sans dépendance Arduino/matériel, testable nativement
 * (pio test -e native).
 *
 * Problème résolu : quand l'écran est en veille sans aucun mode de
 * chauffage actif, la boucle principale ne lit plus le capteur du tout
 * (économie de batterie — voir main.cpp, bloc « 2. Capteur »). Au réveil,
 * sensor_get_temperature() renvoie alors une valeur gelée depuis
 * potentiellement des heures, et l'état d'erreur est lui aussi obsolète.
 * Les gardes I4 de thermostat_start() / setpoint_mode_start(), qui ne
 * testaient que « une lecture valide a existé » et « pas d'erreur en
 * cours », passaient donc à tort.
 *
 * Cette fonction fournit le critère manquant : la dernière lecture
 * VALIDE est-elle assez récente pour fonder une décision de chauffage ?
 */

#ifndef HAL_SENSOR_FRESHNESS_H
#define HAL_SENSOR_FRESHNESS_H

/* stdint.h (et non cstdint) : la toolchain native des tests unitaires
 * ne fournit pas les en-têtes C++ — même choix que touch_mapping.h */
#include <stdint.h>

/**
 * Indique si la dernière lecture valide du capteur est assez récente.
 *
 * Les horodatages sont en uint32_t (même largeur que millis() sur
 * ESP32) : la soustraction non signée `now - last` reste correcte au
 * débordement de millis() (~49,7 jours), tant que l'âge réel de la
 * lecture est inférieur à un tour complet de compteur.
 *
 * @param now_ms            Horodatage courant (millis)
 * @param last_valid_ms     Horodatage de la dernière lecture VALIDE (millis)
 * @param has_valid_reading false si aucune lecture valide n'a jamais eu
 *                          lieu (last_valid_ms est alors sans signification)
 * @param max_age_ms        Âge maximal accepté, borne INCLUSE
 * @return true si une lecture valide existe et date d'au plus max_age_ms
 */
inline bool sensor_reading_is_fresh(uint32_t now_ms, uint32_t last_valid_ms,
                                    bool has_valid_reading, uint32_t max_age_ms)
{
    /* Sans lecture valide, la température courante est le 0.0 initial :
     * aucune fraîcheur possible, quel que soit l'horodatage */
    if (!has_valid_reading) return false;

    /* Arithmétique non signée : gère le débordement de millis() */
    return (uint32_t)(now_ms - last_valid_ms) <= max_age_ms;
}

#endif /* HAL_SENSOR_FRESHNESS_H */
