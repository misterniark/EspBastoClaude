/**
 * @file mode_status.h
 * @brief État affiché d'un mode de chauffage — logique pure, testable
 *
 * PROBLÈME RÉSOLU : l'interface ne montrait l'activité que pendant la
 * CHAUFFE (icône flamme, bouton ARRÊTER du menu). Or un thermostat
 * passe l'essentiel de son temps à ne PAS chauffer : température
 * atteinte, ou zone morte de l'hystérésis. L'utilisateur lançait un
 * mode, revenait au menu, n'y voyait aucune trace de ce mode — et
 * concluait logiquement qu'il s'était arrêté.
 *
 * Distinguer « inactif » de « actif mais pas en train de chauffer »
 * lève cette ambiguïté sans rien changer à la régulation.
 *
 * Header pur (aucune dépendance LVGL/Arduino) : testé par
 * test/test_mode_status/.
 */

#ifndef UI_MODE_STATUS_H
#define UI_MODE_STATUS_H

/** État d'un mode, du point de vue de l'affichage. */
enum ModeUiStatus {
    MODE_UI_IDLE,     /* Mode arrêté */
    MODE_UI_HEATING,  /* Mode actif, chauffage en cours */
    MODE_UI_LOCKED,   /* Mode actif, mais verrou anti-redémarrage en cours */
    MODE_UI_REACHED,  /* Mode actif, température de consigne atteinte */
    MODE_UI_WAITING   /* Mode actif, en deçà de la consigne mais pas
                       * encore sous le seuil de relance (zone morte) */
};

/**
 * Détermine l'état affichable d'un mode.
 *
 * Priorité : la chauffe l'emporte (c'est le fait le plus important),
 * puis le verrou (il explique pourquoi rien ne démarre), puis la
 * comparaison à la consigne.
 *
 * @param active   Le mode est-il actif ?
 * @param heating  Le chauffage est-il effectivement en marche ?
 * @param locked   Verrou anti-redémarrage du relais en cours ?
 * @param temp     Température mesurée
 * @param setpoint Consigne / cible du mode
 */
inline ModeUiStatus mode_status_compute(bool active, bool heating, bool locked,
                                        float temp, float setpoint)
{
    if (!active)  return MODE_UI_IDLE;
    if (heating)  return MODE_UI_HEATING;
    if (locked)   return MODE_UI_LOCKED;
    if (temp >= setpoint) return MODE_UI_REACHED;
    return MODE_UI_WAITING;
}

/**
 * Libellé court correspondant, prêt à afficher.
 * Sans accents : la police embarquée ne les couvre pas toutes.
 */
inline const char *mode_status_text(ModeUiStatus status)
{
    switch (status) {
        case MODE_UI_HEATING: return "CHAUFFE EN COURS";
        case MODE_UI_LOCKED:  return "ATTENTE (VERROU 3 MIN)";
        case MODE_UI_REACHED: return "TEMPERATURE ATTEINTE";
        case MODE_UI_WAITING: return "EN VEILLE - SURVEILLANCE";
        case MODE_UI_IDLE:
        default:              return "";
    }
}

#endif /* UI_MODE_STATUS_H */
