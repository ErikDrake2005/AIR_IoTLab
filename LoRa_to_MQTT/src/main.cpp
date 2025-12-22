#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>     
#include <RTClib.h>   
#include "config.h"
#include "WiFiConfig.h"
#include "LoRatoMQTT.h"
#include "MQTTtoLoRa.h"
#include "PacketDef.h"

// --- KHAI BÁO OBJECT ---
WiFiConfig wifiConfig;
LoRatoMQTT loraModule;
MQTTtoLoRa mqttModule(loraModule);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7*3600, 60000); 
RTC_DS3231 rtc;
int lastHourSynced = -1;
int lastHourRTCSync = -1;
typedef struct {
    uint8_t payload[256];
    size_t length;
} LoraQueueMsg;

QueueHandle_t qMqttToLora; 

// --- TASK LORA: XỬ LÝ SÓNG (CORE 1) ---
void taskLoRa(void *pvParam) {
    LoraQueueMsg msg;
    LoRa.receive();
    while (true) {
        if (xQueueReceive(qMqttToLora, &msg, 0) == pdPASS) {
             Serial.printf("[TaskLoRa] Sending %d bytes...\n", msg.length);
             loraModule.sendBinary(msg.payload, msg.length);
             vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        loraModule.checkIncoming();
        
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// --- TASK MQTT: XỬ LÝ MẠNG (CORE 0) ---
// main.cpp
void taskMQTT(void *pvParam) {
    while (true) {
        wifiConfig.process();
        
        if (wifiConfig.isConnected()) {
            mqttModule.loop();
            
            // Cập nhật giờ mềm từ Server (nhưng chưa ghi vào DS3231 ngay)
            timeClient.update(); 
            
            // --- ĐOẠN CODE BỔ SUNG: ĐỒNG BỘ DS3231 MỖI NỬA NGÀY ---
            // Lấy epoch time hiện tại từ NTP
            unsigned long epoch = timeClient.getEpochTime();
            
            // Kiểm tra nếu đã lấy được giờ (epoch > 2020)
            if (epoch > 1600000000) {
                int currentHour = timeClient.getHours();
                
                // Điều kiện: Vào lúc 0 giờ hoặc 12 giờ, và chưa đồng bộ trong giờ đó
                if ((currentHour == 0 || currentHour == 12) && currentHour != lastHourRTCSync) {
                    
                    Serial.printf("[System] Auto Syncing DS3231 from NTP at %02d:00\n", currentHour);
                    
                    // Ghi giờ từ NTP vào DS3231
                    rtc.adjust(DateTime(epoch));
                    
                    // Đánh dấu đã đồng bộ xong giờ này
                    lastHourRTCSync = currentHour;
                }
            }
            // ------------------------------------------------------
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
void setup() {
    Serial.begin(115200);
    pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);
    qMqttToLora = xQueueCreate(10, sizeof(LoraQueueMsg));
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    if (!rtc.begin()) Serial.println("[Error] DS3231 not found!");
    wifiConfig.begin(); 

    timeClient.begin();

    // --- SETUP LORA ---
    if (!loraModule.begin()) {
        Serial.println("[Gateway] LoRa Init Failed!");
    } else {
        Serial.println("[Gateway] LoRa Init OK!");
    }

    mqttModule.begin();
    xTaskCreatePinnedToCore(taskLoRa, "TaskLoRa", 10240, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(taskMQTT, "TaskMQTT", 8192, NULL, 5, NULL, 0);
}
void loop() { vTaskDelete(NULL); }