#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "config.h"
#include "PacketDef.h"
#include "Security.h"
#include "CRC32.h"
#include "KeyConfig.h"

// --- CẤU TRÚC MESSAGE QUEUE ---
typedef struct {
    uint8_t payload[256]; 
    size_t length;
} LoraMsg;
QueueHandle_t qUartToLora; // Hàng đợi tin nhắn cần gửi ra LoRa
// --- UART ---
HardwareSerial uart(1);
// --- JSON ---
JsonDocument doc; 
// --- PROTOTYPES ---
void Task_LoRa(void *pvParam);      
void Task_UART(void *pvParam);
void processJsonFromMain(String jsonStr); 

void setup() {
    Serial.begin(115200); 
    // 1. Init UART Bridge
    uart.setRxBufferSize(2048); 
    uart.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.printf("--- BRIDGE STARTED: %s ---\n", MY_DEVICE_ID);
    // 2. Init LoRa
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("[ERR] LoRa Init Failed!");
        while (1) delay(100);
    }
    // Cấu hình LoRa
    LoRa.setTxPower(LORA_TX_POWER); 
    LoRa.setSpreadingFactor(LORA_SF); 
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNC_WORD); 
    LoRa.enableCrc();
    // 3. Init Queue
    qUartToLora = xQueueCreate(10, sizeof(LoraMsg));
    // 4. Create Tasks
    xTaskCreatePinnedToCore(Task_LoRa, "TaskLoRa", 8192, NULL, 5, NULL, 1); 
    xTaskCreatePinnedToCore(Task_UART, "TaskUART", 8192, NULL, 4, NULL, 0);
}
void loop() { vTaskDelete(NULL); }
// TASK 1: LORA HANDLE (NHẬN & GỬI BINARY)
void Task_LoRa(void *pvParam) {
    uint8_t rxBuf[256];
    uint8_t decBuf[256];
    LoraMsg txMsg;
    LoRa.receive();
    while (true) {
        // --- A. XỬ LÝ NHẬN (RX) ---
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            int i = 0;
            while (LoRa.available() && i < 255) rxBuf[i++] = LoRa.read();
            if (i > 0) {
                PacketHeader head;
                // GIẢI MÃ VỚI KEY RIÊNG (MY_AES_KEY)
                int plainLen = Security::decryptBinary(rxBuf, i, head, decBuf, MY_AES_KEY);
                // Nếu giải mã thành công & ID trùng khớp (Unicast check)
                if (plainLen > 0 && strcmp(head.deviceId, MY_DEVICE_ID) == 0) {
                    doc.clear();
                    doc["type"] = "unknown";
                    uint8_t msgType = decBuf[0];
                    // 1. Nhận lệnh điều khiển
                    if (msgType == TYPE_CMD_CTRL) {
                        ControlCmd* pkt = (ControlCmd*)decBuf;
                        switch (pkt->cmdCode) {
                            case CMD_OPEN_DOOR:       doc["type"] = "open_door"; break;
                            case CMD_CLOSE_DOOR:      doc["type"] = "close_door"; break;
                            case CMD_FANS_ON:         doc["type"] = "fans_on"; break;
                            case CMD_FANS_OFF:        doc["type"] = "fans_off"; break;
                            case CMD_MODE_AUTO:       doc["type"] = "set_mode"; doc["mode"] = "auto"; break;
                            case CMD_MODE_MANUAL:     doc["type"] = "set_mode"; doc["mode"] = "manual"; break;
                            case CMD_TRIGGER_MEASURE: doc["type"] = "trigger_measure"; break;
                            case CMD_STOP_MEASURE:    doc["type"] = "stop_measure"; break;
                        }
                    }
                    else if (msgType == TYPE_CMD_CONFIG) {
                        ConfigCmd* pkt = (ConfigCmd*)decBuf;
                        if (pkt->cmdCode == CMD_SYSTEM_TIME_OK) {
                            // Gateway gửi Time Sync -> Báo cho Main biết
                            doc["type"] = "time_sync";
                            char ts[32];
                            sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d", 
                                pkt->year, pkt->month, pkt->day, pkt->hour, pkt->minute, pkt->second);
                            doc["timestamp"] = String(ts);
                        }
                        else if (pkt->cmdCode == CMD_SET_CYCLE) {
                            doc["type"] = "set_cycle";
                            doc["measures_per_day"] = pkt->value;
                        }
                        else if (pkt->cmdCode == CMD_SET_SCHEDULE) {
                            doc["type"] = "set_time";
                            char ts[16];
                            sprintf(ts, "%02d:%02d:%02d", pkt->hour, pkt->minute, pkt->second);
                            doc["time"] = String(ts);
                        }
                    }
                    if (doc["type"] != "unknown") {
                        String jsonOut;
                        serializeJson(doc, jsonOut);
                        // Tính CRC cho an toàn đường truyền UART
                        unsigned long crc = CRC32::calculate(jsonOut);
                        uart.printf("%s|%lX\n", jsonOut.c_str(), crc);
                        Serial.println("[LoRa RX -> UART] " + jsonOut);
                    }
                } else {
                     Serial.println("[LoRa] Ignored packet (Wrong Key or ID)");
                }
            }
            LoRa.receive();
        }
        // --- B. XỬ LÝ GỬI (TX) ---
        if (xQueueReceive(qUartToLora, &txMsg, 0) == pdPASS) {
            LoRa.idle();
            LoRa.beginPacket();
            LoRa.write(txMsg.payload, txMsg.length);
            LoRa.endPacket(true);
            LoRa.receive();
            Serial.printf("[LoRa TX] Sent %d bytes encrypted\n", txMsg.length);
        }

        vTaskDelay(5 / portTICK_PERIOD_MS); 
    }
}
// TASK 2: UART HANDLE (NHẬN JSON TỪ MAIN)
void Task_UART(void *pvParam) {
    static char rxBuffer[1024]; 
    static int rxIndex = 0;

    while (true) {
        while (uart.available()) {
            char c = (char)uart.read();
            if (c == '\n') {
                rxBuffer[rxIndex] = '\0';
                String inputLine = String(rxBuffer);
                inputLine.trim();
                rxIndex = 0;
                if (inputLine.length() > 0) {
                    // Check CRC từ Main gửi lên (Format: JSON|CRC)
                    int pipeIdx = inputLine.lastIndexOf('|');
                    if (pipeIdx != -1) {
                        String jsonPart = inputLine.substring(0, pipeIdx);
                        String crcPart = inputLine.substring(pipeIdx + 1);
                        unsigned long calcCrc = CRC32::calculate(jsonPart);
                        unsigned long recvCrc = strtoul(crcPart.c_str(), NULL, 16);
                        if (calcCrc == recvCrc) {
                            processJsonFromMain(jsonPart);
                        } else {
                            Serial.println("[UART] CRC Fail");
                        }
                    }
                }
            } else {
                if (rxIndex < 1023) rxBuffer[rxIndex++] = c;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
// HELPER: PARSE JSON -> ENCRYPT -> PUSH QUEUE
void processJsonFromMain(String jsonStr) {
    doc.clear();
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) return;
    const char* type = doc["type"];
    PacketHeader head;
    strncpy(head.deviceId, MY_DEVICE_ID, ID_MAX_LEN);
    head.counter = millis();
    uint8_t encBuf[256];
    int len = 0;
    // 1. GỬI DATA CẢM BIẾN
    if (strcmp(type, "data") == 0) {
        SensorData d;
        d.msgType = TYPE_SENSOR_DATA;
        d.temp = (int16_t)(doc["temp"].as<float>() * 100.0);
        d.hum = (uint16_t)(doc["hum"].as<float>() * 100.0);
        d.ch4 = (uint16_t)doc["ch4"];
        d.co = (uint16_t)doc["co"];
        d.alc = (uint16_t)doc["alc"];
        d.nh3 = (uint16_t)doc["nh3"];
        d.h2 = (uint16_t)doc["h2"];
        // MÃ HÓA BẰNG KEY RIÊNG
        len = Security::encryptBinary(head, &d, sizeof(SensorData), encBuf, MY_AES_KEY);
        // Data sensor cần delay ngẫu nhiên để tránh va chạm sóng
        vTaskDelay(random(20, 100) / portTICK_PERIOD_MS);
    }
    // 2. GỬI PHẢN HỒI (ACK) HOẶC ERROR
    else if (strcmp(type, "ack") == 0 || strcmp(type, "error") == 0) {
        ResponsePacket resp;
        resp.msgType = TYPE_RESPONSE;
        
        if (strcmp(type, "ack") == 0) {
            String cmd = doc["cmd"];
            if (cmd == "open_door") resp.ackCode = ACK_OPEN_DOOR_OK;
            else if (cmd == "close_door") resp.ackCode = ACK_CLOSE_DOOR_OK;
            else if (cmd == "fans_on") resp.ackCode = ACK_FANS_ON_OK;
            else if (cmd == "fans_off") resp.ackCode = ACK_FANS_OFF_OK;
            else if (cmd == "set_mode_auto") resp.ackCode = ACK_MODE_AUTO_OK;
            else if (cmd == "set_mode_manual") resp.ackCode = ACK_MODE_MANUAL_OK;
            else if (cmd == "trigger_measure") resp.ackCode = ACK_TRIGGER_OK;
            else if (cmd == "stop_measure") resp.ackCode = ACK_STOP_MEASURE_OK;
            else if (cmd == "set_cycle") resp.ackCode = ACK_SET_CYCLE_OK;
            else if (cmd == "set_time") resp.ackCode = ACK_SET_TIME_OK;
            else resp.ackCode = ACK_OK;
        } else {
            String msg = doc["msg"];
            if (msg == "busy_measuring") resp.ackCode = ERR_BUSY;
            else if (msg == "cannot_manual_in_auto") resp.ackCode = ERR_IN_AUTO;
            else resp.ackCode = ERR_UNKNOWN;
        }
        
        len = Security::encryptBinary(head, &resp, sizeof(ResponsePacket), encBuf, MY_AES_KEY);
    }
    // 3. GỬI YÊU CẦU TIME SYNC (Chủ động xin giờ)
    else if (strcmp(type, "time_req") == 0) {
        SimplePacket p;
        p.msgType = TYPE_TIME_REQ;
        len = Security::encryptBinary(head, &p, sizeof(SimplePacket), encBuf, MY_AES_KEY);
    }

    // Đẩy vào Queue
    if (len > 0) {
        LoraMsg msg;
        memcpy(msg.payload, encBuf, len);
        msg.length = len;
        xQueueSend(qUartToLora, &msg, 0);
    }
}