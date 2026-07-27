/**
 * @file test_cli_parser.h
 * @brief Analyse des commandes du banc de test série — logique pure
 *
 * Grammaire des commandes (une par ligne, insensible aux espaces
 * de tête/queue) :
 *   sim <temp>        → injecter une température simulée (°C)
 *   sim error         → simuler une panne capteur (lectures en échec)
 *   sim off           → revenir à la sonde réelle
 *   thermo <sp> <hy>  → démarrer le thermostat (consigne, hystérésis)
 *   consigne <cible>  → démarrer le mode consigne
 *   timer <min>       → démarrer le minuteur
 *   stop              → arrêter tous les modes (et le chauffage)
 *   status            → afficher l'état complet
 *
 * Header pur (aucune dépendance Arduino) : testé par
 * test/test_test_cli_parser/. L'exécution des commandes est dans
 * test_cli.cpp (dépendant du matériel, compilé avec -DTEST_CLI).
 */

#ifndef HAL_TEST_CLI_PARSER_H
#define HAL_TEST_CLI_PARSER_H

#include <stdlib.h>
#include <string.h>

/** Type de commande reconnue */
enum TestCliCmdType {
    TCLI_EMPTY,     /* Ligne vide : ignorer silencieusement */
    TCLI_ERROR,     /* Commande inconnue ou arguments invalides */
    TCLI_SIM_SET,   /* sim <temp>      → a = température */
    TCLI_SIM_ERROR, /* sim error       → panne capteur simulée */
    TCLI_SIM_OFF,   /* sim off */
    TCLI_THERMO,   /* thermo <sp> <hy> → a = consigne, b = hystérésis */
    TCLI_CONSIGNE, /* consigne <cible> → a = cible */
    TCLI_TIMER,    /* timer <min>      → a = durée en minutes */
    TCLI_STOP,     /* stop */
    TCLI_STATUS    /* status */
};

/** Commande analysée */
struct TestCliCmd {
    TestCliCmdType type;
    float a; /* Premier argument numérique (selon le type) */
    float b; /* Second argument numérique (selon le type) */
};

/* --- Aides internes d'analyse (pur C, sans locale) --- */

/** Avance au-delà des espaces et tabulations. */
inline const char *tcli_skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/**
 * Tente de consommer le mot `word` suivi d'une fin de mot (espace ou
 * fin de chaîne). Retourne le pointeur après le mot, ou NULL si le
 * mot ne correspond pas.
 */
inline const char *tcli_match_word(const char *p, const char *word)
{
    size_t n = strlen(word);
    if (strncmp(p, word, n) != 0) return NULL;
    const char *end = p + n;
    if (*end != '\0' && *end != ' ' && *end != '\t') return NULL;
    return end;
}

/**
 * Tente de lire un nombre flottant. Retourne le pointeur après le
 * nombre (et écrit la valeur dans *out), ou NULL si absent/invalide.
 */
inline const char *tcli_parse_float(const char *p, float *out)
{
    p = tcli_skip_ws(p);
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return NULL; /* Aucun chiffre consommé */
    if (*end != '\0' && *end != ' ' && *end != '\t') return NULL;
    *out = (float)v;
    return end;
}

/** Retourne true si la suite de `p` ne contient que des espaces. */
inline bool tcli_at_end(const char *p)
{
    return *tcli_skip_ws(p) == '\0';
}

/**
 * Analyse une ligne de commande du banc de test.
 *
 * @param line Ligne SANS le retour chariot final
 * @return La commande reconnue (TCLI_ERROR si invalide)
 */
inline TestCliCmd test_cli_parse(const char *line)
{
    TestCliCmd cmd = { TCLI_ERROR, 0.0f, 0.0f };
    const char *p = tcli_skip_ws(line);

    if (*p == '\0') {
        cmd.type = TCLI_EMPTY;
        return cmd;
    }

    const char *rest;

    if ((rest = tcli_match_word(p, "sim")) != NULL) {
        const char *arg = tcli_skip_ws(rest);
        const char *after;
        if ((after = tcli_match_word(arg, "off")) != NULL
            && tcli_at_end(after)) {
            cmd.type = TCLI_SIM_OFF;
        } else if ((after = tcli_match_word(arg, "error")) != NULL
                   && tcli_at_end(after)) {
            cmd.type = TCLI_SIM_ERROR;
        } else if ((rest = tcli_parse_float(rest, &cmd.a)) != NULL
                   && tcli_at_end(rest)) {
            cmd.type = TCLI_SIM_SET;
        }
        return cmd;
    }

    if ((rest = tcli_match_word(p, "thermo")) != NULL) {
        if ((rest = tcli_parse_float(rest, &cmd.a)) != NULL
            && (rest = tcli_parse_float(rest, &cmd.b)) != NULL
            && tcli_at_end(rest)) {
            cmd.type = TCLI_THERMO;
        }
        return cmd;
    }

    if ((rest = tcli_match_word(p, "consigne")) != NULL) {
        if ((rest = tcli_parse_float(rest, &cmd.a)) != NULL
            && tcli_at_end(rest)) {
            cmd.type = TCLI_CONSIGNE;
        }
        return cmd;
    }

    if ((rest = tcli_match_word(p, "timer")) != NULL) {
        if ((rest = tcli_parse_float(rest, &cmd.a)) != NULL
            && tcli_at_end(rest)) {
            cmd.type = TCLI_TIMER;
        }
        return cmd;
    }

    if ((rest = tcli_match_word(p, "stop")) != NULL && tcli_at_end(rest)) {
        cmd.type = TCLI_STOP;
        return cmd;
    }

    if ((rest = tcli_match_word(p, "status")) != NULL && tcli_at_end(rest)) {
        cmd.type = TCLI_STATUS;
        return cmd;
    }

    return cmd; /* TCLI_ERROR */
}

#endif /* HAL_TEST_CLI_PARSER_H */
