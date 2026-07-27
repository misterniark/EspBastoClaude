/**
 * @file sensor_validation.h
 * @brief Validation pure des lectures de température DS18B20
 *
 * Fonction sans dépendance Arduino/matériel, testable nativement
 * (pio test -e native).
 *
 * Le DS18B20 peut renvoyer des valeurs particulières qu'il faut
 * rejeter avant de les injecter dans le lissage EMA et la logique
 * de chauffage :
 *   - -127.0 : code DEVICE_DISCONNECTED_C de DallasTemperature
 *              (sonde absente ou erreur de bus)
 *   - +85.0  : valeur de power-on-reset du DS18B20, renvoyée quand
 *              la conversion n'a pas réellement eu lieu (alimentation
 *              instable, lecture trop précoce). Une vraie température
 *              ambiante de 85.0°C pile est impossible dans un van :
 *              on préfère perdre une lecture que d'injecter 85°C
 *              dans le thermostat.
 *   - hors [-55, +125] : plage physique du capteur (datasheet)
 */

#ifndef HAL_SENSOR_VALIDATION_H
#define HAL_SENSOR_VALIDATION_H

/**
 * Vérifie qu'une lecture DS18B20 est plausible et exploitable.
 *
 * @param temp_c Température lue en °C
 * @return true si la valeur peut être utilisée, false si elle doit
 *         être rejetée (erreur de lecture à signaler)
 */
inline bool ds18b20_reading_is_valid(float temp_c)
{
    /* Plage physique du capteur (datasheet DS18B20 : -55 à +125°C).
     * Couvre aussi le code d'erreur -127 de DallasTemperature. */
    if (temp_c < -55.0f || temp_c > 125.0f) return false;

    /* Valeur de power-on-reset : conversion non effectuée */
    if (temp_c == 85.0f) return false;

    return true;
}

#endif /* HAL_SENSOR_VALIDATION_H */
