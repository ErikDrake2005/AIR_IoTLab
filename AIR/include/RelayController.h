#pragma once
#include <Arduino.h>
#include "config.h"

class RelayController {
public:
    RelayController();
    void begin();
    
    // Các hàm cũ (giữ lại để tương thích nếu cần)
    void ON_DOOR();
    void OFF_DOOR();
    void ON_FAN();
    void OFF_FAN();
    void ON();
    void OFF();

    // --- CÁC HÀM MỚI CHO STATE MACHINE (FIX LỖI) ---
    void setFan(bool on);      // true = Bật, false = Tắt
    void setDoor(bool closed); // true = Đóng, false = Mở
};