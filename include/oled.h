#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

/**
 * @brief 初始化 OLED 显示屏 (SSD1306, I2C)
 */
void oledInit();

/**
 * @brief 显示单行消息 (顶部对齐)
 * @param msg 显示内容
 */
void showMessage(String msg);

/**
 * @brief 显示两行消息
 * @param line1 第一行
 * @param line2 第二行
 */
void showMessage(String line1, String line2);

/**
 * @brief 显示三行消息
 * @param line1 第一行
 * @param line2 第二行
 * @param line3 第三行
 */
void showMessage(String line1, String line2, String line3);

/**
 * @brief 清空屏幕
 */
void clearDisplay();

#endif
