# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

基于 ESP32 的 RFID 智能门禁与物联网信息安全监测系统。包含硬件端（ESP32 C++ / Arduino Framework）和上位机（Python + SQLite + MQTT）。

## Build & Development

### ESP32 固件

```bash
# 安装 PlatformIO（如果没有）
pip install platformio

# 编译项目
pio run

# 编译并上传到 ESP32
pio run --target upload

# 编译 + 上传 + 打开串口监视器
pio run --target upload --target monitor

# 仅打开串口监视器
pio device monitor
```

### Python 服务器

```bash
# 依赖安装
pip install paho-mqtt sqlite3

# 启动监听程序
python server/mqtt_listener.py

# 初始化数据库
python server/database.py
```

### MQTT Broker (Mosquitto)

```bash
# 启动 Mosquitto（Windows）
net start mosquitto
# 或手动
mosquitto -v -p 1883
```

## Architecture

```
┌─────────────┐    SPI     ┌──────────┐
│  MFRC522     │◄──────────►│          │
│  RFID 读写器  │            │          │
└─────────────┘            │          │
                           │  ESP32   │── I2C ──► SSD1306 OLED
┌─────────────┐            │  WROOM   │
│  HC-SR501    │── GPIO ──►│  32E     │── GPIO ─► 蜂鸣器 + LED
│  人体红外     │            │          │
└─────────────┘            │          │
                           └────┬─────┘
┌─────────────┐                 │ WiFi
│  MC-38       │── GPIO ──►     │
│  门磁传感器   │            ┌───▼──────┐    ┌────────────┐
└─────────────┘            │ Mosquitto │───►│ Python     │
                           │ MQTT      │    │ SQLite     │
                           └───────────┘    └────────────┘
```

## Pin Mapping (ESP32)

| 功能 | GPIO |
|------|------|
| RFID SS (SDA) | 5 |
| RFID RST | 4 |
| RFID SCK | 18 |
| RFID MISO | 19 |
| RFID MOSI | 23 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| 蜂鸣器 | 25 |
| 门磁传感器 | 26 |
| 人体红外 PIR | 27 |
| 绿色 LED | 32 |
| 红色 LED | 33 |

## Module Map

每个 `src/*.cpp` 对应 `include/*.h`：

| 文件 | 职责 | 核心函数 |
|------|------|----------|
| `main.cpp` | 初始化 + 主循环 | `setup()`, `loop()` |
| `rfid.cpp` | RFID 读写 | `readUID()`, `checkUser()` |
| `sensor.cpp` | 人体/门磁检测 | `detectHuman()`, `detectDoorOpen()` |
| `oled.cpp` | OLED 显示 | `showMessage()` |
| `buzzer.cpp` | 蜂鸣器/LED 控制 | `alarm()`, `successBeep()` |
| `crypto.cpp` | AES 加密 + SHA256 | `encryptData()`, `generateHash()` |
| `mqtt.cpp` | MQTT 通信 | `mqttConnect()`, `publishMessage()` |
| `wifi_manager.cpp` | WiFi 连接 | `connectWifi()` |

## MQTT Topics

| Topic | 用途 |
|-------|------|
| `rfid/auth` | 身份认证结果 |
| `rfid/alarm` | 报警信息 |
| `rfid/door` | 门状态变化 |
| `rfid/system` | 系统状态 |

JSON 格式：`{"uid":"xxx","username":"xxx","event":"login|alarm|door","time":"ISO8601"}`

## Database (SQLite)

- 文件：`server/rfid.db`
- `users` 表：uid (PK), username, role
- `logs` 表：id (PK AUTO), uid, username, event, access_time

## Development Priority

按文档第十二节，优先输出顺序：
1. ESP32 完整代码（src/ + include/）
2. Python 数据库程序
3. MQTT 通信程序
4. 原理图设计
5. PCB 设计
6. 项目报告

## Dependencies

### PlatformIO 库 (platformio.ini)
- `MFRC522` — RFID 读写
- `Adafruit SSD1306` — OLED
- `PubSubClient` — MQTT 客户端
- `AESLib` — AES 加密
- `ArduinoJson` — JSON 序列化

### Python
- `paho-mqtt` — MQTT 客户端
- `sqlite3` — 数据库（标准库）
