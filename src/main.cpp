#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>
#include <string>

#include "env.h"

// from .env file
extern std::string ssid;
extern std::string password;
extern std::string mqtt_server;

const char* nodeName = "nodemcu-1";

WiFiClient wifiClient;
PubSubClient mqttClient(mqtt_server.c_str(), 1883, wifiClient);

u16 count = 0;
void setup() {    
    Serial.begin(115200);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("Connecting to WiFi..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }    
    Serial.println("Connected to WiFi");
    
    while (!mqttClient.connect(nodeName)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("Connected to MQTT Broker");
}

void loop() {
    double value = sin(2.0 * PI * count / 100);
    mqttClient.publish("home/nodemcu-1/temperature", "24.5");
    mqttClient.publish("home/nodemcu-1/counter", String(count).c_str());
    mqttClient.publish("home/nodemcu-1/sin", String(value).c_str());
    count++;
    delay(5000); // Example delay between sending temperature readings
}
 