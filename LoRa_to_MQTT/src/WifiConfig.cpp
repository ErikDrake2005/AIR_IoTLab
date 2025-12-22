#include "WiFiConfig.h"
#include "config.h" 

WiFiConfig::WiFiConfig() {
    // Constructor để trống
}

void WiFiConfig::begin() {
    // Đảm bảo nút được cấu hình Input Pullup
    pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);

    // --- KIỂM TRA NÚT BOOT KHI CẤP ĐIỆN ---
    // Nếu nút đang được giữ (LOW) -> Vào chế độ cấu hình
    if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
        Serial.println("[WiFi] Boot Button Held! Starting Config Portal...");
        
        // 1. Xóa cấu hình cũ để sạch sẽ
        _wifiManager.resetSettings();
        
        // 2. Bật Web Portal (Code sẽ dừng tại đây chờ config xong)
        bool res = _wifiManager.startConfigPortal(WIFI_SSID, WIFI_PASS);
        
        if (res) {
            Serial.println("[WiFi] Config Saved! Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("[WiFi] Config Failed/Timeout.");
        }
    } 
    else {
        Serial.println("[WiFi] Normal Boot. Connecting...");
        _wifiManager.setEnableConfigPortal(false);
        _wifiManager.autoConnect(); 
    }
}

void WiFiConfig::process() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        // 10000ms = 10 giây
        if (now - _lastReconnectAttempt > 10000) { 
            _lastReconnectAttempt = now;
            Serial.println("[WiFi] Connection lost. Attempting Reconnect...");
            WiFi.reconnect(); 
        }
    }
}

bool WiFiConfig::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}