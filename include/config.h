#ifndef CONFIG_H
#define CONFIG_H

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "0.1.0"
#define GITHUB_USER "marcelloemme"
#define GITHUB_REPO "MMpaper"

// ===== AUTO-UPDATE SETTINGS =====
#define UPDATE_CHECK_INTERVAL (24 * 3600 * 1000UL)  // 24 ore in ms
#define MIN_BATTERY_PERCENT 30  // Non aggiornare se batteria < 30%
#define WIFI_CONNECT_TIMEOUT 10000  // 10s timeout connessione WiFi

// ===== WIFI CREDENTIALS =====
// NOTA: in produzione, salvare in SPIFFS/Preferences, non hardcoded!
#define WIFI_SSID "YourWiFiSSID"      // CAMBIARE
#define WIFI_PASSWORD "YourPassword"  // CAMBIARE

// ===== DISPLAY REFRESH SETTINGS =====
#define FULL_REFRESH_MIN_INTERVAL 10000  // 10s tra full refresh
#define PARTIAL_REFRESH_MAX_COUNT 5      // Full refresh ogni 5 partial

// ===== POWER MANAGEMENT =====
#define ENABLE_IMU false  // Disabilita giroscopio di default (risparmio batteria)

#endif // CONFIG_H
