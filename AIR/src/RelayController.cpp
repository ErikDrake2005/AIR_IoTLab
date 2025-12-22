#include "RelayController.h"

RelayController::RelayController() {}

void RelayController::begin() {
    pinMode(CTL_ON_RELAY, OUTPUT);
    pinMode(CTL_OFF_RELAY, OUTPUT);
    pinMode(FANA, OUTPUT);
    pinMode(FAN1, OUTPUT);
    
    // Trạng thái mặc định: Tắt hết
    digitalWrite(CTL_ON_RELAY, LOW); 
    digitalWrite(CTL_OFF_RELAY, LOW);
    digitalWrite(FANA, LOW); 
    digitalWrite(FAN1, LOW);
}

// Hàm cũ giữ nguyên
void RelayController::ON_DOOR() { digitalWrite(CTL_ON_RELAY, HIGH); digitalWrite(CTL_OFF_RELAY, LOW); }
void RelayController::OFF_DOOR() { digitalWrite(CTL_ON_RELAY, LOW); digitalWrite(CTL_OFF_RELAY, HIGH); }
void RelayController::ON_FAN() { digitalWrite(FANA, HIGH); digitalWrite(FAN1, HIGH); }
void RelayController::OFF_FAN() { digitalWrite(FANA, LOW); digitalWrite(FAN1, LOW); }
void RelayController::ON() { ON_DOOR(); OFF_FAN(); }
void RelayController::OFF() { OFF_DOOR(); ON_FAN(); }

// --- HÀM MỚI (IMPLEMENTATION) ---
void RelayController::setFan(bool on) {
    if (on) ON_FAN();
    else OFF_FAN();
}

void RelayController::setDoor(bool closed) {
    if (closed) {
        // Đóng cửa (OFF_DOOR theo định nghĩa cũ của bạn là Đóng cửa)
        OFF_DOOR(); 
    } else {
        // Mở cửa
        ON_DOOR();
    }
}