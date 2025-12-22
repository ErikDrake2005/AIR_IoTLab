#include "StateMachine.h"
#include "Measurement.h"
#include "RelayController.h"
#include "UARTCommander.h"
#include "TimeSync.h"
#include <ArduinoJson.h>
#include "CRC32.h"

// Cấu hình thời gian (ms)
const unsigned long TIME_STABILIZE = 60000;    
const unsigned long TIME_MEASURE_1 = 180000;   
const unsigned long TIME_MEASURE_2 = 480000;   
const unsigned long TIME_MEASURE_3 = 900000;   

StateMachine::StateMachine(Measurement& meas, RelayController& relay, UARTCommander& cmd, TimeSync& timeSync)
    : _meas(meas), _relay(relay), _cmd(cmd), _timeSync(timeSync) {
    
    _mode = MANUAL; // [DEFAULT]
    _cycleState = STATE_IDLE;
    
    _measuresPerDay = DEFAULT_MEASURES_PER_DAY;
    _calculateGridInterval();
    _schedules.clear();
}

void StateMachine::begin() {
    Serial.println("[StateMachine] Started. Mode: MANUAL");
    // Gửi yêu cầu đồng bộ giờ khi khởi động
    _sendResponse(_json.createTimeSyncRequest());
}

// --- LOGIC LOOP CHÍNH ---
void StateMachine::update() {
    // 1. SMART BUSY CHECK: 
    if (_cycleState != STATE_IDLE) {
        _processCycleLogic();
        return; 
    }

    // 2. Nếu RẢNH (IDLE) và đang AUTO -> Kiểm tra lịch
    if (_mode == AUTO) {
        static unsigned long lastTriggerEpoch = 0;
        unsigned long currentEpoch = _timeSync.getCurrentTime();

        // Chỉ check 1 lần mỗi giây để tiết kiệm CPU và tránh trigger kép
        if (currentEpoch > 100000 && currentEpoch != lastTriggerEpoch) {
            
            // Ưu tiên: Check lịch cụ thể trước, sau đó mới check lưới
            bool trigger = false;
            
            if (_isTimeForScheduledMeasure()) {
                Serial.println("[AUTO] Trigger: Scheduled Time Match!");
                trigger = true;
            } 
            else if (_isTimeForGridMeasure()) {
                Serial.println("[AUTO] Trigger: Grid Cycle Match!");
                trigger = true;
            }

            if (trigger) {
                lastTriggerEpoch = currentEpoch; // Đánh dấu giây này đã xử lý
                _startCycle();
            }
        }
    }
}

// --- XỬ LÝ LỆNH (Input: JSON SẠCH) ---
void StateMachine::handleCommand(const String& cleanJson) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, cleanJson);
    
    if (error) {
        Serial.println("[SM] JSON Parse Error");
        _sendResponse(_json.createError("JSON Error"));
        return;
    }

    // ArduinoJson v7: Kiểm tra tồn tại bằng cách check null hoặc check type
    if (!doc["type"].is<const char*>()) return;

    const char* type = doc["type"];
    Serial.printf("[INFO] Cmd: %s\n", type);

    // --- NHÓM CẤU HÌNH ---
    if (strcmp(type, "set_mode") == 0) {
        // [FIX] Dùng is<String>() thay vì containsKey
        if (doc["mode"].is<String>()) {
            String m = doc["mode"];
            if (m == "manual") {
                _mode = MANUAL;
                _stopAndResetCycle(); // Dừng ngay nếu đang chạy
                _sendResponse(_json.createAck("MANUAL_OK"));
            } else {
                _mode = AUTO;
                _sendResponse(_json.createAck("AUTO_OK"));
            }
        }
        return;
    }

    if (strcmp(type, "set_cycle") == 0) {
        // [FIX] Dùng is<int>() thay vì containsKey
        if (doc["measures_per_day"].is<int>()) {
            int n = doc["measures_per_day"];
            if (n > 0 && n <= 48) {
                _measuresPerDay = n;
                _calculateGridInterval();
                Serial.printf("[SM] Cycle updated: %d/day\n", _measuresPerDay);
                _sendResponse(_json.createAck("CYCLE_SET_OK"));
            }
        }
        return;
    }

    if (strcmp(type, "set_time") == 0) {
        // [FIX] Dùng is<String>() thay vì containsKey
        if (doc["time"].is<String>()) {
            _parseSetTime(doc["time"].as<String>());
            _sendResponse(_json.createAck("SCHEDULE_ADDED"));
        }
        return;
    }

    if (strcmp(type, "time_sync") == 0) {
        // [FIX] Dùng is<String>() thay vì containsKey
        if (doc["timestamp"].is<String>()) {
            _timeSync.syncFromTimestamp(doc["timestamp"].as<String>());
            _sendResponse(_json.createAck("TIME_SYNCED"));
        }
        return;
    }

    // --- NHÓM ĐIỀU KHIỂN ---
    // [SMART CHECK] Chỉ cho phép Trigger/Relay khi Manual và Rảnh
    
    if (strcmp(type, "trigger_measure") == 0) {
        if (_mode == MANUAL) {
            if (_cycleState == STATE_IDLE) {
                _startCycle();
                _sendResponse(_json.createAck("MEASURE_STARTED"));
            } else {
                _sendResponse(_json.createError("BUSY"));
            }
        } else {
            _sendResponse(_json.createError("IN AUTO MODE"));
        }
        return;
    }

    if (strcmp(type, "stop_measure") == 0) {
        if (_mode == MANUAL) {
            _stopAndResetCycle();
            _sendResponse(_json.createAck("MEASURE_STOPPED"));
        } else {
            _sendResponse(_json.createError("IN AUTO MODE"));
        }
        return;
    }

    // Relay Control
    if (_mode == MANUAL) {
        if (strcmp(type, "open_door") == 0)  { _relay.ON_DOOR();  _sendResponse(_json.createAck("DOOR_OPENED")); return; }
        if (strcmp(type, "close_door") == 0) { _relay.OFF_DOOR(); _sendResponse(_json.createAck("DOOR_CLOSED")); return; }
        if (strcmp(type, "fans_on") == 0)    { _relay.ON_FAN();   _sendResponse(_json.createAck("FAN_ON_OK"));   return; }
        if (strcmp(type, "fans_off") == 0)   { _relay.OFF_FAN();  _sendResponse(_json.createAck("FAN_OFF_OK"));  return; }
    } else {
        if (strcmp(type, "open_door")==0 || strcmp(type, "close_door")==0 || strcmp(type, "fans_on")==0) {
            _sendResponse(_json.createError("IN AUTO MODE"));
        }
    }
}

// --- HELPER LOGIC THỜI GIAN ---

void StateMachine::_calculateGridInterval() {
    if (_measuresPerDay <= 0) _measuresPerDay = 1;
    _gridIntervalSeconds = SECONDS_IN_DAY / _measuresPerDay;
}

void StateMachine::_parseSetTime(String timeStr) {
    int h, m, s;
    if (sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
        // Kiểm tra trùng lặp
        for (const auto& sch : _schedules) {
            if (sch.hour == h && sch.minute == m && sch.second == s) return;
        }
        // Giới hạn số lượng schedule (ví dụ 10) để tránh tràn RAM
        if (_schedules.size() >= 10) _schedules.erase(_schedules.begin());
        
        _schedules.push_back({h, m, s});
        Serial.printf("[SM] Schedule added: %02d:%02d:%02d\n", h, m, s);
    }
}

bool StateMachine::_isTimeForGridMeasure() {
    unsigned long currentEpoch = _timeSync.getCurrentTime();
    if (currentEpoch < 100000) return false;
    return (currentEpoch % _gridIntervalSeconds) < 5;
}

bool StateMachine::_isTimeForScheduledMeasure() {
    unsigned long currentEpoch = _timeSync.getCurrentTime();
    if (currentEpoch < 100000) return false;
    
    time_t raw = (time_t)currentEpoch;
    struct tm* t = localtime(&raw);

    for (const auto& s : _schedules) {
        if (t->tm_hour == s.hour && t->tm_min == s.minute && t->tm_sec == s.second) return true;
    }
    return false;
}

// --- QUY TRÌNH ĐO (MINI-CYCLE) ---

void StateMachine::_startCycle() {
    _cycleState = STATE_PREPARING;
    _cycleStartMillis = millis();
    memset(_miniData, 0, sizeof(_miniData)); // Reset buffer
    
    _relay.OFF_DOOR(); // Đóng cửa
    _relay.ON_FAN();   // Bật quạt
    Serial.println("[SM] Cycle Started.");
}

void StateMachine::_processCycleLogic() {
    unsigned long elapsed = millis() - _cycleStartMillis;

    switch (_cycleState) {
        case STATE_PREPARING:
            _cycleState = STATE_STABILIZING;
            Serial.println("[SM] -> STABILIZING (60s)");
            break;
            
        case STATE_STABILIZING:
            if (elapsed >= TIME_STABILIZE) {
                _cycleState = STATE_WAIT_MEASURE_1;
                Serial.println("[SM] -> WAIT M1");
            }
            break;

        case STATE_WAIT_MEASURE_1:
            if (elapsed >= TIME_MEASURE_1) {
                Serial.println("[SM] Measuring 1/3...");
                _meas.doFullMeasurement(_miniData[0]);
                _cycleState = STATE_WAIT_MEASURE_2;
            }
            break;

        case STATE_WAIT_MEASURE_2:
            if (elapsed >= TIME_MEASURE_2) {
                Serial.println("[SM] Measuring 2/3...");
                _meas.doFullMeasurement(_miniData[1]);
                _cycleState = STATE_WAIT_MEASURE_3;
            }
            break;

        case STATE_WAIT_MEASURE_3:
            if (elapsed >= TIME_MEASURE_3) {
                Serial.println("[SM] Measuring 3/3...");
                _meas.doFullMeasurement(_miniData[2]);
                _cycleState = STATE_FINISHING;
            }
            break;

        case STATE_FINISHING:
            _finishCycle();
            break;
    }
}

void StateMachine::_finishCycle() {
    // Tính trung bình
    float sum[7] = {0}; // CH4, CO, ALC, NH3, H2, TEMP, HUM
    int count = 0;
    
    for(int i=0; i<3; i++) {
        if(_miniData[i].temp != 0) { // Valid check
            sum[0]+=_miniData[i].ch4; sum[1]+=_miniData[i].co; sum[2]+=_miniData[i].alc;
            sum[3]+=_miniData[i].nh3; sum[4]+=_miniData[i].h2;
            sum[5]+=_miniData[i].temp; sum[6]+=_miniData[i].hum;
            count++;
        }
    }
    
    if(count > 0) {
        for(int k=0; k<7; k++) sum[k] /= count;
    }

    String json = _json.createDataJson(sum[0], sum[1], sum[2], sum[3], sum[4], sum[5], sum[6]);
    _sendResponse(json);
    
    Serial.println("[SM] Cycle Done. Data Sent.");
    _stopAndResetCycle();
    
    _relay.OFF_FAN();
    _relay.ON_DOOR();
}

void StateMachine::_stopAndResetCycle() {
    _cycleState = STATE_IDLE;
    _relay.OFF_FAN();  // Tắt quạt
    _relay.OFF_DOOR(); // Đóng cửa
}

void StateMachine::_sendResponse(String json) {
    unsigned long crc = CRC32::calculate(json);
    String packet = json + "|" + String(crc, HEX);
    _cmd.send(packet); 
}