#pragma once
#include <Arduino.h>
#include "MeasurementData.h"  // Struct chung (doc lap)

// Forward declarations de tham chieu (&) ma khong full include (toi uu OOP, tranh circular)
class RS485Master;
class SHT31Sensor;
class JsonFormatter;

class Measurement{
public:
    Measurement(RS485Master &rs485Master, SHT31Sensor &sht31Sensor, JsonFormatter &jsonFormatter);
    bool doFullMeasurement(String &outputJson);  // Overload tra JSON (tuong thich cu)
    bool doFullMeasurement(MeasurementData& data);  // Overload tra floats qua struct (toi uu cho StateMachine)

private:
    RS485Master& _rs485;
    SHT31Sensor& _sht31;
    JsonFormatter& _json;
    bool measureCH4(float &ch4);
    bool measureMICS(float &co, float &alc, float &nh3, float &h2);
    bool measureSHT31(float &temp, float &hum);
};