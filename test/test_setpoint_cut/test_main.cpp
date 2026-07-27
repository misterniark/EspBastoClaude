/**
 * @file test_main.cpp
 * @brief Tests unitaires du critère de coupure consigne (core/setpoint_cut.h)
 *
 * Vérifie la logique "N lectures brutes consécutives >= cible" qui
 * déclenche l'arrêt définitif du mode consigne : réactive au
 * franchissement réel de la cible, mais insensible à une lecture
 * aberrante isolée (glitch EMI sur le bus OneWire).
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/core/setpoint_cut.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Sous la cible : jamais de coupure --- */
static void test_below_target_never_cuts(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 18.0f, 20.0f));
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 19.5f, 20.0f));
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 19.9f, 20.0f));
    TEST_ASSERT_EQUAL_UINT8(0, cut.consecutive);
}

/* --- Deux lectures consécutives >= cible : coupure à la deuxième --- */
static void test_two_consecutive_readings_cut(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 20.0f, 20.0f)); /* 1re : pas encore */
    TEST_ASSERT_TRUE(setpoint_cut_update(cut, 20.1f, 20.0f));  /* 2e : coupure */
}

/* --- La cible exacte compte comme atteinte (>=, pas >) --- */
static void test_exact_target_counts(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 20.0f, 20.0f));
    TEST_ASSERT_TRUE(setpoint_cut_update(cut, 20.0f, 20.0f));
}

/* --- Un glitch isolé au-dessus de la cible est absorbé --- */
static void test_single_glitch_absorbed(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 45.0f, 20.0f)); /* Glitch EMI */
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 18.2f, 20.0f)); /* Retour réel */
    TEST_ASSERT_EQUAL_UINT8(0, cut.consecutive);               /* Compteur purgé */
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 18.3f, 20.0f)); /* Chauffe continue */
}

/* --- Alternance glitch/normal répétée : jamais de coupure --- */
static void test_alternating_glitches_never_cut(void)
{
    SetpointCut cut;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE(setpoint_cut_update(cut, 45.0f, 20.0f));
        TEST_ASSERT_FALSE(setpoint_cut_update(cut, 18.0f, 20.0f));
    }
}

/* --- Reset : le compteur repart de zéro (changement de cible) --- */
static void test_reset_clears_counter(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 20.5f, 20.0f)); /* 1 lecture comptée */
    setpoint_cut_reset(cut);                                   /* Nouvelle cible */
    TEST_ASSERT_EQUAL_UINT8(0, cut.consecutive);
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 20.5f, 20.0f)); /* Re-compte depuis 0 */
    TEST_ASSERT_TRUE(setpoint_cut_update(cut, 20.5f, 20.0f));
}

/* --- Seuil paramétrable : needed=3 exige bien 3 lectures --- */
static void test_custom_needed_threshold(void)
{
    SetpointCut cut;
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 21.0f, 20.0f, 3));
    TEST_ASSERT_FALSE(setpoint_cut_update(cut, 21.0f, 20.0f, 3));
    TEST_ASSERT_TRUE(setpoint_cut_update(cut, 21.0f, 20.0f, 3));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_below_target_never_cuts);
    RUN_TEST(test_two_consecutive_readings_cut);
    RUN_TEST(test_exact_target_counts);
    RUN_TEST(test_single_glitch_absorbed);
    RUN_TEST(test_alternating_glitches_never_cut);
    RUN_TEST(test_reset_clears_counter);
    RUN_TEST(test_custom_needed_threshold);
    return UNITY_END();
}
