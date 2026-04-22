/**
 * @file mode_thermostat.cpp
 * @brief Implementation du mode A — Thermostat avec hysterese
 *
 * Logique de decision :
 *   - Si temperature < (consigne - hysterese) → allumer le chauffage
 *   - Si temperature >= consigne → eteindre le chauffage
 *   - Entre les deux (zone morte) → ne rien changer
 *
 * Les decisions sont prises toutes les 60 secondes pour eviter
 * des basculements trop frequents. La temperature utilisee est
 * deja lissee par EMA dans le module sensor.
 */

#include "mode_thermostat.h"
#include "heater_fsm.h"
#include "../hal/sensor.h"
#include "../config.h"
#include <Arduino.h>

/* ==========================================
 * Variables internes du module
 * ========================================== */

/** Indique si le mode thermostat est actif */
static bool s_active = false;

/** Consigne de temperature en °C */
static float s_setpoint = DEFAULT_SETPOINT;

/** Hysterese en °C (ecart sous la consigne pour declencher l'allumage) */
static int s_hysteresis = DEFAULT_HYSTERESIS;

/** Timestamp de la derniere evaluation (millis) */
static unsigned long s_last_eval_ms = 0;

/** Premiere evaluation apres demarrage (pour forcer une evaluation immediate) */
static bool s_first_eval = true;

/* ==========================================
 * Fonctions publiques
 * ========================================== */

void thermostat_start(float setpoint, int hysteresis)
{
    /* I4 — Le mode A nécessite un capteur fonctionnel */
    if (!sensor_has_valid_reading() || sensor_is_error()) {
        Serial.println("[THERMOSTAT] Demarrage refuse : capteur non pret");
        return;
    }

    s_setpoint   = constrain(setpoint, SETPOINT_MIN, SETPOINT_MAX);
    s_hysteresis = constrain(hysteresis, HYSTERESIS_MIN, HYSTERESIS_MAX);
    s_active     = true;
    s_first_eval = true;
    s_last_eval_ms = millis();

    Serial.println("[THERMOSTAT] Demarrage — consigne=" + String(s_setpoint, 1)
                   + "°C, hysterese=" + String(s_hysteresis) + "°C");
}

void thermostat_stop()
{
    s_active = false;
    /* I6 — Arrêter le chauffage si en cours (ne pas déléguer à l'appelant) */
    if (heater_get_state() == HEATER_HEATING) {
        heater_request_off();
    }
    Serial.println("[THERMOSTAT] Arret du mode thermostat");
}

void thermostat_update()
{
    /* Ne rien faire si le mode n'est pas actif */
    if (!s_active) {
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

    /* Seuil bas : en dessous, on allume le chauffage */
    float seuil_bas = s_setpoint - (float)s_hysteresis;

    HeaterState etat = heater_get_state();

    /*
     * Decision par hysterese :
     *   - temp < seuil_bas      → demander l'allumage
     *   - temp >= consigne       → demander l'extinction
     *   - entre les deux         → zone morte, on ne change rien
     */
    if (temp < seuil_bas) {
        /* Temperature sous le seuil bas : allumer si pas deja en chauffe */
        if (etat != HEATER_HEATING) {
            Serial.println("[THERMOSTAT] Temp=" + String(temp, 1)
                           + "°C < seuil_bas=" + String(seuil_bas, 1)
                           + "°C → demande allumage");
            heater_request_on();
        }
    } else if (temp >= s_setpoint) {
        /* Temperature atteint ou depasse la consigne : eteindre si en chauffe */
        if (etat == HEATER_HEATING) {
            Serial.println("[THERMOSTAT] Temp=" + String(temp, 1)
                           + "°C >= consigne=" + String(s_setpoint, 1)
                           + "°C → demande extinction");
            heater_request_off();
        }
    } else {
        /* Zone morte : on conserve l'etat actuel, pas de log repetitif */
    }
}

bool thermostat_is_active()
{
    return s_active;
}

float thermostat_get_setpoint()
{
    return s_setpoint;
}

void thermostat_set_setpoint(float setpoint)
{
    /* I2 — Clamp de la valeur pour éviter les valeurs NVS corrompues */
    s_setpoint = constrain(setpoint, SETPOINT_MIN, SETPOINT_MAX);
    Serial.println("[THERMOSTAT] Nouvelle consigne=" + String(s_setpoint, 1) + "°C");
}

int thermostat_get_hysteresis()
{
    return s_hysteresis;
}

void thermostat_set_hysteresis(int hysteresis)
{
    /* I2 — Clamp de la valeur */
    s_hysteresis = constrain(hysteresis, HYSTERESIS_MIN, HYSTERESIS_MAX);
    Serial.println("[THERMOSTAT] Nouvelle hysterese=" + String(s_hysteresis) + "°C");
}
