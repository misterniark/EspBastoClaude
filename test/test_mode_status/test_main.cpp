/**
 * @file test_main.cpp
 * @brief Tests unitaires de l'état affiché d'un mode (ui/mode_status.h)
 *
 * Le cas qui motive tout : un thermostat ACTIF mais qui ne chauffe pas
 * (température atteinte, ou zone morte) ne doit jamais être confondu
 * avec un mode ARRÊTÉ — c'est ce que l'ancienne interface laissait
 * croire, faute d'afficher quoi que ce soit hors chauffe.
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/ui/mode_status.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Mode arrêté : rien à afficher, quel que soit le reste --- */
static void test_inactive_is_idle(void)
{
    TEST_ASSERT_EQUAL(MODE_UI_IDLE,
                      mode_status_compute(false, false, false, 15.0f, 20.0f));
    /* Même si le chauffage tourne encore (arrêt en cours) */
    TEST_ASSERT_EQUAL(MODE_UI_IDLE,
                      mode_status_compute(false, true, false, 15.0f, 20.0f));
    TEST_ASSERT_EQUAL_STRING("", mode_status_text(MODE_UI_IDLE));
}

/* --- En chauffe : l'information la plus importante prime --- */
static void test_heating_wins(void)
{
    TEST_ASSERT_EQUAL(MODE_UI_HEATING,
                      mode_status_compute(true, true, false, 15.0f, 20.0f));
    /* Même au-dessus de la consigne (extinction pas encore effective) */
    TEST_ASSERT_EQUAL(MODE_UI_HEATING,
                      mode_status_compute(true, true, false, 25.0f, 20.0f));
}

/* --- LE CAS DE LA CONFUSION : actif, consigne déjà dépassée --- */
static void test_active_but_setpoint_already_reached(void)
{
    ModeUiStatus s = mode_status_compute(true, false, false, 25.0f, 20.0f);
    TEST_ASSERT_EQUAL(MODE_UI_REACHED, s);
    /* Surtout : ce n'est PAS « arrêté » */
    TEST_ASSERT_NOT_EQUAL(MODE_UI_IDLE, s);
    TEST_ASSERT_EQUAL_STRING("TEMPERATURE ATTEINTE", mode_status_text(s));
}

/* --- Actif, sous la consigne, mais pas encore de chauffe --- */
static void test_active_waiting_in_dead_band(void)
{
    /* Consigne 20, pièce à 18,5 : au-dessus du seuil de relance
     * (20 - hystérésis), donc le thermostat surveille sans chauffer */
    ModeUiStatus s = mode_status_compute(true, false, false, 18.5f, 20.0f);
    TEST_ASSERT_EQUAL(MODE_UI_WAITING, s);
    TEST_ASSERT_NOT_EQUAL(MODE_UI_IDLE, s);
}

/* --- Verrou anti-redémarrage : explique pourquoi rien ne démarre --- */
static void test_locked_explains_inaction(void)
{
    /* Sous la consigne ET actif : sans le verrou ce serait WAITING,
     * mais l'utilisateur doit savoir que c'est le verrou qui bloque */
    TEST_ASSERT_EQUAL(MODE_UI_LOCKED,
                      mode_status_compute(true, false, true, 15.0f, 20.0f));
    /* Le verrou ne masque pas une chauffe réelle */
    TEST_ASSERT_EQUAL(MODE_UI_HEATING,
                      mode_status_compute(true, true, true, 15.0f, 20.0f));
}

/* --- Frontière exacte : temp == consigne compte comme atteinte --- */
static void test_boundary_equal_setpoint(void)
{
    TEST_ASSERT_EQUAL(MODE_UI_REACHED,
                      mode_status_compute(true, false, false, 20.0f, 20.0f));
    TEST_ASSERT_EQUAL(MODE_UI_WAITING,
                      mode_status_compute(true, false, false, 19.9f, 20.0f));
}

/* --- Tous les états actifs produisent un libellé non vide --- */
static void test_active_states_have_text(void)
{
    const ModeUiStatus actifs[] = {MODE_UI_HEATING, MODE_UI_LOCKED,
                                   MODE_UI_REACHED, MODE_UI_WAITING};
    for (unsigned i = 0; i < sizeof(actifs) / sizeof(actifs[0]); i++) {
        TEST_ASSERT_TRUE(mode_status_text(actifs[i])[0] != '\0');
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_inactive_is_idle);
    RUN_TEST(test_heating_wins);
    RUN_TEST(test_active_but_setpoint_already_reached);
    RUN_TEST(test_active_waiting_in_dead_band);
    RUN_TEST(test_locked_explains_inaction);
    RUN_TEST(test_boundary_equal_setpoint);
    RUN_TEST(test_active_states_have_text);
    return UNITY_END();
}
