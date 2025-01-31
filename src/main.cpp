#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <Wire.h>


#include <math.h>
#include <iostream>
#include <fstream>
#include <string>

#include "env.h"

// from .env file
extern std::string ssid;
extern std::string password;
extern std::string mqtt_server;

const char* nodeName = "nodemcu-1";
long lastSent = 0;

WiFiClient wifiClient;
PubSubClient mqttClient(mqtt_server.c_str(), 1883, wifiClient);

u16 count = 0;
void setup() {    
    Serial.begin(115200);    
    Serial.printf("Serial is %d bps.\n", Serial.baudRate());

    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting to WiFi..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }    
    Serial.printf("\nConnected to WiFi. IP address: %s\n", WiFi.localIP().toString().c_str());    

    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {        
        String message = String((char*)payload).substring(0, length) + '\0';        
        Serial.printf("Message arrived [%s], Payload: %s\n",topic, message.c_str());
        if (message.equals("ON")) {
            digitalWrite(LED_BUILTIN, HIGH);  // 스위치 ON
        } else if (message.equals("OFF")) {
            digitalWrite(LED_BUILTIN, LOW);  // 스위치 OFF
        }        
        // publish switch status
        mqttClient.publish("homeassistant/nodemcu-1/switch", message.c_str());        
    });

}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(nodeName, "user", "passwd")) {
      Serial.println("connected");
      // subscribe switch setting
      mqttClient.subscribe("homeassistant/nodemcu-1/switch/set"); 
      // publish switch availability
      mqttClient.publish("homeassistant/nodemcu-1/switch/available", "online"); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {

    if (!mqttClient.connected()) {
        reconnect();
    } 
    mqttClient.loop();

    long now = millis();
    if( now - lastSent > 5000 ) {
        lastSent = now;        
        double value = 25 * sin(2.0 * PI * count / 100);
        double temperature = 25 + (count % 10);        
        mqttClient.publish("homeassistant/nodemcu-1/temperature", String(temperature).c_str());
        mqttClient.publish("homeassistant/nodemcu-1/counter", String(count).c_str());
        mqttClient.publish("homeassistant/nodemcu-1/sin", String(value).c_str());
        Serial.printf("Temperature: %.1f, Counter: %d, Sin: %.2f\n", temperature, count, value);
        count++;
    }
    
}
 
