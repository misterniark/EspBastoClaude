/**
 * @file test_main.cpp
 * @brief Tests unitaires de sensor_reading_is_fresh() (hal/sensor_freshness.h)
 *
 * Vérifie la logique de fraîcheur d'une lecture capteur :
 *   - une lecture récente est fraîche, une lecture trop vieille ne l'est pas
 *   - sans aucune lecture valide, jamais fraîche (quel que soit l'horodatage)
 *   - la borne d'âge maximal est incluse (age == max → encore fraîche)
 *   - le débordement de millis() (uint32_t, ~49,7 jours) est géré par
 *     l'arithmétique non signée : now < last_valid ne doit pas casser le calcul
 *
 * Contexte : cette fonction protège le démarrage des modes thermostat et
 * consigne après un réveil d'écran, quand la dernière lecture capteur peut
 * dater de plusieurs heures (aucune lecture en veille sans mode actif).
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include <stdint.h>
#include "../../src/hal/sensor_freshness.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Lecture récente : fraîche --- */
static void test_recent_reading_is_fresh(void)
{
    /* Lecture il y a 5 s, âge max 60 s → fraîche */
    TEST_ASSERT_TRUE(sensor_reading_is_fresh(65000u, 60000u, true, 60000u));

    /* Lecture à l'instant même (âge 0) → fraîche */
    TEST_ASSERT_TRUE(sensor_reading_is_fresh(60000u, 60000u, true, 60000u));
}

/* --- Lecture trop vieille : pas fraîche --- */
static void test_old_reading_is_stale(void)
{
    /* Lecture il y a 61 s, âge max 60 s → obsolète */
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(161000u, 100000u, true, 60000u));

    /* Lecture il y a 2 h (cas réel : réveil après une longue veille) */
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(7200000u + 1000u, 1000u, true, 60000u));
}

/* --- Borne : âge exactement égal au maximum → encore fraîche --- */
static void test_max_age_boundary_inclusive(void)
{
    /* age == max_age : accepté (borne incluse) */
    TEST_ASSERT_TRUE(sensor_reading_is_fresh(160000u, 100000u, true, 60000u));

    /* age == max_age + 1 ms : refusé */
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(160001u, 100000u, true, 60000u));
}

/* --- Aucune lecture valide : jamais fraîche --- */
static void test_no_valid_reading_never_fresh(void)
{
    /* Même avec des horodatages "parfaits", sans lecture valide
     * la valeur de température est le 0.0 initial : inutilisable */
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(1000u, 1000u, false, 60000u));
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(0u, 0u, false, 60000u));
}

/* --- Débordement de millis() : arithmétique non signée --- */
static void test_millis_wraparound(void)
{
    /* Lecture juste avant le débordement (uint32_t max - 1000),
     * "maintenant" juste après (5000) : âge réel = 6001 ms → fraîche.
     * Sans arithmétique non signée 32 bits, now - last serait négatif
     * ou gigantesque et le calcul serait faux. */
    const uint32_t last = UINT32_MAX - 1000u;
    TEST_ASSERT_TRUE(sensor_reading_is_fresh(5000u, last, true, 60000u));

    /* Même scénario mais avec un âge réel de 2 min → obsolète */
    TEST_ASSERT_FALSE(sensor_reading_is_fresh(120000u, last, true, 60000u));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_recent_reading_is_fresh);
    RUN_TEST(test_old_reading_is_stale);
    RUN_TEST(test_max_age_boundary_inclusive);
    RUN_TEST(test_no_valid_reading_never_fresh);
    RUN_TEST(test_millis_wraparound);
    return UNITY_END();
}
