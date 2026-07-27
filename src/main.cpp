/**
 * @file main.cpp
 * @brief Point d'entrée du firmware ESPBasto
 *
 * Orchestre l'initialisation et la boucle principale de tous les modules :
 *   1. Économie d'énergie (CPU 80MHz, LEDs OFF)
 *   2. LVGL + affichage TFT + tactile
 *   3. Thème et styles UI
 *   4. Capteur AHT21
 *   5. Stockage NVS
 *   6. Communication ESP-NOW
 *   7. Machine d'état chauffage
 *   8. Premier écran (recherche relais ou menu)
 *
 * Boucle principale :
 *   - Gestion de l'économie d'énergie (veille écran)
 *   - Mise à jour du capteur (lecture toutes les 2s)
 *   - Mise à jour de la liaison relais (ping, retry)
 *   - Mise à jour de la machine d'état chauffage
 *   - Mise à jour du mode actif (thermostat, minuteur, consigne)
 *   - Rendu LVGL (lv_timer_handler)
 */

#include <Arduino.h>
#include <lvgl.h>

/* Modules HAL */
#include "hal/display.h"
#include "hal/touchpad.h"
#include "hal/backlight.h"
#include "hal/sensor.h"
#include "hal/battery.h"
#include "hal/usb_cdc_kick.h" /* ZLP périodique anti-échouage USB CDC (S3) */
#ifdef TEST_CLI
#include "hal/test_cli.h" /* Banc de test série (env crowpanel_test) */
#endif

/* Modules communication */
#include "comm/espnow_manager.h"
#include "comm/relay_link.h"

/* Modules logique métier */
#include "core/storage.h"
#include "core/heater_fsm.h"
#include "core/mode_thermostat.h"
#include "core/mode_timer.h"
#include "core/mode_setpoint.h"

/* Modules UI */
#include "ui/ui_common.h"
#include "ui/scr_search.h"
#include "ui/scr_menu.h"
#include "ui/ui_alerts.h"

/* Module économie d'énergie */
#include "power/power_manager.h"

/* Configuration */
#include "config.h"

void setup()
{
    /* --- Série (debug) --- */
    Serial.begin(115200);
    delay(500); /* Laisser le temps à la connexion USB */
    Serial.println("\n=== ESPBasto - Contrôleur Webasto ===");
    Serial.println("Démarrage...\n");

    /* --- 1. Économie d'énergie --- */
    power_init();

    /* --- 2. LVGL + affichage + tactile --- */
    lv_init();
    hal_display_init();
    hal_touchpad_init();
    Serial.println("[INIT] LVGL + Display + Touch OK");

    /* --- 3. Thème et styles UI --- */
    ui_common_init();

    /* --- 4. Capteur de température (AHT21 sur CYD, DS18B20 sur CrowPanel) --- */
    bool sensor_ok = sensor_init();
    if (!sensor_ok) {
        Serial.println("[INIT] ATTENTION : Capteur " SENSOR_NAME " non détecté !");
    }

    /* --- 4b. Jauge batterie MAX17048 (même bus I2C) --- */
    bool batt_ok = battery_init();
    if (!batt_ok) {
        Serial.println("[INIT] ATTENTION : MAX17048 non détecté !");
    }

    /* --- 5. Stockage NVS --- */
    storage_init();

    /* Charger les paramètres sauvegardés */
    float setpoint = storage_load_setpoint();
    int hysteresis = storage_load_hysteresis();
    int timer_min  = storage_load_timer_min();
    Serial.printf("[INIT] Paramètres NVS : setpoint=%.1f, hyst=%d, timer=%d min\n",
                  setpoint, hysteresis, timer_min);

    /* Initialiser les modes avec les valeurs chargées */
    thermostat_set_setpoint(setpoint);
    thermostat_set_hysteresis(hysteresis);
    timer_mode_set_duration_min(timer_min);
    setpoint_mode_set_target(setpoint);

    /* --- 6. Communication ESP-NOW --- */
    espnow_init();
    relay_link_init();

    /* --- 7. Machine d'état chauffage --- */
    heater_fsm_init();

    /* --- 8. Premier écran --- */
    /*
     * Si le relais est déjà connecté (MAC sauvegardée + PONG rapide),
     * aller directement au menu. Sinon, afficher l'écran de recherche.
     */
    if (relay_is_connected()) {
        scr_menu_create();
    } else {
        scr_search_create();
    }

    Serial.println("\n[INIT] Démarrage terminé");
    Serial.printf("[INIT] Mémoire libre : %d octets\n", ESP.getFreeHeap());
}

#ifdef TEST_CLI
/* ================================================================
 * Chronométrage des sections de loop() — BANC DE TEST UNIQUEMENT
 *
 * Diagnostic des gels de boucle observés au banc série (réponses
 * [TCLI] retardées de plusieurs secondes autour de la veille écran) :
 * on horodate un jalon entre chaque section de loop() ; si le tour
 * complet dépasse LP_REPORT_THRESHOLD_MS, on imprime la durée de
 * chaque section pour désigner la fonction bloquante.
 * ================================================================ */
static const char *LP_NAMES[] = {
    "power", "tcli", "sensor", "battery", "relay", "fsm",
    "modes", "alerts", "touch", "lvgl", "delay"
};
constexpr int      LP_COUNT = sizeof(LP_NAMES) / sizeof(LP_NAMES[0]);
constexpr uint32_t LP_REPORT_THRESHOLD_MS = 100;
static uint32_t lp_mark[LP_COUNT + 1]; /* jalons : début + fin de chaque section */
static int      lp_idx = 0;
static uint32_t lp_prev_end = 0;       /* fin du tour précédent (trou inter-tours) */

/* Poser le jalon de fin de la section courante */
#define LP_MARK() do { if (lp_idx <= LP_COUNT) lp_mark[lp_idx++] = millis(); } while (0)

/* Bilan de fin de tour : imprimer les sections d'un tour anormalement long,
 * et signaler un éventuel blocage ENTRE deux tours (hors de loop() :
 * serialEventRun ou autre code de la tâche Arduino). */
static void lp_report()
{
    /* Trou entre la fin du tour précédent et le début de celui-ci */
    if (lp_prev_end != 0 && lp_mark[0] - lp_prev_end >= LP_REPORT_THRESHOLD_MS) {
        Serial.printf("[LOOPTIME] trou inter-tours de %lu ms\n",
                      (unsigned long)(lp_mark[0] - lp_prev_end));
    }
    lp_prev_end = lp_mark[lp_idx - 1];

    uint32_t total = lp_mark[lp_idx - 1] - lp_mark[0];
    if (total < LP_REPORT_THRESHOLD_MS) return;

    Serial.printf("[LOOPTIME] tour de %lu ms :", (unsigned long)total);
    for (int i = 1; i < lp_idx; i++) {
        uint32_t d = lp_mark[i] - lp_mark[i - 1];
        if (d > 0) Serial.printf(" %s=%lu", LP_NAMES[i - 1], (unsigned long)d);
    }
    Serial.println();
}
#else
#define LP_MARK() ((void)0)
#endif /* TEST_CLI */

void loop()
{
#ifdef TEST_CLI
    lp_idx = 0;
    LP_MARK(); /* Jalon de départ du tour */
#endif

    /*
     * 1. Économie d'énergie : vérifier le timeout écran et le réveil.
     *    Si un toucher de réveil est consommé, on ne traite pas LVGL
     *    ce cycle pour éviter un appui fantôme.
     */
    bool touch_consumed = power_update();
    LP_MARK();

#ifdef TEST_CLI
    /* 1bis. Banc de test : exécuter les commandes série en attente
     * (simulation de température, pilotage des modes). */
    test_cli_update();
    LP_MARK();
#endif

    /* 2. Capteur : lecture périodique + lissage EMA.
     * Intervalle adaptatif pour économiser la batterie :
     *   - Écran actif  : 5s (affichage fluide)
     *   - Écran veille + mode actif : 10s (thermostat décide toutes les 60s)
     *   - Écran veille + aucun mode : pas de lecture (inutile)
     *
     * Conséquence assumée du dernier cas : au réveil, la température est
     * gelée depuis la mise en veille. Deux protections compensent :
     *   - power_update() force une lecture immédiate au réveil
     *     (sensor_force_read) — elle a lieu dès le sensor_update()
     *     ci-dessous, l'écran étant rallumé ;
     *   - les gardes I4 de thermostat_start()/setpoint_mode_start()
     *     refusent tout démarrage tant que la dernière lecture VALIDE
     *     date de plus de SENSOR_MAX_AGE_BEFORE_START_MS (60 s).
     */
    {
        bool any_mode = thermostat_is_active() || timer_mode_is_running() || setpoint_mode_is_active();
#ifdef TEST_CLI
        /* Banc de test : avec une température simulée, continuer à
         * alimenter le capteur même écran éteint sans mode actif
         * (sinon la simulation gèlerait dès la veille écran). */
        if (sensor_sim_is_active()) {
            sensor_update(5000);
        } else
#endif
        if (power_is_screen_off() && !any_mode) {
            /* Rien à faire — pas de lecture capteur */
        } else if (power_is_screen_off() && any_mode) {
            sensor_update(10000); /* 10s en veille avec mode actif */
        } else {
            sensor_update(5000);  /* 5s écran allumé */
        }
    }
    LP_MARK();

    /* 2b. Batterie : lecture toutes les 30s */
    battery_update();
    LP_MARK();

    /* 3. Communication relais : ping, découverte, retry */
    relay_link_update();
    LP_MARK();

    /* 4. Machine d'état chauffage : détection perte connexion */
    heater_fsm_update();
    LP_MARK();

    /* 5. Mise à jour du mode actif.
     * I3 — Garde de sécurité : si plus d'un mode est actif
     * simultanément (ne devrait pas arriver), on les arrête tous.
     */
    int active_count = (thermostat_is_active() ? 1 : 0)
                     + (timer_mode_is_running() ? 1 : 0)
                     + (setpoint_mode_is_active() ? 1 : 0);
    if (active_count > 1) {
        Serial.println("[MAIN] SECURITE : plusieurs modes actifs ! Arret de tous les modes.");
        thermostat_stop();
        timer_mode_stop();
        setpoint_mode_stop();
    }

    thermostat_update();
    timer_mode_update();
    setpoint_mode_update();
    LP_MARK();

    /*
     * 5bis. Dispatcher des alertes plein écran : consomme les drapeaux
     * levés par heater_fsm (arrêts de sécurité C1/C4, perte de
     * connexion I5) et l'état capteur. Appelé ici — et non depuis un
     * timer LVGL — pour fonctionner aussi pendant la veille écran :
     * il réveille l'écran si une alerte survient (l'utilisateur doit
     * savoir POURQUOI le chauffage s'est coupé). Placé avant le rendu
     * pour que l'alerte soit dessinée dès ce cycle.
     */
    ui_alerts_update();
    LP_MARK();

    /*
     * 6. Rendu LVGL : traiter les timers, les animations et le redessin.
     *    Le handler gère aussi la lecture du touchpad via le callback enregistré.
     *    Si un toucher de réveil a été consommé, le driver tactile masque
     *    les contacts pour LVGL jusqu'au relâchement du doigt
     *    (hal_touchpad_ignore_until_release, armé par power_update).
     */
    if (!touch_consumed) {
        /* Réinitialiser le timer d'inactivité si un toucher est actif */
        if (hal_touchpad_is_touched()) {
            power_reset_inactivity();
        }
    }
    LP_MARK();

    /*
     * 7. Rendu LVGL : uniquement si l'écran est allumé.
     * En veille, on ne rend rien (économie CPU + SPI).
     * Au réveil, power_update() appelle lv_obj_invalidate()
     * pour forcer un redessin complet.
     */
    if (!power_is_screen_off()) {
        lv_timer_handler();
    }
    LP_MARK();

    /* Délai pour ne pas surcharger le CPU */
    delay(5);
    LP_MARK();

    /*
     * 8. Anti-échouage USB CDC (CrowPanel/ESP32-S3 uniquement, no-op
     * sur CYD) : émet un ZLP pour terminer toute transaction USB
     * laissée ouverte par un paquet de 64 octets pile — sans quoi les
     * lignes série peuvent rester invisibles côté hôte plusieurs
     * secondes (voir hal/usb_cdc_kick.h pour l'analyse complète).
     */
    usb_cdc_kick();

#ifdef TEST_CLI
    /* Bilan du tour : signaler les tours anormalement longs */
    lp_report();
#endif
}
