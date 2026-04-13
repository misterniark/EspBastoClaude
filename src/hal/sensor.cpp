/**
 * @file sensor.cpp
 * @brief Implémentation de la lecture AHT21 avec lissage EMA
 *
 * Le capteur AHT21 est connecté sur le bus I2C externe (CN1) :
 * SDA = GPIO27, SCL = GPIO22.
 *
 * Lissage EMA (Exponential Moving Average) :
 *   valeur_lissée = α × nouvelle_lecture + (1 - α) × valeur_précédente
 *   Avec α = 0.1, la convergence à 95% prend ~60 secondes (30 lectures).
 *
 * Gestion d'erreur :
 *   - Si la lecture échoue, on garde la dernière valeur valide.
 *   - Après 5 minutes d'erreur continue, on signale une erreur critique
 *     pour que le chauffage soit arrêté par sécurité.
 */

#include "sensor.h"
#include "../config.h"
#include <Adafruit_AHTX0.h>
#include <Wire.h>

static Adafruit_AHTX0 aht;

/* Valeurs lissées */
static float filtered_temp     = 0.0f;
static float current_humidity  = 0.0f;

/* État d'erreur */
static bool         in_error         = false;
static bool         first_reading    = true;
static unsigned long error_start_ms  = 0;
static unsigned long last_read_ms    = 0;

/* Flag d'initialisation réussie */
static bool initialized = false;

bool sensor_init()
{
    /* Initialiser le bus I2C sur les pins du connecteur CN1 */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    /* Tenter d'initialiser le capteur AHT21 */
    if (!aht.begin(&Wire)) {
        Serial.println("[SENSOR] Erreur : AHT21 non détecté sur I2C (SDA=27, SCL=22)");
        in_error = true;
        error_start_ms = millis();
        return false;
    }

    Serial.println("[SENSOR] AHT21 initialisé avec succès");
    initialized = true;
    last_read_ms = millis();
    return true;
}

void sensor_update(unsigned long interval_ms)
{
    /* Utiliser l'intervalle par défaut si non spécifié */
    unsigned long interval = (interval_ms > 0) ? interval_ms : SENSOR_READ_INTERVAL_MS;

    /* Ne rien faire si l'intervalle n'est pas écoulé */
    unsigned long now = millis();
    if (now - last_read_ms < interval) return;
    last_read_ms = now;

    /* Tenter une lecture */
    sensors_event_t humidity_event, temp_event;

    if (!initialized) {
        /* Tenter de réinitialiser le capteur s'il n'était pas détecté */
        if (aht.begin(&Wire)) {
            initialized = true;
            Serial.println("[SENSOR] AHT21 reconnecté");
        } else {
            /* Toujours en erreur, mettre à jour la durée */
            if (!in_error) {
                in_error = true;
                error_start_ms = now;
            }
            return;
        }
    }

    if (aht.getEvent(&humidity_event, &temp_event)) {
        /* Lecture réussie */
        float raw_temp = temp_event.temperature;
        current_humidity = humidity_event.relative_humidity;

        /* Première lecture : initialiser directement (pas de lissage) */
        if (first_reading) {
            filtered_temp = raw_temp;
            first_reading = false;
            Serial.printf("[SENSOR] Première lecture : %.1f°C, %.0f%%\n",
                          raw_temp, current_humidity);
        } else {
            /* Lissage EMA : α × nouveau + (1 - α) × ancien */
            filtered_temp = SENSOR_EMA_ALPHA * raw_temp
                          + (1.0f - SENSOR_EMA_ALPHA) * filtered_temp;
        }

        /* Réinitialiser l'état d'erreur */
        if (in_error) {
            Serial.println("[SENSOR] Erreur résolue, lecture reprise");
        }
        in_error = false;
        error_start_ms = 0;
    } else {
        /* Lecture échouée */
        if (!in_error) {
            in_error = true;
            error_start_ms = now;
            Serial.println("[SENSOR] Erreur de lecture AHT21");
        }
        /* La valeur lissée reste inchangée (dernière valeur valide) */
    }
}

float sensor_get_temperature()
{
    return filtered_temp;
}

float sensor_get_humidity()
{
    return current_humidity;
}

bool sensor_is_error()
{
    return in_error;
}

bool sensor_is_critical_error()
{
    if (!in_error) return false;
    return (millis() - error_start_ms) >= SENSOR_ERROR_TIMEOUT_MS;
}

unsigned long sensor_error_duration_ms()
{
    if (!in_error) return 0;
    return millis() - error_start_ms;
}

bool sensor_has_valid_reading()
{
    return !first_reading;
}
