/**
 * crypto.cpp - 数据加密与哈希模块
 * 基于 ESP32 内置 mbed TLS 库:
 *   - AES-128-CBC 加密/解密
 *   - SHA-256 哈希校验
 */

#include "crypto.h"
#include "config.h"
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>

// --- AES-128 密钥 (16 字节) ---
static const unsigned char AES_KEY[16] = { AES_KEY_BYTES };

// --- 固定 IV (CBC 模式; 生产环境应使用随机 IV) ---
static unsigned char iv[16] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
};

// --- 前向声明 ---
static inline uint8_t hexCharToByte(char c);

// ============================================
// AES-128-CBC 加密
// 输入: 明文字符串
// 输出: 十六进制密文 (大写)
// ============================================

String encryptData(String plainText) {
    if (plainText.length() == 0) return "";

    // PKCS7 填充 -> 对齐到 16 字节块
    size_t len = plainText.length();
    size_t paddedLen = ((len / 16) + 1) * 16;
    uint8_t input[paddedLen];
    uint8_t output[paddedLen];

    memset(input, 0, paddedLen);
    memcpy(input, plainText.c_str(), len);

    uint8_t padByte = paddedLen - len;
    for (size_t i = len; i < paddedLen; i++) {
        input[i] = padByte;
    }

    // mbedtls AES 加密
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    unsigned char localIv[16];
    memcpy(localIv, iv, 16);

    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT,
                                     paddedLen, localIv, input, output);
    mbedtls_aes_free(&aes);

    if (ret != 0) {
        Serial.printf("CRYPTO: AES encrypt failed with code %d\n", ret);
        return "";
    }

    // 转为十六进制字符串
    String hexStr = "";
    char buf[3];
    for (size_t i = 0; i < paddedLen; i++) {
        sprintf(buf, "%02X", output[i]);
        hexStr += buf;
    }
    return hexStr;
}

// ============================================
// AES-128-CBC 解密
// 输入: 十六进制密文字符串
// 输出: 明文字符串
// ============================================

String decryptData(String cipherHex) {
    if (cipherHex.length() == 0 || cipherHex.length() % 2 != 0) return "";

    size_t len = cipherHex.length() / 2;
    uint8_t input[len];
    uint8_t output[len];

    // 十六进制 -> 字节数组
    for (size_t i = 0; i < len; i++) {
        char high = cipherHex.charAt(i * 2);
        char low  = cipherHex.charAt(i * 2 + 1);
        input[i] = (hexCharToByte(high) << 4) | hexCharToByte(low);
    }

    // mbedtls AES 解密
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    unsigned char localIv[16];
    memcpy(localIv, iv, 16);

    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT,
                                     len, localIv, input, output);
    mbedtls_aes_free(&aes);

    if (ret != 0) {
        Serial.printf("CRYPTO: AES decrypt failed with code %d\n", ret);
        return "";
    }

    // 移除 PKCS7 填充
    uint8_t padByte = output[len - 1];
    if (padByte > 16 || padByte == 0) {
        return "";  // 填充非法
    }
    size_t plainLen = len - padByte;

    char result[plainLen + 1];
    memcpy(result, output, plainLen);
    result[plainLen] = '\0';
    return String(result);
}

// ============================================
// SHA-256 哈希
// 输入: 消息字符串
// 输出: 64 字符十六进制哈希 (小写)
// ============================================

String generateHash(String msg) {
    unsigned char hash[32];  // SHA-256 = 32 字节

    mbedtls_sha256_ret((const unsigned char*)msg.c_str(),
                        msg.length(), hash, 0);  // 0 = SHA-256, 1 = SHA-224

    char hashStr[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hashStr + i * 2, "%02x", hash[i]);
    }
    hashStr[64] = '\0';
    return String(hashStr);
}

// ============================================
// 辅助: 十六进制字符 -> 数值
// ============================================

static inline uint8_t hexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}
