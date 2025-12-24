#pragma once
#include <Arduino.h>
#include <WiFiManager.h> // Đảm bảo cài thư viện WiFiManager by tzapu

class WiFiConfig {
public:
    WiFiConfig();
    void begin();          // Khởi tạo ban đầu
    void process();        // Hàm xử lý trong vòng lặp (loop)
    void resetSettings();  // Xóa WiFi đã lưu

    bool isConnected();    // Kiểm tra nhanh trạng thái

private:
    WiFiManager _wifiManager;
    bool _portalRunning = false;
    unsigned long _lastWifiCheck = 0;
    const unsigned long _checkInterval = 5000; // Kiểm tra mạng mỗi 5s
    unsigned long _lastReconnectAttempt = 0;
};