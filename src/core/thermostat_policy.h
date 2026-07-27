/**
 * @file thermostat_policy.h
 * @brief Décision d'allumage/extinction du thermostat — logique pure, testable
 *
 * Problème résolu : avec la seule bande d'hystérésis, un thermostat
 * démarré alors que la température est DANS la bande
 * [consigne − hystérésis, consigne[ ne lançait jamais le chauffage —
 * l'utilisateur active le mode, le bouton passe à « ARRÊTER », et rien
 * ne se passe alors que la pièce est sous la consigne demandée.
 *
 * Règle retenue (comportement d'un thermostat d'ambiance classique) :
 *   - PREMIÈRE décision après l'activation du mode : allumer dès que
 *     temp < consigne — l'utilisateur vient de demander de la chaleur,
 *     la bande n'a pas à retarder ce premier allumage ;
 *   - décisions SUIVANTES : allumer seulement si
 *     temp < consigne − hystérésis — c'est là que la bande joue son
 *     rôle anti-cyclage, en laissant la température redescendre
 *     franchement avant de relancer le Webasto ;
 *   - dans tous les cas : éteindre dès que temp >= consigne.
 *
 * Header pur (aucune dépendance Arduino) : testé par
 * test/test_thermostat_policy/.
 */

#ifndef CORE_THERMOSTAT_POLICY_H
#define CORE_THERMOSTAT_POLICY_H

/** Action décidée par le thermostat pour ce cycle d'évaluation. */
enum ThermostatAction {
    THERMO_NONE,     /* Ne rien changer (zone morte ou état déjà correct) */
    THERMO_HEAT_ON,  /* Demander l'allumage du chauffage */
    THERMO_HEAT_OFF  /* Demander l'extinction du chauffage */
};

/**
 * Décide de l'action du thermostat.
 *
 * L'ALLUMAGE exige l'accord des DEUX estimateurs : température lissée
 * (EMA) ET dernière lecture brute sous le seuil. Raison (oscillation
 * constatée au banc de test du 27/07/2026) : l'extinction rapide se
 * décide sur les lectures brutes, or l'EMA traîne ~45 s derrière —
 * juste après une coupure, l'EMA peut être encore SOUS le seuil de
 * réallumage alors que la pièce est chaude, et le thermostat
 * rallumerait dans la seconde (cycle ON/OFF ~10 s). Avec l'accord des
 * deux : une brute encore chaude bloque le réallumage le temps que le
 * lissage rattrape la réalité. Symétriquement, une brute isolée
 * anormalement basse ne peut PAS déclencher d'allumage tant que l'EMA
 * ne confirme pas (anti-glitch).
 *
 * @param temp       Température lissée (EMA) en °C
 * @param raw        Dernière lecture BRUTE valide en °C
 * @param setpoint   Consigne en °C
 * @param hysteresis Hystérésis en °C (écart sous la consigne pour un
 *                   RE-démarrage ; ignorée à la première décision)
 * @param heating    true si le chauffage est actuellement en chauffe
 * @param initial    true pour la PREMIÈRE décision après l'activation
 *                   du mode (seuil d'allumage = consigne, pas la bande)
 * @return L'action à effectuer (jamais redondante avec l'état courant :
 *         HEAT_ON seulement si non chauffant, HEAT_OFF seulement si
 *         chauffant)
 */
inline ThermostatAction thermostat_decide(float temp, float raw,
                                          float setpoint, float hysteresis,
                                          bool heating, bool initial)
{
    /* Seuil d'allumage : la consigne elle-même au premier passage,
     * la borne basse de la bande ensuite (anti-cyclage). */
    float seuil_allumage = initial ? setpoint : (setpoint - hysteresis);

    if (temp < seuil_allumage && raw < seuil_allumage) {
        return heating ? THERMO_NONE : THERMO_HEAT_ON;
    }
    if (temp >= setpoint) {
        return heating ? THERMO_HEAT_OFF : THERMO_NONE;
    }
    /* Zone morte (ou estimateurs en désaccord) : conserver l'état */
    return THERMO_NONE;
}

#endif /* CORE_THERMOSTAT_POLICY_H */
