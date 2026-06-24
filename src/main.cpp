/**
 * main.cpp - RFID 智能门禁系统主程序
 *
 * 系统流程:
 *   人体检测 -> OLED "请刷卡" -> RFID 读卡 -> 身份认证
 *     ├─ 合法用户 -> 舵机开锁 -> 蜂鸣器成功 -> AES加密 -> MQTT上传
 *     └─ 非法卡   -> 蜂鸣器报警           -> AES加密 -> MQTT上传
 *   环境监测: 火焰/烟雾/温湿度 (始终运行)
 *
 * 硬件: ESP32-WROOM-32E + MFRC522 + SSD1306 + HC-SR501 + SG90
 *       火焰传感器 + MQ-2 烟雾 + DHT22 温湿度
 */

#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "mqtt.h"
#include "rfid.h"
#include "sensor.h"
#include "oled.h"
#include "buzzer.h"
#include "crypto.h"
#include "servo.h"
#include "env_sensor.h"

// ============================================
// 系统状态枚举
// ============================================

enum SystemState {
    STATE_IDLE,          // 空闲, 等待人员靠近
    STATE_WAIT_CARD,     // 有人靠近, 等待刷卡
    STATE_AUTHENTICATED, // 认证通过
    STATE_ALARM,         // 报警状态
    STATE_FIRE           // 火灾: 开锁 + 持续报警
};

static SystemState  state = STATE_IDLE;
static String       currentUid  = "";
static String       currentUser = "";
static unsigned long stateEnterTime = 0;
static unsigned long lastHeartbeat  = 0;

// ============================================
// 辅助函数声明
// ============================================
static String buildJson(String uid, String username, String event);
static String buildEnvJson(float temp, float hum, uint16_t smoke, bool fire);
static void   handleCardAuth();
static void   handleEnvMonitor();
static void   publishSystemHeartbeat();
static void   resetToIdle();

// ============================================
// setup() - 系统初始化
// ============================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("\n========================================"));
    Serial.println(F("  RFID Smart Access Control System"));
    Serial.println(F("  ESP32 + MFRC522 + MQTT + Servo + Env"));
    Serial.println(F("========================================\n"));

    // 1. OLED 显示初始化
    oledInit();
    showMessage("System Booting...");
    Serial.println(F("MAIN: OLED initialized"));

    // 2. 传感器初始化
    sensorInit();

    // 3. RFID 初始化
    rfidInit();

    // 4. 蜂鸣器 + LED 初始化
    buzzerInit();

    // 5. 舵机初始化 (默认上锁)
    servoInit();

    // 6. 环境传感器初始化 (火焰 + 烟雾 + DHT22)
    envSensorInit();

    successBeep();  // 启动提示

    // 7. WiFi 连接 + NTP 时间同步
    connectWifi();

    // 8. MQTT 初始化并连接 + 注册远程指令回调
    mqttInit();
    onRemoteCommand([](const char* cmd) {
        Serial.printf("MAIN: Remote command received: %s\n", cmd);
        if (strcmp(cmd, "unlock") == 0) {
            servoUnlock();
            successBeep();
            // 上报远程开门事件到前端
            String doorMsg = buildJson("", "REMOTE", "door_open");
            String doorEnc = encryptData(doorMsg);
            publishMessage(TOPIC_DOOR, doorEnc.c_str());
            state = STATE_AUTHENTICATED;
            stateEnterTime = millis();
        } else if (strcmp(cmd, "lock") == 0) {
            servoLock();
            // 上报远程锁门事件
            String doorMsg = buildJson("", "REMOTE", "door_close");
            String doorEnc = encryptData(doorMsg);
            publishMessage(TOPIC_DOOR, doorEnc.c_str());
            resetToIdle();
        } else if (strcmp(cmd, "alarm_test") == 0) {
            alarm();
        }
    });
    mqttConnect();

    // 9. 上报系统启动事件
    String msg = buildJson("", "SYSTEM", "system_start");
    publishMessage(TOPIC_SYSTEM, msg.c_str());

    // 10. 进入空闲状态
    state = STATE_IDLE;
    stateEnterTime = millis();
    showMessage("System Ready");
    Serial.println(F("MAIN: System ready\n"));
}

// ============================================
// loop() - 主循环
// ============================================

void loop() {
    // MQTT 心跳维持
    mqttLoop();

    // --- 环境监测 (始终运行, 火灾时强制切状态) ---
    handleEnvMonitor();

    // --- 根据系统状态执行不同逻辑 ---
    switch (state) {

        // ========================================
        // 空闲: 等待人员靠近
        // ========================================
        case STATE_IDLE: {
            if (detectHuman()) {
                state = STATE_WAIT_CARD;
                stateEnterTime = millis();
                showMessage("Welcome!", "Please Swipe Card");
                Serial.println(F("MAIN: Human detected, waiting for card..."));
            }
            break;
        }

        // ========================================
        // 等待刷卡: 人体检测 + RFID 读卡
        // ========================================
        case STATE_WAIT_CARD: {
            // 人离开超时 -> 退回空闲
            if (!detectHuman() &&
                (millis() - stateEnterTime > HUMAN_TIMEOUT)) {
                resetToIdle();
                break;
            }

            // 检测到卡片
            if (isNewCardPresent()) {
                handleCardAuth();
            }
            break;
        }

        // ========================================
        // 认证通过: 开锁, 绿灯, 超时自动上锁
        // ========================================
        case STATE_AUTHENTICATED: {
            ledGreen(true);

            // 超时上锁
            if (millis() - stateEnterTime > AUTH_TIMEOUT) {
                Serial.println(F("MAIN: Auth timeout, locking..."));
                servoLock();
                ledGreen(false);

                // 上报锁门事件
                String msg = buildJson(currentUid, currentUser, "door_close");
                String encrypted = encryptData(msg);
                publishMessage(TOPIC_DOOR, encrypted.c_str());

                resetToIdle();
                break;
            }
            break;
        }

        // ========================================
        // 报警状态: 短暂报警后自动恢复
        // ========================================
        case STATE_ALARM: {
            if (millis() - stateEnterTime > 3000) {
                resetToIdle();
            }
            break;
        }

        // ========================================
        // 火灾: 保持开锁 + 持续报警
        // ========================================
        case STATE_FIRE: {
            ledRed(true);

            // 持续蜂鸣 (500ms 间隔)
            if (millis() % 1000 < 500) {
                digitalWrite(25, HIGH);
            } else {
                digitalWrite(25, LOW);
            }

            // 火灭后恢复
            if (!detectFire()) {
                Serial.println(F("MAIN: Fire cleared, locking..."));
                servoLock();
                ledRed(false);
                digitalWrite(25, LOW);
                resetToIdle();
            }
            break;
        }

    } // end switch

    // --- 系统心跳 (含环境数据) ---
    publishSystemHeartbeat();

    delay(50);  // 主循环频率 ~20Hz
}

// ============================================
// 读卡 + 认证处理
// ============================================

static void handleCardAuth() {
    String uid = readUID();
    Serial.printf("MAIN: Card detected, UID = %s\n", uid.c_str());

    if (checkUser(uid)) {
        // === 合法用户 ===
        currentUid  = uid;
        currentUser = getUsername(uid);

        Serial.printf("MAIN: Authorized - %s\n", currentUser.c_str());

        // OLED 显示欢迎
        showMessage("Access Granted!", currentUser);

        // 舵机开锁
        servoUnlock();

        // 蜂鸣器成功提示
        successBeep();

        // 构建 JSON 并加密上传
        String msg = buildJson(uid, currentUser, "login");
        Serial.printf("MAIN: Plain JSON = %s\n", msg.c_str());

        String hash = generateHash(msg);
        Serial.printf("MAIN: SHA256 = %s\n", hash.c_str());

        String encrypted = encryptData(msg);
        Serial.printf("MAIN: AES Encrypted = %s\n", encrypted.c_str());

        publishMessage(TOPIC_AUTH, encrypted.c_str());

        // 上报开门事件
        String doorMsg = buildJson(uid, currentUser, "door_open");
        String doorEnc = encryptData(doorMsg);
        publishMessage(TOPIC_DOOR, doorEnc.c_str());

        state = STATE_AUTHENTICATED;
        stateEnterTime = millis();

    } else {
        // === 非法卡 ===
        Serial.printf("MAIN: UNAUTHORIZED - UID = %s\n", uid.c_str());

        showMessage("Access Denied!", "Unknown Card");

        alarm();

        String msg = buildJson(uid, "Unknown", "unauthorized");
        String encrypted = encryptData(msg);
        publishMessage(TOPIC_ALARM, encrypted.c_str());

        state = STATE_ALARM;
        stateEnterTime = millis();
    }

    // 卡进入休眠
    haltCard();
}

// ============================================
// 环境监测 (火焰/烟雾/温湿度)
// ============================================

static void handleEnvMonitor() {
    static unsigned long lastEnvSample = 0;
    if (millis() - lastEnvSample < 5000) return;  // 5s 采样一次
    lastEnvSample = millis();

    bool fire = detectFire();
    bool smoke = isSmokeAlarm();
    float temp = readTemperature();
    float hum  = readHumidity();
    uint16_t smokeVal = readSmoke();

    // --- 火灾: 最高优先级 ---
    if (fire && state != STATE_FIRE) {
        Serial.println(F("MAIN: !!! FIRE DETECTED !!!"));
        showMessage("FIRE ALARM!", "Door Unlocked!");

        // 紧急开锁
        servoUnlock();

        // 持续报警
        digitalWrite(25, HIGH);
        ledRed(true);

        // MQTT 紧急上报
        String fireMsg = buildEnvJson(temp, hum, smokeVal, true);
        String encrypted = encryptData(fireMsg);
        publishMessage(TOPIC_ALARM, encrypted.c_str());

        state = STATE_FIRE;
        stateEnterTime = millis();
        return;
    }

    // --- 烟雾超标 ---
    if (smoke && !fire && state != STATE_ALARM && state != STATE_FIRE) {
        Serial.printf("MAIN: SMOKE ALARM! value=%d\n", smokeVal);
        showMessage("SMOKE ALARM!", String(smokeVal));
        alarm();

        String smokeMsg = buildEnvJson(temp, hum, smokeVal, false);
        String encrypted = encryptData(smokeMsg);
        publishMessage(TOPIC_ALARM, encrypted.c_str());

        state = STATE_ALARM;
        stateEnterTime = millis();
    }
}

// ============================================
// 系统心跳上报 (含环境数据, 每 HEARTBEAT_INTERVAL ms)
// ============================================

static void publishSystemHeartbeat() {
    if (millis() - lastHeartbeat < HEARTBEAT_INTERVAL) return;

    // 附带环境数据
    float temp = readTemperature();
    float hum  = readHumidity();
    uint16_t smoke = readSmoke();
    bool fire = detectFire();

    String msg = buildEnvJson(temp, hum, smoke, fire);
    String encrypted = encryptData(msg);
    publishMessage(TOPIC_SYSTEM, encrypted.c_str());

    Serial.printf("MAIN: Env T=%.1fC H=%.1f%% Smoke=%d Fire=%d\n",
                  temp, hum, smoke, fire);

    lastHeartbeat = millis();
}

// ============================================
// 构建 JSON 消息
// ============================================

static String buildJson(String uid, String username, String event) {
    String json = "{";
    json += "\"uid\":\""     + uid      + "\",";
    json += "\"username\":\"" + username + "\",";
    json += "\"event\":\""    + event    + "\",";
    json += "\"time\":\""     + getTimestamp() + "\"";
    json += "}";
    return json;
}

static String buildEnvJson(float temp, float hum, uint16_t smoke, bool fire) {
    String json = "{";
    json += "\"uid\":\"\",";
    json += "\"username\":\"" + String(DEVICE_ID) + "\",";
    json += "\"event\":\"heartbeat\",";
    json += "\"time\":\"" + getTimestamp() + "\",";
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"humidity\":" + String(hum, 1) + ",";
    json += "\"smoke\":" + String(smoke) + ",";
    json += "\"fire\":" + String(fire ? "true" : "false") + ",";
    json += "\"human\":" + String(detectHuman() ? "true" : "false");
    json += "}";
    return json;
}

// ============================================
// 重置到空闲状态
// ============================================

static void resetToIdle() {
    state = STATE_IDLE;
    stateEnterTime = millis();
    currentUid = "";
    currentUser = "";
    showMessage("System Ready");
    ledGreen(false);
    ledRed(false);
    digitalWrite(25, LOW);
    Serial.println(F("MAIN: -> IDLE"));
}