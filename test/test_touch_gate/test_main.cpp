/**
 * @file test_main.cpp
 * @brief Tests unitaires du filtre touch_gate (hal/touch_gate.h)
 *
 * Vérifie la logique "ignorer jusqu'au relâchement" utilisée pour
 * consommer le toucher de réveil de l'écran : une fois armé, le
 * filtre masque tous les contacts jusqu'à observer un relâchement
 * complet, puis redevient transparent.
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/hal/touch_gate.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Filtre non armé : transparent --- */
static void test_transparent_by_default(void)
{
    TouchGate gate;
    TEST_ASSERT_TRUE(touch_gate_pass(gate, true));   /* Contact transmis */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, false)); /* Pas de contact */
}

/* --- Armé : le contact en cours est masqué tant qu'il dure --- */
static void test_armed_masks_ongoing_touch(void)
{
    TouchGate gate;
    touch_gate_arm(gate);

    /* Le doigt de réveil est encore posé : masqué, à chaque échantillon */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, true));
    TEST_ASSERT_FALSE(touch_gate_pass(gate, true));
    TEST_ASSERT_FALSE(touch_gate_pass(gate, true));
}

/* --- Le relâchement désarme le filtre ; le contact suivant passe --- */
static void test_release_disarms(void)
{
    TouchGate gate;
    touch_gate_arm(gate);

    TEST_ASSERT_FALSE(touch_gate_pass(gate, true));  /* Doigt posé : masqué */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, false)); /* Relâchement : désarme */
    TEST_ASSERT_TRUE(touch_gate_pass(gate, true));   /* Nouveau contact : transmis */
}

/* --- Armé puis relâchement immédiat (tap très bref pendant le réveil) --- */
static void test_armed_then_immediate_release(void)
{
    TouchGate gate;
    touch_gate_arm(gate);

    /* Le doigt a déjà été levé avant le premier échantillon LVGL :
     * le filtre se désarme sans avoir masqué de contact */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, false));
    TEST_ASSERT_TRUE(touch_gate_pass(gate, true));
}

/* --- Ré-armement pendant un contact déjà transmis --- */
static void test_rearm_during_touch(void)
{
    TouchGate gate;

    TEST_ASSERT_TRUE(touch_gate_pass(gate, true)); /* Contact normal */
    touch_gate_arm(gate);                           /* Nouvelle veille/réveil */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, true)); /* Masqué à nouveau */
    TEST_ASSERT_FALSE(touch_gate_pass(gate, false));
    TEST_ASSERT_TRUE(touch_gate_pass(gate, true));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_transparent_by_default);
    RUN_TEST(test_armed_masks_ongoing_touch);
    RUN_TEST(test_release_disarms);
    RUN_TEST(test_armed_then_immediate_release);
    RUN_TEST(test_rearm_during_touch);
    return UNITY_END();
}
