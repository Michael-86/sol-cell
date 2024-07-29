#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <RTClib.h> // RTC library for ESP32

const char* ssid = "gtfast";
const char* password = "darktitan01";

AsyncWebServer server(90);
RTC_DS3231 rtc; // Initialize the RTC object
const int batteryPin = 13; // Analog input pin connected to the voltage divider

// Define your desired username and password
const char* expectedUsername = "admin";
const char* expectedPassword = "secure123";

// Static IP configuration (adjust as needed)
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);


  // Attempt WiFi connection with retry
  WiFi.begin(ssid, password);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("WiFi connection failed");
    // Handle WiFi connection failure (e.g., LED indication, retry logic)
  }

  // Configure static IP
  WiFi.config(local_IP, gateway, subnet);

  // Initialize the RTC
  if (!rtc.begin()) {
    Serial.println("RTC initialization failed!");
    while (1);
  } else {
    Serial.println("RTC initialized successfully!");
    // Set the correct time and date (if needed)
    rtc.adjust(DateTime(2024, 7, 29, 12, 0, 0)); // Example
  }

  // Set the wake-up time to 07:00 (7 AM)
  esp_sleep_enable_timer_wakeup(7 * 3600e6); // 7 hours * 3600 seconds/hour

  // Set up the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if the user is authenticated
    if (!request->authenticate(expectedUsername, expectedPassword)) {
      return request->requestAuthentication();
    }

    // Read battery voltage
    float batteryVoltage = readBatteryVoltage();
    int batteryPercentage = map(batteryVoltage, 3.9, 4.0, 0, 100);

    String html;
    html.reserve(50); // Reserve memory for the string
    html += "<html><body>";
    html += "<p>Battery Level: " + String(batteryPercentage) + "%</p>";
    html += "</body></html>";

    request->send(200, "text/html", html);
  });

  server.begin();

  // Enter deep sleep mode until the next wake-up time
  esp_deep_sleep_start();
}

void loop() {
  // This code won't execute due to deep sleep
}

float readBatteryVoltage() {
  int adcValue = analogRead(batteryPin);
  float voltage = (adcValue / 4095.0) * 3.3; // Assuming 12-bit ADC and 3.3V reference
  return voltage;
}
