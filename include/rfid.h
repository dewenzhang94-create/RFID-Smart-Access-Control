#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <MFRC522.h>

// --- 本地白名单最大容量 ---
#define MAX_LOCAL_USERS 50

/**
 * @brief 初始化 RFID 模块 (SPI + MFRC522) 并加载默认白名单
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
 * @brief 验证 UID 是否为授权用户 (先查动态白名单, 再查默认回退)
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

// ============================================
// 动态白名单操作
// ============================================

/**
 * @brief 加载出厂默认用户 (断网时的硬回退)
 *   "63A24B9F" → "张三"
 *   "A1B2C3D4" → "李四"
 *   "F1E2D3C4" → "王五"
 *   "FE320102" → "ZDW"
 */
void rfidLoadDefaults();

/**
 * @brief 添加/更新一个用户到动态白名单
 * @param uid  卡 UID
 * @param name 用户姓名
 */
void rfidAddUser(const char* uid, const char* name);

/**
 * @brief 从动态白名单删除一个用户
 * @param uid 卡 UID
 */
void rfidDelUser(const char* uid);

/**
 * @brief 清空整个动态白名单
 */
void rfidClearUsers();

/**
 * @brief 获取当前白名单用户数
 */
int rfidUserCount();

/**
 * @brief 加载服务器下发的完整白名单 JSON
 *        格式: [{"uid":"XX","username":"YY"},...]
 * @param json JSON 字符串
 * @return 成功添加的条目数
 */
int rfidLoadFromJson(const char* json);

#endif