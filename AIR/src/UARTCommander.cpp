#include "UARTCommander.h"

// Khởi tạo Singleton
UARTCommander* UARTCommander::instance = nullptr;

UARTCommander::UARTCommander() : _serial(nullptr) {
    instance = this;
    // Tạo Queue nội bộ (giữ lại logic cũ của bạn)
    _commandQueue = xQueueCreate(20, 512); // 20 msg, mỗi msg 512 bytes
    _rxBuffer = "";
}

void UARTCommander::begin(HardwareSerial& serial) {
    _serial = &serial; 
    
    // [QUAN TRỌNG]: Không gọi serial.begin() ở đây nữa vì main.cpp đã gọi rồi.
    // Tránh lỗi "Double Initialization" làm treo ESP32.
    
    Serial.println("[UARTCommander] Initialized (Full Mode).");
}

void UARTCommander::send(const String& data) {
    if (_serial) {
        _serial->println(data);
        // Debug: Serial.printf("[TX->Bridge] %s\n", data.c_str());
    }
}

// --- CÁC HÀM CŨ ĐƯỢC KHÔI PHỤC ---

// Main Task có thể gọi hàm này để đẩy data vào UARTCommander
// giúp các hàm getCommand/hasCommand hoạt động đúng.
void UARTCommander::pushToQueue(String data) {
    if (xQueueSend(_commandQueue, data.c_str(), 0) != pdTRUE) {
        Serial.println("[UARTCommander] Queue Full!");
    }
}

bool UARTCommander::hasCommand() {
    return uxQueueMessagesWaiting(_commandQueue) > 0;
}

String UARTCommander::getCommand() {
    char buffer[512];
    if (xQueueReceive(_commandQueue, &buffer, 0) == pdTRUE) {
        return String(buffer);
    }
    return "";
}

void UARTCommander::clearCommand() {
    xQueueReset(_commandQueue);
}

// Giữ lại hàm này để tương thích code cũ, nhưng KHÔNG dùng attachInterrupt/onReceive
// vì main.cpp đã có Task riêng để đọc Serial (an toàn hơn ISR).
void UARTCommander::uartISR() {
    // Để trống hoặc xử lý logic phụ nếu cần.
    // Việc đọc dữ liệu thực tế đã được chuyển sang uartRxTask ở main.cpp
}