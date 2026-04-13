/**
 * @file battery.h
 * @brief Lecture du niveau de batterie via MAX17048 (I2C)
 *
 * Le MAX17048 est un fuel gauge LiPo/Li-ion qui fournit
 * directement le pourcentage de charge et la tension.
 * Connecté sur le même bus I2C que l'AHT21 (SDA=27, SCL=22).
 *
 * Lecture toutes les 30 secondes (la batterie ne change pas vite).
 */

#ifndef HAL_BATTERY_H
#define HAL_BATTERY_H

/**
 * Initialise le MAX17048 sur le bus I2C existant.
 * Wire.begin() doit avoir été appelé avant (fait par sensor_init).
 *
 * @return true si le MAX17048 est détecté
 */
bool battery_init();

/**
 * Met à jour la lecture batterie si l'intervalle est écoulé (30s).
 * À appeler dans loop().
 */
void battery_update();

/** Retourne le pourcentage de charge (0.0 à 100.0). */
float battery_get_percent();

/** Retourne la tension en volts. */
float battery_get_voltage();

/** Retourne true si le MAX17048 est détecté et fonctionnel. */
bool battery_is_available();

/** Retourne true si la batterie est basse (< 10%). */
bool battery_is_low();

#endif /* HAL_BATTERY_H */
