#ifndef SERVO_H
#define SERVO_H

#include <Arduino.h>

/**
 * @brief 初始化舵机 (默认锁定 0°), 使用 ESP32 原生 LEDC
 */
void servoInit();

/**
 * @brief 开锁: 转到 90°
 */
void servoUnlock();

/**
 * @brief 上锁: 回到 0°
 */
void servoLock();

#endif