/**
 * @file test_main.cpp
 * @brief Tests unitaires de la décision thermostat (core/thermostat_policy.h)
 *
 * Vérifie la règle d'allumage/extinction avec hystérésis :
 *   - correctif « premier allumage » : un thermostat activé avec une
 *     température dans la bande [consigne − hyst, consigne[ doit
 *     chauffer immédiatement ;
 *   - correctif « accord des deux estimateurs » : l'allumage exige
 *     EMA ET lecture brute sous le seuil — sinon, après une extinction
 *     rapide (décidée sur les brutes alors que l'EMA traîne), le
 *     thermostat rallumait dans la seconde (oscillation constatée au
 *     banc de test du 27/07/2026).
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/core/thermostat_policy.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Bien sous la bande (les deux estimateurs d'accord) : allumage --- */
static void test_below_band_heats(void)
{
    /* consigne 20, hyst 3 → bande [17, 20[ ; temp 15 : toujours allumer */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(15.0f, 15.0f, 20.0f, 3.0f, false, true));
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(15.0f, 15.0f, 20.0f, 3.0f, false, false));
    /* Déjà en chauffe : rien à changer */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(15.0f, 15.0f, 20.0f, 3.0f, true, true));
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(15.0f, 15.0f, 20.0f, 3.0f, true, false));
}

/* --- BUG CORRIGÉ n°1 : dans la bande au démarrage → allumer --- */
static void test_in_band_initial_heats(void)
{
    /* consigne 31.5, hyst 4 → bande [27.5, 31.5[ ; pièce à 28.2 :
     * l'ancien code ne lançait jamais le chauffage. */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON,
                      thermostat_decide(28.2f, 28.2f, 31.5f, 4.0f, false, true));
}

/* --- BUG CORRIGÉ n°2 : pas de réallumage si la brute est chaude --- */
static void test_no_reignition_when_raw_is_hot(void)
{
    /* Situation post-extinction-rapide : la coupure a eu lieu sur les
     * lectures brutes (26°C) alors que l'EMA traînait à 21.6°C, SOUS le
     * seuil de réallumage (22). Sans l'accord des deux estimateurs, le
     * thermostat rallumait dans la seconde → cycle ON/OFF de ~10 s. */
    TEST_ASSERT_EQUAL(THERMO_NONE,
                      thermostat_decide(21.6f, 26.0f, 25.0f, 3.0f, false, false));
    /* Même protection au premier passage (seuil = consigne) */
    TEST_ASSERT_EQUAL(THERMO_NONE,
                      thermostat_decide(20.0f, 26.0f, 25.0f, 3.0f, false, true));
}

/* --- Glitch bas isolé : la brute seule ne déclenche pas non plus --- */
static void test_no_ignition_on_low_raw_glitch(void)
{
    /* EMA à 23 (zone morte), une brute aberrante à 10°C : ne pas
     * allumer sur un glitch — l'EMA doit confirmer. */
    TEST_ASSERT_EQUAL(THERMO_NONE,
                      thermostat_decide(23.0f, 10.0f, 25.0f, 3.0f, false, false));
}

/* --- Dans la bande en régime établi : zone morte, ne rien faire --- */
static void test_in_band_steady_does_nothing(void)
{
    /* Non chauffant (vient de s'arrêter à la consigne) : attendre que
     * la température retombe sous la bande avant de relancer */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(28.2f, 28.2f, 31.5f, 4.0f, false, false));
    /* Chauffant (monte vers la consigne) : continuer de chauffer */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(28.2f, 28.2f, 31.5f, 4.0f, true, false));
}

/* --- Consigne atteinte (EMA) : extinction si en chauffe --- */
static void test_at_or_above_setpoint_stops(void)
{
    /* Exactement la consigne (>=, pas >) */
    TEST_ASSERT_EQUAL(THERMO_HEAT_OFF, thermostat_decide(20.0f, 20.0f, 20.0f, 3.0f, true, false));
    /* Au-dessus */
    TEST_ASSERT_EQUAL(THERMO_HEAT_OFF, thermostat_decide(22.0f, 22.0f, 20.0f, 3.0f, true, false));
    /* Pas en chauffe : rien à éteindre */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(22.0f, 22.0f, 20.0f, 3.0f, false, false));
}

/* --- Démarrage pièce déjà chaude : ne pas allumer --- */
static void test_initial_above_setpoint_does_not_heat(void)
{
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(25.0f, 25.0f, 20.0f, 3.0f, false, true));
}

/* --- Frontières exactes de la bande --- */
static void test_band_boundaries(void)
{
    /* temp = consigne − hyst exactement : dans la bande (pas d'allumage
     * en régime établi — le seuil est strictement inférieur) */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(17.0f, 17.0f, 20.0f, 3.0f, false, false));
    /* Juste sous la bande : allumage */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(16.9f, 16.9f, 20.0f, 3.0f, false, false));
    /* Premier passage, juste sous la consigne : allumage */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(19.9f, 19.9f, 20.0f, 3.0f, false, true));
}

/* --- Scénario complet : cycle de vie d'une session de chauffe --- */
static void test_full_lifecycle(void)
{
    const float SP = 20.0f, HY = 3.0f;

    /* Activation à 18.5 (dans la bande) : premier allumage */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(18.5f, 18.5f, SP, HY, false, true));
    /* Monte : on continue */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(19.5f, 19.6f, SP, HY, true, false));
    /* Consigne atteinte : extinction */
    TEST_ASSERT_EQUAL(THERMO_HEAT_OFF, thermostat_decide(20.1f, 20.3f, SP, HY, true, false));
    /* Juste après la coupure : brute encore chaude, EMA qui redescend —
     * PAS de relance même si l'EMA passe sous la bande */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(16.8f, 20.1f, SP, HY, false, false));
    /* Redescend dans la bande (les deux) : zone morte, pas de relance */
    TEST_ASSERT_EQUAL(THERMO_NONE, thermostat_decide(18.5f, 18.4f, SP, HY, false, false));
    /* Passe sous la bande (les deux) : relance */
    TEST_ASSERT_EQUAL(THERMO_HEAT_ON, thermostat_decide(16.8f, 16.5f, SP, HY, false, false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_below_band_heats);
    RUN_TEST(test_in_band_initial_heats);
    RUN_TEST(test_no_reignition_when_raw_is_hot);
    RUN_TEST(test_no_ignition_on_low_raw_glitch);
    RUN_TEST(test_in_band_steady_does_nothing);
    RUN_TEST(test_at_or_above_setpoint_stops);
    RUN_TEST(test_initial_above_setpoint_does_not_heat);
    RUN_TEST(test_band_boundaries);
    RUN_TEST(test_full_lifecycle);
    return UNITY_END();
}
