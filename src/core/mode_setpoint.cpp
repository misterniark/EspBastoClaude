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
 * Les decisions sont prises a CHAQUE nouvelle lecture du capteur
 * (~5-10 s) et non toutes les 60 s comme le thermostat : la coupure
 * etant unique et definitive, il n'y a aucun risque de cyclage du
 * relais, donc aucune raison d'attendre — un intervalle de 60 s
 * laissait le chauffage depasser la cible demandee.
 *
 * Deux criteres de coupure complementaires (voir setpoint_cut.h) :
 *   (a) temperature lissee EMA >= cible : preuve durable, mais en
 *       retard (~45 s) sur la temperature reelle ;
 *   (b) SETPOINT_CUT_READINGS lectures BRUTES consecutives >= cible :
 *       reactif (~10 s apres le franchissement reel) et insensible a
 *       une lecture aberrante isolee.
 */

#include "mode_setpoint.h"
#include "heater_fsm.h"
#include "setpoint_cut.h"
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

/** Horodatage de la derniere lecture capteur deja evaluee : une
 *  decision n'est prise que lorsqu'une NOUVELLE lecture est disponible
 *  (inutile de re-evaluer une valeur qui n'a pas change). */
static unsigned long s_last_reading_ms = 0;

/** Premiere evaluation apres demarrage (pour forcer une evaluation immediate) */
static bool s_first_eval = true;

/** Critere de coupure sur lectures brutes consecutives (anti-glitch) */
static SetpointCut s_cut;

/* ==========================================
 * Fonctions publiques
 * ========================================== */

bool setpoint_mode_start(float target)
{
    /* I4 — Le mode C nécessite un capteur fonctionnel */
    if (!sensor_has_valid_reading() || sensor_is_error()) {
        Serial.println("[SETPOINT] Demarrage refuse : capteur non pret");
        return false;
    }

    /* I4bis — La lecture doit aussi être RÉCENTE (même garde que le
     * thermostat, voir mode_thermostat.cpp) : au réveil d'une veille
     * sans mode actif, la température est gelée depuis des heures et
     * les deux tests ci-dessus passeraient à tort. */
    if (!sensor_reading_is_recent(SENSOR_MAX_AGE_BEFORE_START_MS)) {
        Serial.println("[SETPOINT] Demarrage refuse : mesure obsolete, "
                       "lecture en cours");
        return false;
    }

    /* Demander l'allumage AVANT d'activer le mode : si le chauffage est
     * indisponible (verrou anti-redémarrage, relais déconnecté), le mode
     * consigne — qui ne réessaie jamais — tournerait pour rien : cible
     * jamais atteinte, chauffage jamais allumé, sans aucun message.
     * (Constaté au banc de test du 27/07/2026.) */
    if (!heater_request_on()) {
        Serial.println("[SETPOINT] Demarrage refuse : chauffage indisponible "
                       "(verrou ou relais deconnecte)");
        return false;
    }

    s_target   = constrain(target, SETPOINT_MIN, SETPOINT_MAX);
    s_active   = true;
    s_reached  = false;
    s_first_eval = true;
    setpoint_cut_reset(s_cut);
    /* Mémoriser la lecture courante : la première évaluation (forcée)
     * la consommera, les suivantes attendront des lectures neuves. */
    s_last_reading_ms = sensor_last_valid_reading_ms();

    Serial.println("[SETPOINT] Demarrage — cible=" + String(s_target, 1) + "°C");
    return true;
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

    /*
     * Garde de securite C1bis (comme le thermostat) : avec un capteur en
     * erreur critique, la temperature est gelee et la cible ne sera jamais
     * legitimement "atteinte" — le mode resterait actif indefiniment alors
     * que heater_fsm (C1) a deja coupe le chauffage. On arrete le mode.
     */
    if (sensor_is_critical_error()) {
        Serial.println("[SETPOINT] Erreur capteur critique → arret du mode");
        setpoint_mode_stop();
        return;
    }

    /*
     * Ne decider que sur du neuf : soit la premiere evaluation (forcee
     * juste apres le demarrage), soit l'arrivee d'une NOUVELLE lecture
     * capteur (~5-10 s). Pas d'intervalle de decision arbitraire : la
     * coupure est unique et definitive, aucun risque de cyclage.
     */
    unsigned long reading_ms = sensor_last_valid_reading_ms();
    bool          fresh      = (reading_ms != s_last_reading_ms);

    if (!s_first_eval && !fresh) {
        return;
    }
    s_first_eval      = false;
    s_last_reading_ms = reading_ms;

    float filtered = sensor_get_temperature();
    float raw      = sensor_get_raw_temperature();

    /*
     * Critere (a) : la temperature lissee EMA a atteint la cible.
     * Preuve durable (le lissage absorbe les glitchs) mais en retard
     * sur la realite — couvre aussi le cas d'un demarrage alors que
     * la piece est deja a temperature.
     */
    bool cut = (filtered >= s_target);

    /*
     * Critere (b) : SETPOINT_CUT_READINGS lectures brutes consecutives
     * >= cible. Coupe ~10 s apres le franchissement reel, sans subir
     * le retard EMA, tout en absorbant une lecture aberrante isolee.
     * (Uniquement sur lecture neuve : re-compter la meme valeur
     * fausserait la notion de "lectures consecutives".)
     */
    if (!cut && fresh) {
        cut = setpoint_cut_update(s_cut, raw, s_target);
    }

    if (cut) {
        /*
         * La temperature cible est atteinte : arret definitif.
         * Le flag reached empechera toute reprise automatique.
         */
        s_reached = true;
        s_active  = false;

        Serial.println("[SETPOINT] Cible atteinte — brute=" + String(raw, 1)
                       + "°C, lissee=" + String(filtered, 1)
                       + "°C, cible=" + String(s_target, 1)
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
    /* Les lectures deja comptees l'ont ete contre l'ANCIENNE cible :
     * repartir de zero pour que le critere multi-lectures reste
     * coherent avec la nouvelle. */
    setpoint_cut_reset(s_cut);
    Serial.println("[SETPOINT] Nouvelle cible=" + String(s_target, 1) + "°C");
}
