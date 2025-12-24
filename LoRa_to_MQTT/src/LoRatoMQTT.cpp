#include "LoRatoMQTT.h"
#include "MQTTtoLoRa.h" 
#include "KeyStore.h"
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <RTClib.h>
extern NTPClient timeClient; 
extern MQTTtoLoRa mqttModule; 
extern RTC_DS3231 rtc; 
LoRatoMQTT::LoRatoMQTT() {}
bool LoRatoMQTT::begin() {
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
    LoRa.setPins(_csPin, _resetPin, _dio0Pin);
    if (!LoRa.begin(LORA_FREQ)) {
        return false;
    }
    LoRa.setTxPower(LORA_TX_POWER); 
    LoRa.setSpreadingFactor(LORA_SF);            
    LoRa.setSignalBandwidth(LORA_BW);            
    LoRa.setCodingRate4(LORA_CR);                
    LoRa.setSyncWord(LORA_SYNC_WORD);            
    LoRa.enableCrc(); 
    LoRa.receive();
    return true;
}
void LoRatoMQTT::sendBinary(uint8_t* buffer, int len) {
    if (len <= 0) return;
    Serial.printf("[LoRa TX] Sending %d bytes...\n", len);
    LoRa.beginPacket();
    LoRa.write(buffer, len);
    LoRa.endPacket(true);
    LoRa.receive();
}
void LoRatoMQTT::checkIncoming() {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        uint8_t rxBuf[256];
        int idx = 0;
        // Đọc toàn bộ gói tin vào buffer
        while (LoRa.available() && idx < 255) {
            rxBuf[idx++] = LoRa.read();
        }
        PacketHeader head;
        uint8_t payload[256];
        int plainLen = -1;
        const char* identifiedKey = nullptr;
        // --- BƯỚC 1: DÒ TÌM CHÌA KHÓA ---
        for (int i = 0; i < NUM_NODES; i++) {
            const NodeKey* nk = KeyStore::getNodeByIndex(i);
            int res = Security::decryptBinary(rxBuf, idx, head, payload, nk->key);
            if (res > 0 && strcmp(head.deviceId, nk->id) == 0) {
                plainLen = res;
                identifiedKey = nk->key;
                Serial.printf("[Security] Match Key: %s\n", nk->id);
                break;
            }
        }
        // --- BƯỚC 2: XỬ LÝ DỮ LIỆU ĐÃ GIẢI MÃ ---
        if (plainLen > 0 && identifiedKey != nullptr) {
            JsonDocument doc;
            bool needPublish = false;
            uint8_t msgType = payload[0];
            doc["device_id"] = head.deviceId;
            doc["rssi"] = LoRa.packetRssi();
            doc["snr"] = LoRa.packetSnr();
            // CASE A: DATA CẢM BIẾN
            if (msgType == TYPE_SENSOR_DATA) {
                SensorData* s = (SensorData*)payload;
                doc["type"] = "sensor_data";
                doc["temp"] = (float)s->temp / 100.0;
                doc["hum"] = (float)s->hum / 100.0;
                doc["ch4"] = s->ch4;
                doc["co"] = s->co;
                doc["alc"]= s->alc;
                doc["nh3"] = s->nh3;
                doc["h2"] = s->h2;
                needPublish = true;
            }
            // CASE B: PHẢN HỒI (ACK / ERROR)
            else if (msgType == TYPE_RESPONSE) {
                ResponsePacket* r = (ResponsePacket*)payload;
                doc["type"] = "response";
                switch (r->ackCode) {
                    case ACK_OK:              doc["status"] = "ok"; break;
                    case ACK_OPEN_DOOR_OK:    doc["status"] = "open_door_success"; break;
                    case ACK_CLOSE_DOOR_OK:   doc["status"] = "close_door_success"; break;
                    case ACK_FANS_ON_OK:      doc["status"] = "fans_on_success"; break;
                    case ACK_FANS_OFF_OK:     doc["status"] = "fans_off_success"; break;
                    case ACK_MODE_MANUAL_OK:  doc["status"] = "mode_manual_success"; break;
                    case ACK_MODE_AUTO_OK:    doc["status"] = "mode_auto_success"; break;
                    case ACK_TRIGGER_OK:      doc["status"] = "trigger_success"; break;
                    case ACK_STOP_MEASURE_OK: doc["status"] = "stop_measure_success"; break;
                    case ACK_SET_CYCLE_OK:    doc["status"] = "set_cycle_success"; break;
                    case ACK_SET_TIME_OK:     doc["status"] = "set_schedule_success"; break;
                    case ERR_BUSY:            doc["status"] = "error_busy"; break;
                    case ERR_IN_AUTO:         doc["status"] = "error_in_auto"; break;
                    case ERR_EXECUTE_FAIL:    doc["status"] = "error_hardware"; break;
                    case ERR_UNKNOWN:         doc["status"] = "error_unknown_cmd"; break;
                    default:                  doc["status"] = "unknown_code"; break;
                }
                needPublish = true;
            }
            else if (msgType == TYPE_TIME_REQ) {
                Serial.printf("[Gateway] Sync Time Req from %s\n", head.deviceId);
                _sendTimeSyncResponse(head.deviceId);
            }
            if (needPublish) {
                String jsonStr;
                serializeJson(doc, jsonStr);
                mqttModule.publish(jsonStr);
                Serial.println("[MQTT UP] " + jsonStr);
            }
        } 
        else {
            Serial.println("[Security] No matching key found for incoming LoRa packet.");
        }
        LoRa.receive();
    }
}

// HÀM PHỤ: Gửi phản hồi đồng bộ giờ
void LoRatoMQTT::_sendTimeSyncResponse(const char* targetId) {
    // 1. Tìm Key của thiết bị đích
    const char* targetKey = KeyStore::getKeyById(targetId);
    if (targetKey == nullptr) {
        Serial.printf("[Error] Cannot sync time: No key for %s\n", targetId);
        return;
    }
    PacketHeader head;
    strncpy(head.deviceId, targetId, ID_MAX_LEN); 
    head.counter = millis(); 
    ConfigCmd timePkt;
    timePkt.msgType = TYPE_CMD_CONFIG;
    timePkt.cmdCode = CMD_SYSTEM_TIME_OK; 
    unsigned long epoch = timeClient.getEpochTime();
    if (epoch < 1600000000) { 
        DateTime now = rtc.now();
        timePkt.year   = now.year();
        timePkt.month  = now.month();
        timePkt.day    = now.day();
        timePkt.hour   = now.hour();
        timePkt.minute = now.minute();
        timePkt.second = now.second();
    } else {
        time_t rawTime = epoch;
        struct tm * ti = localtime(&rawTime);
        timePkt.year   = ti->tm_year + 1900;
        timePkt.month  = ti->tm_mon + 1;
        timePkt.day    = ti->tm_mday;
        timePkt.hour   = ti->tm_hour;
        timePkt.minute = ti->tm_min;
        timePkt.second = ti->tm_sec;
    }
    uint8_t encBuf[128];
    int len = Security::encryptBinary(head, &timePkt, sizeof(ConfigCmd), encBuf, targetKey);
    LoRa.idle();
    LoRa.beginPacket();
    LoRa.write(encBuf, len);
    LoRa.endPacket(false); 
    LoRa.receive();
    Serial.printf("[Gateway] Sent TimeSync to %s\n", targetId);
}
void LoRatoMQTT::sendTimeToAllNodes() {
    Serial.println("[Gateway] >>> START HOURLY TIME SYNC BROADCAST <<<");
    for (int i = 0; i < NUM_NODES; i++) {
        const NodeKey* nk = KeyStore::getNodeByIndex(i);
        if (nk != nullptr) {
            _sendTimeSyncResponse(nk->id); 
            vTaskDelay(3000 / portTICK_PERIOD_MS); 
        }
    }
    Serial.println("[Gateway] >>> FINISHED BROADCAST <<<");
}