#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp32fota.h>
#include "config.h"

// ===== GLOBAL OBJECTS =====
Preferences prefs;
esp32FOTA FOTA("MMpaper", FIRMWARE_VERSION, false);

// ===== DISPLAY REFRESH MANAGEMENT =====
int partialRefreshCount = 0;
unsigned long lastFullRefresh = 0;
bool displayDirty = false;

// ===== AUTO-UPDATE FUNCTIONS =====

/**
 * Controlla se è il momento di verificare aggiornamenti
 * Basato su: timing (24h) + ultima verifica salvata
 */
bool shouldCheckUpdate() {
  prefs.begin("mmconfig", false);
  unsigned long lastCheck = prefs.getULong("lastUpdateCheck", 0);
  unsigned long now = millis();

  // Se sono passate più di 24h dall'ultimo check
  bool shouldCheck = (now - lastCheck > UPDATE_CHECK_INTERVAL);

  if (shouldCheck) {
    prefs.putULong("lastUpdateCheck", now);
  }

  prefs.end();
  return shouldCheck;
}

/**
 * Verifica batteria sufficiente per update
 */
bool isBatteryOkForUpdate() {
  int batteryLevel = M5.Power.getBatteryLevel();
  return (batteryLevel >= MIN_BATTERY_PERCENT);
}

/**
 * Mostra messaggio su display e-ink
 */
void displayMessage(const char* message, int y = 270) {
  M5.Display.setFont(&fonts::FreeSans18pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.drawString(message, 480, y);
  M5.Display.display();  // Full refresh
}

/**
 * Check GitHub e installa update se disponibile
 */
void checkGitHubAndUpdate() {
  Serial.println("=== AUTO-UPDATE CHECK ===");

  // 1. Check batteria
  if (!isBatteryOkForUpdate()) {
    Serial.printf("Battery too low (%d%%), skipping update\n", M5.Power.getBatteryLevel());
    return;
  }

  // 2. Mostra messaggio su display
  displayMessage("Checking for updates...");

  // 3. Connetti WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed, skipping update");
    displayMessage("No WiFi - Starting app");
    delay(1000);
    return;
  }

  Serial.println("WiFi connected!");

  // 4. Configura esp32FOTA - crea URL del manifest JSON
  String manifestURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                       "/" + String(GITHUB_REPO) + "/main/firmware.json";

  Serial.printf("Checking for updates at: %s\n", manifestURL.c_str());

  // 5. Check per update
  bool updateAvailable = FOTA.execHTTPcheck();

  if (updateAvailable) {
    Serial.println("New version found! Installing...");

    displayMessage("Update found!", 200);
    displayMessage("Installing...", 300);

    // 6. Download e installa
    FOTA.execOTA();

    // Se arriviamo qui, OTA è riuscito
    displayMessage("Update successful!", 200);
    displayMessage("Restarting...", 300);
    delay(2000);
    ESP.restart();  // Riavvia con nuova versione
  } else {
    Serial.println("No update available, version up to date");
  }

  // 7. Disconnect WiFi per risparmiare batteria
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected");
}

// ===== DISPLAY REFRESH FUNCTIONS =====

/**
 * Smart refresh: alterna partial/full refresh
 * Rispetta limite 10s tra full refresh
 */
void smartRefresh() {
  if (!displayDirty) return;

  unsigned long now = millis();
  bool canDoFullRefresh = (now - lastFullRefresh) >= FULL_REFRESH_MIN_INTERVAL;
  bool needsGhostingFix = (partialRefreshCount >= PARTIAL_REFRESH_MAX_COUNT);

  if (needsGhostingFix && canDoFullRefresh) {
    // Full refresh
    Serial.println("Full refresh (ghosting fix)");
    M5.Display.display();  // Full refresh (default)
    lastFullRefresh = now;
    partialRefreshCount = 0;
  } else {
    // Partial refresh
    Serial.println("Partial refresh");
    // M5.Display.displayPartial();  // Da implementare con EPDIY
    partialRefreshCount++;
  }

  displayDirty = false;
}

// ===== MAIN APPLICATION =====

void initializeApp() {
  Serial.println("=== INITIALIZING MMPAPER ===");

  // Display splash screen
  M5.Display.setColorDepth(8);  // 8-bit grayscale per antialiasing
  M5.Display.fillScreen(TFT_WHITE);

  // Titolo
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.drawString("MMpaper", 480, 100);

  // Versione
  M5.Display.setFont(&fonts::FreeSans18pt7b);
  M5.Display.setTextDatum(MC_DATUM);
  String versionText = "v" + String(FIRMWARE_VERSION);
  M5.Display.drawString(versionText, 480, 270);

  // Footer
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.setTextDatum(BC_DATUM);
  M5.Display.setTextColor(0x888888);
  M5.Display.drawString("Auto-updating e-ink app", 480, 500);

  M5.Display.display();  // Full refresh

  Serial.println("MMpaper initialized!");
}

void runApp() {
  // TODO: Implementare logica app principale
  // Per ora: mostra splash screen e aspetta
  delay(100);
}

// ===== ARDUINO SETUP & LOOP =====

void setup() {
  // Inizializza seriale per debug
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== MMPAPER STARTING ===");
  Serial.printf("Firmware version: %s\n", FIRMWARE_VERSION);

  // Inizializza M5Unified
  auto cfg = M5.config();
  cfg.internal_imu = ENABLE_IMU;  // Disabilita IMU per risparmio batteria
  M5.begin(cfg);

  Serial.println("M5Unified initialized");

  // AUTO-UPDATE: check prima di tutto!
  if (shouldCheckUpdate()) {
    Serial.println("Update check interval reached, checking for updates...");
    checkGitHubAndUpdate();
  } else {
    Serial.println("Skipping update check (too soon)");
  }

  // Inizializza app principale
  initializeApp();
}

void loop() {
  M5.update();  // Update button state

  // Run app logic
  runApp();

  // Smart refresh se necessario
  smartRefresh();
}
