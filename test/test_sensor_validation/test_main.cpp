/**
 * @file test_main.cpp
 * @brief Tests unitaires de ds18b20_reading_is_valid() (hal/sensor_validation.h)
 *
 * Vérifie le rejet des valeurs invalides du DS18B20 :
 * code d'erreur -127, valeur de power-on-reset 85.0,
 * valeurs hors plage physique [-55, +125].
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/hal/sensor_validation.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Valeurs normales d'un van : acceptées --- */
static void test_normal_values(void)
{
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(21.5f));
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(0.0f));
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(-10.25f));
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(39.9f));
}

/* --- Code d'erreur DallasTemperature (sonde déconnectée) --- */
static void test_disconnected_code(void)
{
    /* DEVICE_DISCONNECTED_C = -127 : hors plage, doit être rejeté */
    TEST_ASSERT_FALSE(ds18b20_reading_is_valid(-127.0f));
}

/* --- Valeur de power-on-reset : conversion non effectuée --- */
static void test_power_on_reset_value(void)
{
    TEST_ASSERT_FALSE(ds18b20_reading_is_valid(85.0f));

    /* Mais les valeurs proches de 85 restent valides : seule la
     * valeur exacte du registre de reset est suspecte */
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(84.9375f)); /* 85 - 1 LSB (12 bits) */
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(85.0625f)); /* 85 + 1 LSB (12 bits) */
}

/* --- Limites de la plage physique du capteur --- */
static void test_physical_range_bounds(void)
{
    /* Bornes incluses : -55 et +125 sont des valeurs datasheet valides */
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(-55.0f));
    TEST_ASSERT_TRUE(ds18b20_reading_is_valid(125.0f));

    /* Juste au-delà : rejeté */
    TEST_ASSERT_FALSE(ds18b20_reading_is_valid(-55.1f));
    TEST_ASSERT_FALSE(ds18b20_reading_is_valid(125.1f));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_normal_values);
    RUN_TEST(test_disconnected_code);
    RUN_TEST(test_power_on_reset_value);
    RUN_TEST(test_physical_range_bounds);
    return UNITY_END();
}
