#pragma once
#include <Arduino.h>

struct NodeKey {
    const char* id;
    const char* key;
};
// --- DANH SÁCH CÁC NODE VÀ KEY RIÊNG --- (16 ký tự mỗi key)
const NodeKey NODE_KEYS[] = {
    {"AIR_VL_01", "IoTLab@AIRNode01"}, 
    {"AIR_VL_02", "IoTLab@AIRNode02"},
    {"AIR_VL_03", "IoTLab@AIRNode03"},
    {"AIR_VL_04", "IoTLab@AIRNode04"},
    {"AIR_VL_05", "IoTLab@AIRNode05"}
};
const int NUM_NODES = sizeof(NODE_KEYS) / sizeof(NodeKey);
class KeyStore {
public:
    static const char* getKeyById(const char* deviceId) {
        for (int i = 0; i < NUM_NODES; i++) {
            if (strcmp(NODE_KEYS[i].id, deviceId) == 0) {
                return NODE_KEYS[i].key;
            }
        }
        return nullptr;
    }

    static const NodeKey* getNodeByIndex(int index) {
        if (index >= 0 && index < NUM_NODES) {
            return &NODE_KEYS[index];
        }
        return nullptr;
    }
};