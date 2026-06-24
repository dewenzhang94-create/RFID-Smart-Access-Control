"""
crypto_helper.py - AES-128-CBC 解密工具 (Python 端)
与 ESP32 端 crypto.cpp 的加密逻辑对应

依赖: pip install pycryptodome
"""

from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad
from config import AES_KEY

# --- 固定 IV (与 ESP32 端 crypto.cpp 完全一致) ---
_IV = bytes([
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
])


def aes_decrypt(hex_cipher: str) -> str | None:
    """
    AES-128-CBC 解密
    输入: 十六进制密文字符串  (与 ESP32 encryptData() 输出对应)
    输出: 明文字符串, 失败返回 None
    """
    try:
        # 十六进制 -> 字节
        raw = bytes.fromhex(hex_cipher)

        # AES-128-CBC 解密
        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv=_IV)
        padded = cipher.decrypt(raw)

        # 移除 PKCS7 填充
        plain = unpad(padded, AES.block_size, style='pkcs7')
        return plain.decode('utf-8')

    except ValueError as e:
        print(f"[CRYPTO] Padding error (data may be corrupted): {e}")
        return None
    except Exception as e:
        print(f"[CRYPTO] Decrypt error: {e}")
        return None


def aes_encrypt(plain_text: str) -> str | None:
    """
    AES-128-CBC 加密 (用于测试/调试)
    输入: 明文字符串
    输出: 十六进制密文字符串
    """
    try:
        from Crypto.Util.Padding import pad

        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv=_IV)
        padded = pad(plain_text.encode('utf-8'), AES.block_size, style='pkcs7')
        encrypted = cipher.encrypt(padded)
        return encrypted.hex().upper()

    except Exception as e:
        print(f"[CRYPTO] Encrypt error: {e}")
        return None


# ============================================
# 自测试: 验证加解密一致性
# ============================================
if __name__ == "__main__":
    test_msg = '{"uid":"63A24B9F","username":"张三","event":"login","time":"2026-06-14T20:00:00"}'

    print("=== AES-128-CBC 加解密测试 ===")
    print(f"原文: {test_msg}")

    enc = aes_encrypt(test_msg)
    print(f"密文: {enc}")

    dec = aes_decrypt(enc)
    print(f"解密: {dec}")

    assert dec == test_msg, "加解密不一致!"
    print("\n✅ 加解密测试通过!")
