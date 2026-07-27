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

void loop()
{
    /*
     * 1. Économie d'énergie : vérifier le timeout écran et le réveil.
     *    Si un toucher de réveil est consommé, on ne traite pas LVGL
     *    ce cycle pour éviter un appui fantôme.
     */
    bool touch_consumed = power_update();

#ifdef TEST_CLI
    /* 1bis. Banc de test : exécuter les commandes série en attente
     * (simulation de température, pilotage des modes). */
    test_cli_update();
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

    /* 2b. Batterie : lecture toutes les 30s */
    battery_update();

    /* 3. Communication relais : ping, découverte, retry */
    relay_link_update();

    /* 4. Machine d'état chauffage : détection perte connexion */
    heater_fsm_update();

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

    /*
     * 7. Rendu LVGL : uniquement si l'écran est allumé.
     * En veille, on ne rend rien (économie CPU + SPI).
     * Au réveil, power_update() appelle lv_obj_invalidate()
     * pour forcer un redessin complet.
     */
    if (!power_is_screen_off()) {
        lv_timer_handler();
    }

    /* Délai pour ne pas surcharger le CPU */
    delay(5);
}
