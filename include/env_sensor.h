#ifndef ENV_SENSOR_H
#define ENV_SENSOR_H

#include <Arduino.h>

/**
 * @brief 初始化环境传感器 (火焰 + MQ-2 + DHT22)
 */
void envSensorInit();

/**
 * @brief 检测火焰 (0 = 有火)
 * @return true 检测到火焰
 */
bool detectFire();

/**
 * @brief 读取烟雾值 (MQ-2)
 * @return 0-4095 模拟值, 值越大烟雾越浓
 */
uint16_t readSmoke();

/**
 * @brief 烟雾是否超标
 * @return true 烟雾超标
 */
bool isSmokeAlarm();

/**
 * @brief 读取温度 (°C)
 * @return 温度值
 */
float readTemperature();

/**
 * @brief 读取湿度 (%)
 * @return 湿度值
 */
float readHumidity();

#endif