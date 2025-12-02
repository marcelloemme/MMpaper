#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <time.h>
#include "config.h"

// ===== GLOBAL OBJECTS =====
Preferences prefs;
bool isFirstBoot = true;  // Flag per controllo update al primo avvio

// ===== DISPLAY REFRESH MANAGEMENT =====
int partialRefreshCount = 0;
unsigned long lastFullRefresh = 0;
bool displayDirty = false;

// ===== AUTO-UPDATE FUNCTIONS =====

/**
 * Controlla se è il momento di verificare aggiornamenti
 * Trigger:
 * 1. Al primo avvio (sempre)
 * 2. Ogni giorno alle 2:00 AM (se non già fatto oggi)
 */
bool shouldCheckUpdate() {
  // 1. Primo avvio: controlla sempre
  if (isFirstBoot) {
    Serial.println("First boot - checking for updates");
    return true;
  }

  // 2. Controlla se è l'orario schedulato (2:00 AM)
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time, skipping scheduled update check");
    return false;
  }

  // Leggi ultimo giorno di controllo
  prefs.begin("mmconfig", false);
  int lastCheckDay = prefs.getInt("lastUpdateDay", -1);
  int currentDay = timeinfo.tm_mday;  // Giorno del mese (1-31)
  prefs.end();

  // Se è un nuovo giorno E siamo nell'orario giusto
  bool isNewDay = (currentDay != lastCheckDay);
  bool isScheduledTime = (timeinfo.tm_hour == UPDATE_CHECK_HOUR &&
                          timeinfo.tm_min >= UPDATE_CHECK_MINUTE &&
                          timeinfo.tm_min < UPDATE_CHECK_MINUTE + 5);  // Finestra di 5 minuti

  if (isNewDay && isScheduledTime) {
    Serial.printf("Scheduled update time reached (%02d:%02d)\n",
                  timeinfo.tm_hour, timeinfo.tm_min);

    // Salva giorno corrente
    prefs.begin("mmconfig", false);
    prefs.putInt("lastUpdateDay", currentDay);
    prefs.end();

    return true;
  }

  return false;
}

/**
 * Verifica batteria sufficiente per update
 */
bool isBatteryOkForUpdate() {
  int batteryLevel = M5.Power.getBatteryLevel();
  return (batteryLevel >= MIN_BATTERY_PERCENT);
}

/**
 * Sincronizza ora via NTP (richiede WiFi connesso)
 */
void syncTimeFromNTP() {
  Serial.println("Syncing time from NTP...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  // Aspetta max 10s per sync
  int timeout = 10;
  while (timeout > 0) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return;
    }
    delay(1000);
    timeout--;
  }
  Serial.println("NTP sync timeout");
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
 * Download file da URL e salva su SD
 * Returns: true se successo, false se fallito
 */
bool downloadFileToSD(const char* url, const char* filename) {
  HTTPClient http;
  http.begin(url);

  Serial.printf("Downloading: %s\n", url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }

  // Apri file su SD per scrittura
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file on SD for writing");
    http.end();
    return false;
  }

  // Ottieni dimensione totale
  int totalLength = http.getSize();
  int currentLength = 0;

  // Buffer per download
  uint8_t buff[512] = { 0 };
  WiFiClient* stream = http.getStreamPtr();

  // Download con progress
  while (http.connected() && (currentLength < totalLength || totalLength == -1)) {
    size_t availableSize = stream->available();

    if (availableSize) {
      int bytesRead = stream->readBytes(buff, min(availableSize, sizeof(buff)));
      file.write(buff, bytesRead);
      currentLength += bytesRead;

      // Progress ogni 100KB
      if (currentLength % 102400 == 0) {
        Serial.printf("Downloaded: %d KB / %d KB\n", currentLength / 1024, totalLength / 1024);
      }
    }
    delay(1);
  }

  file.close();
  http.end();

  Serial.printf("Download complete! %d bytes written\n", currentLength);
  return true;
}

/**
 * Check GitHub e aggiorna .bin su microSD se disponibile
 */
void checkGitHubAndUpdate() {
  Serial.println("=== AUTO-UPDATE CHECK ===");

  // 1. Check batteria
  if (!isBatteryOkForUpdate()) {
    Serial.printf("Battery too low (%d%%), skipping update\n", M5.Power.getBatteryLevel());
    return;
  }

  // 2. Inizializza SD card
  if (!SD.begin()) {
    Serial.println("SD card not found, skipping update");
    return;
  }
  Serial.println("SD card initialized");

  // 3. Mostra messaggio su display
  displayMessage("Checking for updates...");

  // 4. Connetti WiFi
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
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  Serial.println("WiFi connected!");

  // 4b. Sincronizza ora NTP (solo al primo avvio)
  if (isFirstBoot) {
    syncTimeFromNTP();
  }

  // 5. Download firmware.json da GitHub
  HTTPClient http;
  String manifestURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                       "/" + String(GITHUB_REPO) + "/main/firmware.json";

  Serial.printf("Checking version at: %s\n", manifestURL.c_str());
  http.begin(manifestURL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Failed to get manifest: %d\n", httpCode);
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 6. Parse JSON per ottenere versione remota
  String payload = http.getString();
  http.end();

  // Parsing semplice del JSON (cerca "version")
  int versionStart = payload.indexOf("\"version\":") + 11;
  int versionEnd = payload.indexOf("\"", versionStart);
  String remoteVersion = payload.substring(versionStart, versionEnd);

  Serial.printf("Current version: %s\n", FIRMWARE_VERSION);
  Serial.printf("Remote version: %s\n", remoteVersion.c_str());

  // 7. Confronta versioni (confronto semplice stringhe)
  if (remoteVersion == String(FIRMWARE_VERSION)) {
    Serial.println("Already up to date!");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  Serial.println("New version found! Downloading to SD...");
  displayMessage("Update found!", 200);
  displayMessage("Downloading...", 300);

  // 8. Download nuovo MMpaper.bin su SD
  String binURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                  "/" + String(GITHUB_REPO) + "/main/MMpaper.bin";

  // Scarica in file temporaneo prima
  const char* tempFile = "/MMpaper_new.bin";
  const char* finalFile = "/MMpaper.bin";

  bool downloadSuccess = downloadFileToSD(binURL.c_str(), tempFile);

  if (!downloadSuccess) {
    Serial.println("Download failed!");
    displayMessage("Download failed!", 200);
    displayMessage("Starting old version...", 300);
    delay(2000);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 9. Sostituisci vecchio file con nuovo
  displayMessage("Installing...", 270);

  // Rimuovi vecchio file
  if (SD.exists(finalFile)) {
    SD.remove(finalFile);
    Serial.println("Old firmware.bin removed");
  }

  // Rinomina nuovo file
  SD.rename(tempFile, finalFile);
  Serial.println("New firmware.bin installed on SD!");

  // 10. Successo! Riavvia per far caricare da Launcher
  displayMessage("Update successful!", 200);
  displayMessage("Restarting...", 300);
  displayMessage("Launcher will load new version", 400);
  delay(3000);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("Rebooting to Launcher...");
  ESP.restart();  // Launcher ricaricherà MMpaper.bin aggiornato dalla SD!
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
    Serial.println("Update check triggered, checking for updates...");
    checkGitHubAndUpdate();
    isFirstBoot = false;  // Reset flag dopo primo check
  } else {
    Serial.println("Skipping update check (not scheduled time)");
  }

  // Inizializza app principale
  initializeApp();
}

void loop() {
  M5.update();  // Update button state

  // Controlla periodicamente se è l'ora di aggiornare (ogni minuto)
  static unsigned long lastUpdateCheck = 0;
  if (millis() - lastUpdateCheck > 60000) {  // Ogni 60 secondi
    lastUpdateCheck = millis();
    if (shouldCheckUpdate()) {
      Serial.println("Scheduled update time reached");
      checkGitHubAndUpdate();
    }
  }

  // Run app logic
  runApp();

  // Smart refresh se necessario
  smartRefresh();
}
