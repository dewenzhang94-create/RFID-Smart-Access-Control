/**
 * mqtt.cpp - MQTT 通信模块
 * 基于 PubSubClient, 负责消息发布 + 接收远程指令
 */

#include "mqtt.h"

// --- MQTT 客户端实例 ---
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// --- 远程指令回调 ---
static void (*_cmdCallback)(const char* cmd) = nullptr;

// --- 重连参数 ---
#define MQTT_RETRY_DELAY 5000
#define TOPIC_CMD       "rfid/cmd"

// ============================================
// 内部: 收到订阅消息
// ============================================

static void _onMessage(char* topic, byte* payload, unsigned int length) {
    // 只处理 rfid/cmd
    if (strcmp(topic, TOPIC_CMD) != 0) return;
    if (!_cmdCallback || length == 0) return;

    char cmd[32] = {0};
    unsigned int len = length < 31 ? length : 31;
    memcpy(cmd, payload, len);
    cmd[len] = '\0';

    Serial.printf("MQTT: Received command: [%s] -> %s\n", topic, cmd);
    _cmdCallback(cmd);
}

// ============================================
// 初始化
// ============================================

void mqttInit() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(512);
    mqttClient.setCallback(_onMessage);
    Serial.printf("MQTT: Broker set to %s:%d\n", MQTT_BROKER, MQTT_PORT);
}

// ============================================
// 连接 Broker (阻塞, 自动重连), 订阅命令主题
// ============================================

void mqttConnect() {
    if (mqttClient.connected()) return;

    Serial.printf("MQTT: Connecting to %s...\n", MQTT_BROKER);
    while (!mqttClient.connected()) {
        if (mqttClient.connect(DEVICE_ID)) {
            Serial.println("MQTT: Connected!");
            // 订阅远程指令
            mqttClient.subscribe(TOPIC_CMD);
            Serial.printf("MQTT: Subscribed to %s\n", TOPIC_CMD);
        } else {
            Serial.printf("MQTT: Failed (rc=%d), retrying in %dms...\n",
                          mqttClient.state(), MQTT_RETRY_DELAY);
            delay(MQTT_RETRY_DELAY);
        }
    }
}

// ============================================
// 发布消息 (自动重连)
// ============================================

bool publishMessage(const char* topic, const char* msg) {
    if (!mqttClient.connected()) {
        Serial.println("MQTT: Reconnecting before publish...");
        if (!mqttClient.connect(DEVICE_ID)) {
            Serial.println("MQTT: Reconnect failed, message dropped");
            return false;
        }
        mqttClient.subscribe(TOPIC_CMD);  // 重连后重新订阅
    }

    bool ok = mqttClient.publish(topic, msg);
    if (ok) {
        Serial.printf("MQTT: Published to [%s] -> %s\n", topic, msg);
    } else {
        Serial.printf("MQTT: Publish to [%s] failed!\n", topic);
    }
    return ok;
}

// ============================================
// MQTT 心跳 (必须在 loop() 中周期性调用)
// ============================================

void mqttLoop() {
    mqttClient.loop();
}

// ============================================
// 查询连接状态
// ============================================

bool isMqttConnected() {
    return mqttClient.connected();
}

// ============================================
// 注册远程指令回调
// ============================================

void onRemoteCommand(void (*cb)(const char* cmd)) {
    _cmdCallback = cb;
}