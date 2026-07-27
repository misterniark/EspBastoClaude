/**
 * @file ui_alerts.cpp
 * @brief Implémentation du dispatcher central des alertes
 *
 * Fait le lien entre les drapeaux du core (heater_fsm, sensor) et les
 * écrans d'alerte LVGL (scr_alert), en déléguant la décision — quoi
 * afficher, quand, avec quelles priorités — à la politique pure
 * alert_policy.h (testée nativement : pio test -e native).
 *
 * Correction du défaut UX : les coupures de sécurité C1 (capteur
 * critique) et C4 (température max) levaient un drapeau qu'aucun
 * écran ne consommait — l'utilisateur voyait le chauffage s'éteindre
 * sans explication, y compris pendant la veille écran.
 */

#include "ui_alerts.h"
#include "alert_policy.h"
#include "scr_alert.h"
#include "../core/heater_fsm.h"
#include "../hal/sensor.h"
#include "../power/power_manager.h"

/* État persistant de la politique d'alerte (détection de front) */
static AlertPolicyState policy_state;

void ui_alerts_update()
{
    /* Traduire la raison typée du core en drapeaux pour la politique */
    HeaterSafetyReason reason = heater_get_safety_alert_reason();

    AlertKind show = alert_policy_step(policy_state,
                                       reason == HEATER_SAFETY_SENSOR,
                                       reason == HEATER_SAFETY_OVERTEMP,
                                       heater_has_connection_alert(),
                                       sensor_is_critical_error());

    if (show == ALERT_NONE) {
        return; /* Rien de nouveau ce cycle */
    }

    /* L'événement peut survenir pendant la veille (chauffe en tâche de
     * fond) : réveiller l'écran AVANT de créer l'écran d'alerte, pour
     * que loop() reprenne le rendu LVGL dès ce cycle. */
    power_force_wake();

    /* Charger l'écran d'alerte correspondant. L'écran courant est
     * détruit par lv_scr_load_anim(auto_del) ; chaque écran nettoie
     * ses timers LVGL dans son callback LV_EVENT_DELETE. */
    switch (show) {
        case ALERT_SAFETY_SENSOR:   scr_alert_safety_sensor();   break;
        case ALERT_SAFETY_OVERTEMP: scr_alert_safety_overtemp(); break;
        case ALERT_CONNECTION:      scr_alert_connection_lost(); break;
        case ALERT_SENSOR_STATE:    scr_alert_sensor_error();    break;
        default: break;
    }
}
