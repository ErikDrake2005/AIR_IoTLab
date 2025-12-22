#include "StateMachine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h" 
#include "config.h"
#include "JsonFormatter.h"
#include "TimeSync.h"
#include "RelayController.h"
#include "Measurement.h"
#include "UARTCommander.h"
#include "RS485Master.h"
#include "SHT31Sensor.h"
#include "CRC32.h" 

// --- CẤU HÌNH QUEUE & MUTEX ---
typedef struct {
    char payload[512]; 
} UartMessage;

QueueHandle_t commandQueue;   // Queue chính cho StateMachine xử lý lệnh
SemaphoreHandle_t sysMutex;   // Mutex bảo vệ tài nguyên chung

// --- KHỞI TẠO ĐỐI TƯỢNG ---
HardwareSerial rs485Serial(2);
HardwareSerial commandSerial(1); // Giao tiếp với Bridge

RS485Master rs485(rs485Serial);
SHT31Sensor sht31;
RelayController relay;
JsonFormatter jsonFormatter;
Measurement measurement(rs485, sht31, jsonFormatter);

UARTCommander uartCommander; 
TimeSync timeSync(jsonFormatter);
StateMachine stateMachine(measurement, relay, uartCommander, timeSync);
// src/main.cpp

// --- TASK 1: NHẬN UART (Bridge -> Node) ---
// Nhiệm vụ: Nhận Raw -> Check CRC -> Gửi Clean JSON vào Queue
void uartRxTask(void* pvParameters) {
    String inputBuffer = "";
    inputBuffer.reserve(512);

    for (;;) {
        while (commandSerial.available()) {
            char c = (char)commandSerial.read();
            if (c == '\n') {
                inputBuffer.trim(); // Xóa \r\n thừa
                if (inputBuffer.length() > 0) {
                    
                    // 1. Luôn đẩy Raw vào UARTCommander (Để giữ tính năng cũ getCommand nếu cần)
                    uartCommander.pushToQueue(inputBuffer);

                    // 2. Tách CRC để verify
                    int pipeIdx = inputBuffer.lastIndexOf('|');
                    if (pipeIdx != -1) {
                        String jsonPart = inputBuffer.substring(0, pipeIdx);
                        String crcPart = inputBuffer.substring(pipeIdx + 1);
                        
                        // Tính toán CRC
                        unsigned long calcCrc = CRC32::calculate(jsonPart);
                        unsigned long recvCrc = strtoul(crcPart.c_str(), NULL, 16);

                        if (calcCrc == recvCrc) {
                            // CRC OK -> Gửi JSON SẠCH vào Queue cho StateMachine
                            UartMessage msg;
                            // Copy jsonPart (không phải inputBuffer) vào payload
                            strncpy(msg.payload, jsonPart.c_str(), 511);
                            msg.payload[511] = '\0';
                            
                            if (xQueueSend(commandQueue, &msg, 0) != pdTRUE) {
                                Serial.println("[RX] Queue Full - Dropped Packet");
                            }
                        } else {
                            Serial.printf("[RX ERROR] CRC Fail! Calc:%lX != Recv:%lX\n", calcCrc, recvCrc);
                            // Gửi báo lỗi ngược lại Bridge nếu cần (tùy chọn)
                            // uartCommander.send(jsonFormatter.createError("CRC Fail")); 
                        }
                    } else {
                        Serial.println("[RX ERROR] Invalid Format (No CRC separator)");
                    }
                    
                    inputBuffer = "";
                }
            } 
            else if (c != '\r') { 
                if (inputBuffer.length() < 511) inputBuffer += c;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}
// ============================================================
// TASK 2: XỬ LÝ LỆNH (Consume Queue)
// ============================================================
void cmdProcessTask(void* pvParameters) {
    UartMessage rcvMsg;
    for (;;) {
        if (xQueueReceive(commandQueue, &rcvMsg, portMAX_DELAY) == pdTRUE) {
            String rawData = String(rcvMsg.payload);
            
            // [CRITICAL] LOCK Mutex trước khi gọi StateMachine
            // Để đảm bảo không xung đột với quy trình đo đang chạy
            if (xSemaphoreTake(sysMutex, portMAX_DELAY) == pdTRUE) {
                stateMachine.handleCommand(rawData);
                xSemaphoreGive(sysMutex); // UNLOCK
            }
        }
    }
}

// ============================================================
// TASK 3: LOGIC CHÍNH (StateMachine Loop)
// ============================================================
void machineTask(void* pvParameters) {
    for (;;) {
        // [CRITICAL] LOCK Mutex khi update logic đo đạc
        if (xSemaphoreTake(sysMutex, portMAX_DELAY) == pdTRUE) {
            stateMachine.update();
            xSemaphoreGive(sysMutex);
        }
        // Chu kỳ update 100ms là đủ nhạy cho các hẹn giờ
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    // 1. Debug Serial
    Serial.begin(115200);
    
    // 2. Init UART Bridge (Tốc độ cao 460800 khớp với config.h)
    commandSerial.setRxBufferSize(2048);
    commandSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    
    // 3. Init Mutex & Queue
    sysMutex = xSemaphoreCreateMutex();
    commandQueue = xQueueCreate(10, sizeof(UartMessage));

    // 4. Init Modules
    relay.begin();
    rs485.begin();
    sht31.begin();
    
    // Truyền tham chiếu Serial cho UARTCommander (không init lại)
    uartCommander.begin(commandSerial); 
    setCpuFrequencyMhz(80);
    
    // Khởi động StateMachine (Sẽ gửi time_sync request ngay)
    stateMachine.begin();

    // 5. Init RTOS Tasks
    // Stack size: RX (4k), CMD (8k - vì JSON), SM (8k - vì tính toán float)
    xTaskCreatePinnedToCore(uartRxTask, "UART_RX", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(cmdProcessTask, "CMD_PROC", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(machineTask, "SM_LOOP", 8192, NULL, 1, NULL, 1);
    
    Serial.println("[NODE] System Started. Mode: Slave (No ACK Wait).");
}

void loop() {
    vTaskDelete(NULL); 
}