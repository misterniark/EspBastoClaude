/**
 * @file test_main.cpp
 * @brief Tests unitaires du parseur de commandes du banc de test
 *        (hal/test_cli_parser.h)
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/hal/test_cli_parser.h"

void setUp(void) {}
void tearDown(void) {}

/* --- sim <temp> --- */
static void test_sim_set(void)
{
    TestCliCmd c = test_cli_parse("sim 26.5");
    TEST_ASSERT_EQUAL(TCLI_SIM_SET, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 26.5f, c.a);

    /* Valeur négative (van en hiver) */
    c = test_cli_parse("sim -12.5");
    TEST_ASSERT_EQUAL(TCLI_SIM_SET, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.5f, c.a);
}

/* --- sim off / sim error --- */
static void test_sim_off(void)
{
    TEST_ASSERT_EQUAL(TCLI_SIM_OFF, test_cli_parse("sim off").type);
}

static void test_sim_error(void)
{
    TEST_ASSERT_EQUAL(TCLI_SIM_ERROR, test_cli_parse("sim error").type);
    TEST_ASSERT_EQUAL(TCLI_SIM_ERROR, test_cli_parse("  sim  error ").type);
    /* Préfixe/suffixe : pas des commandes valides */
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("sim errors").type);
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("sim error 5").type);
}

/* --- thermo <sp> <hyst> --- */
static void test_thermo(void)
{
    TestCliCmd c = test_cli_parse("thermo 25 3");
    TEST_ASSERT_EQUAL(TCLI_THERMO, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, c.a);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, c.b);

    /* Consigne décimale */
    c = test_cli_parse("thermo 21.5 2");
    TEST_ASSERT_EQUAL(TCLI_THERMO, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.5f, c.a);

    /* Argument manquant : erreur */
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("thermo 25").type);
}

/* --- consigne <cible> / timer <min> --- */
static void test_consigne_and_timer(void)
{
    TestCliCmd c = test_cli_parse("consigne 24");
    TEST_ASSERT_EQUAL(TCLI_CONSIGNE, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.0f, c.a);

    c = test_cli_parse("timer 2");
    TEST_ASSERT_EQUAL(TCLI_TIMER, c.type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, c.a);

    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("consigne").type);
}

/* --- stop / status --- */
static void test_stop_and_status(void)
{
    TEST_ASSERT_EQUAL(TCLI_STOP, test_cli_parse("stop").type);
    TEST_ASSERT_EQUAL(TCLI_STATUS, test_cli_parse("status").type);
}

/* --- Espaces de tête/queue tolérés --- */
static void test_whitespace_tolerance(void)
{
    TEST_ASSERT_EQUAL(TCLI_SIM_SET, test_cli_parse("  sim  26.5  ").type);
    TEST_ASSERT_EQUAL(TCLI_STOP, test_cli_parse("\tstop ").type);
}

/* --- Lignes vides et commandes invalides --- */
static void test_empty_and_invalid(void)
{
    TEST_ASSERT_EQUAL(TCLI_EMPTY, test_cli_parse("").type);
    TEST_ASSERT_EQUAL(TCLI_EMPTY, test_cli_parse("   ").type);

    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("chauffe").type);
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("sim abc").type);
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("sim").type);
    /* Préfixe d'un mot valide : pas une commande */
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("stopnow").type);
    /* Arguments en trop : erreur (protège d'une faute de frappe) */
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("sim 26.5 27").type);
    TEST_ASSERT_EQUAL(TCLI_ERROR, test_cli_parse("stop 5").type);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_sim_set);
    RUN_TEST(test_sim_off);
    RUN_TEST(test_sim_error);
    RUN_TEST(test_thermo);
    RUN_TEST(test_consigne_and_timer);
    RUN_TEST(test_stop_and_status);
    RUN_TEST(test_whitespace_tolerance);
    RUN_TEST(test_empty_and_invalid);
    return UNITY_END();
}
