/**
 * @file sensor.h
 * @brief Lecture du capteur AHT21 avec lissage EMA
 *
 * Gère la lecture périodique (toutes les 2s) du capteur de température
 * et d'humidité AHT21 sur le bus I2C externe (CN1).
 * Applique un lissage par moyenne mobile exponentielle (α=0.1).
 * Détecte les erreurs de lecture et gère le timeout de sécurité (5 min).
 */

#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

/**
 * Initialise le bus I2C externe et le capteur AHT21.
 * Wire.begin(SDA=27, SCL=22) sur le connecteur CN1.
 *
 * @return true si le capteur est détecté et initialisé
 */
bool sensor_init();

/**
 * Met à jour la lecture du capteur si l'intervalle est écoulé.
 * À appeler dans la boucle principale (loop).
 * Applique le lissage EMA sur la température.
 *
 * @param interval_ms Intervalle de lecture en ms (0 = utiliser la valeur par défaut).
 *                    Permet d'espacer les lectures en mode veille.
 */
void sensor_update(unsigned long interval_ms = 0);

/** Retourne la température lissée (EMA) en °C.
 *  ATTENTION : retourne 0.0 si aucune lecture valide n'a été faite.
 *  Vérifier sensor_has_valid_reading() avant d'utiliser cette valeur. */
float sensor_get_temperature();

/** Retourne true si au moins une lecture valide a été effectuée.
 *  Tant que false, sensor_get_temperature() n'est pas fiable. */
bool sensor_has_valid_reading();

/** Retourne l'humidité relative en %. */
float sensor_get_humidity();

/** Retourne true si le capteur est en erreur (lecture échouée). */
bool sensor_is_error();

/**
 * Retourne true si le capteur est en erreur critique :
 * plus de 5 minutes sans lecture valide.
 * Le chauffage doit être arrêté par sécurité.
 */
bool sensor_is_critical_error();

/** Retourne la durée de l'erreur en cours (en ms), ou 0 si pas d'erreur. */
unsigned long sensor_error_duration_ms();

#endif /* HAL_SENSOR_H */
