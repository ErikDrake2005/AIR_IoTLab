#pragma once 
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

class JsonFormatter {
public:
    JsonFormatter();
    // Giữ nguyên các hàm không có tham số ts để không gây lỗi Domino
    String createDataJson(float ch4, float co, float alc, float nh3, float h2, float temp, float hum);
    String createAck(const String& cmd);
    String createError(const String& msg);
    String createTimeSyncRequest(); 

private:
    unsigned long getTimestamp(); // Hàm nội bộ lấy giờ từ TimeSync
};