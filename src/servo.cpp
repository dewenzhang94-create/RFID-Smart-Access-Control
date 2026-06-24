/**
 * servo.cpp - 舵机电子锁模块 (ESP32 原生 LEDC)
 * SG90: 50Hz PWM, 0.5ms=0°, 2.5ms=180°
 */

#include "servo.h"

#define SERVO_PIN      14     // 舵机信号脚
#define SERVO_LOCK     0      // 上锁角度
#define SERVO_UNLOCK   90     // 开锁角度

// LEDC 参数
#define LEDC_CHANNEL   0
#define LEDC_FREQ      50     // 50Hz
#define LEDC_RES       16     // 16位精度

// ============================================
// 角度 → 脉宽 (us) → duty
// ============================================
static uint16_t angleToDuty(uint8_t angle) {
    uint16_t pulse = map(angle, 0, 180, 500, 2500);  // us
    return (uint32_t)pulse * 65536 / 20000;           // 20000us 周期, 16位
}

// ============================================
// 初始化
// ============================================

void servoInit() {
    ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RES);
    ledcAttachPin(SERVO_PIN, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, angleToDuty(SERVO_LOCK));
    Serial.println(F("SERVO: Electronic lock initialized (native LEDC)"));
}

// ============================================
// 开锁
// ============================================

void servoUnlock() {
    ledcWrite(LEDC_CHANNEL, angleToDuty(SERVO_UNLOCK));
    Serial.println(F("SERVO: Unlocked"));
}

// ============================================
// 上锁
// ============================================

void servoLock() {
    ledcWrite(LEDC_CHANNEL, angleToDuty(SERVO_LOCK));
    Serial.println(F("SERVO: Locked"));
}