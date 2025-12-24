#include "MQTTtoLoRa.h"
#include <ArduinoJson.h>
#include "PacketDef.h"
#include "Security.h"
#include "KeyStore.h" // <--- QUAN TRỌNG: Phải có file này để lấy Key

// Struct Queue
typedef struct {
    uint8_t payload[256];
    size_t length;
} LoraQueueMsg;

extern QueueHandle_t qMqttToLora; 

MQTTtoLoRa* MQTTtoLoRa::instance = nullptr;

MQTTtoLoRa::MQTTtoLoRa(LoRatoMQTT& lora) : _lora(lora) {
    instance = this;
    _mqttClient.setClient(_wifiClient);
    _mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    _mqttClient.setCallback(MQTTtoLoRa::callback);
    _mqttClient.setBufferSize(1024); 
}

bool MQTTtoLoRa::begin() { return reconnect(); }

void MQTTtoLoRa::loop() {
    if (!_mqttClient.connected()) _connectMQTT();
    _mqttClient.loop();
}

void MQTTtoLoRa::publish(const String& json) {
    if (_mqttClient.connected()) {
        _mqttClient.publish(MQTT_TOPIC_UP, json.c_str());
    }
}

void MQTTtoLoRa::_connectMQTT() {
    if (WiFi.status() == WL_CONNECTED && !_mqttClient.connected()) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            reconnect();
        }
    }
}

bool MQTTtoLoRa::reconnect() {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (_mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        Serial.println("[MQTT] Connected");
        _mqttClient.subscribe(MQTT_TOPIC_DOWN);
        return true;
    }
    return false;
}
// --- MQTTtoLoRa.cpp ---

void MQTTtoLoRa::callback(char* topic, byte* payload, unsigned int length) {
    // Debug
    char debugBuf[length + 1];
    memcpy(debugBuf, payload, length);
    debugBuf[length] = '\0';
    Serial.printf("\n[MQTT RX] %s\n", debugBuf);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("[ERROR] JSON Parse Failed: %s\n", error.c_str());
        return;
    }

    const char* devId = doc["device_id"];
    if (!devId) devId = doc["device"]; 

    const char* typeStr = doc["type"];
    if (!typeStr || !devId) return;

    // LẤY KEY TỪ KEYSTORE
    const char* targetKey = KeyStore::getKeyById(devId);
    if (targetKey == nullptr) {
        Serial.printf("[ERROR] Key not found for: %s\n", devId);
        return; 
    }

    String type = String(typeStr);
    
    PacketHeader head;
    strncpy(head.deviceId, devId, ID_MAX_LEN); 
    head.counter = millis();

    uint8_t encBuf[256];
    int len = 0;

    // --- NHÓM 1: LỆNH CONTROL CŨ (2 Bytes) ---
    if (type == "open_door" || type == "close_door" || 
        type == "fans_on" || type == "fans_off" ||
        type == "stop_measure" || type == "set_mode") {
        
        ControlCmd cmd;
        cmd.msgType = TYPE_CMD_CTRL;
        
        if (type == "open_door")       cmd.cmdCode = CMD_OPEN_DOOR;
        else if (type == "close_door") cmd.cmdCode = CMD_CLOSE_DOOR;
        else if (type == "fans_on")    cmd.cmdCode = CMD_FANS_ON;
        else if (type == "fans_off")   cmd.cmdCode = CMD_FANS_OFF;
        else if (type == "stop_measure") cmd.cmdCode = CMD_STOP_MEASURE;
        else if (type == "set_mode") {
            String m = doc["mode"];
            cmd.cmdCode = (m == "auto") ? CMD_MODE_AUTO : CMD_MODE_MANUAL;
        }

        len = Security::encryptBinary(head, &cmd, sizeof(ControlCmd), encBuf, targetKey);
    }
    
    // --- NHÓM 2: LỆNH TRIGGER MỚI (3 Bytes) ---
    else if (type == "trigger_measure") {
        TriggerCmd tCmd; 
        tCmd.msgType = TYPE_CMD_CTRL; 
        tCmd.cmdCode = CMD_TRIGGER_MEASURE;
        
        // Lấy tham số cycle (mặc định = 1)
        int cyc = doc["cycle"] | 1; 
        if (cyc < 1) cyc = 1;
        if (cyc > 255) cyc = 255;
        tCmd.cycle = (uint8_t)cyc;

        len = Security::encryptBinary(head, &tCmd, sizeof(TriggerCmd), encBuf, targetKey);
    }

    // --- NHÓM 3: LỆNH CONFIG ---
    else if (type == "set_cycle" || type == "set_time") {
        ConfigCmd cfg;
        cfg.msgType = TYPE_CMD_CONFIG;
        
        if (type == "set_cycle") {
            cfg.cmdCode = CMD_SET_CYCLE;
            cfg.value = doc["measures_per_day"];
        }
        else if (type == "set_time") {
            cfg.cmdCode = CMD_SET_SCHEDULE;
            const char* tStr = doc["time"];
            int h, m, s;
            if (sscanf(tStr, "%d:%d:%d", &h, &m, &s) == 3) {
                cfg.hour = h; cfg.minute = m; cfg.second = s;
            }
        }
        
        len = Security::encryptBinary(head, &cfg, sizeof(ConfigCmd), encBuf, targetKey);
    } 

    // GỬI RA LORA
    if (len > 0) {
        LoraQueueMsg msg;
        if (len <= sizeof(msg.payload)) {
            memcpy(msg.payload, encBuf, len);
            msg.length = len;
            xQueueSend(qMqttToLora, &msg, 0);
            Serial.printf("[Gateway] Forwarding to %s (Type: %s)\n", devId, typeStr);
        }
    }
}