/**
 * @file mode_thermostat.cpp
 * @brief Implementation du mode A — Thermostat avec hysterese
 *
 * Logique de decision (voir thermostat_policy.h pour la regle pure) :
 *   - PREMIERE decision apres activation : allumer si temperature < consigne
 *     (la bande d'hysterese ne doit pas retarder le premier allumage
 *     explicitement demande par l'utilisateur)
 *   - Ensuite : allumer si temperature < (consigne - hysterese)
 *   - Si temperature >= consigne → eteindre le chauffage
 *     (extinction rapide : aussi des que SETPOINT_CUT_READINGS lectures
 *     BRUTES consecutives atteignent la consigne, sans attendre le
 *     rattrapage de l'EMA — meme critere anti-glitch que le mode C)
 *   - Entre les deux (zone morte) → ne rien changer
 *
 * Les decisions sont prises a CHAQUE nouvelle lecture du capteur
 * (~5-10 s). L'ancien tick de 60 s laissait passer un depassement
 * bref de la consigne entre deux decisions (aucune coupure alors que
 * l'ecran affichait une temperature au-dessus de la consigne).
 * L'anti-cyclage du relais est garanti par la bande d'hysterese
 * elle-meme : apres une extinction a la consigne, le reallumage exige
 * une chute reelle sous (consigne - hysterese) — de la dynamique
 * thermique, pas de la lenteur d'evaluation.
 */

#include "mode_thermostat.h"
#include "heater_fsm.h"
#include "thermostat_policy.h"
#include "setpoint_cut.h"
#include "../hal/sensor.h"
#include "../comm/relay_link.h"
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

/** Horodatage de la derniere lecture capteur deja evaluee : une
 *  decision n'est prise que lorsqu'une NOUVELLE lecture est disponible
 *  (inutile de re-evaluer une valeur qui n'a pas change). */
static unsigned long s_last_reading_ms = 0;

/** Premiere evaluation apres demarrage (pour forcer une evaluation immediate) */
static bool s_first_eval = true;

/** Critere d'extinction rapide sur lectures brutes consecutives
 *  (anti-glitch, partage avec le mode consigne — setpoint_cut.h) */
static SetpointCut s_off_cut;

/* ==========================================
 * Fonctions publiques
 * ========================================== */

bool thermostat_start(float setpoint, int hysteresis)
{
    /* I4 — Le mode A nécessite un capteur fonctionnel */
    if (!sensor_has_valid_reading() || sensor_is_error()) {
        Serial.println("[THERMOSTAT] Demarrage refuse : capteur non pret");
        return false;
    }

    /* I4bis — La lecture doit aussi être RÉCENTE : en veille écran sans
     * mode actif, le capteur n'est plus lu du tout ; au réveil, la
     * température est gelée depuis la mise en veille et l'état d'erreur
     * est lui aussi obsolète — les deux tests ci-dessus passeraient à
     * tort. Une lecture fraîche est forcée au réveil (power_manager) :
     * elle arrive en ~0 s (AHT21) à ~800 ms (conversion DS18B20). */
    if (!sensor_reading_is_recent(SENSOR_MAX_AGE_BEFORE_START_MS)) {
        Serial.println("[THERMOSTAT] Demarrage refuse : mesure obsolete, "
                       "lecture en cours");
        return false;
    }

    s_setpoint   = constrain(setpoint, SETPOINT_MIN, SETPOINT_MAX);
    s_hysteresis = constrain(hysteresis, HYSTERESIS_MIN, HYSTERESIS_MAX);
    s_active     = true;
    s_first_eval = true;
    setpoint_cut_reset(s_off_cut);
    /* Mémoriser la lecture courante : la première évaluation (forcée)
     * la consommera, les suivantes attendront des lectures neuves. */
    s_last_reading_ms = sensor_last_valid_reading_ms();

    Serial.println("[THERMOSTAT] Demarrage — consigne=" + String(s_setpoint, 1)
                   + "°C, hysterese=" + String(s_hysteresis) + "°C");
    return true;
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

    /*
     * Garde de securite C1bis : ne jamais decider avec un capteur mort.
     * En erreur critique (>5 min), la temperature est gelee a sa derniere
     * valeur : sans cette garde, si la valeur gelee est sous le seuil bas,
     * le thermostat redemanderait l'allumage a chaque decision alors que
     * heater_fsm_update() (C1) recouperait aussitot → cyclage ON/OFF du
     * Webasto toutes les 60 s. On arrete donc le mode.
     */
    if (sensor_is_critical_error()) {
        Serial.println("[THERMOSTAT] Erreur capteur critique → arret du mode");
        thermostat_stop();
        return;
    }

    /*
     * Ne decider que sur du neuf : soit la premiere evaluation (forcee
     * juste apres le demarrage), soit l'arrivee d'une NOUVELLE lecture
     * capteur (~5-10 s). L'ancien tick de 60 s pouvait laisser passer
     * un depassement bref de la consigne entre deux decisions ;
     * l'anti-cyclage est assure par la bande d'hysterese, pas par la
     * lenteur des evaluations.
     */
    unsigned long reading_ms = sensor_last_valid_reading_ms();
    bool          fresh      = (reading_ms != s_last_reading_ms);

    if (!s_first_eval && !fresh) {
        return;
    }

    /* La premiere decision utilise la consigne comme seuil d'allumage
     * (pas la bande) : memoriser AVANT de consommer le flag. */
    bool initial = s_first_eval;
    s_first_eval      = false;
    s_last_reading_ms = reading_ms;

    /* Les deux estimateurs : lissee (EMA) et derniere lecture brute.
     * L'allumage exige leur accord (anti-oscillation apres une
     * extinction rapide — voir thermostat_policy.h). */
    float temp = sensor_get_temperature();
    float raw  = sensor_get_raw_temperature();

    bool heating = (heater_get_state() == HEATER_HEATING);

    /* Decision pure (allumage/extinction/rien) — voir thermostat_policy.h */
    ThermostatAction action = thermostat_decide(temp, raw, s_setpoint,
                                                (float)s_hysteresis,
                                                heating, initial);

    /*
     * Extinction RAPIDE : l'EMA traine ~45 s derriere la temperature
     * reelle ; sans ce raccourci, le chauffage continuerait bien
     * au-dela de la consigne. Couper des que SETPOINT_CUT_READINGS
     * lectures BRUTES consecutives atteignent la consigne (critere
     * anti-glitch partage avec le mode C). L'ALLUMAGE, lui, reste
     * uniquement sur l'EMA : allumer le Webasto sur une lecture
     * aberrante serait pire qu'attendre une lecture de plus.
     */
    bool raw_cut = false;
    if (heating) {
        if (action == THERMO_NONE && fresh &&
            setpoint_cut_update(s_off_cut, raw, s_setpoint)) {
            action  = THERMO_HEAT_OFF;
            raw_cut = true;
        }
    } else {
        /* Pas en chauffe : le compteur repart de zero pour la
         * prochaine session */
        setpoint_cut_reset(s_off_cut);
    }

    switch (action) {
        case THERMO_HEAT_ON:
            /* N'allumer que depuis IDLE et relais joignable : pendant
             * le verrou anti-redémarrage (LOCKED) ou une coupure de
             * liaison, demander en boucle ne ferait que du bruit
             * (refus systématique) — on attend silencieusement, la
             * demande aboutira à la lecture suivant le retour à la
             * normale (reprise automatique du chauffage). */
            if (heater_get_state() == HEATER_IDLE && relay_is_connected()) {
                Serial.println("[THERMOSTAT] Temp=" + String(temp, 1)
                               + "°C < seuil=" + String(initial ? s_setpoint
                                     : s_setpoint - (float)s_hysteresis, 1)
                               + "°C" + (initial ? " (1er allumage)" : "")
                               + " → demande allumage");
                heater_request_on();
            }
            break;

        case THERMO_HEAT_OFF:
            if (raw_cut) {
                Serial.println("[THERMOSTAT] " + String(SETPOINT_CUT_READINGS)
                               + " lectures brutes >= consigne="
                               + String(s_setpoint, 1)
                               + "°C (brute=" + String(raw, 1)
                               + "°C, lissee=" + String(temp, 1)
                               + "°C) → demande extinction rapide");
            } else {
                Serial.println("[THERMOSTAT] Temp=" + String(temp, 1)
                               + "°C >= consigne=" + String(s_setpoint, 1)
                               + "°C → demande extinction");
            }
            heater_request_off();
            break;

        case THERMO_NONE:
        default:
            /* Zone morte ou etat deja correct : pas de log repetitif */
            break;
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
