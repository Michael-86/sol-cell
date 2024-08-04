#include <WiFi.h>
#include <WebServer.h>
#include "esp_sleep.h"
#include "time.h"

// Wi-Fi credentials
const char* ssid = "gtfast";
const char* password = "darktitan01";

// Web server on port 80
WebServer server(90);

const int batteryPin = 34; // ADC pin
const float voltageDividerRatio = 2.0; // Because of the two 10k resistors
const int sleepHour = 21; // 9 PM
const int wakeHour = 7; // 11 AM
const int wakeMinute = 00; // 20 minutes past the hour

unsigned long previousMillis = 0;
const long interval = 60000; // 1 minute
unsigned long lastClientCheck = 0;
const long clientCheckInterval = 300000; // 5 minutes

bool justWokeUp = true;

// RTC memory to retain time
RTC_DATA_ATTR struct timeval rtcTime;

void setup() {
  Serial.begin(115200);

  // Print wake-up reason
  printWakeupReason();
  
  // Shutdown BT
  btStop();
  
  // Connect to Wi-Fi and get the current time
  connectToWiFi();

  // Initialize RTC with Stockholm time zone
  configTime(3600, 3600, "pool.ntp.org"); // CET/CEST
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  // Start the web server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Check current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;


  // Sleep management
  if (currentHour >= sleepHour || (currentHour < wakeHour || (currentHour == wakeHour && currentMinute < wakeMinute))) {
    if (!justWokeUp) {
      Serial.println("Going to sleep...");
      WiFi.disconnect(true); // Disconnect Wi-Fi
      WiFi.mode(WIFI_OFF); // Turn off Wi-Fi

      // Calculate the time to sleep until wakeHour:wakeMinute
      int sleepDuration = ((wakeHour - currentHour + 24) % 24) * 60 * 60 * 1000000; // Convert to microseconds
      sleepDuration += (wakeMinute - currentMinute) * 60 * 1000000; // Add minutes to microseconds

      // Save current time to RTC memory
      gettimeofday(&rtcTime, NULL);

      delay(1000); // Add a delay before going to sleep
      esp_sleep_enable_timer_wakeup(sleepDuration);
      delay(100); // Small delay to ensure everything is properly shut down
      esp_deep_sleep_start();
    } else {
      Serial.println("Just woke up, not going to sleep immediately.");
      justWokeUp = false;
    }
  } else {
    justWokeUp = false; // Ensure this flag is reset after the first loop
    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi();
    }

    server.handleClient();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      Serial.println("Awake and running...");
      int adcValue = analogRead(batteryPin);
      float batteryVoltage = (adcValue / 4095.0) * 3.3 * voltageDividerRatio;
      Serial.print("Battery Voltage: ");
      Serial.println(batteryVoltage);
    }

    // Check for client connections
    if (WiFi.softAPgetStationNum() == 0) {
      if (currentMillis - lastClientCheck >= clientCheckInterval) {
        lastClientCheck = currentMillis;
        Serial.println("No clients connected, enabling modem sleep...");
        WiFi.setSleep(true);
      }
    } else {
      lastClientCheck = currentMillis;
      WiFi.setSleep(false);
      Serial.println("Client connected, disabling modem sleep...");
    }
  }
}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Sync time after connecting to Wi-Fi
    configTime(3600, 3600, "pool.ntp.org"); // CET/CEST
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
    } else {
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi.");
  }
}

void handleRoot() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    server.send(200, "text/plain", "Failed to obtain time");
    return;
  }
  int adcValue = analogRead(batteryPin);
  float batteryVoltage = (adcValue / 4095.0) * 3.3 * voltageDividerRatio;

  char timeString[64];
  strftime(timeString, sizeof(timeString), "%A, %B %d %Y %H:%M:%S", &timeinfo);

  char html[256];
  snprintf(html, sizeof(html), "<html><body><h1>ESP32 Battery Monitor</h1><p>Battery Voltage: %.2fV</p><p>Current Time: %s</p></body></html>", batteryVoltage, timeString);

  server.send(200, "text/html", html);
}

void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}
