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

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    WiFi.waitForConnectResult();

    // Initialize the RTC
    if (!rtc.begin()) {
        Serial.println("RTC initialization failed!");
        while (1);
    } else {
        Serial.println("RTC initialized successfully!");
    }

    // Set the wake-up time to 07:00 (7 AM)
    esp_sleep_enable_timer_wakeup(7 * 3600e6); // 7 hours * 3600 seconds/hour

    // Set up the web server
    server.on("/", HTTP_GET,  {
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
