#pragma once
#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include "config.h"
#include "PacketDef.h"
#include "Security.h"
class MQTTtoLoRa; 
class LoRatoMQTT {
public:
    LoRatoMQTT();
    bool begin();
    void checkIncoming(); 
    void sendBinary(uint8_t* buffer, int len);
    void sendTimeToAllNodes();
    void _sendTimeSyncResponse(const char* targetId);

private:
    const int _csPin = LORA_CS_PIN;
    const int _resetPin = LORA_RST_PIN;
    const int _dio0Pin = LORA_DIO0_PIN;
};