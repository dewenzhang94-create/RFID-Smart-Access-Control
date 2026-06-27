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
    STATE_FIRE,          // 火灾: 开锁 + 持续报警
    STATE_LEARN_CARD     // 刷卡录入: 等待刷入新卡
};

static SystemState  state = STATE_IDLE;
static String       currentUid  = "";
static String       currentUser = "";
static unsigned long stateEnterTime = 0;
static unsigned long lastHeartbeat  = 0;
static bool          doorOpen       = false;
static String        learnCardName  = "";

// ============================================
// 辅助函数声明
// ============================================
static String buildJson(String uid, String username, String event);
static String buildLearnJson(String uid, String name);
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

    // 0. 强制静音 (防止上电误响, HIGH=静音)
    pinMode(25, OUTPUT);
    digitalWrite(25, HIGH);

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

    // 8. MQTT 初始化并连接 + 注册远程指令回调 + 用户同步回调
    mqttInit();
    onRemoteCommand([](const char* cmd) {
        Serial.printf("MAIN: Remote command received: %s\n", cmd);

        // --- 远程门禁控制 ---
        if (strcmp(cmd, "unlock") == 0) {
            servoUnlock();
            doorOpen = true;
            successBeep();
            String doorMsg = buildJson("", "REMOTE", "door_open");
            String doorEnc = encryptData(doorMsg);
            publishMessage(TOPIC_DOOR, doorEnc.c_str());
            state = STATE_AUTHENTICATED;
            stateEnterTime = millis();
        } else if (strcmp(cmd, "lock") == 0) {
            servoLock();
            doorOpen = false;
            String doorMsg = buildJson("", "REMOTE", "door_close");
            String doorEnc = encryptData(doorMsg);
            publishMessage(TOPIC_DOOR, doorEnc.c_str());
            resetToIdle();
        } else if (strcmp(cmd, "alarm_test") == 0) {
            alarm();

        // --- 用户白名单同步 (服务器推送) ---
        } else if (strncmp(cmd, "sync_user_add:", 14) == 0) {
            // 格式: sync_user_add:FE320102:ZDW
            char buf[64];
            strncpy(buf, cmd, 63);
            char* uid  = buf + 14;   // 跳过 "sync_user_add:"
            char* name = strchr(uid, ':');
            if (name) {
                *name = '\0';
                name++;
                rfidAddUser(uid, name);
                showMessage("User Added", uid);
            }
        } else if (strncmp(cmd, "sync_user_del:", 14) == 0) {
            // 格式: sync_user_del:FE320102
            char buf[32];
            strncpy(buf, cmd + 14, 31);
            rfidDelUser(buf);
            showMessage("User Removed", buf);

        // --- 完整白名单下发 (list命令) ---
        } else if (strcmp(cmd, "sync_user_list") == 0) {
            showMessage("Syncing...", "User List");

        // --- 刷卡录入模式 ---
        } else if (strncmp(cmd, "learn:", 6) == 0) {
            learnCardName = String(cmd + 6);
            state = STATE_LEARN_CARD;
            stateEnterTime = millis();
            showMessage("Scan Card For", learnCardName);
        }
    });

    // 注册用户同步回调: 接收服务器下发的完整白名单 JSON
    onUsersSync([](const char* json) {
        int count = rfidLoadFromJson(json);
        char msg[32];
        snprintf(msg, 32, "%d Users Loaded", count);
        showMessage("Sync OK!", msg);
    });

    mqttConnect();

    // 9. 上报系统启动事件 + 请求用户白名单同步
    String msg = buildJson("", "SYSTEM", "system_start");
    publishMessage(TOPIC_SYSTEM, msg.c_str());

    // 请求服务器下发完整白名单 (服务器会通过 rfid/users 回复)
    Serial.println(F("MAIN: Requesting user sync from server..."));
    publishMessage(TOPIC_CMD, "sync_users");

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
                doorOpen = false;
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
                digitalWrite(25, LOW);   // LOW = 响
            } else {
                digitalWrite(25, HIGH);  // HIGH = 静音
            }

            // 火灭后恢复
            if (!detectFire()) {
                Serial.println(F("MAIN: Fire cleared, locking..."));
                servoLock();
                doorOpen = false;
                ledRed(false);
                digitalWrite(25, HIGH);
                resetToIdle();
            }
            break;
        }

        // ========================================
        // 刷卡录入: 读取UID后上报服务器
        // ========================================
        case STATE_LEARN_CARD: {
            if (millis() - stateEnterTime > 30000) {
                showMessage("Learn Timeout");
                delay(1500);
                resetToIdle();
                break;
            }
            if (isNewCardPresent()) {
                String uid = readUID();
                haltCard();
                String learnMsg = buildLearnJson(uid, learnCardName);
                String learnEnc = encryptData(learnMsg);
                publishMessage(TOPIC_SYSTEM, learnEnc.c_str());
                showMessage("Card Registered!", learnCardName);
                successBeep();
                delay(2000);
                state = STATE_IDLE;
                stateEnterTime = millis();
                showMessage("System Ready");
            }
            break;
        }

    } // end switch

    // --- 系统心跳 (含环境数据) ---
    publishSystemHeartbeat();

    // 非报警状态下强制静音 (防止 GPIO 悬空误响)
    if (state != STATE_ALARM && state != STATE_FIRE) {
        digitalWrite(25, HIGH);  // HIGH = 静音
    }

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
        doorOpen = true;

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

        // 立即锁门 (防止门开着时非法闯入)
        if (doorOpen) {
            servoLock();
            doorOpen = false;
            String doorMsg = buildJson(uid, "Unknown", "door_close");
            String doorEnc = encryptData(doorMsg);
            publishMessage(TOPIC_DOOR, doorEnc.c_str());
            Serial.println(F("MAIN: Door locked on unauthorized!"));
        }

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
        doorOpen = true;

        // 持续报警
        digitalWrite(25, LOW);   // LOW = 响
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

        // 烟雾报警开锁疏散
        if (!doorOpen) {
            servoUnlock();
            doorOpen = true;
            String unlockMsg = buildJson("", "SYSTEM", "door_open");
            String unlockEnc = encryptData(unlockMsg);
            publishMessage(TOPIC_DOOR, unlockEnc.c_str());
            Serial.println(F("MAIN: Door unlocked on smoke alarm!"));
        }

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

static String buildLearnJson(String uid, String name) {
    String json = "{";
    json += "\"uid\":\""     + uid  + "\",";
    json += "\"username\":\"" + name + "\",";
    json += "\"event\":\"card_learned\",";
    json += "\"time\":\""     + getTimestamp() + "\"";
    json += "}";
    return json;
}

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
    json += "\"door\":"   + String(doorOpen ? "true" : "false") + ",";
    json += "\"human\":"  + String(detectHuman() ? "true" : "false");
    json += "}";
    return json;
}

// ============================================
// 重置到空闲状态
// ============================================

static void resetToIdle() {
    // 强制锁门 (安全兜底, 无论从哪个状态退回都必须锁)
    if (doorOpen) {
        servoLock();
        doorOpen = false;
        String msg = buildJson(currentUid, currentUser, "door_close");
        String encrypted = encryptData(msg);
        publishMessage(TOPIC_DOOR, encrypted.c_str());
        Serial.println(F("MAIN: Door locked by resetToIdle"));
    }
    state = STATE_IDLE;
    stateEnterTime = millis();
    currentUid = "";
    currentUser = "";
    showMessage("System Ready");
    ledGreen(false);
    ledRed(false);
    digitalWrite(25, HIGH);
    Serial.println(F("MAIN: -> IDLE"));
}