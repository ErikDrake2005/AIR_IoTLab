#include "WiFiConfig.h"
#include "config.h" 

WiFiConfig::WiFiConfig() {
    // Constructor để trống
}

void WiFiConfig::begin() {
    pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);
    if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
        Serial.println("[WiFi] Boot Button Held! Starting Config Portal...");
        _wifiManager.resetSettings();
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