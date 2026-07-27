/**
 * @file test_main.cpp
 * @brief Tests unitaires de la réconciliation contrôleur ↔ relais
 *        (core/relay_reconcile.h)
 *
 * Couvre les trois désynchronisations relevées par l'audit de
 * production du 27/07/2026, dont deux étaient bloquantes :
 *   - reboot du contrôleur pendant la chauffe (le relais chauffe hors
 *     de tout contrôle, sécurités C1/C4 inertes) ;
 *   - reboot du relais pendant la chauffe (chauffage éteint en
 *     silence, thermostat qui ne relance jamais) ;
 *   - ACK perdu par la radio (mêmes conséquences).
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/core/relay_reconcile.h"

void setUp(void) {}
void tearDown(void) {}

/* ================= relay_reconcile_step ================= */

/* --- États cohérents : ne rien faire --- */
static void test_consistent_states_do_nothing(void)
{
    /* On chauffe, le relais chauffe */
    TEST_ASSERT_EQUAL(RECONCILE_NONE,
                      relay_reconcile_step(true, RELAY_REPORT_ON));
    /* On ne chauffe pas, le relais non plus */
    TEST_ASSERT_EQUAL(RECONCILE_NONE,
                      relay_reconcile_step(false, RELAY_REPORT_OFF));
}

/* --- BLOQUANT n°1 : le relais chauffe hors de notre contrôle --- */
static void test_relay_heating_without_us_forces_off(void)
{
    /* Cas typique : notre propre reboot pendant une chauffe. La FSM
     * repart en IDLE, le relais est resté fermé, et nos pings
     * satisfont son watchdog — rien d'autre ne le couperait. */
    TEST_ASSERT_EQUAL(RECONCILE_SEND_OFF,
                      relay_reconcile_step(false, RELAY_REPORT_ON));
}

/* --- BLOQUANT n°2 : le relais s'est arrêté sans qu'on le demande --- */
static void test_relay_off_while_heating_adopts_off(void)
{
    /* Reboot du relais, son watchdog, ou un arrêt manuel au bouton :
     * on doit adopter la réalité ET alerter l'utilisateur. */
    TEST_ASSERT_EQUAL(RECONCILE_ADOPT_OFF,
                      relay_reconcile_step(true, RELAY_REPORT_OFF));
}

/* --- Rapport inconnu : ne jamais agir sur une supposition --- */
static void test_unknown_report_never_acts(void)
{
    /* Relais au firmware antérieur, ou rapport trop proche d'une
     * commande : dans le doute, on ne touche à rien. */
    TEST_ASSERT_EQUAL(RECONCILE_NONE,
                      relay_reconcile_step(true, RELAY_REPORT_UNKNOWN));
    TEST_ASSERT_EQUAL(RECONCILE_NONE,
                      relay_reconcile_step(false, RELAY_REPORT_UNKNOWN));
}

/* ================= relay_report_usable ================= */

/* --- Rapport postérieur à la commande + marge : exploitable --- */
static void test_report_after_grace_is_usable(void)
{
    TEST_ASSERT_TRUE(relay_report_usable(10000, 5000, 2000));  /* 5 s après */
    TEST_ASSERT_TRUE(relay_report_usable(7000, 5000, 2000));   /* pile 2 s */
}

/* --- Rapport dans la marge : inexploitable (PONG déjà en vol) --- */
static void test_report_within_grace_is_not_usable(void)
{
    /* Sans cette garde, un PONG parti AVANT que le relais ne traite
     * notre allumage ferait immédiatement annuler cet allumage. */
    TEST_ASSERT_FALSE(relay_report_usable(5000, 5000, 2000));  /* instantané */
    TEST_ASSERT_FALSE(relay_report_usable(6999, 5000, 2000));  /* 1,999 s */
}

/* --- Débordement de millis() (49,7 jours) : toujours correct --- */
static void test_report_usable_wraps_safely(void)
{
    /* Commande envoyée 1 s avant le débordement de millis()... */
    uint32_t cmd = 0xFFFFFFFFUL - 1000UL;

    /* ...rapport 3 s après le passage par zéro : 4 s d'écart réel,
     * donc exploitable malgré le débordement. */
    TEST_ASSERT_TRUE(relay_report_usable(3000UL, cmd, 2000));

    /* Rapport 0,5 s après le passage par zéro : 1,5 s d'écart réel,
     * dans la marge → inexploitable. */
    TEST_ASSERT_FALSE(relay_report_usable(500UL, cmd, 2000));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_consistent_states_do_nothing);
    RUN_TEST(test_relay_heating_without_us_forces_off);
    RUN_TEST(test_relay_off_while_heating_adopts_off);
    RUN_TEST(test_unknown_report_never_acts);
    RUN_TEST(test_report_after_grace_is_usable);
    RUN_TEST(test_report_within_grace_is_not_usable);
    RUN_TEST(test_report_usable_wraps_safely);
    return UNITY_END();
}
