# RFID 智能安防管控平台

基于 ESP32 的物联网门禁系统，集成 RFID 刷卡认证、环境安全监测、Web 可视化管理、AI 语音控制。

## 系统架构

```
┌──────────────────────────────────────────────────────────┐
│  浏览器 (Vue3 SPA)                                       │
│  http://127.0.0.1:5000                                  │
│  仪表盘 / 实时监控 / 远程控制 / 数据分析 / 权限管理          │
│  语音控制 (Web Speech API + DeepSeek AI)                  │
└──────────┬───────────────────────────────────────────────┘
           │ HTTP (REST API)
┌──────────▼──────────┐     MQTT      ┌──────────────────┐
│  Python Flask 后端   │◄────────────►│  Mosquitto Broker  │
│  web_app.py          │    port 1884 │  port 1884         │
│  SQLite 数据库        │              └────────┬─────────┘
│  AES-128 解密        │                        │ MQTT
│  智能语音引擎         │              ┌─────────▼─────────┐
│  环境分析引擎         │              │  ESP32 硬件终端     │
└──────────────────────┘              │  WiFi / MFRC522   │
                                      │  传感器 / 舵机     │
                                      └───────────────────┘
```

## 硬件清单

| 元件 | 型号 | 功能 |
|------|------|------|
| 主控 | ESP32-WROOM-32E | WiFi + 蓝牙 双核 |
| RFID 读卡器 | MFRC522 | 13.56MHz IC卡读写 |
| OLED 屏幕 | SSD1306 128x64 | 状态显示 (I2C) |
| 人体红外 | HC-SR501 PIR | 检测人员靠近 |
| 舵机 | SG90 | 电子门锁 (0°锁 / 90°开) |
| 火焰传感器 | 红外接收管模块 | 检测明火 |
| 烟雾传感器 | MQ-2 | 可燃气体/烟雾检测 |
| 温湿度 | DHT22 | 环境温湿度采集 |
| 蜂鸣器 | 有源 3.3V | 报警提示音 |
| LED | 红 + 绿 | 状态指示灯 |

## 引脚接线

```
ESP32                     外设
────────────────────────────────────
GPIO 4   ───────────────── MFRC522 RST
GPIO 5   ───────────────── MFRC522 SDA/SS
GPIO 13  ───────────────── DHT22 DATA
GPIO 14  ───────────────── SG90 舵机信号 (LEDC PWM)
GPIO 18  ───────────────── MFRC522 SCK
GPIO 19  ───────────────── MFRC522 MISO
GPIO 21  ───────────────── OLED SDA (I2C)
GPIO 22  ───────────────── OLED SCL (I2C)
GPIO 23  ───────────────── MFRC522 MOSI
GPIO 25  ───────────────── 有源蜂鸣器 I/O
GPIO 26  ───────────────── 火焰传感器 DO
GPIO 27  ───────────────── HC-SR501 PIR OUT
GPIO 32  ───────────────── 绿色 LED (+)
GPIO 33  ───────────────── 红色 LED (+)
GPIO 34  ───────────────── MQ-2 烟雾 AO (ADC)
────────────────────────────────────
3.3V    ──── MFRC522, OLED, DHT22, PIR, 火焰, MQ-2
5V      ──── SG90 舵机
GND     ──── 所有模块共地
```

## 项目目录结构

```
RFID/
├── platformio.ini          # PlatformIO 编译配置
├── mosquitto.conf          # MQTT Broker 配置
├── include/                # C++ 头文件
│   ├── config.h            #    WiFi / MQTT / AES 配置
│   ├── rfid.h              #    RFID 读卡 + 白名单
│   ├── mqtt.h              #    MQTT 通信 + 远程指令
│   ├── oled.h              #    OLED 显示
│   ├── buzzer.h            #    蜂鸣器 + LED
│   ├── sensor.h            #    人体红外
│   ├── servo.h             #    舵机电子锁
│   ├── env_sensor.h        #    环境传感器 (火焰/烟雾/DHT22)
│   ├── crypto.h            #    AES-128-CBC 加密 + SHA-256
│   └── wifi_manager.h      #    WiFi 连接 + NTP 对时
├── src/                    # C++ 源文件 (与 include 一一对应)
├── server/                 # Python 后端
│   ├── web_app.py          #    Flask Web 服务器 (核心)
│   ├── database.py         #    SQLite 数据库初始化
│   ├── crypto_helper.py    #    AES 解密 (服务端)
│   ├── smart_cmd.py        #    AI 语音意图识别引擎
│   ├── analysis_engine.py  #    环境数据分析 (国标参考)
│   ├── mqtt_listener.py    #    独立 MQTT 监听程序
│   ├── config.py           #    服务端配置 (不上传 GitHub)
│   ├── config.example.py   #    配置模板
│   └── static/
│       └── spa.html        #    Vue3 单页前端
└── docs/                   # 文档/报告
```

## 快速启动

### 1. 安装依赖

```bash
# Python 依赖
pip install flask paho-mqtt pycryptodome requests

# PlatformIO (ESP32 编译)
pip install platformio
```

### 2. 配置参数

在 `server/config.py` 中填写实际参数 (MQTT Broker IP、DeepSeek API Key 等)。模板见 `server/config.example.py`。

在 `include/config.h` 中修改 WiFi 名称和密码。

### 3. 启动 Mosquitto MQTT Broker

```bash
# Windows
"C:/Program Files/Mosquitto/mosquitto.exe" -c mosquitto.conf -v

# 或安装为系统服务后 net start mosquitto
```

### 4. 编译并烧录 ESP32 固件

```bash
pio run --target upload
```

### 5. 启动 Flask 后端

```bash
cd server
python web_app.py
```

### 6. 打开浏览器

访问 **http://127.0.0.1:5000**

```
管理员: admin / admin123
普通用户: user / user123
```

## 刷卡认证流程 (双重方案)

```
有人靠近 → PIR 检测 → OLED 显示 "请刷卡"
    ↓
刷卡 → MFRC522 读取 UID
    ↓
┌─ 1. 查动态白名单 (服务器 MQTT 同步, 可远程增删)
│   匹配 → 开门 ✅
├─ 2. 查出厂默认名单 (4人, 断网也能用)
│   匹配 → 开门 ✅
└─ 都不匹配 → 蜂鸣器报警 ❌
    ↓
AES-128-CBC 加密 → MQTT 上报 → 服务器记录日志
```

- **在线模式**：管理员在网页增删卡 → 数据库写入 → MQTT 推送 → ESP32 秒级同步
- **离线模式**：自动回退到出厂默认 4 人白名单，断网也能正常刷卡
- **重启恢复**：ESP32 启动时自动向服务器请求完整白名单

## MQTT 通信协议

| Topic | 方向 | 说明 |
|-------|------|------|
| `rfid/auth` | ESP32→服务器 | 身份认证结果 (加密) |
| `rfid/alarm` | ESP32→服务器 | 报警信息 (加密) |
| `rfid/door` | ESP32→服务器 | 门状态变化 (加密) |
| `rfid/system` | ESP32→服务器 | 心跳 + 环境数据 (加密) |
| `rfid/cmd` | 服务器→ESP32 | 远程控制指令 |
| `rfid/users` | 服务器→ESP32 | 用户白名单同步 |

数据格式：AES-128-CBC 加密 → 十六进制字符串，SHA-256 哈希校验。密钥与 IV 在两端保持一致。

## 网页功能

### 仪表盘
今日通行统计、事件分布图、最近日志

### 实时监控
环境数据 (温度/湿度/烟雾/火焰)、门锁状态、人体检测、实时日志流

### 远程控制
开门 / 锁门 / 测试报警 (圆形按钮)

### 数据分析
7天趋势图、事件分布饼图、用户活跃度排行

### 智能分析
基于国标 (GB/T 18883-2022, GB 50736-2012, GB 50325-2020) 的环境数据解读和建议

### 权限管理
- 账户管理：增删 Web 登录用户，分配 admin/user 角色
- 卡片管理：增删 RFID 授权卡，自动同步到 ESP32

### 语音控制
点右下角麦克风按钮，支持：
- "开门" / "锁门" / "测试报警" → 直接控制硬件
- "温度多少" / "湿度多少" / "空气质量" → 查询环境数据
- "有人吗" / "什么状态" → 查询系统状态

## 数据库表结构

**accounts** — 网页登录账户 (username, password-SHA256, role)
**users** — RFID 卡白名单 (uid, username, role)
**logs** — 事件日志 (uid, username, event, time)

event 类型: `login`(认证通过), `door_open`(开门), `door_close`(关门), `unauthorized`(非法刷卡), `heartbeat`(心跳), `system_start`(启动)

## 安全机制

- 所有 MQTT 消息 AES-128-CBC 加密传输
- SHA-256 哈希校验数据完整性
- 密码 SHA-256 哈希存储 (不明文)
- 网页 session 认证 + 角色权限控制
- API key 不入库 (config.py 已加入 .gitignore)