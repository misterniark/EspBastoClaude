/**
 * @file sensor.h
 * @brief Lecture du capteur de température avec lissage EMA
 *
 * Gère la lecture périodique du capteur de température :
 *   - CYD : AHT21 (température + humidité) sur I2C externe (CN1)
 *   - CrowPanel : sonde étanche DS18B20 (température seule) sur
 *     OneWire, connecteur UART1-OUT (lecture asynchrone, 750ms)
 * Applique un lissage par moyenne mobile exponentielle (α=0.1).
 * Détecte les erreurs de lecture et gère le timeout de sécurité (5 min).
 */

#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

/**
 * Initialise le capteur de température.
 *   - CYD : Wire.begin(SDA=27, SCL=22) puis détection AHT21.
 *   - CrowPanel : détection DS18B20 sur le bus OneWire (pin 18)
 *     et lancement de la première conversion.
 *
 * @return true si le capteur est détecté et initialisé
 */
bool sensor_init();

/**
 * Met à jour la lecture du capteur si l'intervalle est écoulé.
 * À appeler dans la boucle principale (loop).
 * Applique le lissage EMA sur la température.
 *
 * @param interval_ms Intervalle de lecture en ms (0 = utiliser la valeur par défaut).
 *                    Permet d'espacer les lectures en mode veille.
 */
void sensor_update(unsigned long interval_ms = 0);

/** Retourne la température lissée (EMA) en °C.
 *  ATTENTION : retourne 0.0 si aucune lecture valide n'a été faite.
 *  Vérifier sensor_has_valid_reading() avant d'utiliser cette valeur. */
float sensor_get_temperature();

/** Retourne la dernière lecture BRUTE valide (non lissée) en °C.
 *  Plus réactive que la valeur EMA (qui traîne ~45 s derrière la
 *  réalité) mais sensible au bruit : à n'utiliser qu'avec un critère
 *  multi-lectures (voir core/setpoint_cut.h). Mêmes précautions que
 *  sensor_get_temperature() : 0.0 tant qu'aucune lecture valide. */
float sensor_get_raw_temperature();

/** Retourne true si au moins une lecture valide a été effectuée.
 *  Tant que false, sensor_get_temperature() n'est pas fiable. */
bool sensor_has_valid_reading();

/** Retourne l'horodatage millis() de la dernière lecture VALIDE.
 *  Sans signification tant que sensor_has_valid_reading() est false. */
unsigned long sensor_last_valid_reading_ms();

/**
 * Retourne true si la dernière lecture VALIDE date d'au plus max_age_ms.
 * false si aucune lecture valide n'a jamais eu lieu.
 *
 * En veille écran sans mode actif, le capteur n'est plus lu du tout :
 * au réveil, sensor_get_temperature() peut renvoyer une valeur gelée
 * depuis des heures alors que sensor_is_error() est resté false.
 * Cette fonction fournit le critère de fraîcheur manquant, utilisé
 * par les gardes I4 des modes thermostat et consigne.
 *
 * @param max_age_ms Âge maximal accepté (borne incluse),
 *                   typiquement SENSOR_MAX_AGE_BEFORE_START_MS
 */
bool sensor_reading_is_recent(unsigned long max_age_ms);

/**
 * Force une lecture immédiate au prochain appel de sensor_update()
 * (l'intervalle de lecture est ignoré pour ce passage).
 * À appeler au réveil de l'écran pour rafraîchir la température
 * avant que l'utilisateur ne démarre un mode.
 *
 *   - CYD (AHT21)       : la lecture I2C est synchrone, la valeur est
 *                         fraîche dès le sensor_update() suivant.
 *   - CrowPanel (DS18B20) : une éventuelle conversion restée en attente
 *                         depuis la veille est ABANDONNÉE (son résultat
 *                         daterait d'avant la veille) et une conversion
 *                         neuve est lancée ; la valeur fraîche arrive
 *                         ~800 ms plus tard (DS18B20_CONVERSION_MS).
 */
void sensor_force_read();

/** Retourne l'humidité relative en %.
 *  ATTENTION : toujours 0.0 sur CrowPanel (le DS18B20 ne mesure
 *  que la température). */
float sensor_get_humidity();

/** Retourne true si le capteur est en erreur (lecture échouée). */
bool sensor_is_error();

/**
 * Retourne true si le capteur est en erreur critique :
 * plus de 5 minutes sans lecture valide.
 * Le chauffage doit être arrêté par sécurité.
 */
bool sensor_is_critical_error();

/**
 * Retourne true si le capteur est en erreur DEPUIS AU MOINS ms.
 *
 * Sert au seuil raccourci appliqué PENDANT une chauffe
 * (SENSOR_ERROR_TIMEOUT_HEATING_MS) : tolérer 5 minutes d'aveuglement
 * est acceptable à l'arrêt, pas avec un appareil à combustion allumé —
 * d'autant que la panne de sonde la plus probable (parasites du Webasto
 * sur le câble OneWire) survient précisément pendant la chauffe.
 *
 * Le même critère est utilisé par heater_fsm (coupure) et par les modes
 * thermostat/consigne (arrêt du mode) : les deux doivent se déclencher
 * au même instant, sinon un mode encore actif redemanderait aussitôt
 * l'allumage que la sécurité vient de couper — cyclage du relais.
 */
bool sensor_blind_longer_than(unsigned long ms);

/** Retourne la durée de l'erreur en cours (en ms), ou 0 si pas d'erreur. */
unsigned long sensor_error_duration_ms();

#ifdef TEST_CLI
/* ==========================================
 * Simulation de température — BANC DE TEST UNIQUEMENT (-DTEST_CLI)
 * Jamais compilé dans les firmwares normaux.
 * ========================================== */

/** Active la simulation : les lectures matérielles sont remplacées
 *  par cette valeur (injectée via apply_reading, donc EMA/fraîcheur
 *  réelles), au rythme normal des lectures. */
void sensor_sim_set(float temp);

/** Simule une PANNE capteur : chaque lecture échoue (exerce le vrai
 *  chemin d'erreur — in_error, chrono critique C1 de 5 min). */
void sensor_sim_set_error();

/** Coupe la simulation : retour aux lectures matérielles. */
void sensor_sim_off();

/** Retourne true si la simulation est active. */
bool sensor_sim_is_active();
#endif /* TEST_CLI */

#endif /* HAL_SENSOR_H */
