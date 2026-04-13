/**
 * @file battery.cpp
 * @brief Implémentation de la lecture batterie MAX17048
 *
 * Le MAX17048 utilise un algorithme interne (ModelGauge) pour
 * calculer le pourcentage de charge sans shunt ni calibration.
 * Il suffit de lire les registres via I2C.
 *
 * Lecture espacée (30s) car le pourcentage évolue lentement.
 */

#include "battery.h"
#include "../config.h"
#include <Adafruit_MAX1704X.h>
#include <Wire.h>
#include <Arduino.h>

static Adafruit_MAX17048 fuel_gauge;
static bool available = false;

static float current_percent = 0.0f;
static float current_voltage = 0.0f;
static unsigned long last_read_ms = 0;

/* Intervalle de lecture : 30 secondes (la batterie ne bouge pas vite) */
constexpr unsigned long BATTERY_READ_INTERVAL_MS = 30000;

/* Seuil de batterie basse */
constexpr float BATTERY_LOW_PERCENT = 10.0f;

bool battery_init()
{
    /* Le bus I2C est déjà initialisé par sensor_init() (Wire.begin(27, 22)) */
    if (!fuel_gauge.begin(&Wire)) {
        Serial.println("[BATTERY] MAX17048 non detecte sur I2C");
        available = false;
        return false;
    }

    /* Première lecture immédiate */
    current_percent = fuel_gauge.cellPercent();
    current_voltage = fuel_gauge.cellVoltage();
    last_read_ms = millis();
    available = true;

    Serial.printf("[BATTERY] MAX17048 OK : %.0f%% (%.2fV)\n",
                  current_percent, current_voltage);
    return true;
}

void battery_update()
{
    if (!available) return;

    unsigned long now = millis();
    if (now - last_read_ms < BATTERY_READ_INTERVAL_MS) return;
    last_read_ms = now;

    current_percent = fuel_gauge.cellPercent();
    current_voltage = fuel_gauge.cellVoltage();

    /* Clamp le pourcentage entre 0 et 100 */
    if (current_percent < 0.0f) current_percent = 0.0f;
    if (current_percent > 100.0f) current_percent = 100.0f;
}

float battery_get_percent()
{
    return current_percent;
}

float battery_get_voltage()
{
    return current_voltage;
}

bool battery_is_available()
{
    return available;
}

bool battery_is_low()
{
    return available && (current_percent < BATTERY_LOW_PERCENT);
}
