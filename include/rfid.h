#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <MFRC522.h>

/**
 * @brief 初始化 RFID 模块 (SPI + MFRC522)
 */
void rfidInit();

/**
 * @brief 检测是否有新卡进入感应区
 * @return true 检测到新卡, false 无卡
 */
bool isNewCardPresent();

/**
 * @brief 读取当前卡的 UID (16进制大写字符串)
 * @return UID 字符串, 无卡返回空串
 */
String readUID();

/**
 * @brief 验证 UID 是否为授权用户
 * @param uid 待验证的 UID
 * @return true 授权用户, false 未授权
 */
bool checkUser(String uid);

/**
 * @brief 通过 UID 获取用户名
 * @param uid UID 字符串
 * @return 用户姓名, 未找到返回 "Unknown"
 */
String getUsername(String uid);

/**
 * @brief 使当前卡进入休眠 (Halt)
 */
void haltCard();

#endif
