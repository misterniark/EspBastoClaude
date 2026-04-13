# Spécifications Techniques - ESPBastoRelay

## Matériel

### Microcontrôleur

| Paramètre | Valeur |
|-----------|--------|
| Modèle | Seeed Studio XIAO ESP32S3 |
| Architecture | Xtensa LX7 dual-core 32-bit |
| Fréquence CPU | 80 MHz (mode économie) |
| Flash | 8 MB |
| PSRAM | 8 MB (ESP32S3 Sense) / Non (standard) |
| RAM | 512 KB SRAM |
| WiFi | 802.11 b/g/n |
| BLE | Bluetooth 5.0 LE |
| USB | USB natif (CDC) |

### Module Relais

| Paramètre | Valeur |
|-----------|--------|
| Modèle | 1-Channel Relay for Seeed Studio XIAO |
| Format | Shield enfichable directement sur XIAO |
| Tension alimentation | 5V DC |
| Tension commande | 3.3V (compatible ESP32) |
| Logique | Active HIGH |
| Courant commutation | 10A max |
| Pin de commande | D1 (GPIO2) |

## Brochage

### GPIO Utilisés

| GPIO | Pin XIAO | Fonction | Direction | Notes |
|------|----------|----------|-----------|-------|
| 2    | D1       | Commande relais | OUTPUT | Connecté au shield relais |
| 21   | -        | LED statut | OUTPUT | LED utilisateur intégrée au XIAO ESP32S3 |

### Connexions

```
XIAO ESP32S3                  1-Channel Relay Shield
┌─────────────────┐          ┌─────────────────────┐
│                 │          │                     │
│   (enfichable directement sur le shield)        │
│                 │          │                     │
│          D1/GPIO2├────────►│Commande Relais      │
│               5V├────────►│VCC                  │
│              GND├────────►│GND                  │
└─────────────────┘          └─────────────────────┘
                                    │
                              Vers Webasto
```

## Communication ESP-NOW

### Caractéristiques

| Paramètre | Valeur |
|-----------|--------|
| Protocole | ESP-NOW (Espressif) |
| Portée | ~200m (ligne de vue) |
| Latence | < 10ms |
| Encryption | Non (désactivée) |
| Canal WiFi | Auto (canal 0) |

### Structure des messages

```c
typedef struct {
    uint8_t command;  // 1 octet
} esp_now_message_t;
```

### Codes commandes

| Direction | Code | Nom | Description |
|-----------|------|-----|-------------|
| Contrôleur → Relais | 1 | CMD_HEAT_ON | Activer chauffage |
| Contrôleur → Relais | 2 | CMD_HEAT_OFF | Désactiver chauffage |
| Contrôleur → Relais | 3 | CMD_PING | Vérifier connexion |
| Relais → Contrôleur | 11 | ACK_ON | Confirmation activation |
| Relais → Contrôleur | 12 | ACK_OFF | Confirmation désactivation |
| Relais → Contrôleur | 13 | ACK_PONG | Réponse ping |
| Relais → Contrôleur | 14 | ACK_LOCKED | Redémarrage bloqué (délai sécurité) |
| Relais → Contrôleur | 15 | ACK_UNLOCKED | Redémarrage autorisé (délai expiré) |

## Sécurités

### 1. Timeout watchdog (perte de connexion)

| Paramètre | Valeur |
|-----------|--------|
| Durée | 180 000 ms (3 minutes) |
| Condition | Chauffage actif ET aucune commande reçue |
| Action | Arrêt automatique du relais |

### 2. Anti-redémarrage rapide

| Paramètre | Valeur |
|-----------|--------|
| Durée | 180 000 ms (3 minutes) |
| Déclencheur | Passage du relais à OFF |
| Action | Blocage de CMD_HEAT_ON, réponse ACK_LOCKED |

Cette protection évite les cycles ON/OFF rapides qui pourraient endommager le chauffage Webasto.

### Diagramme de sécurité watchdog

```
Chauffage ON
    │
    ▼
┌─────────────────┐
│ Attente commande│◄──────┐
│ ou ping         │       │
└────────┬────────┘       │
         │                │
    Commande reçue?       │
         │                │
    ┌────┴────┐           │
    │OUI      │NON        │
    ▼         ▼           │
  Reset    Timer > 3min?  │
  timer         │         │
    │      ┌────┴────┐    │
    │      │NON      │OUI │
    │      ▼         ▼    │
    └──────┘    ARRÊT     │
                SECURITE  │
                    │     │
                    ▼     │
              Relais OFF  │
              Log erreur  │
```

### Diagramme anti-redémarrage

```
Relais passe à OFF
    │
    ▼
Verrou activé (3 min)
    │
    ▼
┌─────────────────────┐
│ Loop: vérification  │◄─────┐
│ périodique          │      │
└─────────┬───────────┘      │
          │                  │
     Délai écoulé ?          │
          │                  │
     ┌────┴────┐             │
     │NON      │OUI          │
     ▼         ▼             │
   Attente   ACK_UNLOCKED    │
     │       envoyé          │
     │       Verrou OFF      │
     └───────────────────────┘


CMD_HEAT_ON reçu
    │
    ▼
┌─────────────────┐
│ Verrou actif ?  │
└────────┬────────┘
         │
    ┌────┴────┐
    │NON      │OUI
    ▼         ▼
 Relais ON  ACK_LOCKED
 ACK_ON     (bloqué)
```

## Séquence de démarrage

1. Initialisation Serial (115200 bauds)
2. Attente USB CDC (max 3 secondes)
3. Configuration CPU 80 MHz
4. Initialisation NVS
5. Configuration GPIO (relais OFF + LED éteinte)
6. Initialisation WiFi (mode STA)
7. Affichage adresse MAC
8. Initialisation ESP-NOW
9. Enregistrement callbacks
10. Attente commandes

## Consommation électrique

| État | Consommation estimée |
|------|---------------------|
| Veille (attente) | ~25 mA |
| Réception ESP-NOW | ~100 mA |
| Relais actif | +70-80 mA (shield relais) |

## Compatibilité

### Framework

| Composant | Version |
|-----------|---------|
| Platform | espressif32 |
| Framework | Arduino |
| Board | seeed_xiao_esp32s3 |

### Flags de compilation

```ini
build_flags = 
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

## Limitations connues

1. **Pas de chiffrement ESP-NOW** : Les commandes sont envoyées en clair
2. **Single peer** : Un seul contrôleur peut être enregistré (le premier à envoyer une commande)
3. **Pas de persistance** : L'adresse du contrôleur n'est pas sauvegardée après redémarrage
4. **Délai anti-redémarrage** : 3 minutes obligatoires après arrêt (le contrôleur doit gérer ACK_LOCKED)

## Historique des versions

| Version | Date | Modifications |
|---------|------|---------------|
| 1.0.0 | Janvier 2026 | Version initiale production (ESP32-C3 Super Mini) |
| 1.1.0 | Janvier 2026 | Ajout sécurité anti-redémarrage rapide (3 min) |
| 2.0.0 | Mars 2026 | Migration vers XIAO ESP32S3 + 1-Channel Relay Shield |
