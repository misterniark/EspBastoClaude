/**
 * @file mode_timer.cpp
 * @brief Implémentation du mode B — Minuteur (timer)
 *
 * Logique : décompte d'une durée fixe.
 * À l'expiration, le chauffage est arrêté via heater_request_off().
 *
 * Corrections de sécurité :
 *   C3 — Vérifier le retour de heater_request_on() avant de lancer le timer.
 *   C4bis — Arrêter le décompte si un arrêt de sécurité (C1/C4 dans
 *           heater_fsm) a coupé le chauffage pendant le décompte.
 *   I2 — Clamp de la durée dans le setter.
 *   I6 — timer_mode_stop() appelle heater_request_off() directement.
 */

#include "mode_timer.h"
#include "heater_fsm.h"
#include "timer_safety.h"
#include "../hal/sensor.h"
#include "../config.h"
#include <Arduino.h>

/* Variables internes */
static bool s_running = false;
static int s_duration_min = DEFAULT_TIMER_MIN;
static unsigned long s_start_ms = 0;
static unsigned long s_duration_ms = 0;

/* Détection de front des arrêts de sécurité (voir timer_safety.h) */
static TimerSafetyGate s_safety_gate;

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

    /* Ignorer une alerte de sécurité antérieure non encore acquittée :
     * elle concerne un épisode passé, pas ce décompte (voir
     * timer_safety.h — sans cela le minuteur s'arrêtait aussitôt). */
    timer_safety_arm(s_safety_gate, heater_get_safety_alert_reason());
}

void timer_mode_stop()
{
    if (s_running) {
        s_running = false;
        Serial.println("[TIMER] Arret manuel");

        /* I6 — Arrêter le chauffage, INCONDITIONNELLEMENT
         * (voir heater_force_off : cas de l'ACK_ON perdu) */
        heater_force_off();
    }
}

void timer_mode_update()
{
    if (!s_running) return;

    /*
     * Garde de securite C4bis (pendant du C1bis des modes A/C) : si un
     * arret de securite (capteur mort, temperature max, ou relais
     * arrete de lui-meme) a coupe le chauffage PENDANT le decompte,
     * arreter aussi le minuteur. Sans cette garde, le decompte
     * continuerait de s'afficher alors que le chauffage est coupe et ne
     * redemarrera jamais (le minuteur ne rallume jamais en cours de
     * route).
     *
     * Detection de FRONT (timer_safety.h) : une alerte anterieure non
     * acquittee ne doit pas tuer un decompte qui vient de demarrer.
     */
    if (timer_safety_triggered(s_safety_gate, heater_get_safety_alert_reason())) {
        s_running = false;
        Serial.println("[TIMER] Arret de securite du chauffage -> arret du minuteur");
        /* heater_fsm a normalement deja coupe, mais ne pas en dependre :
         * un arret redondant est sans effet cote relais, alors qu'un
         * chauffage laisse allume sans minuteur n'a plus aucune borne. */
        heater_force_off();
        return;
    }

    unsigned long now     = millis();
    unsigned long elapsed = now - s_start_ms;

    /* Vérifier si le décompte est terminé */
    if (elapsed >= s_duration_ms) {
        s_running = false;
        Serial.println("[TIMER] Decompte termine — arret du chauffage");

        /* Arrêt INCONDITIONNEL : c'est LA borne de ce mode. Si un
         * ACK_ON perdu a laissé la FSM en IDLE, ne pas envoyer l'arrêt
         * ici laisserait le Webasto tourner sans aucune limite. */
        heater_force_off();
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
