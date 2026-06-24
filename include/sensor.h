#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

/**
 * @brief 初始化传感器引脚 (PIR 人体红外)
 */
void sensorInit();

/**
 * @brief 检测是否有人靠近 (HC-SR501)
 * @return true 有人, false 无人
 */
bool detectHuman();

#endif