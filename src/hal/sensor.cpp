/**
 * @file sensor.cpp
 * @brief Implémentation de la lecture du capteur de température avec lissage EMA
 *
 * Deux backends matériels, sélectionnés à la compilation :
 *
 *   - CYD (défaut) : AHT21 (température + humidité) sur le bus I2C
 *     externe CN1 (SDA=27, SCL=22). Lecture synchrone.
 *
 *   - CROWPANEL (-DHW_CROWPANEL) : sonde étanche DS18B20 sur bus
 *     OneWire (broche 18, connecteur UART1-OUT). Température seule.
 *     Lecture ASYNCHRONE : la conversion 12 bits dure 750ms, on lance
 *     la conversion puis on relève le résultat à un passage ultérieur
 *     de la boucle, sans jamais bloquer LVGL.
 *
 * Logique commune aux deux backends :
 *
 * Lissage EMA (Exponential Moving Average) :
 *   valeur_lissée = α × nouvelle_lecture + (1 - α) × valeur_précédente
 *   Avec α = 0.1, la convergence à 95% prend ~30 lectures, soit
 *   ~2,5 minutes à l'intervalle de 5 s utilisé écran allumé
 *   (10 s en veille avec un mode actif — voir main.cpp).
 *
 * Gestion d'erreur :
 *   - Si la lecture échoue, on garde la dernière valeur valide.
 *   - Après 5 minutes d'erreur continue, on signale une erreur critique
 *     pour que le chauffage soit arrêté par sécurité.
 */

#include "sensor.h"
#include "sensor_freshness.h" /* Logique pure de fraîcheur (testée nativement) */
#include "../config.h"
#include <Arduino.h>

/* ================================================================
 * État commun aux deux backends
 * ================================================================ */

/* Valeurs lissées */
static float filtered_temp     = 0.0f;
static float current_humidity  = 0.0f;

/* Dernière lecture BRUTE valide (non lissée) : utilisée par le mode
 * consigne pour couper au plus près de la cible réelle, sans subir le
 * retard du lissage EMA (voir core/setpoint_cut.h). */
static float last_raw_temp     = 0.0f;

/* État d'erreur */
static bool         in_error         = false;
static bool         first_reading    = true;
static unsigned long error_start_ms  = 0;
static unsigned long last_read_ms    = 0;

/* Horodatage de la dernière lecture VALIDE (posé dans apply_reading).
 * Distinct de last_read_ms qui horodate les TENTATIVES de lecture :
 * la fraîcheur d'une décision de chauffage se juge sur les valeurs
 * réellement obtenues, pas sur les essais. */
static unsigned long last_valid_ms = 0;

/* Lecture forcée : armé par sensor_force_read() au réveil de l'écran,
 * consommé par le prochain sensor_update() qui ignore alors son
 * intervalle. Permet de rafraîchir la température immédiatement après
 * une veille pendant laquelle le capteur n'était plus lu. */
static bool force_read = false;

/* Flag d'initialisation réussie */
static bool initialized = false;

#ifdef TEST_CLI
/* ================================================================
 * Simulation de température — BANC DE TEST UNIQUEMENT (-DTEST_CLI)
 *
 * Permet d'injecter des températures arbitraires par le port série
 * pour tester les modes sans toucher la sonde physique. Les valeurs
 * simulées passent par apply_reading() : EMA, fraîcheur et gestion
 * d'erreur réelles sont exercées à l'identique.
 * ================================================================ */
static bool  sim_active = false;
static bool  sim_error  = false; /* Panne capteur simulée (test C1) */
static float sim_temp   = 20.0f;

void sensor_sim_set(float t)
{
    sim_active = true;
    sim_error  = false;
    sim_temp   = t;
}

void sensor_sim_set_error()
{
    sim_active = true;
    sim_error  = true;
}

void sensor_sim_off()
{
    sim_active = false;
    sim_error  = false;
}

bool sensor_sim_is_active()
{
    return sim_active;
}
#endif /* TEST_CLI */

/**
 * Intègre une lecture valide : première lecture directe, puis lissage
 * EMA. Réinitialise l'état d'erreur. Commun aux deux backends.
 *
 * @param raw_temp Température brute en °C
 * @param humidity Humidité relative en % (0 si le capteur n'en fournit pas)
 */
static void apply_reading(float raw_temp, float humidity)
{
    /* RÉ-AMORÇAGE du lissage après une longue interruption des
     * lectures. En veille écran sans mode actif, le capteur n'est plus
     * lu du tout : au réveil, mélanger la première mesure fraîche à une
     * valeur vieille de plusieurs heures donne une température affichée
     * fausse pendant 2 à 3 minutes — et retarde d'autant la coupure de
     * sécurité C4, qui s'appuie sur cette valeur lissée. On repart donc
     * de la mesure. Évalué AVANT de rafraîchir last_valid_ms. */
    if (!first_reading
        && !sensor_reading_is_fresh(millis(), last_valid_ms, true,
                                    SENSOR_EMA_RESET_AGE_MS)) {
        first_reading = true;
        Serial.println("[SENSOR] Lectures interrompues trop longtemps : "
                       "lissage re-amorce");
    }

    current_humidity = humidity;
    last_raw_temp    = raw_temp;
    last_valid_ms    = millis();

    /* Première lecture : initialiser directement (pas de lissage) */
    if (first_reading) {
        filtered_temp = raw_temp;
        first_reading = false;
        Serial.printf("[SENSOR] Première lecture : %.1f°C, %.0f%%\n",
                      raw_temp, humidity);
    } else {
        /* Lissage EMA : α × nouveau + (1 - α) × ancien */
        filtered_temp = SENSOR_EMA_ALPHA * raw_temp
                      + (1.0f - SENSOR_EMA_ALPHA) * filtered_temp;
    }

    /* Réinitialiser l'état d'erreur */
    if (in_error) {
        Serial.println("[SENSOR] Erreur résolue, lecture reprise");
    }
    in_error = false;
    error_start_ms = 0;
}

/**
 * Signale une lecture échouée. Démarre le chronomètre d'erreur au
 * premier échec ; la valeur lissée reste inchangée (dernière valide).
 */
static void mark_read_error()
{
    if (!in_error) {
        in_error = true;
        error_start_ms = millis();
        Serial.println("[SENSOR] Erreur de lecture " SENSOR_NAME);
    }
}

#ifdef HW_CROWPANEL
/* ================================================================
 * Backend DS18B20 (OneWire) — CrowPanel Advance 2.8
 * ================================================================ */

#include <OneWire.h>
#include <DallasTemperature.h>
#include "sensor_validation.h"

static OneWire       onewire(PIN_ONEWIRE);
static DallasTemperature ds(&onewire);

/* Adresse ROM 64 bits de la sonde (première trouvée sur le bus) */
static DeviceAddress ds_addr;

/* Machine d'état de conversion asynchrone */
static bool          conversion_pending  = false;
static unsigned long conversion_start_ms = 0;

/* Nombre d'échecs de lecture consécutifs avant de relancer une
 * re-détection complète de la sonde : un glitch CRC isolé (câble
 * OneWire long, EMI du Webasto) ne doit pas déclencher le chemin
 * de re-détection. */
static constexpr uint8_t DS_MAX_CONSECUTIVE_FAILURES = 3;
static uint8_t consecutive_failures = 0;

/**
 * Recherche la sonde sur le bus et la configure en 12 bits,
 * mode non-bloquant (requestTemperatures() rend la main tout de suite).
 *
 * IMPORTANT : recherche OneWire directe (~5 ms), SANS appeler
 * DallasTemperature::begin() — en v4, begin() contient jusqu'à
 * 3 × delay(50) bloquants ; appelé depuis la boucle lors d'une
 * re-détection, il gèlerait l'UI LVGL 150 ms toutes les 5 s tant
 * que la sonde est absente.
 *
 * @return true si une sonde DS18B20 a été trouvée
 */
static bool ds_probe()
{
    /* Armer le pull-up interne (~45 kΩ) du GPIO : le module Crowtail
     * n'intègre PAS de résistance de rappel externe (vérifié sur
     * matériel : bus lu à 0 en entrée flottante) — sans ce pull-up,
     * le bus OneWire flotte et la sonde est indétectable. Le registre
     * de pull-up de l'ESP32 est indépendant de la direction : il
     * survit aux bascules entrée/sortie en accès direct de la lib
     * OneWire. Pour un câble long en environnement bruité (EMI du
     * Webasto), une vraie résistance externe de 4,7 kΩ entre DATA et
     * 3V3 resterait préférable. */
    pinMode(PIN_ONEWIRE, INPUT_PULLUP);

    onewire.reset_search();
    if (!onewire.search(ds_addr)) return false;

    /* Valider l'adresse ROM : CRC8 + code famille DS18B20 (0x28) */
    if (OneWire::crc8(ds_addr, 7) != ds_addr[7]) return false;
    if (ds_addr[0] != 0x28) return false;

    ds.setResolution(ds_addr, 12);
    ds.setWaitForConversion(false); /* Ne jamais bloquer la boucle */
    return true;
}

/**
 * Comptabilise un échec de lecture et déclenche la re-détection
 * complète de la sonde après DS_MAX_CONSECUTIVE_FAILURES échecs
 * consécutifs (sonde réellement débranchée, pas un glitch isolé).
 */
static void ds_read_failed()
{
    mark_read_error();
    if (++consecutive_failures >= DS_MAX_CONSECUTIVE_FAILURES) {
        consecutive_failures = 0;
        initialized = false;
    }
}

bool sensor_init()
{
    if (!ds_probe()) {
        Serial.printf("[SENSOR] Erreur : DS18B20 non detecte sur OneWire (pin %d)\n",
                      PIN_ONEWIRE);
        Serial.println("[SENSOR] Verifier le branchement sur UART1-OUT");
        in_error = true;
        error_start_ms = millis();
        return false;
    }

    Serial.printf("[SENSOR] DS18B20 initialise (pin %d, resolution 12 bits)\n",
                  PIN_ONEWIRE);
    initialized = true;
    last_read_ms = millis();

    /* Lancer immédiatement la première conversion pour avoir une
     * température dès que possible après le boot */
    ds.requestTemperaturesByAddress(ds_addr);
    conversion_pending  = true;
    conversion_start_ms = millis();
    return true;
}

void sensor_force_read()
{
    force_read = true;

    /* Abandonner une éventuelle conversion restée en attente : si la
     * veille écran a figé la boucle capteur pendant qu'une conversion
     * était en cours, le scratchpad du DS18B20 contient une température
     * mesurée AVANT la veille (potentiellement vieille de plusieurs
     * heures). La relever maintenant la ferait passer pour fraîche
     * (last_valid_ms = maintenant) et réintroduirait exactement le
     * défaut que la lecture forcée corrige. On relance donc une
     * conversion neuve — au pire on perd une conversion légitime en
     * vol (~800 ms de délai supplémentaire, sans conséquence). */
    conversion_pending = false;
}

void sensor_update(unsigned long interval_ms)
{
    /* Utiliser l'intervalle par défaut si non spécifié */
    unsigned long interval = (interval_ms > 0) ? interval_ms : SENSOR_READ_INTERVAL_MS;
    unsigned long now = millis();

    /* Lecture forcée (réveil écran) : consommer le drapeau, les
     * vérifications d'intervalle sont ignorées pour ce passage */
    bool force = force_read;
    force_read = false;

#ifdef TEST_CLI
    /* Banc de test : température simulée injectée au rythme normal
     * des lectures. Le matériel n'est pas sollicité, et une éventuelle
     * conversion en vol est abandonnée (son résultat écraserait la
     * simulation). Fonctionne même sonde absente (initialized false).
     * En mode « sim error », chaque lecture échoue : exerce le vrai
     * chemin d'erreur (in_error, chrono critique C1 de 5 min). */
    if (sim_active) {
        conversion_pending = false;
        if (!force && now - last_read_ms < interval) return;
        last_read_ms = now;
        if (sim_error) {
            mark_read_error();
        } else {
            apply_reading(sim_temp, 0.0f);
        }
        return;
    }
#endif

    if (!initialized) {
        /* Sonde absente au boot : retenter périodiquement */
        if (!force && now - last_read_ms < interval) return;
        last_read_ms = now;

        if (ds_probe()) {
            initialized = true;
            Serial.println("[SENSOR] DS18B20 reconnecte");
        } else {
            mark_read_error();
            return;
        }
    }

    if (conversion_pending) {
        /* Phase 2 : la conversion est-elle terminée ? */
        if (now - conversion_start_ms < DS18B20_CONVERSION_MS) return;

        conversion_pending = false;

        /* Lecture avec 1 relecture automatique en cas d'erreur CRC
         * (~5 ms de bus supplémentaires, sans delay bloquant) */
        float raw_temp = ds.getTempC(ds_addr, 1);

        /* Rejeter les valeurs invalides : -127 (déconnectée),
         * 85.0 (power-on-reset), hors plage physique.
         * Voir sensor_validation.h pour le détail. */
        if (ds18b20_reading_is_valid(raw_temp)) {
            consecutive_failures = 0;
            apply_reading(raw_temp, 0.0f); /* Pas d'humidité sur DS18B20 */
        } else {
            ds_read_failed();
        }
    } else {
        /* Phase 1 : lancer une nouvelle conversion si l'intervalle
         * de lecture est écoulé (ou si une lecture forcée est demandée) */
        if (!force && now - last_read_ms < interval) return;
        last_read_ms = now;

        /* Un échec immédiat de la requête (aucune présence sur le bus)
         * signifie une sonde absente : inutile d'attendre le budget
         * de conversion pour le constater. */
        if (!ds.requestTemperaturesByAddress(ds_addr).result) {
            ds_read_failed();
            return;
        }
        conversion_pending  = true;
        /* Horodatage APRÈS la transaction OneWire (~18 ms) : le budget
         * de conversion court depuis l'ordre de conversion, pas avant.
         * Décompter avant laisserait une marge nulle vis-à-vis du tCONV
         * max de la datasheet (750 ms en 12 bits). */
        conversion_start_ms = millis();
    }
}

#else
/* ================================================================
 * Backend AHT21 (I2C) — CYD ESP32-2432S028R
 *
 * ⚠️ CIBLE NON MAINTENUE depuis le 27/07/2026 (voir platformio.ini).
 * Limite connue : aht.getEvent() est SYNCHRONE (~80 ms de blocage
 * toutes les 5 s), là où le backend DS18B20 du CrowPanel est
 * asynchrone. À reprendre si la cible est un jour réanimée.
 * ================================================================ */

#include <Adafruit_AHTX0.h>
#include <Wire.h>

static Adafruit_AHTX0 aht;

bool sensor_init()
{
    /* Initialiser le bus I2C sur les pins du connecteur CN1 */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    /* Tenter d'initialiser le capteur AHT21 */
    if (!aht.begin(&Wire)) {
        Serial.println("[SENSOR] Erreur : AHT21 non détecté sur I2C (SDA=27, SCL=22)");
        in_error = true;
        error_start_ms = millis();
        return false;
    }

    Serial.println("[SENSOR] AHT21 initialisé avec succès");
    initialized = true;
    last_read_ms = millis();
    return true;
}

void sensor_force_read()
{
    /* AHT21 : lecture I2C synchrone, il suffit d'ignorer l'intervalle
     * au prochain sensor_update() pour obtenir une valeur fraîche */
    force_read = true;
}

void sensor_update(unsigned long interval_ms)
{
    /* Utiliser l'intervalle par défaut si non spécifié */
    unsigned long interval = (interval_ms > 0) ? interval_ms : SENSOR_READ_INTERVAL_MS;

    /* Lecture forcée (réveil écran) : consommer le drapeau, la
     * vérification d'intervalle est ignorée pour ce passage */
    bool force = force_read;
    force_read = false;

    /* Ne rien faire si l'intervalle n'est pas écoulé */
    unsigned long now = millis();
    if (!force && now - last_read_ms < interval) return;
    last_read_ms = now;

#ifdef TEST_CLI
    /* Banc de test : température simulée injectée au rythme normal
     * des lectures (voir le backend DS18B20 pour le détail). */
    if (sim_active) {
        if (sim_error) {
            mark_read_error();
        } else {
            apply_reading(sim_temp, 0.0f);
        }
        return;
    }
#endif

    /* Tenter une lecture */
    sensors_event_t humidity_event, temp_event;

    if (!initialized) {
        /* Tenter de réinitialiser le capteur s'il n'était pas détecté */
        if (aht.begin(&Wire)) {
            initialized = true;
            Serial.println("[SENSOR] AHT21 reconnecté");
        } else {
            /* Toujours en erreur, mettre à jour la durée */
            mark_read_error();
            return;
        }
    }

    if (aht.getEvent(&humidity_event, &temp_event)) {
        /* Lecture réussie */
        apply_reading(temp_event.temperature,
                      humidity_event.relative_humidity);
    } else {
        /* Lecture échouée : la valeur lissée reste inchangée */
        mark_read_error();
    }
}

#endif /* HW_CROWPANEL */

/* ================================================================
 * Accesseurs communs aux deux backends
 * ================================================================ */

float sensor_get_temperature()
{
    return filtered_temp;
}

float sensor_get_raw_temperature()
{
    return last_raw_temp;
}

float sensor_get_humidity()
{
    /* Toujours 0.0 sur CrowPanel : le DS18B20 ne mesure pas l'humidité.
     * (Valeur non utilisée par l'UI actuellement.) */
    return current_humidity;
}

bool sensor_is_error()
{
    return in_error;
}

bool sensor_is_critical_error()
{
    if (!in_error) return false;
    return (millis() - error_start_ms) >= SENSOR_ERROR_TIMEOUT_MS;
}

unsigned long sensor_error_duration_ms()
{
    if (!in_error) return 0;
    return millis() - error_start_ms;
}

bool sensor_has_valid_reading()
{
    return !first_reading;
}

bool sensor_blind_longer_than(unsigned long ms)
{
    return in_error && (millis() - error_start_ms) >= ms;
}

unsigned long sensor_last_valid_reading_ms()
{
    return last_valid_ms;
}

bool sensor_reading_is_recent(unsigned long max_age_ms)
{
    /* Délégation à la logique pure (testée nativement). Le cast en
     * uint32_t est sans perte : millis() est déjà sur 32 bits. */
    return sensor_reading_is_fresh((uint32_t)millis(), (uint32_t)last_valid_ms,
                                   !first_reading, (uint32_t)max_age_ms);
}
