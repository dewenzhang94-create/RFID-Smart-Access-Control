/**
 * mqtt.cpp - MQTT 通信模块
 * 基于 PubSubClient, 负责消息发布 + 接收远程指令 + 用户白名单同步
 */

#include "mqtt.h"

// --- MQTT 客户端实例 ---
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// --- 回调指针 ---
static void (*_cmdCallback)(const char* cmd)   = nullptr;
static void (*_usersCallback)(const char* json) = nullptr;

// --- 重连参数 ---
#define MQTT_RETRY_DELAY 5000

// --- MQTT 接收缓冲区 (PubSubClient 默认 256B 不够用) ---
#define MQTT_RX_BUF 1024

// ============================================
// 内部: 收到订阅消息
// ============================================

static void _onMessage(char* topic, byte* payload, unsigned int length) {
    // 空消息忽略
    if (length == 0) return;

    // 复制 payload 到 C 字符串
    char buf[MQTT_RX_BUF] = {0};
    unsigned int len = length < (MQTT_RX_BUF - 1) ? length : (MQTT_RX_BUF - 1);
    memcpy(buf, payload, len);

    if (strcmp(topic, TOPIC_CMD) == 0) {
        // === rfid/cmd: 控制指令 ===
        Serial.printf("MQTT: cmd = %s\n", buf);
        if (_cmdCallback) _cmdCallback(buf);

    } else if (strcmp(topic, TOPIC_USERS) == 0) {
        // === rfid/users: 用户白名单同步 ===
        Serial.printf("MQTT: users sync (%d bytes)\n", len);
        if (_usersCallback) _usersCallback(buf);
    }
}

// ============================================
// 初始化
// ============================================

void mqttInit() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(MQTT_RX_BUF);
    mqttClient.setCallback(_onMessage);
    Serial.printf("MQTT: Broker set to %s:%d (buf=%d)\n",
                  MQTT_BROKER, MQTT_PORT, MQTT_RX_BUF);
}

// ============================================
// 连接 Broker + 订阅主题
// ============================================

void mqttConnect() {
    if (mqttClient.connected()) return;

    Serial.printf("MQTT: Connecting to %s...\n", MQTT_BROKER);
    while (!mqttClient.connected()) {
        if (mqttClient.connect(DEVICE_ID)) {
            Serial.println("MQTT: Connected!");
            mqttClient.subscribe(TOPIC_CMD);
            mqttClient.subscribe(TOPIC_USERS);
            Serial.printf("MQTT: Subscribed to %s, %s\n", TOPIC_CMD, TOPIC_USERS);
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
        mqttClient.subscribe(TOPIC_CMD);
        mqttClient.subscribe(TOPIC_USERS);
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
// MQTT 心跳
// ============================================

void mqttLoop() {
    mqttClient.loop();
}

bool isMqttConnected() {
    return mqttClient.connected();
}

// ============================================
// 注册回调
// ============================================

void onRemoteCommand(void (*cb)(const char* cmd)) {
    _cmdCallback = cb;
}

void onUsersSync(void (*cb)(const char* json)) {
    _usersCallback = cb;
}