/**
 * @file relay_reconcile.h
 * @brief Réconciliation de l'état contrôleur ↔ relais — logique pure
 *
 * PROBLÈME RÉSOLU (audit de production du 27/07/2026, deux constats
 * bloquants) : tout le système reposait sur l'hypothèse jamais garantie
 * que l'état de la machine à états du contrôleur reflète l'état réel du
 * relais. Trois événements banals la brisaient DÉFINITIVEMENT :
 *
 *   1. Reboot du contrôleur pendant la chauffe (microcoupure 12 V,
 *      crash) : il repart en IDLE, mais le relais est toujours fermé et
 *      ses pings de découverte satisfont le watchdog du relais. Or les
 *      sécurités C1 (capteur mort) et C4 (surchauffe) ne s'exécutent
 *      QUE dans l'état HEATING : le Webasto chauffait alors en boucle
 *      ouverte, sans thermostat et sans aucune sécurité, jusqu'à
 *      intervention humaine.
 *   2. Reboot du relais pendant la chauffe : il repart ouvert
 *      (chauffage éteint) mais continue de répondre aux pings ; le
 *      contrôleur affichait « en chauffe » toute la nuit et le
 *      thermostat, se croyant déjà en chauffe, ne relançait jamais.
 *   3. ACK_ON ou ACK_OFF perdu par la radio : mêmes conséquences.
 *
 * SOLUTION : le relais porte son état réel dans chaque ACK_PONG
 * (protocol.h) et le contrôleur se remet d'accord avec la réalité à
 * chaque rapport — donc en un intervalle de ping au pire, quelle que
 * soit la cause de l'écart.
 *
 * Deux règles seulement, appliquées dans heater_fsm_update() :
 *   - contrôleur en chauffe, relais ouvert → adopter l'arrêt (retour
 *     IDLE) et alerter l'utilisateur : le chauffage s'est arrêté sans
 *     qu'il le demande, il doit le savoir ;
 *   - contrôleur PAS en chauffe, relais fermé → ordonner l'arrêt : on
 *     ne laisse jamais tourner un appareil à combustion que le
 *     contrôleur ne régule pas. Couvre le cas 1 ci-dessus.
 *
 * Header pur (aucune dépendance Arduino) : testé par
 * test/test_relay_reconcile/.
 */

#ifndef CORE_RELAY_RECONCILE_H
#define CORE_RELAY_RECONCILE_H

#include <stdint.h>

/** État du relais tel que rapporté par le dernier ACK_PONG. */
enum RelayReport {
    RELAY_REPORT_UNKNOWN, /* Aucun rapport exploitable (voir ci-dessous) */
    RELAY_REPORT_OFF,     /* Le relais dit : chauffage éteint */
    RELAY_REPORT_ON       /* Le relais dit : chauffage allumé */
};

/** Action de réconciliation à effectuer ce cycle. */
enum ReconcileAction {
    RECONCILE_NONE,      /* États cohérents (ou rapport inexploitable) */
    RECONCILE_ADOPT_OFF, /* Le relais s'est arrêté : passer IDLE + alerter */
    RECONCILE_SEND_OFF   /* Le relais chauffe sans nous : ordonner l'arrêt */
};

/**
 * Un rapport n'est exploitable que s'il est postérieur à la dernière
 * commande envoyée, plus une marge : sinon un PONG parti AVANT que le
 * relais ne traite notre commande décrirait un état déjà périmé, et la
 * réconciliation défferait l'ordre qu'on vient juste de donner.
 *
 * Arithmétique de soustraction non signée : correct au débordement de
 * millis() (49,7 jours). Horodatages en uint32_t — même largeur que
 * millis() sur la cible, ce qui rend le débordement reproductible dans
 * les tests natifs (sur l'hôte, `unsigned long` fait 64 bits et ne
 * déborderait jamais au bon endroit).
 *
 * @param now_ms      Horodatage du rapport (millis à sa réception)
 * @param last_cmd_ms Horodatage du dernier CMD_HEAT_ON/OFF envoyé
 * @param grace_ms    Marge couvrant un aller-retour radio
 */
inline bool relay_report_usable(uint32_t now_ms,
                                uint32_t last_cmd_ms,
                                uint32_t grace_ms)
{
    return (uint32_t)(now_ms - last_cmd_ms) >= grace_ms;
}

/**
 * Décide de l'action de réconciliation.
 *
 * @param fsm_heating true si la machine à états se croit en chauffe
 *                    (HEATER_HEATING)
 * @param report      Dernier état exploitable rapporté par le relais
 */
inline ReconcileAction relay_reconcile_step(bool fsm_heating,
                                            RelayReport report)
{
    if (report == RELAY_REPORT_UNKNOWN) return RECONCILE_NONE;

    if (fsm_heating && report == RELAY_REPORT_OFF) {
        return RECONCILE_ADOPT_OFF;
    }
    if (!fsm_heating && report == RELAY_REPORT_ON) {
        return RECONCILE_SEND_OFF;
    }
    return RECONCILE_NONE;
}

#endif /* CORE_RELAY_RECONCILE_H */
