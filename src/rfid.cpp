/**
 * rfid.cpp - RFID 读写模块
 * 使用 MFRC522 通过 SPI 读取 IC 卡 UID
 * 本地维护授权用户列表进行身份认证
 */

#include "rfid.h"

// --- 引脚定义 ---
#define RFID_SS    5
#define RFID_RST   4
#define RFID_SCK   18
#define RFID_MISO  19
#define RFID_MOSI  23

// --- MFRC522 实例 ---
MFRC522 mfrc522(RFID_SS, RFID_RST);

// --- 本地授权用户列表 (生产环境应使用服务器端数据库) ---
struct AuthorizedUser {
    const char* uid;
    const char* username;
};

static const AuthorizedUser users[] = {
    {"63A24B9F", "张三"},
    {"A1B2C3D4", "李四"},
    {"F1E2D3C4", "王五"},
    {"FE320102", "ZDW"},
    {nullptr, nullptr}  // 哨兵
};

// ============================================
// 公共函数实现
// ============================================

void rfidInit() {
    SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
    mfrc522.PCD_Init();
    delay(50);
    mfrc522.PCD_DumpVersionToSerial();  // 打印版本信息到串口
    Serial.println(F("RFID: MFRC522 initialized"));
}

bool isNewCardPresent() {
    // 检测新卡
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return false;
    }
    // 读取卡序列号
    if (!mfrc522.PICC_ReadCardSerial()) {
        return false;
    }
    return true;
}

String readUID() {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        // 补零对齐 (如 0x5 -> "05")
        if (mfrc522.uid.uidByte[i] < 0x10) {
            uid += "0";
        }
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

bool checkUser(String uid) {
    for (int i = 0; users[i].uid != nullptr; i++) {
        if (uid.equals(users[i].uid)) {
            return true;
        }
    }
    return false;
}

String getUsername(String uid) {
    for (int i = 0; users[i].uid != nullptr; i++) {
        if (uid.equals(users[i].uid)) {
            return String(users[i].username);
        }
    }
    return String("Unknown");
}

void haltCard() {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}
