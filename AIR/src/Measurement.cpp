#include "Measurement.h"
#include "RS485Master.h"  // Full include trong .cpp (toi uu, tranh circular)
#include "SHT31Sensor.h"
#include "JsonFormatter.h"

Measurement::Measurement(RS485Master& rs485, SHT31Sensor& sht31, JsonFormatter& json)
    : _rs485(rs485), _sht31(sht31), _json(json) {}

bool Measurement::doFullMeasurement(String& outputJson) {  // Overload cu (tra JSON)
    MeasurementData data;  // Su dung struct chung
    if (!doFullMeasurement(data)) return false;  // Goi overload moi
    outputJson = _json.createDataJson(data.ch4, data.co, data.alc, data.nh3, data.h2, data.temp, data.hum);
    return true;
}

bool Measurement::doFullMeasurement(MeasurementData& data) {  // Overload moi (tra floats, voi retry - toi uu de tranh fail)
    float ch4 = 0, co = 0, alc = 0, nh3 = 0, h2 = 0, temp = 0, hum = 0;
    const int MAX_RETRY = 2;
    bool success = false;

    for (int retry = 0; retry <= MAX_RETRY; ++retry) {
        if (measureCH4(ch4) && measureMICS(co, alc, nh3, h2) && measureSHT31(temp, hum)) {
            success = true;
            break;
        }
        if (retry < MAX_RETRY) {
            Serial.println("[Measurement] Do that bai - retry...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }

    if (success) {
        data.ch4 = ch4; data.co = co; data.alc = alc;
        data.nh3 = nh3; data.h2 = h2; data.temp = temp; data.hum = hum;
        Serial.printf("[Measurement] Do thanh cong: CH4=%.2f, CO=%.2f, Alc=%.2f, NH3=%.2f, H2=%.2f, Temp=%.2f, Hum=%.2f\n", ch4, co, alc, nh3, h2, temp, hum);  // Log de debug
    } else {
        Serial.println("[Measurement] Do that bai hoan toan - mac dinh 0");
        // data da =0 tu init
    }
    return success;
}

bool Measurement::measureCH4(float& ch4) {
    if (!_rs485.sendCommand(1, 2)) return false;
    String resp = _rs485.readResponse(3000);
    return _rs485.parseCH4(resp, ch4);
}

bool Measurement::measureMICS(float& co, float& alc, float& nh3, float& h2) {
    if (!_rs485.sendCommand(2, 2)) return false;
    String resp = _rs485.readResponse(3000);
    return _rs485.parseMICS(resp, co, alc, nh3, h2);
}

bool Measurement::measureSHT31(float& temp, float& hum) {
    return _sht31.readData(temp, hum);
}