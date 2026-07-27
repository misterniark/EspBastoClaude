/**
 * @file alert_policy.h
 * @brief Politique pure de dispatch des alertes plein écran
 *
 * Logique sans dépendance Arduino/LVGL, testable nativement
 * (pio test -e native), sur le modèle de hal/touch_gate.h.
 *
 * Problème résolu : les coupures de sécurité C1 (capteur critique) et
 * C4 (température max) de heater_fsm levaient un drapeau qu'aucun
 * écran ne consommait — l'utilisateur voyait le chauffage s'éteindre
 * sans explication. Par ailleurs, la consommation des alertes vivait
 * dans le timer LVGL du menu, qui ne tourne ni en veille écran ni sur
 * les écrans de mode.
 *
 * Cette politique est évaluée à CHAQUE itération de loop() (donc aussi
 * pendant la veille écran) par ui_alerts_update(), et décide s'il faut
 * créer un écran d'alerte ce cycle. Elle garantit :
 *
 *   - Priorité : sécurité (C1/C4) > connexion perdue > état capteur.
 *   - Détection de front : une alerte n'est affichée qu'UNE fois par
 *     épisode (pas de re-création de l'écran à chaque itération, pas
 *     de boucle « OK → re-alerte » sur les alertes d'état).
 *   - File d'attente implicite : si une alerte plus prioritaire en
 *     remplace une autre à l'écran, la moins prioritaire réapparaît
 *     après l'acquittement de la première (tant que son drapeau est
 *     encore levé).
 *   - L'alerte d'état capteur (câblage) est considérée comme « vue »
 *     si l'alerte de sécurité capteur a déjà été montrée pendant le
 *     même épisode d'erreur : on n'inflige pas deux écrans successifs
 *     pour la même panne.
 */

#ifndef UI_ALERT_POLICY_H
#define UI_ALERT_POLICY_H

/** Alertes plein écran que le dispatcher peut afficher. */
enum AlertKind {
    ALERT_NONE = 0,        /* Rien à afficher ce cycle */
    ALERT_SAFETY_SENSOR,   /* C1 — capteur HS : chauffage coupé par sécurité */
    ALERT_SAFETY_OVERTEMP, /* C4 — température max : chauffage coupé */
    ALERT_CONNECTION,      /* I5 — connexion relais perdue pendant la chauffe */
    ALERT_SENSOR_STATE     /* État : capteur en erreur critique (infos câblage) */
};

/**
 * État interne de la politique (à conserver entre les appels).
 *
 * - current : dernière alerte « due » encore pertinente. Sert à la
 *   détection de front : tant que l'alerte due ne change pas, on ne
 *   recrée pas l'écran.
 * - sensor_info_seen : l'information « capteur mort » a déjà été
 *   montrée pendant l'épisode d'erreur en cours (via l'alerte d'état
 *   OU via l'alerte de sécurité C1). Réarmé quand l'erreur disparaît.
 */
struct AlertPolicyState {
    AlertKind current = ALERT_NONE;
    bool sensor_info_seen = false;
};

/**
 * Évalue la politique d'alerte pour ce cycle.
 *
 * @param st              État persistant de la politique
 * @param safety_sensor   Drapeau C1 levé (arrêt sécurité capteur, acquittable)
 * @param safety_overtemp Drapeau C4 levé (arrêt sécurité température, acquittable)
 * @param connection      Drapeau I5 levé (connexion perdue, acquittable)
 * @param sensor_critical État : capteur en erreur critique (> 5 min)
 * @return L'alerte à afficher MAINTENANT, ou ALERT_NONE s'il n'y a
 *         rien de nouveau (alerte déjà affichée, ou aucune alerte).
 */
static inline AlertKind alert_policy_step(AlertPolicyState &st,
                                          bool safety_sensor,
                                          bool safety_overtemp,
                                          bool connection,
                                          bool sensor_critical)
{
    /* Fin d'épisode d'erreur capteur : réarmer l'alerte d'état pour
     * qu'une future panne soit à nouveau signalée. */
    if (!sensor_critical) {
        st.sensor_info_seen = false;
    }

    /*
     * Alerte « due » = la plus prioritaire actuellement pendante.
     * Les drapeaux de sécurité et de connexion restent levés jusqu'à
     * l'acquittement (bouton OK) : ils sont pendants tant que levés.
     * L'alerte d'état capteur n'est due qu'une fois par épisode.
     */
    AlertKind due = ALERT_NONE;
    if (safety_sensor) {
        due = ALERT_SAFETY_SENSOR;
    } else if (safety_overtemp) {
        due = ALERT_SAFETY_OVERTEMP;
    } else if (connection) {
        due = ALERT_CONNECTION;
    } else if (sensor_critical && !st.sensor_info_seen) {
        due = ALERT_SENSOR_STATE;
    }

    /* Pas de changement → rien à créer ce cycle (écran déjà affiché,
     * ou toujours aucune alerte). */
    if (due == st.current) {
        return ALERT_NONE;
    }

    st.current = due;

    /* L'information « capteur mort » est montrée par ces deux écrans :
     * marquer l'épisode comme vu pour ne pas doubler l'alerte. */
    if (due == ALERT_SAFETY_SENSOR || due == ALERT_SENSOR_STATE) {
        st.sensor_info_seen = true;
    }

    return due;
}

#endif /* UI_ALERT_POLICY_H */
