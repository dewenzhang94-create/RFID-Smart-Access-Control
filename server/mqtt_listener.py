"""
mqtt_listener.py - MQTT 消息监听程序
功能:
  1. 订阅 rfid/# 所有 topic
  2. 接收 AES 加密消息 -> 解密
  3. 写入 SQLite 数据库
  4. 实时打印认证/报警信息

用法:
  python mqtt_listener.py

依赖:
  pip install paho-mqtt
"""

import json
import sys
import os
from datetime import datetime

import paho.mqtt.client as mqtt

# 确保能找到同目录的模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import (
    MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE,
    MQTT_TOPICS, AES_KEY, DEBUG_PRINT_CIPHER
)
from database import init_db, insert_log
from crypto_helper import aes_decrypt


# ============================================
# MQTT 回调: 连接成功
# ============================================

def on_connect(client, userdata, flags, reason_code, properties=None):
    """连接 Broker 后订阅所有 Topic"""
    if reason_code == 0:
        print(f"[MQTT] Connected to {MQTT_BROKER}:{MQTT_PORT}")
        for topic in MQTT_TOPICS:
            client.subscribe(topic)
            print(f"[MQTT] Subscribed: {topic}")
    else:
        print(f"[MQTT] Connection failed with code {reason_code}")


# ============================================
# MQTT 回调: 收到消息
# ============================================

def on_message(client, userdata, msg):
    """收到 MQTT 消息 -> 解密 -> 入库"""
    try:
        raw = msg.payload.decode("utf-8")

        # 判断是否为 AES 加密数据 (纯十六进制)
        is_encrypted = all(c in "0123456789ABCDEFabcdef" for c in raw) and len(raw) > 0

        if is_encrypted:
            if DEBUG_PRINT_CIPHER:
                print(f"[CRYPTO] Encrypted: {raw[:60]}...")

            plain = aes_decrypt(raw)
            if plain is None:
                print(f"[ERROR] AES decrypt failed for topic [{msg.topic}]")
                return

            print(f"[CRYPTO] Decrypted: {plain}")
            payload = plain
        else:
            # 明文消息 (调试模式或兼容旧版)
            print(f"[MQTT] Plaintext: {raw}")
            payload = raw

        # JSON 解析
        data = json.loads(payload)
        uid         = data.get("uid", "")
        username    = data.get("username", "")
        event       = data.get("event", "unknown")
        access_time = data.get("time", datetime.now().strftime("%Y-%m-%dT%H:%M:%S"))

        # === 根据事件类型输出格式化信息 ===
        topic = msg.topic
        print(f"\n{'='*55}")

        if event in ("login", "authorized"):
            print(f"  [OK] Auth Granted | {username} (UID: {uid})")
            print(f"  Time: {access_time}")
        elif event in ("unauthorized", "illegal_door"):
            print(f"  [ALARM] Security Alert | {event}")
            print(f"  Time: {access_time}")
            if uid:
                print(f"  UID: {uid}")
        elif event == "door_open":
            print(f"  [DOOR] Opened | {username or 'Unknown'}")
            print(f"  Time: {access_time}")
        elif event == "door_close":
            print(f"  [DOOR] Closed | {access_time}")
        elif event == "heartbeat":
            print(f"  [BEAT] Heartbeat | ESP32_GATE_001")
        elif event == "system_start":
            print(f"  [SYS] System Start | ESP32_GATE_001")
            print(f"  Time: {access_time}")
        else:
            print(f"  [EVENT] {event} | {username} | {access_time}")

        print(f"  Topic: {topic}")
        print(f"{'='*55}\n")

        # 写入数据库
        insert_log(uid, username, event, access_time)

    except json.JSONDecodeError as e:
        print(f"[ERROR] JSON parse error: {e}")
        print(f"        Raw data: {msg.payload.decode('utf-8', errors='replace')[:200]}")
    except Exception as e:
        print(f"[ERROR] Message processing failed: {e}")
        import traceback
        traceback.print_exc()


# ============================================
# MQTT 回调: 断开连接
# ============================================

def on_disconnect(client, userdata, flags, reason_code, properties=None):
    print(f"[MQTT] Disconnected (rc={reason_code}), auto-reconnecting...")


# ============================================
# 主函数
# ============================================

def main():
    print("=" * 55)
    print("  RFID 智能门禁系统 - MQTT 监听服务")
    print("=" * 55)
    print()

    # 初始化数据库
    init_db()

    # 创建 MQTT 客户端
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="RFID_Server_Python", clean_session=False)
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect

    # 连接 Broker
    print(f"[MQTT] Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)
    except Exception as e:
        print(f"[FATAL] Cannot connect to MQTT Broker: {e}")
        print("  Please ensure Mosquitto is running:")
        print("    mosquitto -v -p 1883")
        sys.exit(1)

    # 进入消息循环 (阻塞)
    print("[MQTT] Listening for messages... (Ctrl+C to stop)\n")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[MQTT] Shutting down...")
        client.disconnect()
        print("[MQTT] Goodbye!")


if __name__ == "__main__":
    main()
