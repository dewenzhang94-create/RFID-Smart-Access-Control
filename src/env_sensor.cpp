/**
 * env_sensor.cpp - 环境传感器模块
 * 火焰传感器 + MQ-2 烟雾 + DHT22 温湿度
 */

#include "env_sensor.h"
#include <DHT.h>

// --- 引脚定义 ---
#define FLAME_PIN      26    // 火焰传感器 (数字, 0=有火)
#define SMOKE_PIN      34    // MQ-2 烟雾 (ADC1)
#define DHT_PIN        13    // DHT22 温湿度

#define DHT_TYPE       DHT22

// --- 阈值 ---
#define SMOKE_THRESHOLD   3700  // MQ-2 报警阈值 (0-4095, 需预热后校准)

// --- 采样间隔 ---
#define ENV_SAMPLE_MS     5000  // 环境数据采样间隔

static DHT dht(DHT_PIN, DHT_TYPE);

static float    lastTemp  = 0;
static float    lastHum   = 0;
static uint16_t lastSmoke = 0;
static bool     lastFire  = false;
static unsigned long lastEnvSample = 0;

// ============================================
// 初始化
// ============================================

void envSensorInit() {
    pinMode(FLAME_PIN, INPUT_PULLUP);   // 火焰: 内部上拉, 0=有火
    analogReadResolution(12);           // ADC 12位 → 0-4095

    dht.begin();
    Serial.println(F("ENV: Flame + MQ-2 + DHT22 initialized"));
}

// ============================================
// 火焰检测 (低电平触发: 0 = 有火)
// ============================================

bool detectFire() {
    // 软件去抖: 连续两次低电平才确认
    if (digitalRead(FLAME_PIN) == LOW) {
        delay(10);
        if (digitalRead(FLAME_PIN) == LOW) {
            return true;
        }
    }
    return false;
}

// ============================================
// MQ-2 烟雾读取
// ============================================

uint16_t readSmoke() {
    return analogRead(SMOKE_PIN);
}

bool isSmokeAlarm() {
    return (readSmoke() > SMOKE_THRESHOLD);
}

// ============================================
// DHT22 温湿度
// ============================================

float readTemperature() {
    float t = dht.readTemperature();
    if (isnan(t)) return lastTemp;
    lastTemp = t;
    return t;
}

float readHumidity() {
    float h = dht.readHumidity();
    if (isnan(h)) return lastHum;
    lastHum = h;
    return h;
}