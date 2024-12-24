#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "time.h"

const char* ssid = "gtfast";
const char* password = "darktitan01";
const char* googleScriptID = "AKfycbyLY4mG7jsEr7e_ThgBmbvAVQ-7K9qn3ssKUHLbZDGckmYwUiyFYesmirTBkNEMqjjdVg";

// Static IP configuration
IPAddress local_IP(192, 168, 10, 184);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // Optional
IPAddress secondaryDNS(8, 8, 4, 4); // Optional

WiFiClient client;
HTTPClient http;
WiFiUDP ntpUDP;
const char* ntpServers[] = {"time.nist.gov", "time.google.com", "pool.ntp.org"};
NTPClient timeClient(ntpUDP, ntpServers[0], 0, 60000); // No offset, update every minute

const int batteryPin = 34; // GPIO pin for battery measurement

const int sleepHour = 21; // Hour to go to deep sleep
const int wakeHour = 8;   // Hour to wake up from deep sleep

RTC_DATA_ATTR int bootCount = 0;

const int relayPin = 16; // GPIO pin connected to the relay
//const int relayPin = 2; // GPIO pin connected to the led

unsigned long previousMillis = 0;
unsigned long interval = 1000; // Interval for the main loop
unsigned long relayOnTime = 500; // Time to keep the relay on

void setTimezone(const char* timezone) {
  Serial.printf("Setting Timezone to %s\n", timezone);
  setenv("TZ", timezone, 1);
  tzset();
}

void initTime(const char* timezone) {
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org"); // First connect to NTP server, with 0 TZ offset
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println("Got the time from NTP");
  setTimezone(timezone); // Now set the real timezone
}

void setup() {
  Serial.begin(115200);
  //battery pin
  pinMode(batteryPin, INPUT);
  // Relay control
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Ensure the relay is off initially

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");
  
  timeClient.begin();
  Serial.println("NTP client started");
  
  // Try updating time with alternative NTP servers if the first one fails
  for (int i = 0; i < 3; i++) {
    timeClient.setPoolServerName(ntpServers[i]);
    Serial.print("Trying NTP server: ");
    Serial.println(ntpServers[i]);
    if (timeClient.update()) {
      Serial.print("Time updated using NTP server: ");
      Serial.println(ntpServers[i]);
      break;
    } else {
      Serial.print("Failed to update time using NTP server: ");
      Serial.println(ntpServers[i]);
    }
  }

  // Set timezone to your location (example: Central European Time with DST)
  initTime("CET-1CEST,M3.5.0/2,M10.5.0/3");

  // Check wake-up cause
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup reason: ");
  Serial.println(wakeup_reason);

  // Increment boot count if waking up from deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    bootCount++;
  } else {
    bootCount = 0; // Reset boot count on power cycle
  }

  Serial.print("Boot count: ");
  Serial.println(bootCount);
}



void loop() {
  timeClient.update();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  Serial.print("Current time: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.println(currentMinute);

  unsigned long currentMillis = millis();
  float batteryVoltage = 0.0; // Declare batteryVoltage here
  
  if (currentHour >= sleepHour || currentHour < wakeHour) {

    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Reconnecting to WiFi...");
      }
      Serial.println("Reconnected to WiFi");
    }

    Serial.println("Entering deep sleep...");
    goToDeepSleep(currentHour);
  } else {
      // Check if it's time to take a new measurement
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Turn on the relay to energize the voltage divider
        digitalWrite(relayPin, HIGH);
        Serial.println("Led on");
        // Wait for the relay to switch
        unsigned long relayMillis = millis();
        while (millis() - relayMillis < relayOnTime) {
          // Do nothing, just wait
        }

        int rawADC = analogRead(batteryPin);
        Serial.print("Raw ADC Value: ");
        Serial.println(rawADC);
        // Adjust the factor based on your resistor values (4.606kΩ and 10kΩ)
        batteryVoltage = rawADC * (3.3 / 4095.0) * ((10.0 + 4.6) / 4.6);
        Serial.print("Calculated Battery Voltage: ");
        Serial.println(batteryVoltage);

        // Turn off the relay to save power
        digitalWrite(relayPin, LOW);
        Serial.println("Led off");
      }

    
    // Debug print to check if the function is being called
    Serial.println("Connecting to wifi...");
    
    // Ensure WiFi is connected before sending data
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Reconnecting to WiFi...");
      }
      Serial.println("Reconnected to WiFi");
    }
    Serial.println("Calling sendToGoogleSheets function...");
    sendToGoogleSheets(batteryVoltage, bootCount);
    
    Serial.println("Going to light sleep...");
    goToLightSleep();
  }
}



void sendToGoogleSheets(float batteryVoltage, int bootCount) {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update(); // Ensure the time is updated
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentSecond = timeinfo.tm_sec;
    char timeStringBuff[50]; // 50 chars should be enough
    snprintf(timeStringBuff, sizeof(timeStringBuff), "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    Serial.print("Time:");
    Serial.println(asString);

    // Generate a unique ID based on the current time and boot count
    String uniqueID = String(millis()) + String(bootCount);
    
    String urlFinal = "https://script.google.com/macros/s/" + String(googleScriptID) + "/exec?" + "date=" + asString + "&batteryVoltage=" + String(batteryVoltage, 2) + "&bootCount=" + String(bootCount) + "&uniqueID=" + uniqueID + "&batteryVoltage=N/A";
    Serial.print("POST data to spreadsheet:");
    Serial.println(urlFinal);
    
    int retryCount = 0;
    const int maxRetries = 3;
    int httpCode = -1;
    
    while (retryCount < maxRetries && httpCode != 200) {
      HTTPClient http;
      http.begin(urlFinal.c_str());
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      httpCode = http.GET(); 
      Serial.print("HTTP Status Code: ");
      Serial.println(httpCode);
      
      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("Payload: " + payload);    
      } else {
        Serial.print("Failed to send data to Google Sheets. HTTP status code: ");
        Serial.println(httpCode);
        Serial.print("Error: ");
        Serial.println(http.errorToString(httpCode).c_str());
      }
      
      http.end();
      retryCount++;
      
      if (httpCode != 200) {
        Serial.println("Retrying...");
        delay(2000); // Wait before retrying
      }
    }
    
    if (httpCode != 200) {
      Serial.println("Failed to send data after 3 retries. Going to light sleep...");
    }
  } else {
    Serial.println("WiFi not connected");
  }
}



void goToDeepSleep(int currentHour) {
  Serial.println("Preparing for Deep sleep");
  uint64_t sleepDuration1; // Use uint64_t for large numbers
  
  if (currentHour >= sleepHour) {
    sleepDuration1 = ((24 - currentHour + wakeHour) % 24) * 60 * 60 * 1000000ULL; // Calculate sleep duration until 08:00 next day
  } else {
    sleepDuration1 = (wakeHour - currentHour) * 60 * 60 * 1000000ULL; // Calculate sleep duration until 08:00 same day
  }

  // Convert sleep duration to hours for logging
  float sleepDurationHours = sleepDuration1 / 3600000000.0;

  // Get the current time
  timeClient.update();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  int localHour = timeinfo.tm_hour;
  int localMinute = timeinfo.tm_min;
  int localSecond = timeinfo.tm_sec;
  char timeStringBuff[50]; // 50 chars should be enough
  snprintf(timeStringBuff, sizeof(timeStringBuff), "%02d:%02d:%02d", localHour, localMinute, localSecond);
  String asString(timeStringBuff);
  asString.replace(" ", "-");

  // Generate a unique ID based on the current time and boot count
  String uniqueID2 = String(millis()) + String(bootCount);

  // Send timestamp and sleep duration to Google Sheets
  String urlFinal = "https://script.google.com/macros/s/" + String(googleScriptID) + "/exec?" + "date2=" + asString + "&sleepDuration=" + String(sleepDurationHours, 2) + "h" + "&uniqueID2=" + uniqueID2;
  Serial.print("POST data to spreadsheet:");
  Serial.println(urlFinal);

  int retryCount = 0;
  const int maxRetries = 3;
  int httpCode = -1;

  while (retryCount < maxRetries && httpCode != 200) {
    HTTPClient http;
    http.begin(urlFinal.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpCode = http.GET(); 
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Payload: " + payload);    
    } else {
      Serial.print("Failed to send data to Google Sheets. HTTP status code: ");
      Serial.println(httpCode);
      Serial.print("Error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();
    retryCount++;

    if (httpCode != 200) {
      Serial.println("Retrying...");
      delay(2000); // Wait before retrying
    }
  }

  if (httpCode != 200) {
    Serial.println("Failed to send data after 3 retries. Going to deep sleep...");
  }

  Serial.println("Going to deep sleep...");
  WiFi.disconnect(true); // Turn off WiFi to save power
  esp_sleep_enable_timer_wakeup(sleepDuration1);
  esp_deep_sleep_start();
}


void goToLightSleep() {
  Serial.println("Preparing to go to light sleep...");
  Serial.println("Feeling sleepy. Going to take a nap ZZZzzz");
  
  // Set the duration for light sleep (1 hour) directly
  uint64_t sleepDuration = 3600000000ULL; // 1 hour in microseconds
  
  WiFi.disconnect(true); // Turn off WiFi to save power
  esp_sleep_enable_timer_wakeup(sleepDuration);
  
  // Enter light sleep
  esp_light_sleep_start();
  
  Serial.println("Waking up from light sleep...");
}
