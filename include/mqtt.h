#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"

/**
 * @brief 初始化 MQTT 客户端
 */
void mqttInit();

/**
 * @brief 连接 MQTT Broker (阻塞直到成功)
 */
void mqttConnect();

/**
 * @brief 发布消息到指定 Topic
 * @param topic MQTT Topic
 * @param msg JSON 格式消息
 * @return true 发布成功
 */
bool publishMessage(const char* topic, const char* msg);

/**
 * @brief 处理 MQTT 心跳 (需在主循环中调用)
 */
void mqttLoop();

/**
 * @brief 查询 MQTT 连接状态
 * @return true 已连接
 */
bool isMqttConnected();

/**
 * @brief 注册收到远程指令时的回调
 * @param cb 回调函数, 参数为指令字符串 (如 "unlock", "lock", "alarm_test")
 */
void onRemoteCommand(void (*cb)(const char* cmd));

#endif
