#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

/**
 * @brief 初始化蜂鸣器 + LED 引脚
 */
void buzzerInit();

/**
 * @brief 报警: 红灯闪烁 + 蜂鸣器三声
 */
void alarm();

/**
 * @brief 成功提示: 绿灯亮 + 蜂鸣器短鸣一声
 */
void successBeep();

/**
 * @brief 控制绿色 LED
 * @param on true 亮, false 灭
 */
void ledGreen(bool on);

/**
 * @brief 控制红色 LED
 * @param on true 亮, false 灭
 */
void ledRed(bool on);

#endif
