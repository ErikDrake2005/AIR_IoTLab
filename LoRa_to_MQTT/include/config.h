#pragma once
#include <Arduino.h>

// ========== WiFi & MQTT ==========
#define WIFI_SSID "H6_T2"
#define WIFI_PASS  "billH62015"
#define WIFI_RESET_BUTTON_PIN 0 
#define MQTT_SERVER "<Server Host Name>"
#define MQTT_FALLBACK_IP "<Server IP>"
#define MQTT_PORT  1883
#define MQTT_USER "<USER NAME MQTT>"
#define MQTT_PASS "<USER PASS MQTT>"
#define MQTT_CLIENT_ID  "AIR_GATEWAY_VL"  //SINK's NAME
#define MQTT_TOPIC_UP "ra02/up"     // Sent topic
#define MQTT_TOPIC_DOWN "ra02/down"  // Recieve topic
// ========== I2C / RTC ==========
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
// ========== LoRa CONFIG (Khớp Bridge 100%) ==========
#define LORA_FREQ 433E6
#define LORA_SF 8      
#define LORA_BW 125E3  
#define LORA_CR 5      
#define LORA_SYNC_WORD 0xF3   
#define LORA_CS_PIN 5    
#define LORA_RST_PIN 4
#define LORA_DIO0_PIN 2    
#define LORA_SCK_PIN  18
#define LORA_MISO_PIN 19
#define LORA_MOSI_PIN 23
#define LORA_TX_POWER 20   // 20dBm (Cần nguồn cấp tốt)
#define QUEUE_LORA_OUT_SIZE 10