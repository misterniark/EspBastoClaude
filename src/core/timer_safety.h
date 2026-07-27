/**
 * @file timer_safety.h
 * @brief Détection de front sur les arrêts de sécurité — logique pure
 *
 * PROBLÈME RÉSOLU (constaté au banc le 27/07/2026) : la garde C4bis du
 * minuteur testait un NIVEAU — « une alerte de sécurité est-elle
 * pendante ? » — au lieu d'un FRONT — « une coupure de sécurité
 * vient-elle de se produire pendant mon décompte ? ».
 *
 * Or le drapeau d'alerte reste levé jusqu'à ce que l'utilisateur
 * acquitte l'écran d'alerte. Un minuteur démarré alors qu'une alerte
 * antérieure n'a pas encore été acquittée s'arrêtait donc à son premier
 * cycle de mise à jour, immédiatement après son démarrage — et, plus
 * grave, sans couper le chauffage (ce chemin suppose que heater_fsm
 * vient de le faire), qui restait alors allumé sans aucune borne.
 *
 * L'ajout de l'alerte de désynchronisation (le relais s'est arrêté
 * seul) a rendu ce défaut nettement plus atteignable : un reboot du
 * relais suffit à lever le drapeau.
 *
 * RÈGLE RETENUE : mémoriser au démarrage l'alerte éventuellement déjà
 * pendante et l'ignorer ; ne réagir qu'à une alerte DIFFÉRENTE, ou à
 * une nouvelle alerte après acquittement de la précédente.
 *
 * Header pur (aucune dépendance Arduino ; heater_fsm.h ne fournit que
 * l'énumération) : testé par test/test_timer_safety/.
 */

#ifndef CORE_TIMER_SAFETY_H
#define CORE_TIMER_SAFETY_H

#include "heater_fsm.h"

/** État de la détection de front (à conserver entre les appels). */
struct TimerSafetyGate {
    /* Alerte présente au démarrage du décompte, à ignorer : elle
     * concerne un épisode antérieur. Réarmé dès qu'aucune alerte n'est
     * pendante, pour qu'une nouvelle coupure de MÊME cause soit bien
     * détectée. */
    HeaterSafetyReason ignored = HEATER_SAFETY_NONE;
};

/** À appeler au démarrage du décompte. */
inline void timer_safety_arm(TimerSafetyGate &gate, HeaterSafetyReason current)
{
    gate.ignored = current;
}

/**
 * À appeler à chaque cycle de décompte.
 *
 * @param gate    État de la détection de front
 * @param current Alerte de sécurité actuellement pendante
 * @return true si une coupure de sécurité vient de survenir et que le
 *         décompte doit s'arrêter
 */
inline bool timer_safety_triggered(TimerSafetyGate &gate,
                                   HeaterSafetyReason current)
{
    if (current == HEATER_SAFETY_NONE) {
        /* Plus rien de pendant : réarmer pour la suite */
        gate.ignored = HEATER_SAFETY_NONE;
        return false;
    }
    /* Alerte pendante : ne réagir que si ce n'est pas celle qu'on
     * traînait déjà au démarrage */
    return current != gate.ignored;
}

#endif /* CORE_TIMER_SAFETY_H */
