/**
 * @file setpoint_cut.h
 * @brief Critère de coupure du mode Consigne — logique pure, testable
 *
 * Problème résolu : la coupure du mode consigne est DÉFINITIVE (pas de
 * reprise automatique). Elle doit donc être à la fois :
 *   - RÉACTIVE : couper dès que la température réelle atteint la cible,
 *     sans attendre que la valeur lissée EMA (en retard de ~45 s) ni
 *     qu'un intervalle de décision arbitraire ne rattrapent la réalité —
 *     sinon le chauffage dépasse la consigne demandée ;
 *   - ROBUSTE : une lecture brute aberrante isolée (glitch EMI du
 *     Webasto sur le câble OneWire) ne doit jamais provoquer un arrêt
 *     définitif injustifié.
 *
 * Solution : exiger SETPOINT_CUT_READINGS lectures BRUTES consécutives
 * au-dessus de la cible. Avec 2 lectures espacées de ~5 s, la coupure
 * intervient ~10 s après le franchissement réel, et un glitch isolé est
 * absorbé (le compteur retombe à zéro à la lecture suivante).
 *
 * Header pur (aucune dépendance Arduino) : testé par
 * test/test_setpoint_cut/.
 */

#ifndef CORE_SETPOINT_CUT_H
#define CORE_SETPOINT_CUT_H

#include <stdint.h>

/** Nombre de lectures brutes consécutives >= cible exigées avant la
 *  coupure définitive du mode consigne. */
constexpr uint8_t SETPOINT_CUT_READINGS = 2;

/** État du critère de coupure (un compteur, remis à zéro par toute
 *  lecture repassant sous la cible). */
struct SetpointCut {
    uint8_t consecutive = 0;
};

/**
 * Intègre une NOUVELLE lecture brute du capteur.
 *
 * @param cut      État du critère (compteur persistant entre lectures)
 * @param raw_temp Lecture brute en °C (non lissée)
 * @param target   Température cible en °C
 * @param needed   Nombre de lectures consécutives exigées
 * @return true quand `needed` lectures consécutives ont atteint la
 *         cible : la coupure doit avoir lieu
 */
inline bool setpoint_cut_update(SetpointCut &cut, float raw_temp, float target,
                                uint8_t needed = SETPOINT_CUT_READINGS)
{
    if (raw_temp >= target) {
        if (++cut.consecutive >= needed) {
            return true;
        }
    } else {
        /* Lecture sous la cible : le glitch éventuel est absorbé */
        cut.consecutive = 0;
    }
    return false;
}

/** Réinitialise le critère (à appeler au démarrage du mode). */
inline void setpoint_cut_reset(SetpointCut &cut)
{
    cut.consecutive = 0;
}

#endif /* CORE_SETPOINT_CUT_H */
