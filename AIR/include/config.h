#pragma once
#include <Arduino.h>

// ========== THIẾT BỊ ==========
#define DEVICE_ID           "AIR_VL_01"
#define DEFAULT_MEASURES_PER_DAY   4
#define SECONDS_IN_DAY             86400UL

// ========== NTP (Dự phòng) ==========
#define NTP_SERVER          "pool.ntp.org"
#define NTP_TIMEZONE        "ICT-7" 

// ========== UART GIAO TIẾP BRIDGE ==========
#define UART_BAUD      921600 
  // Tốc độ cao để truyền JSON + CRC nhanh
#define UART_RX_PIN         16
#define UART_TX_PIN         17

// ========== RS485 CẢM BIẾN ==========
#define RS485_TX_PIN        26
#define RS485_RX_PIN        25
#define PIN_RS485_DE        4

// ========== RELAY + FAN ==========
#define CTL_ON_RELAY        13
#define CTL_OFF_RELAY       12
#define FANA                27
#define FAN1                14

// ========== SHT31 ==========
#define SHT31_I2C_ADDR      0x44

// ========== OTA ==========
#define OTA_CHUNK_SIZE      1024