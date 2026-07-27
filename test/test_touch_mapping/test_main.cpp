/**
 * @file test_main.cpp
 * @brief Tests unitaires de touch_map_point() (hal/touch_mapping.h)
 *
 * Vérifie la transformation des coordonnées tactiles brutes vers
 * les coordonnées écran : identité, échange d'axes, miroirs,
 * bornage aux limites de l'écran.
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/hal/touch_mapping.h"

/* Dimensions écran du projet (portrait) */
static constexpr int16_t W = 240;
static constexpr int16_t H = 320;

void setUp(void) {}
void tearDown(void) {}

/* --- Transformation identité : aucun drapeau actif --- */
static void test_identity(void)
{
    TouchPoint p = touch_map_point(100, 200, W, H, false, false, false);
    TEST_ASSERT_EQUAL_INT16(100, p.x);
    TEST_ASSERT_EQUAL_INT16(200, p.y);
}

/* --- Échange des axes X/Y --- */
static void test_swap_xy(void)
{
    /* Un point brut (50, 120) devient (120, 50) après échange */
    TouchPoint p = touch_map_point(50, 120, W, H, true, false, false);
    TEST_ASSERT_EQUAL_INT16(120, p.x);
    TEST_ASSERT_EQUAL_INT16(50, p.y);
}

/* --- Miroir horizontal : x devient largeur-1-x --- */
static void test_mirror_x(void)
{
    TouchPoint p = touch_map_point(0, 10, W, H, false, true, false);
    TEST_ASSERT_EQUAL_INT16(W - 1, p.x);
    TEST_ASSERT_EQUAL_INT16(10, p.y);
}

/* --- Miroir vertical : y devient hauteur-1-y --- */
static void test_mirror_y(void)
{
    TouchPoint p = touch_map_point(10, 0, W, H, false, false, true);
    TEST_ASSERT_EQUAL_INT16(10, p.x);
    TEST_ASSERT_EQUAL_INT16(H - 1, p.y);
}

/* --- Combinaison échange + double miroir (rotation 180° + swap) --- */
static void test_swap_and_mirrors(void)
{
    /* Brut (10, 20) → swap → (20, 10) → miroirs → (W-1-20, H-1-10) */
    TouchPoint p = touch_map_point(10, 20, W, H, true, true, true);
    TEST_ASSERT_EQUAL_INT16(W - 1 - 20, p.x);
    TEST_ASSERT_EQUAL_INT16(H - 1 - 10, p.y);
}

/* --- Bornage : coordonnées négatives ramenées à 0 --- */
static void test_clamp_negative(void)
{
    TouchPoint p = touch_map_point(-5, -10, W, H, false, false, false);
    TEST_ASSERT_EQUAL_INT16(0, p.x);
    TEST_ASSERT_EQUAL_INT16(0, p.y);
}

/* --- Bornage : coordonnées trop grandes ramenées au bord --- */
static void test_clamp_overflow(void)
{
    /* Le FT6236 peut renvoyer des valeurs jusqu'à 4095 (12 bits)
     * lors de glitches : elles ne doivent jamais sortir de l'écran */
    TouchPoint p = touch_map_point(4095, 4095, W, H, false, false, false);
    TEST_ASSERT_EQUAL_INT16(W - 1, p.x);
    TEST_ASSERT_EQUAL_INT16(H - 1, p.y);
}

/* --- Bornage après miroir : un brut hors plage resterait négatif --- */
static void test_clamp_after_mirror(void)
{
    /* Brut x=300 avec miroir : 240-1-300 = -61 → borné à 0 */
    TouchPoint p = touch_map_point(300, 10, W, H, false, true, false);
    TEST_ASSERT_EQUAL_INT16(0, p.x);
    TEST_ASSERT_EQUAL_INT16(10, p.y);
}

/* --- Coins de l'écran : valeurs limites exactes --- */
static void test_corners(void)
{
    TouchPoint p1 = touch_map_point(0, 0, W, H, false, false, false);
    TEST_ASSERT_EQUAL_INT16(0, p1.x);
    TEST_ASSERT_EQUAL_INT16(0, p1.y);

    TouchPoint p2 = touch_map_point(W - 1, H - 1, W, H, false, false, false);
    TEST_ASSERT_EQUAL_INT16(W - 1, p2.x);
    TEST_ASSERT_EQUAL_INT16(H - 1, p2.y);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_identity);
    RUN_TEST(test_swap_xy);
    RUN_TEST(test_mirror_x);
    RUN_TEST(test_mirror_y);
    RUN_TEST(test_swap_and_mirrors);
    RUN_TEST(test_clamp_negative);
    RUN_TEST(test_clamp_overflow);
    RUN_TEST(test_clamp_after_mirror);
    RUN_TEST(test_corners);
    return UNITY_END();
}
