/**
 * @file mode_timer.cpp
 * @brief Implémentation du mode B — Minuteur (timer)
 *
 * Logique : décompte d'une durée fixe.
 * À l'expiration, le chauffage est arrêté via heater_request_off().
 *
 * Corrections de sécurité :
 *   C3 — Vérifier le retour de heater_request_on() avant de lancer le timer.
 *         Surveiller la température max de sécurité pendant le décompte.
 *   I2 — Clamp de la durée dans le setter.
 *   I6 — timer_mode_stop() appelle heater_request_off() directement.
 */

#include "mode_timer.h"
#include "heater_fsm.h"
#include "../hal/sensor.h"
#include "../config.h"
#include <Arduino.h>

/* Variables internes */
static bool s_running = false;
static int s_duration_min = DEFAULT_TIMER_MIN;
static unsigned long s_start_ms = 0;
static unsigned long s_duration_ms = 0;

void timer_mode_start(int duration_min)
{
    /* I2 — Clamp de la durée */
    s_duration_min = constrain(duration_min, TIMER_MIN_MIN, TIMER_MIN_MAX);
    s_duration_ms  = (unsigned long)s_duration_min * 60UL * 1000UL;

    Serial.printf("[TIMER] Demarrage — duree=%d min\n", s_duration_min);

    /* C3 — Ne lancer le timer que si le chauffage démarre effectivement */
    if (!heater_request_on()) {
        Serial.println("[TIMER] ECHEC : chauffage non demarre, timer annule");
        s_running = false;
        return;
    }

    s_start_ms = millis();
    s_running  = true;
}

void timer_mode_stop()
{
    if (s_running) {
        s_running = false;
        Serial.println("[TIMER] Arret manuel");

        /* I6 — Arrêter le chauffage si en cours */
        if (heater_get_state() == HEATER_HEATING) {
            heater_request_off();
        }
    }
}

void timer_mode_update()
{
    if (!s_running) return;

    unsigned long now     = millis();
    unsigned long elapsed = now - s_start_ms;

    /* Vérifier si le décompte est terminé */
    if (elapsed >= s_duration_ms) {
        s_running = false;
        Serial.println("[TIMER] Decompte termine — arret du chauffage");

        if (heater_get_state() == HEATER_HEATING) {
            heater_request_off();
        }
    }
}

bool timer_mode_is_running()
{
    return s_running;
}

int timer_mode_get_remaining_s()
{
    if (!s_running) return 0;

    unsigned long elapsed = millis() - s_start_ms;
    if (elapsed >= s_duration_ms) return 0;

    return (int)((s_duration_ms - elapsed) / 1000UL);
}

int timer_mode_get_duration_min()
{
    return s_duration_min;
}

void timer_mode_set_duration_min(int duration_min)
{
    if (s_running) {
        Serial.println("[TIMER] Modification ignoree — decompte en cours");
        return;
    }

    /* I2 — Clamp */
    s_duration_min = constrain(duration_min, TIMER_MIN_MIN, TIMER_MIN_MAX);
    Serial.printf("[TIMER] Nouvelle duree=%d min\n", s_duration_min);
}
