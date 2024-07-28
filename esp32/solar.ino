#include <WiFi.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

AsyncWebServer server(80);

const int batteryPin = 13; // Analog input pin connected to the voltage divider

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    server.on("/", HTTP_GET, {
        float batteryVoltage = readBatteryVoltage(); // Read battery voltage
        int batteryPercentage = map(batteryVoltage, 3.9, 4.0, 0, 100);

        String html = "<html><body>";
        html += "<p>Battery Level: " + String(batteryPercentage) + "%</p>";
        html += "</body></html>";

        request->send(200, "text/html", html);
    });

    server.begin();
}

void loop() {
    // Other tasks or deep sleep if needed
}

float readBatteryVoltage() {
    int adcValue = analogRead(batteryPin);
    float voltage = (adcValue / 4095.0) * 3.3; // Assuming 12-bit ADC and 3.3V reference
    return voltage;
}
