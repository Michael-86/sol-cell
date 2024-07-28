#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <RTClib.h> // RTC library for ESP32

const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

AsyncWebServer server(80);
RTC_DS3231 rtc; // Initialize the RTC object
const int batteryPin = 13; // Analog input pin connected to the voltage divider

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    // Initialize the RTC
    if (!rtc.begin()) {
        Serial.println("RTC initialization failed!");
        while (1);
    }

    // Set the wake-up time to 07:00 (7 AM)
    rtc.adjust(DateTime(__DATE__, "07:00:00"));

    // Enable the alarm interrupt
    rtc.enableAlarm(ALM1);

    // Set up the web server
    server.on("/", HTTP_GET, {
        float batteryVoltage = readBatteryVoltage(); // Read battery voltage
        int batteryPercentage = map(batteryVoltage, 3.9, 4.0, 0, 100);

        String html = "<html><body>";
        html += "<p>Battery Level: " + String(batteryPercentage) + "%</p>";
        html += "</body></html>";

        request->send(200, "text/html", html);
    });

    server.begin();

    // Enter deep sleep mode
    esp_sleep_enable_timer_wakeup(7 * 3600 * 1e6); // 7 hours in microseconds
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
