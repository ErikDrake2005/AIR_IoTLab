#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "LoRatoMQTT.h"
#include "WiFiConfig.h"

extern WiFiConfig wifiConfig;

class MQTTtoLoRa {
public:
    MQTTtoLoRa(LoRatoMQTT& lora);
    bool begin();
    void publish(const String& json);
    void loop();
    void processQueue(); 
    static void callback(char* topic, byte* payload, unsigned int length);

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    LoRatoMQTT& _lora;

    void _connectMQTT();
    bool reconnect();
    static MQTTtoLoRa* instance; 
};