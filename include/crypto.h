#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>

/**
 * @brief AES-128-CBC 加密, 输出十六进制字符串
 * @param plainText 明文
 * @return 加密后的十六进制字符串, 失败返回空串
 */
String encryptData(String plainText);

/**
 * @brief AES-128-CBC 解密 (十六进制密文 -> 明文)
 * @param cipherHex 十六进制密文字符串
 * @return 解密后的明文, 失败返回空串
 */
String decryptData(String cipherHex);

/**
 * @brief SHA-256 哈希计算
 * @param msg 输入消息
 * @return 64 位十六进制哈希字符串
 */
String generateHash(String msg);

#endif
