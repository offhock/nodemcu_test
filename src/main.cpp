// Standalone MQTT client for ESP8266 NodeMCU
#include <Arduino.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>

// ESP8266 NodeMCU MQTT Client
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// IR Remote
#include <assert.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

// DHT Sensor
#include "DHTesp.h" 

// User defined
#include "env.h"

// from .env file
extern std::string ssid;
extern std::string password;
extern std::string mqtt_server;

// MQTT
const char* nodeName = "nodemcu-1";
long lastSentTime = 0;
long lastSubscribedTime = 0;
WiFiClient wifiClient;  // Use this for the MQTT client
PubSubClient mqttClient(mqtt_server.c_str(), 1883, wifiClient); // MQTT client


// IR Remote
const uint16_t kRecvPin = 14;   // D5 on a NodeMCU board. An IR detector/demodulator is connected to GPIO pin 14
const uint32_t kBaudRate = 115200; // The Serial connection baud rate.
const uint16_t kCaptureBufferSize = 1024; // As this program is a special purpose capture/decoder, let us use a larger than normal buffer so we can handle Air Conditioner remote codes.
const uint8_t kTimeout = 50; // kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a message ended.
const uint16_t kMinUnknownSize = 12; // Ignore messages with less than minimum on or off pulses.
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%
IRsend irsend(D2);  // An IR LED is connected to GPIO pin 4 (D2 on a NodeMCU board).
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true); // Use turn on the save buffer feature for more complete capture coverage.
decode_results results;  // Somewhere to store the results

struct IRCommand {
  uint16_t address;
  uint16_t command;
  uint64_t data;
};

// IR Command Map  
std::map<String, IRCommand> irCommandMap = {
  {"SLEEP_LIGHT",   {0xDEA8, 0x06, 0x157B609F}},
  {"ON",            {0xDEA8, 0xFF, 0x157BFF00}},
  {"OFF",           {0xDEA8, 0x01, 0x157B807F}},
  {"BRIGHT_UP",     {0xDEA8, 0x00, 0x157B00FF}},
  {"BRIGHT_DOWN",   {0xDEA8, 0x0D, 0x157BB04F}},
  {"LEFT",          {0xDEA8, 0x09, 0x157B906F}},
  {"RIGHT",         {0xDEA8, 0x15, 0x157BA857}},
  {"OK",            {0xDEA8, 0x1A, 0x157B58A7}},
  {"BRIGHTNESS_3",  {0xDEA8, 0x0A, 0x157B50AF}}, 
  {"TIMER_10MIN",   {0xDEA8, 0x16, 0x157B6897}},
  {"TIMER_20MIN",   {0xDEA8, 0x14, 0x157B28D7}},
  {"TIMER_30MIN",   {0xDEA8, 0x08, 0x157B10EF}},
  {"REPEAT",        {0x0000, 0x00, 0xFFFFFFFF}}  
};


// User defined
u16 count = 0;
DHTesp dht;

void setup() {    
    Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
    Serial.printf("Serial is %d bps.\n", Serial.baudRate());

    // Wait for the serial connection to be establised.
    while (!Serial) delay(50); // Wait for the serial connection to be establised.          
    assert(irutils::lowLevelSanityCheck() == 0); // Perform a low level sanity checks that the compiler performs bit field packing as we expect and Endianness is as we expect.
    Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);    
    irrecv.setUnknownThreshold(kMinUnknownSize); // Ignore messages with less than minimum on or off pulses.
    irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
    irrecv.enableIRIn();  // Start the receiver

    dht.setup(5, DHTesp::DHT11); // Connect DHT sensor to GPIO 5

    pinMode(D2, OUTPUT);
    digitalWrite(D2, LOW); // Switch OFF

    // Set LED_BUILTIN as output
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Switch OFF

    // Connect to WiFi
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting to WiFi..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }    
    Serial.printf("\nConnected to WiFi. IP address: %s\n", WiFi.localIP().toString().c_str());    

    // Set MQTT callback
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
        String topicStr{topic};
        String message{String{(char*)payload}.substring(0, length) + String{'\0'}};        
        Serial.printf("Message arrived [%s], Payload: %s\n",topic, message.c_str());

        if (topicStr == "homeassistant/nodemcu-1/remote/commands") {
            // IR Command
            if (irCommandMap.find(message) != irCommandMap.end()) {
                IRCommand irCmd = irCommandMap[message];
                Serial.printf("IR Code Transmit: CommandStr=%s Address=0x%X, Command=0x%X, Data=0x%llX\n", message.c_str(), irCmd.address, irCmd.command, irCmd.data);
                irsend.sendNEC(irCmd.data, 32);
            } else {
                Serial.println("Unknown Command from MQTT");
            }
            mqttClient.publish("homeassistant/nodemcu-1/remote", message.c_str(),true);
        } else if (topicStr == "homeassistant/nodemcu-1/switch/set") {
            if (message.equals("ON")) {
                digitalWrite(LED_BUILTIN, LOW);  // Switch ON
            } else if (message.equals("OFF")) {
                digitalWrite(LED_BUILTIN, HIGH);  // Switch OFF
            }        
            // publish switch status
            mqttClient.publish("homeassistant/nodemcu-1/switch", message.c_str(),true);
        } else {
            Serial.println("Unknown topic from MQTT");
        }
    });

}

// The repeating section of the code
void IR_loop() {
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
    // Check if we got an IR message that was to big for our capture buffer.
    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
    // Display the library version the message was captured with.
    Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
    // Display the tolerance percentage if it has been change from the default.
    if (kTolerancePercentage != kTolerance)
      Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));    
    // Display any extra A/C info if we have it.    
    String description = IRAcUtils::resultAcToString(&results);
    if (description.length()) 
      Serial.println(D_STR_MESGDESC ": " + description);
    yield();  // Feed the WDT as the text output can take a while to print.
    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println();    // Blank line between entries
    yield();             // Feed the WDT (again)

    
    auto it = std::find_if(irCommandMap.begin(), irCommandMap.end(), 
      [](const std::pair<String, IRCommand>& p) {
        return p.second.command == results.command;
      });
      
    if (it != irCommandMap.end()) {
        Serial.printf("Found key: %s\n", it->first.c_str());
        if (mqttClient.connected()) {
            mqttClient.publish("homeassistant/nodemcu-1/remote", it->first.c_str(),true);
            irrecv.pause(); // Pause receiving
            // Transmit the IR code
            irsend.sendNEC(it->second.data, 32);
            irrecv.resume();  // Resume receiving
        } else {
            Serial.printf("MQTT not connected. Cannot publish IR command: %s", it->first.c_str());
        }           
    } else {  // Unknown key
        Serial.println("Key not found.");
    }

  }
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(nodeName, "user", "passwd")) {
      Serial.println("connected");
      // subscribe switch setting, publish switch available
      mqttClient.subscribe("homeassistant/nodemcu-1/switch/set");       
      mqttClient.publish("homeassistant/nodemcu-1/switch/available", "online",true);
      // subscribe remote setting, publish remote available
      mqttClient.subscribe("homeassistant/nodemcu-1/remote/commands");
      mqttClient.publish("homeassistant/nodemcu-1/remote/available", "online",true);

      // publish configuration to the broker
      // mqttClient.publish("homeassistant/nodemcu-1/config", 
      //   "{"
      //   "\"name\": \"NodeMCU\", "
      //   "\"type\": \"switch\", "
      //   "\"remote_state_topic\": \"homeassistant/nodemcu-1/remote\", "
      //   "\"remote_command_topic\": \"homeassistant/nodemcu-1/remote/commands\", "
      //   "\"remote_availability_topic\": \"homeassistant/nodemcu-1/remote/available\", "
      //   "\"switch_state_topic\": \"homeassistant/nodemcu-1/switch\", "
      //   "\"switch_command_topic\": \"homeassistant/nodemcu-1/switch/set\", "
      //   "\"switch_availability_topic\": \"homeassistant/nodemcu-1/switch/available\", "
      //   "\"temperature_topic\": \"homeassistant/nodemcu-1/temperature\", "
      //   "\"counter_topic\": \"homeassistant/nodemcu-1/counter\", "
      //   "\"sin_topic\": \"homeassistant/nodemcu-1/sin\""
      //   "}",true);  

      mqttClient.publish("homeassistant/nodemcu-1/counter", String(0).c_str(),true);
      mqttClient.publish("homeassistant/nodemcu-1/sin", String(0).c_str(),true);
      mqttClient.publish("homeassistant/nodemcu-1/temperature", String(25).c_str(),true);
      mqttClient.publish("homeassistant/nodemcu-1/humidity", String(25).c_str(),true);

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
        mqttClient.publish("homeassistant/nodemcu-1/switch/available", "offline",true); 
        mqttClient.publish("homeassistant/nodemcu-1/remote/available", "offline",true);
        reconnect();
    } else {
      long now = millis();
      // publish temperature, counter, sin value every 60 seconds
      if( now - lastSentTime > 60000 ) {
          lastSentTime = now;        
          double value = 25 * sin(2.0 * PI * count / 100);
          double temperature = dht.getTemperature();    
          double humidity = dht.getHumidity();          
          mqttClient.publish("homeassistant/nodemcu-1/counter", String(count).c_str(),true);
          mqttClient.publish("homeassistant/nodemcu-1/sin", String(value).c_str(),true);
          mqttClient.publish("homeassistant/nodemcu-1/temperature", String(temperature).c_str(),true);          
          mqttClient.publish("homeassistant/nodemcu-1/humidity", String(humidity).c_str(),true);
          Serial.printf("Temperature: %.1f, Humidity: %.1f,  Counter: %d, Sin: %.2f\n", temperature, humidity, count, value);
          count++;
      }    

      // subscribe switch setting every 10 seconds
      if( now - lastSubscribedTime > 10000 ) {
          lastSubscribedTime = now;
          mqttClient.subscribe("homeassistant/nodemcu-1/switch/set");
          mqttClient.subscribe("homeassistant/nodemcu-1/remote/commands");          
      }
    }

    mqttClient.loop();
    IR_loop();
}
