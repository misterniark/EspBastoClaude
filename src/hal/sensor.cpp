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
#include "../config.h"
#include <Arduino.h>

/* ================================================================
 * État commun aux deux backends
 * ================================================================ */

/* Valeurs lissées */
static float filtered_temp     = 0.0f;
static float current_humidity  = 0.0f;

/* État d'erreur */
static bool         in_error         = false;
static bool         first_reading    = true;
static unsigned long error_start_ms  = 0;
static unsigned long last_read_ms    = 0;

/* Flag d'initialisation réussie */
static bool initialized = false;

/**
 * Intègre une lecture valide : première lecture directe, puis lissage
 * EMA. Réinitialise l'état d'erreur. Commun aux deux backends.
 *
 * @param raw_temp Température brute en °C
 * @param humidity Humidité relative en % (0 si le capteur n'en fournit pas)
 */
static void apply_reading(float raw_temp, float humidity)
{
    current_humidity = humidity;

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
        Serial.println("[SENSOR] Verifier le branchement sur UART1-OUT (signal = IO18)");
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

void sensor_update(unsigned long interval_ms)
{
    /* Utiliser l'intervalle par défaut si non spécifié */
    unsigned long interval = (interval_ms > 0) ? interval_ms : SENSOR_READ_INTERVAL_MS;
    unsigned long now = millis();

    if (!initialized) {
        /* Sonde absente au boot : retenter périodiquement */
        if (now - last_read_ms < interval) return;
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
         * de lecture est écoulé */
        if (now - last_read_ms < interval) return;
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

void sensor_update(unsigned long interval_ms)
{
    /* Utiliser l'intervalle par défaut si non spécifié */
    unsigned long interval = (interval_ms > 0) ? interval_ms : SENSOR_READ_INTERVAL_MS;

    /* Ne rien faire si l'intervalle n'est pas écoulé */
    unsigned long now = millis();
    if (now - last_read_ms < interval) return;
    last_read_ms = now;

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
