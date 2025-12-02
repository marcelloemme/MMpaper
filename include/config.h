#ifndef CONFIG_H
#define CONFIG_H

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "0.4.0"
#define GITHUB_USER "marcelloemme"
#define GITHUB_REPO "MMpaper"

// ===== AUTO-UPDATE SETTINGS =====
// Firmware check: SOLO al boot (non più schedulato)
#define MIN_BATTERY_PERCENT 30  // Non aggiornare se batteria < 30%
#define WIFI_CONNECT_TIMEOUT 10000  // 10s timeout connessione WiFi

// ===== IMAGE UPDATE SETTINGS =====
// Check immagine: 6:00, 9:00, 12:00, 15:00, 18:00, 21:00, 00:00
#define IMAGE_CHECK_HOURS {6, 9, 12, 15, 18, 21, 0}  // Orari check immagine
#define IMAGE_CHECK_START_HOUR 6    // Inizio check giornalieri
#define IMAGE_CHECK_END_HOUR 0      // Fine check (0 = mezzanotte)

// ===== WIFI CREDENTIALS =====
// NOTA: in produzione, salvare in SPIFFS/Preferences, non hardcoded!
#define WIFI_SSID "YourWiFiSSID"      // CAMBIARE
#define WIFI_PASSWORD "YourPassword"  // CAMBIARE

// ===== NTP TIME SYNC =====
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600        // GMT+1 (Italia, inverno)
#define DAYLIGHT_OFFSET_SEC 3600   // +1 ora per ora legale (estate)

// ===== DISPLAY REFRESH SETTINGS =====
#define FULL_REFRESH_MIN_INTERVAL 10000  // 10s tra full refresh
#define PARTIAL_REFRESH_MAX_COUNT 5      // Full refresh ogni 5 partial

// ===== POWER MANAGEMENT =====
#define ENABLE_IMU false  // Disabilita giroscopio di default (risparmio batteria)

#endif // CONFIG_H
