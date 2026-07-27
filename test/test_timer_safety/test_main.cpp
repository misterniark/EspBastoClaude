/**
 * @file test_main.cpp
 * @brief Tests unitaires de la détection de front des arrêts de
 *        sécurité du minuteur (core/timer_safety.h)
 *
 * Défaut corrigé (constaté au banc le 27/07/2026) : la garde du
 * minuteur réagissait au NIVEAU du drapeau d'alerte, qui reste levé
 * jusqu'à acquittement par l'utilisateur. Un minuteur démarré avec une
 * alerte antérieure encore pendante s'arrêtait donc immédiatement — en
 * laissant le chauffage allumé sans aucune borne.
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/core/timer_safety.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Aucune alerte : le décompte continue --- */
static void test_no_alert_never_triggers(void)
{
    TimerSafetyGate g;
    timer_safety_arm(g, HEATER_SAFETY_NONE);
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_NONE));
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_NONE));
}

/* --- LE BUG : alerte antérieure pendante au démarrage → ignorée --- */
static void test_pending_alert_at_start_is_ignored(void)
{
    TimerSafetyGate g;
    /* Une désynchronisation non acquittée traîne depuis tout à l'heure */
    timer_safety_arm(g, HEATER_SAFETY_DESYNC);

    /* Le décompte doit se dérouler normalement */
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_DESYNC));
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_DESYNC));
}

/* --- Coupure survenant PENDANT le décompte → arrêt --- */
static void test_new_alert_during_countdown_triggers(void)
{
    TimerSafetyGate g;
    timer_safety_arm(g, HEATER_SAFETY_NONE);

    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_NONE));
    /* Surchauffe en cours de décompte */
    TEST_ASSERT_TRUE(timer_safety_triggered(g, HEATER_SAFETY_OVERTEMP));
}

/* --- Alerte pendante puis coupure d'une AUTRE cause → arrêt --- */
static void test_different_alert_triggers(void)
{
    TimerSafetyGate g;
    timer_safety_arm(g, HEATER_SAFETY_DESYNC);

    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_DESYNC));
    /* Le capteur meurt pendant le décompte : cause différente */
    TEST_ASSERT_TRUE(timer_safety_triggered(g, HEATER_SAFETY_SENSOR));
}

/* --- Réarmement : après acquittement, une coupure de MÊME cause
       est de nouveau détectée --- */
static void test_rearms_after_acknowledgement(void)
{
    TimerSafetyGate g;
    timer_safety_arm(g, HEATER_SAFETY_SENSOR);

    /* L'alerte traînante est ignorée... */
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_SENSOR));
    /* ...l'utilisateur l'acquitte (drapeau retombé) */
    TEST_ASSERT_FALSE(timer_safety_triggered(g, HEATER_SAFETY_NONE));
    /* ...et une NOUVELLE coupure capteur doit bien arrêter le décompte */
    TEST_ASSERT_TRUE(timer_safety_triggered(g, HEATER_SAFETY_SENSOR));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_no_alert_never_triggers);
    RUN_TEST(test_pending_alert_at_start_is_ignored);
    RUN_TEST(test_new_alert_during_countdown_triggers);
    RUN_TEST(test_different_alert_triggers);
    RUN_TEST(test_rearms_after_acknowledgement);
    return UNITY_END();
}
