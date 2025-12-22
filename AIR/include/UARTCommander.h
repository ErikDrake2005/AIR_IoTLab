#ifndef UART_COMMANDER_H
#define UART_COMMANDER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class UARTCommander {
public:
    UARTCommander();
    
    // Static instance để truy cập từ bất cứ đâu (Singleton pattern cũ của bạn)
    static UARTCommander* instance;

    // [MODIFIED] Nhận tham chiếu Serial thay vì tự khởi tạo
    void begin(HardwareSerial& serial);
    
    // Gửi lệnh xuống Bridge
    void send(const String& data);

    // --- CÁC HÀM CŨ ĐƯỢC KHÔI PHỤC ---
    bool hasCommand();              // Kiểm tra có lệnh trong queue nội bộ không
    String getCommand();            // Lấy lệnh từ queue nội bộ
    void clearCommand();            // Xóa lệnh
    void uartISR();                 // Hàm xử lý ngắt (giữ lại vỏ để tương thích)
    
    // Hàm đẩy dữ liệu vào Queue (để main task có thể đẩy dữ liệu vào đây nếu cần)
    void pushToQueue(String data); 

private:
    HardwareSerial* _serial;       // Con trỏ tới Serial thực tế
    QueueHandle_t _commandQueue;   // Queue nội bộ của class này
    String _rxBuffer;              // Buffer đệm
};

#endif