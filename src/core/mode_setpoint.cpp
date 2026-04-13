/**
 * @file mode_setpoint.cpp
 * @brief Implementation du mode C — Consigne simple (arret definitif)
 *
 * Logique de decision :
 *   - Si temperature < cible → le chauffage reste allume
 *   - Si temperature >= cible → arret definitif (flag reached = true)
 *
 * Contrairement au thermostat, une fois la cible atteinte le chauffage
 * ne redemarrera jamais automatiquement. Il faut relancer le mode
 * explicitement pour une nouvelle session de chauffe.
 *
 * Les decisions sont prises toutes les 60 secondes
 * (THERMOSTAT_DECISION_INTERVAL_MS).
 */

#include "mode_setpoint.h"
#include "heater_fsm.h"
#include "../hal/sensor.h"
#include "../config.h"
#include <Arduino.h>

/* ==========================================
 * Variables internes du module
 * ========================================== */

/** Indique si le mode consigne est actif */
static bool s_active = false;

/** Temperature cible en °C */
static float s_target = DEFAULT_SETPOINT;

/** Flag definitif : la consigne a ete atteinte, le chauffage est arrete */
static bool s_reached = false;

/** Timestamp de la derniere evaluation (millis) */
static unsigned long s_last_eval_ms = 0;

/** Premiere evaluation apres demarrage (pour forcer une evaluation immediate) */
static bool s_first_eval = true;

/* ==========================================
 * Fonctions publiques
 * ========================================== */

void setpoint_mode_start(float target)
{
    s_target   = target;
    s_active   = true;
    s_reached  = false;
    s_first_eval = true;
    s_last_eval_ms = millis();

    Serial.println("[SETPOINT] Demarrage — cible=" + String(s_target, 1) + "°C");

    /* Demander l'allumage du chauffage */
    heater_request_on();
}

void setpoint_mode_stop()
{
    if (s_active) {
        s_active = false;

        Serial.println("[SETPOINT] Arret du mode consigne");

        /* Eteindre le chauffage si en chauffe */
        if (heater_get_state() == HEATER_HEATING) {
            heater_request_off();
        }
    }
}

void setpoint_mode_update()
{
    /* Ne rien faire si le mode n'est pas actif ou si la cible est deja atteinte */
    if (!s_active || s_reached) {
        return;
    }

    unsigned long now = millis();

    /*
     * Verifier si l'intervalle de decision est ecoule.
     * La premiere evaluation est forcee immediatement apres le demarrage.
     */
    if (!s_first_eval && (now - s_last_eval_ms < THERMOSTAT_DECISION_INTERVAL_MS)) {
        return;
    }

    s_first_eval   = false;
    s_last_eval_ms = now;

    /* Lire la temperature lissee (EMA appliquee par le module sensor) */
    float temp = sensor_get_temperature();

    if (temp >= s_target) {
        /*
         * La temperature cible est atteinte : arret definitif.
         * Le flag reached empechera toute reprise automatique.
         */
        s_reached = true;
        s_active  = false;

        Serial.println("[SETPOINT] Cible atteinte — temp=" + String(temp, 1)
                       + "°C >= cible=" + String(s_target, 1)
                       + "°C → arret definitif");

        /* Eteindre le chauffage uniquement si en chauffe */
        if (heater_get_state() == HEATER_HEATING) {
            heater_request_off();
        }
    }
    /* Si temp < cible, on ne fait rien : le chauffage reste allume */
}

bool setpoint_mode_is_active()
{
    return s_active;
}

bool setpoint_mode_is_reached()
{
    return s_reached;
}

float setpoint_mode_get_target()
{
    return s_target;
}

void setpoint_mode_set_target(float target)
{
    /* I2 — Clamp de la valeur */
    s_target = constrain(target, SETPOINT_MIN, SETPOINT_MAX);
    Serial.println("[SETPOINT] Nouvelle cible=" + String(s_target, 1) + "°C");
}
