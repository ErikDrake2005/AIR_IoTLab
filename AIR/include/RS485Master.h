#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"

class RS485Master {
public:
    RS485Master(HardwareSerial& serial, uint8_t dePin = PIN_RS485_DE);
    void begin();

    // --- HÀM MỚI: Đọc tất cả cảm biến và lưu vào biến nội bộ ---
    bool readSensors(); 

    // --- HÀM MỚI: Getter (Trả về giá trị float cho Measurement dùng) ---
    float getCH4();
    float getCO();
    float getAlcohol();
    float getNH3();
    float getH2();

    // Các hàm cấp thấp (giữ nguyên để dùng nội bộ hoặc debug)
    bool sendCommand(uint8_t slaveId, uint8_t cmd, uint16_t timeoutMs = 500);
    String readResponse(uint16_t timeoutMs = 500);
    
    // Hàm parse (giữ nguyên)
    bool parseCH4(const String& line, float& ch4);
    bool parseMICS(const String& line, float& co, float& alc, float& nh3, float& h2);

private:
    HardwareSerial& _serial;
    uint8_t _dePin;
    bool _initialized = false;

    // --- BIẾN LƯU TRỮ DỮ LIỆU ---
    float _ch4 = 0.0;
    float _co  = 0.0;
    float _alc = 0.0;
    float _nh3 = 0.0;
    float _h2  = 0.0;

    void _transmitMode();
    void _receiveMode();
};