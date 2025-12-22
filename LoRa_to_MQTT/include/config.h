#pragma once
#include <Arduino.h>

// ========== WiFi & MQTT ==========
#define WIFI_SSID   "AIR_SINK_WFC"
#define WIFI_PASS "IoTLab@2025"
#define WIFI_RESET_BUTTON_PIN 0 

#define MQTT_SERVER     "dev.iotlab.net.vn"
#define MQTT_FALLBACK_IP "103.221.220.183"
#define MQTT_PORT  1883
#define MQTT_USER  "api1@Iotlab"
#define MQTT_PASS  "Iotlab@2023"
#define MQTT_CLIENT_ID  "AIR_GATEWAY_VL" 
#define MQTT_TOPIC_UP   "ra02/up"     
#define MQTT_TOPIC_DOWN "ra02/down" 
// ========== DS3231 ==========
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
// ========== LoRa CONFIG ==========
#define LORA_FREQ    433E6
#define LORA_SF    8      
#define LORA_BW    125E3  
#define LORA_CR    5      
#define LORA_SYNC_WORD  0xF3   
// Pinmap
#define LORA_CS_PIN     5    
#define LORA_RST_PIN    4
#define LORA_DIO0_PIN   2    
#define LORA_SCK_PIN    18
#define LORA_MISO_PIN   19
#define LORA_MOSI_PIN   23
#define LORA_TX_POWER   20   // dBm
// Kích thước Queue 
#define QUEUE_LORA_OUT_SIZE 10