/**
 * rfid.cpp - RFID 读写模块 (双重认证: 动态白名单 + 默认回退)
 *
 * 认证策略:
 *   1. 优先查动态白名单 (从服务器 MQTT 同步, 可远程增删)
 *   2. 未匹配时回退到出厂默认用户 (断网也能用)
 *
 * 使用 MFRC522 通过 SPI 读取 IC 卡 UID
 */

#include "rfid.h"
#include <ArduinoJson.h>

// --- 引脚定义 ---
#define RFID_SS    5
#define RFID_RST   4
#define RFID_SCK   18
#define RFID_MISO  19
#define RFID_MOSI  23

// --- MFRC522 实例 ---
MFRC522 mfrc522(RFID_SS, RFID_RST);

// --- 动态白名单 (从服务器同步) ---
struct AuthUser {
    char uid[16];
    char username[32];
};

static AuthUser localUsers[MAX_LOCAL_USERS];
static int      localUserCount = 0;

// --- 出厂默认用户 (硬回退, 保证断网可用) ---
struct DefaultUser {
    const char* uid;
    const char* username;
};

static const DefaultUser defaultUsers[] = {
    {"63A24B9F", "张三"},
    {"A1B2C3D4", "李四"},
    {"F1E2D3C4", "王五"},
    {"FE320102", "ZDW"},
    {nullptr, nullptr}
};

// ============================================
// 内部辅助
// ============================================

static int findInDynamic(const char* uid) {
    for (int i = 0; i < localUserCount; i++) {
        if (strcasecmp(localUsers[i].uid, uid) == 0) {
            return i;
        }
    }
    return -1;
}

static int findInDefaults(const char* uid) {
    for (int i = 0; defaultUsers[i].uid != nullptr; i++) {
        if (strcasecmp(defaultUsers[i].uid, uid) == 0) {
            return i;
        }
    }
    return -1;
}

// ============================================
// 公共函数: 初始化
// ============================================

void rfidInit() {
    SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
    mfrc522.PCD_Init();
    delay(50);
    mfrc522.PCD_DumpVersionToSerial();
    Serial.println(F("RFID: MFRC522 initialized"));

    // 加载出厂默认用户作为回退
    rfidLoadDefaults();
}

// ============================================
// 刷卡
// ============================================

bool isNewCardPresent() {
    if (!mfrc522.PICC_IsNewCardPresent()) return false;
    if (!mfrc522.PICC_ReadCardSerial())   return false;
    return true;
}

String readUID() {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

void haltCard() {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

// ============================================
// 双重认证
// ============================================

bool checkUser(String uid) {
    // 1. 优先查动态白名单
    if (findInDynamic(uid.c_str()) >= 0) return true;
    // 2. 回退查默认用户
    if (findInDefaults(uid.c_str()) >= 0) return true;
    return false;
}

String getUsername(String uid) {
    // 1. 查动态白名单
    int idx = findInDynamic(uid.c_str());
    if (idx >= 0) return String(localUsers[idx].username);
    // 2. 查默认用户
    int didx = findInDefaults(uid.c_str());
    if (didx >= 0) return String(defaultUsers[didx].username);
    return String("Unknown");
}

// ============================================
// 动态白名单操作
// ============================================

void rfidLoadDefaults() {
    localUserCount = 0;
    for (int i = 0; defaultUsers[i].uid != nullptr && i < MAX_LOCAL_USERS; i++) {
        strncpy(localUsers[i].uid, defaultUsers[i].uid, 15);
        localUsers[i].uid[15] = '\0';
        strncpy(localUsers[i].username, defaultUsers[i].username, 31);
        localUsers[i].username[31] = '\0';
        localUserCount++;
    }
    Serial.printf("RFID: Loaded %d default users (fallback)\n", localUserCount);
}

void rfidAddUser(const char* uid, const char* name) {
    if (!uid || !name) return;
    int idx = findInDynamic(uid);
    if (idx >= 0) {
        // 更新已有记录
        strncpy(localUsers[idx].username, name, 31);
        localUsers[idx].username[31] = '\0';
        Serial.printf("RFID: Updated user %s -> %s\n", uid, name);
        return;
    }
    if (localUserCount >= MAX_LOCAL_USERS) {
        Serial.println(F("RFID: User list full!"));
        return;
    }
    strncpy(localUsers[localUserCount].uid, uid, 15);
    localUsers[localUserCount].uid[15] = '\0';
    strncpy(localUsers[localUserCount].username, name, 31);
    localUsers[localUserCount].username[31] = '\0';
    localUserCount++;
    Serial.printf("RFID: Added user %s -> %s (total=%d)\n", uid, name, localUserCount);
}

void rfidDelUser(const char* uid) {
    if (!uid) return;
    // 不能删除默认用户
    if (findInDefaults(uid) >= 0) {
        Serial.printf("RFID: Cannot delete default user %s\n", uid);
        return;
    }
    int idx = findInDynamic(uid);
    if (idx < 0) return;
    // 用最后一个覆盖当前, 然后减计数
    localUsers[idx] = localUsers[localUserCount - 1];
    localUserCount--;
    Serial.printf("RFID: Deleted user %s (total=%d)\n", uid, localUserCount);
}

void rfidClearUsers() {
    localUserCount = 0;
    Serial.println(F("RFID: All dynamic users cleared"));
}

int rfidUserCount() {
    return localUserCount;
}

int rfidLoadFromJson(const char* json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("RFID: JSON parse error: %s\n", err.c_str());
        return 0;
    }
    if (!doc.is<JsonArray>()) {
        Serial.println(F("RFID: JSON is not an array"));
        return 0;
    }

    // 清空旧数据, 但保留默认用户
    localUserCount = 0;

    int added = 0;
    for (JsonVariant item : doc.as<JsonArray>()) {
        const char* uid  = item["uid"]      | "";
        const char* name = item["username"] | "";
        if (strlen(uid) == 0) continue;
        rfidAddUser(uid, name);
        added++;
    }

    Serial.printf("RFID: Synced %d users from server\n", added);
    return added;
}