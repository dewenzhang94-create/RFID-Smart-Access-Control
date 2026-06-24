/**
 * sensor.cpp - 人体红外传感器模块
 * HC-SR501 PIR 传感器
 */

#include "sensor.h"

// --- 引脚定义 ---
#define PIR_SENSOR_PIN   27   // HC-SR501 人体红外

// --- 去抖时间 (ms) ---
#define PIR_DEBOUNCE_MS  200

// ============================================
// 初始化
// ============================================

void sensorInit() {
    pinMode(PIR_SENSOR_PIN, INPUT);
    delay(1000);  // 上电预热
    Serial.println(F("SENSOR: PIR initialized"));
}

// ============================================
// 人体检测 (带软件去抖)
// ============================================

bool detectHuman() {
    static bool lastState = false;
    static unsigned long lastChange = 0;

    bool current = (digitalRead(PIR_SENSOR_PIN) == HIGH);

    if (current != lastState) {
        lastChange = millis();
        lastState = current;
    }

    if (millis() - lastChange > PIR_DEBOUNCE_MS) {
        return current;
    }
    return !current;
}