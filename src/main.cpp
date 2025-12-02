#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>
#include "config.h"
#include "lgfx/utility/lgfx_tjpgd.h"  // For JPEG dimension parsing

// ===== GLOBAL OBJECTS =====
Preferences prefs;
bool isFirstBoot = true;  // Flag per controllo update al primo avvio

// ===== IMAGE MANAGEMENT =====
uint8_t* imageBuffer = nullptr;  // Buffer per immagine JPEG
size_t imageBufferSize = 0;

// ===== DISPLAY REFRESH MANAGEMENT =====
int partialRefreshCount = 0;
unsigned long lastFullRefresh = 0;
bool displayDirty = false;

// ===== WIFI CONNECTION =====

/**
 * Connette al WiFi provando tutte le reti disponibili
 * - Prova tutte le reti in sequenza
 * - Ripete fino a WIFI_MAX_ATTEMPTS volte
 * - Pausa di WIFI_RETRY_DELAY tra un loop e l'altro
 * Returns: true se connesso, false se tutti i tentativi falliscono
 */
bool connectToWiFi() {
  Serial.println("=== CONNECTING TO WIFI ===");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Loop per numero massimo di tentativi
  for (int attempt = 1; attempt <= WIFI_MAX_ATTEMPTS; attempt++) {
    Serial.printf("Attempt %d/%d\n", attempt, WIFI_MAX_ATTEMPTS);

    // Prova tutte le reti disponibili
    for (int i = 0; i < WIFI_NETWORKS_COUNT; i++) {
      const char* ssid = WIFI_NETWORKS[i].ssid;
      const char* password = WIFI_NETWORKS[i].password;

      Serial.printf("Trying network %d/%d: %s\n", i + 1, WIFI_NETWORKS_COUNT, ssid);

      WiFi.begin(ssid, password);

      // Aspetta connessione (timeout per singola rete)
      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED &&
             millis() - startAttempt < WIFI_CONNECT_TIMEOUT_PER_NET) {
        delay(100);
        Serial.print(".");
      }
      Serial.println();

      // Connessione riuscita?
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("✅ Connected to: %s\n", ssid);
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
        return true;
      }

      Serial.printf("❌ Failed to connect to: %s\n", ssid);
      WiFi.disconnect();
      delay(500);  // Breve pausa tra una rete e l'altra
    }

    // Se non è l'ultimo tentativo, aspetta prima di riprovare
    if (attempt < WIFI_MAX_ATTEMPTS) {
      Serial.printf("Waiting %d seconds before retry...\n", WIFI_RETRY_DELAY / 1000);
      delay(WIFI_RETRY_DELAY);
    }
  }

  // Tutti i tentativi falliti
  Serial.println("❌ Failed to connect to any WiFi network");
  WiFi.mode(WIFI_OFF);
  return false;
}

// ===== AUTO-UPDATE FUNCTIONS =====

/**
 * Controlla se è il momento di verificare aggiornamenti firmware
 * Trigger: SOLO al primo avvio (non più schedulato)
 */
bool shouldCheckFirmwareUpdate() {
  if (isFirstBoot) {
    Serial.println("First boot - checking for firmware update");
    return true;
  }
  return false;
}

/**
 * Controlla se è il momento di verificare aggiornamenti immagine
 * Trigger: 6:00, 9:00, 12:00, 15:00, 18:00, 21:00, 00:00
 */
bool shouldCheckImageUpdate() {
  // Al primo avvio: sempre
  if (isFirstBoot) {
    Serial.println("First boot - checking for image");
    return true;
  }

  // Ottieni ora corrente
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time, skipping image check");
    return false;
  }

  int currentHour = timeinfo.tm_hour;
  int currentMin = timeinfo.tm_min;

  // Orari di check
  const int checkHours[] = IMAGE_CHECK_HOURS;
  const int numCheckHours = sizeof(checkHours) / sizeof(checkHours[0]);

  // Verifica se siamo in un orario di check (finestra di 5 minuti)
  for (int i = 0; i < numCheckHours; i++) {
    if (currentHour == checkHours[i] && currentMin < 5) {
      // Verifica se non abbiamo già fatto check in questa ora
      prefs.begin("mmconfig", false);
      int lastCheckHour = prefs.getInt("lastImageCheckHour", -1);
      int lastCheckDay = prefs.getInt("lastImageCheckDay", -1);
      int currentDay = timeinfo.tm_mday;
      prefs.end();

      // Check necessario se: nuovo giorno O nuova ora
      if (currentDay != lastCheckDay || currentHour != lastCheckHour) {
        Serial.printf("Image check time reached: %02d:00\n", currentHour);

        // Salva orario check
        prefs.begin("mmconfig", false);
        prefs.putInt("lastImageCheckHour", currentHour);
        prefs.putInt("lastImageCheckDay", currentDay);
        prefs.end();

        return true;
      }
    }
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
  // Non spegniamo display qui perché potrebbero esserci più messaggi in sequenza
}

// ===== IMAGE FUNCTIONS =====

/**
 * Scarica metadata immagine da GitHub
 * Returns: MD5 hash dell'immagine remota, o stringa vuota se errore
 */
String downloadImageMetadata() {
  HTTPClient http;
  String metadataURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                       "/" + String(GITHUB_REPO) + "/main/image/image_meta.json";

  Serial.printf("Downloading metadata: %s\n", metadataURL.c_str());
  http.begin(metadataURL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Metadata download failed: %d\n", httpCode);
    http.end();
    return "";
  }

  String payload = http.getString();
  http.end();

  // Parse JSON per ottenere MD5
  int md5Start = payload.indexOf("\"md5\":") + 7;
  int md5End = payload.indexOf("\"", md5Start);
  String md5 = payload.substring(md5Start, md5End);

  Serial.printf("Remote image MD5: %s\n", md5.c_str());
  return md5;
}

/**
 * Scarica immagine da GitHub e la salva nel buffer
 * Returns: true se successo, false se fallito
 */
bool downloadImage() {
  HTTPClient http;
  String imageURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                    "/" + String(GITHUB_REPO) + "/main/image/current.jpg";

  Serial.printf("Downloading image: %s\n", imageURL.c_str());
  http.begin(imageURL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Image download failed: %d\n", httpCode);
    http.end();
    return false;
  }

  int imageSize = http.getSize();
  Serial.printf("Image size: %d bytes\n", imageSize);

  // Alloca buffer per immagine
  if (imageBuffer != nullptr) {
    free(imageBuffer);
  }
  imageBuffer = (uint8_t*)malloc(imageSize);

  if (imageBuffer == nullptr) {
    Serial.println("Failed to allocate image buffer!");
    http.end();
    return false;
  }

  // Scarica immagine
  WiFiClient* stream = http.getStreamPtr();
  int bytesRead = 0;

  while (http.connected() && bytesRead < imageSize) {
    size_t available = stream->available();
    if (available) {
      int chunk = stream->readBytes(imageBuffer + bytesRead, min(available, (size_t)(imageSize - bytesRead)));
      bytesRead += chunk;

      if (bytesRead % 51200 == 0) {  // Progress ogni 50KB
        Serial.printf("Downloaded: %d / %d KB (%d%%)\n",
                      bytesRead / 1024, imageSize / 1024,
                      (bytesRead * 100) / imageSize);
      }
    }
    delay(1);
  }

  http.end();
  imageBufferSize = bytesRead;

  Serial.printf("Image download complete: %d bytes\n", bytesRead);
  return (bytesRead == imageSize);
}

/**
 * Mostra immagine JPEG a schermo intero
 * Smart crop: mantiene aspect ratio, riempie schermo, croppa dal centro
 */
void displayImageFullscreen() {
  if (imageBuffer == nullptr || imageBufferSize == 0) {
    Serial.println("No image to display!");
    return;
  }

  Serial.println("Displaying image fullscreen...");

  M5.Display.wakeup();  // Sveglia display se in sleep
  M5.Display.setColorDepth(8);  // 8-bit grayscale
  M5.Display.fillScreen(TFT_BLACK);

  // Get JPEG dimensions using TJpgDec parser
  lgfxJdec jdec;
  uint8_t workbuf[3100];  // Working buffer for TJpgDec

  // JPEG data source for parser
  struct JpgDataSource {
    const uint8_t* buffer;
    size_t size;
    size_t pos;
  };

  // Data read callback
  auto jpgRead = [](void* data, uint8_t* buf, uint32_t len) -> uint32_t {
    JpgDataSource* src = (JpgDataSource*)data;
    if (src->pos >= src->size) return 0;
    uint32_t remain = src->size - src->pos;
    if (len > remain) len = remain;
    memcpy(buf, src->buffer + src->pos, len);
    src->pos += len;
    return len;
  };

  JpgDataSource jpgData = { imageBuffer, imageBufferSize, 0 };

  // Parse JPEG header to get dimensions
  JRESULT res = lgfx_jd_prepare(&jdec, jpgRead, &jpgData, 3100, workbuf);

  if (res != JDR_OK) {
    Serial.printf("Failed to parse JPEG: %d\n", res);
    M5.Display.drawString("Invalid JPEG", 480, 270);
    M5.Display.display();
    M5.Display.sleep();
    return;
  }

  int jpgWidth = jdec.width;
  int jpgHeight = jdec.height;

  Serial.printf("Image dimensions: %dx%d\n", jpgWidth, jpgHeight);

  // Calcola aspect ratio
  float imgRatio = (float)jpgWidth / (float)jpgHeight;
  float screenRatio = 960.0f / 540.0f;  // 16:9 = 1.778

  int drawX = 0, drawY = 0;
  int drawWidth = 960, drawHeight = 540;

  // Smart crop: scala per riempire, poi offset per centrare
  if (imgRatio > screenRatio) {
    // Immagine più larga: scala in base all'altezza, croppa i lati
    drawHeight = 540;
    drawWidth = (int)((float)jpgWidth * 540.0f / (float)jpgHeight);
    drawX = -(drawWidth - 960) / 2;  // Centra orizzontalmente
    Serial.printf("Wide image: crop sides (draw at x=%d, width=%d)\n", drawX, drawWidth);
  } else {
    // Immagine più alta: scala in base alla larghezza, croppa top/bottom
    drawWidth = 960;
    drawHeight = (int)((float)jpgHeight * 960.0f / (float)jpgWidth);
    drawY = -(drawHeight - 540) / 2;  // Centra verticalmente
    Serial.printf("Tall image: crop top/bottom (draw at y=%d, height=%d)\n", drawY, drawHeight);
  }

  // Disegna con dimensioni calcolate (overflow viene clippato automaticamente)
  M5.Display.drawJpg(imageBuffer, imageBufferSize, drawX, drawY, drawWidth, drawHeight);

  M5.Display.display();  // Full refresh
  M5.Display.sleep();    // Spegni display

  Serial.println("Image displayed with smart crop!");
}

/**
 * Check e update immagine da GitHub
 */
void checkAndUpdateImage() {
  Serial.println("=== IMAGE UPDATE CHECK ===");

  // 1. Check batteria
  if (!isBatteryOkForUpdate()) {
    Serial.printf("Battery too low (%d%%), skipping image check\n", M5.Power.getBatteryLevel());
    return;
  }

  // 2. Connetti WiFi (prova tutte le reti disponibili)
  if (!connectToWiFi()) {
    Serial.println("Failed to connect to WiFi, skipping image check");
    return;
  }

  // 3. Scarica metadata
  String remoteMD5 = downloadImageMetadata();

  if (remoteMD5.length() == 0) {
    Serial.println("Failed to get image metadata");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 4. Confronta con MD5 salvato
  prefs.begin("mmconfig", false);
  String localMD5 = prefs.getString("imageMD5", "");
  prefs.end();

  Serial.printf("Local MD5: %s\n", localMD5.c_str());
  Serial.printf("Remote MD5: %s\n", remoteMD5.c_str());

  if (remoteMD5 == localMD5 && localMD5.length() > 0) {
    Serial.println("Image already up to date!");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 5. Nuova immagine! Scarica
  Serial.println("New image found! Downloading...");

  bool success = downloadImage();

  if (!success) {
    Serial.println("Image download failed!");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 6. Spegni WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // 7. Mostra nuova immagine
  displayImageFullscreen();

  // 8. Salva MD5
  prefs.begin("mmconfig", false);
  prefs.putString("imageMD5", remoteMD5);
  prefs.end();

  Serial.println("Image updated successfully!");
}

// ===== FIRMWARE UPDATE FUNCTIONS =====

/**
 * Download firmware da URL e installa via OTA sulla flash
 * Returns: true se successo, false se fallito
 */
bool downloadAndUpdateOTA(const char* url) {
  HTTPClient http;
  http.begin(url);

  Serial.printf("Downloading firmware: %s\n", url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }

  // Ottieni dimensione totale
  int totalLength = http.getSize();
  int currentLength = 0;

  // Inizia OTA update
  if (!Update.begin(totalLength)) {
    Serial.printf("OTA begin failed! Error: %s\n", Update.errorString());
    http.end();
    return false;
  }

  Serial.println("OTA update started...");

  // Buffer per download
  uint8_t buff[512] = { 0 };
  WiFiClient* stream = http.getStreamPtr();

  // Download e scrittura diretta su flash OTA partition
  while (http.connected() && (currentLength < totalLength || totalLength == -1)) {
    size_t availableSize = stream->available();

    if (availableSize) {
      int bytesRead = stream->readBytes(buff, min(availableSize, sizeof(buff)));

      // Scrivi su OTA partition
      if (Update.write(buff, bytesRead) != bytesRead) {
        Serial.println("OTA write failed!");
        Update.abort();
        http.end();
        return false;
      }

      currentLength += bytesRead;

      // Progress ogni 100KB
      if (currentLength % 102400 == 0) {
        Serial.printf("OTA Progress: %d KB / %d KB (%d%%)\n",
                      currentLength / 1024,
                      totalLength / 1024,
                      (currentLength * 100) / totalLength);
      }
    }
    delay(1);
  }

  http.end();

  // Finalizza OTA update
  if (Update.end(true)) {
    Serial.printf("OTA update complete! %d bytes written\n", currentLength);

    // Verifica MD5 (se disponibile)
    if (Update.isFinished()) {
      Serial.println("Update successfully completed. Rebooting...");
      return true;
    } else {
      Serial.println("Update not finished! Something went wrong!");
      return false;
    }
  } else {
    Serial.printf("OTA error: %s\n", Update.errorString());
    return false;
  }
}

/**
 * Check GitHub e aggiorna firmware via OTA
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

  // 3. Connetti WiFi (prova tutte le reti disponibili)
  if (!connectToWiFi()) {
    Serial.println("Failed to connect to WiFi, skipping firmware update");
    displayMessage("No WiFi - Continuing");
    delay(1000);
    return;
  }

  // 4. Sincronizza ora NTP (solo al primo avvio)
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

  Serial.println("New version found! Downloading via OTA...");
  displayMessage("Update found!", 200);
  displayMessage("Downloading...", 300);

  // 8. Download e installa nuovo firmware via OTA
  String binURL = "https://raw.githubusercontent.com/" + String(GITHUB_USER) +
                  "/" + String(GITHUB_REPO) + "/main/MMpaper.bin";

  bool updateSuccess = downloadAndUpdateOTA(binURL.c_str());

  if (!updateSuccess) {
    Serial.println("OTA update failed!");
    displayMessage("Update failed!", 200);
    displayMessage("Continuing with current version...", 300);
    delay(2000);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  // 9. Successo! Riavvia con nuova versione
  displayMessage("Update successful!", 200);
  displayMessage("Restarting...", 300);
  delay(3000);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("Rebooting with new firmware...");
  ESP.restart();  // Boot con nuova versione dalla flash!
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
    M5.Display.sleep();     // Spegni display dopo refresh
    lastFullRefresh = now;
    partialRefreshCount = 0;
  } else {
    // Partial refresh
    Serial.println("Partial refresh");
    // M5.Display.displayPartial();  // Da implementare con EPDIY
    M5.Display.sleep();     // Spegni display dopo refresh
    partialRefreshCount++;
  }

  displayDirty = false;
}

// ===== DEEP SLEEP =====

/**
 * Calcola secondi fino al prossimo check immagine
 */
uint64_t getSecondsUntilNextImageCheck() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Se non riusciamo a ottenere l'ora, aspetta 1 ora
    return 3600;
  }

  int currentHour = timeinfo.tm_hour;
  int currentMin = timeinfo.tm_min;

  // Orari di check
  const int checkHours[] = IMAGE_CHECK_HOURS;
  const int numCheckHours = sizeof(checkHours) / sizeof(checkHours[0]);

  // Trova prossimo orario di check
  int nextCheckHour = -1;
  for (int i = 0; i < numCheckHours; i++) {
    if (checkHours[i] > currentHour ||
        (checkHours[i] == currentHour && currentMin < 5)) {
      nextCheckHour = checkHours[i];
      break;
    }
  }

  // Se non troviamo check oggi, prendi il primo di domani
  if (nextCheckHour == -1) {
    nextCheckHour = checkHours[0];
    // Calcola secondi fino a domani alle nextCheckHour
    int hoursUntilMidnight = 24 - currentHour;
    int minutesUntilMidnight = 60 - currentMin;
    uint64_t secondsUntilMidnight = (hoursUntilMidnight * 3600) + (minutesUntilMidnight * 60);
    uint64_t secondsAfterMidnight = nextCheckHour * 3600;
    return secondsUntilMidnight + secondsAfterMidnight;
  }

  // Calcola secondi fino al prossimo check oggi
  int hoursUntilCheck = nextCheckHour - currentHour;
  int minutesUntilCheck = 0 - currentMin;  // Vogliamo arrivare a :00
  uint64_t secondsUntilCheck = (hoursUntilCheck * 3600) + (minutesUntilCheck * 60);

  // Minimo 5 minuti di sleep
  if (secondsUntilCheck < 300) {
    secondsUntilCheck = 300;
  }

  Serial.printf("Next check in %llu seconds (~%llu minutes)\n",
                secondsUntilCheck, secondsUntilCheck / 60);

  return secondsUntilCheck;
}

/**
 * Entra in deep sleep fino al prossimo check
 */
void enterDeepSleep() {
  Serial.println("=== ENTERING DEEP SLEEP ===");

  // Calcola tempo fino al prossimo check
  uint64_t sleepSeconds = getSecondsUntilNextImageCheck();

  Serial.printf("Sleeping for %llu seconds...\n", sleepSeconds);

  // Prepara deep sleep
  M5.Display.sleep();
  WiFi.mode(WIFI_OFF);

  // Configura wakeup timer
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);

  // Vai in deep sleep
  esp_deep_sleep_start();
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
  cfg.internal_imu = ENABLE_IMU;  // Disabilita IMU
  M5.begin(cfg);

  Serial.println("M5Unified initialized");

  // 1. FIRMWARE UPDATE CHECK (solo al boot)
  if (shouldCheckFirmwareUpdate()) {
    Serial.println("Checking for firmware update...");
    checkGitHubAndUpdate();
    // Se arriviamo qui, non c'era update (altrimenti restart)
    isFirstBoot = false;
  }

  // 2. IMAGE UPDATE CHECK (boot + schedulato)
  if (shouldCheckImageUpdate()) {
    Serial.println("Checking for image update...");
    checkAndUpdateImage();
    isFirstBoot = false;
  }

  // 3. Se non abbiamo immagine, prova a scaricare l'immagine corrente
  if (imageBuffer == nullptr) {
    Serial.println("No new image downloaded, attempting to download current image");

    if (connectToWiFi()) {
      downloadImage();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);

      if (imageBuffer != nullptr) {
        displayImageFullscreen();
      }
    } else {
      Serial.println("No WiFi available, skipping image display");
    }
  }

  // 4. Entra in deep sleep fino al prossimo check
  Serial.println("Setup complete, entering deep sleep...");
  delay(100);  // Dai tempo al serial di inviare
  enterDeepSleep();
}

void loop() {
  // Non dovremmo mai arrivare qui (deep sleep in setup)
  // Ma se arriviamo qui, aspetta e riprova
  delay(1000);
  enterDeepSleep();
}
