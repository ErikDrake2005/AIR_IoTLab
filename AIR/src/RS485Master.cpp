// src/RS485Master.cpp
#include "RS485Master.h"
RS485Master::RS485Master(HardwareSerial& serial, uint8_t dePin)
    : _serial(serial), _dePin(dePin) {}
void RS485Master::begin() {
    _serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    _serial.setTimeout(200);
    pinMode(_dePin, OUTPUT);
    _receiveMode();
    _initialized = true;
    Serial.println("[RS485] Khoi dong thanh cong");
}
void RS485Master::_transmitMode() { digitalWrite(_dePin, HIGH); }
void RS485Master::_receiveMode()  { digitalWrite(_dePin, LOW); }
bool RS485Master::sendCommand(uint8_t slaveId, uint8_t cmd, uint16_t timeoutMs) {
    while (_serial.available()) _serial.read(); 
    _transmitMode();
    _serial.printf(":S%d,CMD%d\n", slaveId, cmd);
    _serial.flush();
    _receiveMode();
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (_serial.available()) {
            String resp = _serial.readStringUntil('\n');
            resp.trim();
            if (resp.indexOf(",ACK,") != -1) return true;
        }
    }
    return false;
}

String RS485Master::readResponse(uint16_t timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (_serial.available()) {
            String line = _serial.readStringUntil('\n');
            line.trim();
            if (line.startsWith(":S")) return line;
        }
    }
    return "";
}
// Parse cho SLAVE1 (CH4)
bool RS485Master::parseCH4(const String& line, float& ch4) {
    if (!_initialized) {
        ch4 = 0;
        return false;
    }
    if (!line.startsWith(":S1,DATA,")) return false;
    ch4 = line.substring(9).toFloat();
    return true;
}
// Parse cho SLAVE2 (MICS)
bool RS485Master::parseMICS(const String& line, float& co, float& alc, float& nh3, float& h2) {
    if (!_initialized) {
        co = alc = nh3 = h2 = 0;
        return false;
    }
    if (!line.startsWith(":S2,DATA,")) return false;
    String data = line.substring(9);
    int p1 = data.indexOf(',');
    int p2 = data.indexOf(',', p1 + 1);
    int p3 = data.indexOf(',', p2 + 1);
    if (p3 == -1) return false;
    co = data.substring(0, p1).toFloat();
    alc = data.substring(p1 + 1, p2).toFloat();
    nh3 = data.substring(p2 + 1, p3).toFloat();
    h2 = data.substring(p3 + 1).toFloat();
    return true;
}