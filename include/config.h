#ifndef CONFIG_H
#define CONFIG_H

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "0.6.1"
#define GITHUB_USER "marcelloemme"
#define GITHUB_REPO "MMpaper"

// ===== AUTO-UPDATE SETTINGS =====
// Firmware check: SOLO al boot (non più schedulato)
#define MIN_BATTERY_PERCENT 30  // Non aggiornare se batteria < 30%

// ===== IMAGE UPDATE SETTINGS =====
// Check immagine: 6:00, 9:00, 12:00, 15:00, 18:00, 21:00, 00:00
#define IMAGE_CHECK_HOURS {6, 9, 12, 15, 18, 21, 0}  // Orari check immagine
#define IMAGE_CHECK_START_HOUR 6    // Inizio check giornalieri
#define IMAGE_CHECK_END_HOUR 0      // Fine check (0 = mezzanotte)

// ===== WIFI CREDENTIALS =====
// Configurazione multi-WiFi con fallback
// Il sistema prova tutte le reti in sequenza, fino a 3 tentativi totali
#define WIFI_NETWORKS_COUNT 3
#define WIFI_MAX_ATTEMPTS 3           // Tentativi totali (loop attraverso tutte le reti)
#define WIFI_CONNECT_TIMEOUT_PER_NET 5000  // 5s timeout per ogni rete
#define WIFI_RETRY_DELAY 5000         // 5s pausa tra un loop e l'altro

// Elenco reti WiFi (in ordine di priorità)
struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

const WiFiNetwork WIFI_NETWORKS[WIFI_NETWORKS_COUNT] = {
  {"FASTWEB-RNHDU3", "C9FLCJDDRY"},    // Rete 1: Casa
  {"OfficeWiFi_Guest", "officeGuest456"},         // Rete 2: Ufficio
  {"MobileHotspot_5G", "hotspot789xyz"}           // Rete 3: Hotspot mobile
};

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
