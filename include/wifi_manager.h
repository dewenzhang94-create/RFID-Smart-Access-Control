#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

/**
 * @brief 连接 WiFi 并同步 NTP 时间 (阻塞直到成功)
 */
void connectWifi();

/**
 * @brief 检查 WiFi 连接状态
 * @return true 已连接
 */
bool isWifiConnected();

/**
 * @brief 获取当前 ISO 8601 格式时间戳 "YYYY-MM-DDTHH:MM:SS"
 * @return 时间字符串
 */
String getTimestamp();

#endif
