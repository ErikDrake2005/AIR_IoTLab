#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector> // Dùng vector cho danh sách hẹn giờ linh hoạt
#include "Config.h"
#include "MeasurementData.h"
#include "JsonFormatter.h"

// Forward declarations
class Measurement;
class RelayController;
class UARTCommander;
class TimeSync;

class StateMachine {
public:
    StateMachine(Measurement& meas, RelayController& relay, UARTCommander& cmd, TimeSync& timeSync);
    
    void begin();
    void update(); 
    
    // Lưu ý: Input bây giờ là JSON sạch (đã check CRC ở main)
    void handleCommand(const String& cleanJson); 

private:
    enum Mode { AUTO, MANUAL };
    
    enum CycleState { 
        STATE_IDLE,             
        STATE_PREPARING,        
        STATE_STABILIZING,      
        STATE_WAIT_MEASURE_1,   
        STATE_WAIT_MEASURE_2,   
        STATE_WAIT_MEASURE_3,   
        STATE_FINISHING         
    };

    Mode _mode; 
    CycleState _cycleState; 

    // --- QUẢN LÝ THỜI GIAN (SMART CHECK) ---
    int _measuresPerDay;
    unsigned long _gridIntervalSeconds; // Khoảng cách giây giữa các lần đo lưới
    
    struct ScheduleTime {
        int hour;
        int minute;
        int second;
    };
    std::vector<ScheduleTime> _schedules; // Danh sách các mốc giờ set_time

    // Biến đo đạc
    unsigned long _cycleStartMillis;     
    MeasurementData _miniData[3];            

    // References
    Measurement& _meas;
    RelayController& _relay;
    UARTCommander& _cmd;
    TimeSync& _timeSync;
    JsonFormatter _json;

    // Helper Functions
    void _calculateGridInterval();      // Tính lại interval khi set_cycle
    void _parseSetTime(String timeStr); // Xử lý chuỗi "HH:MM:SS"
    
    // Logic kiểm tra thời gian
    bool _isTimeForGridMeasure();       // Kiểm tra lưới (set_cycle)
    bool _isTimeForScheduledMeasure();  // Kiểm tra lịch (set_time)

    // Quy trình đo
    void _startCycle();
    void _processCycleLogic();
    void _finishCycle();        
    void _stopAndResetCycle();  
    
    void _sendResponse(String json);
};