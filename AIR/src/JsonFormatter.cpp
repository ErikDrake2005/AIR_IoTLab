#include "JsonFormatter.h"
#include "TimeSync.h"

// Tham chiếu đến đối tượng timeSync toàn cục trong main.cpp
extern TimeSync timeSync; 

JsonFormatter::JsonFormatter() {}

unsigned long JsonFormatter::getTimestamp() {
    // Gọi trực tiếp hàm getCurrentTime của đối tượng toàn cục
    return timeSync.getCurrentTime();
}

String JsonFormatter::createDataJson(float ch4, float co, float alc, float nh3, float h2, float temp, float hum) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["type"] = "data";
    doc["timestamp"] = getTimestamp();
    doc["ch4"] = ch4; doc["co"] = co; doc["alc"] = alc;
    doc["nh3"] = nh3; doc["h2"] = h2; doc["temp"] = temp; doc["hum"] = hum;
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonFormatter::createAck(const String& cmd) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    doc["timestamp"] = getTimestamp();
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonFormatter::createError(const String& msg) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["type"] = "error";
    doc["msg"] = msg;
    doc["timestamp"] = getTimestamp();
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonFormatter::createTimeSyncRequest() {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["type"] = "request_time_sync";
    String output;
    serializeJson(doc, output);
    return output;
}