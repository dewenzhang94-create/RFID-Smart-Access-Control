# RFID 智能门禁与安防系统

基于 ESP32 的物联网门禁系统，集成 RFID 刷卡认证、环境安全监测、Web 可视化管理、AI 语音控制、HTTPS 加密通信。

## 系统架构

```
┌──────────────────────────────────────────────────────────┐
│  浏览器 (Vue3 SPA)                                       │
│  https://127.0.0.1:5000                                 │
│  仪表盘 / 实时监控 / 远程控制 / 数据分析 / 智能分析        │
│  权限管理 / MQTT 监控 / 语音控制                          │
└──────────┬───────────────────────────────────────────────┘
           │ HTTPS (TLS 1.3, 自签证书)
┌──────────▼──────────┐     MQTT      ┌──────────────────┐
│  Python Flask 后端   │◄────────────►│  Mosquitto Broker  │
│  web_app.py          │    port 1884 │  port 1884         │
│  SQLite 数据库        │              └────────┬─────────┘
│  AES-128 解密/加密   │                        │ MQTT
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
GPIO 16  ───────────────── OLED SDA (I2C)
GPIO 17  ───────────────── OLED SCL (I2C)
GPIO 18  ───────────────── MFRC522 SCK
GPIO 19  ───────────────── MFRC522 MISO
GPIO 21  ───────────────── (空闲)
GPIO 22  ───────────────── (空闲)
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

> **注意：** MFRC522 必须接 3.3V，接 5V 会烧毁模块。OLED 使用 GPIO16/17 (原 GPIO21/22 不稳定)。蜂鸣器为低电平触发 (LOW=响, HIGH=静音)。

## 项目目录结构

```
RFID/
├── platformio.ini          # PlatformIO 编译配置
├── mosquitto.conf          # MQTT Broker 配置 (端口1884, 匿名)
├── include/                # C++ 头文件
│   ├── config.h            #    WiFi / MQTT / AES 配置
│   ├── rfid.h              #    RFID 读卡 + 双重认证白名单 (MAX 50)
│   ├── mqtt.h              #    MQTT 通信 + 远程指令回调
│   ├── oled.h              #    OLED 显示 (自适应字体)
│   ├── buzzer.h            #    蜂鸣器 + LED
│   ├── sensor.h            #    人体红外 (软件去抖)
│   ├── servo.h             #    舵机电子锁 (LEDC PWM)
│   ├── env_sensor.h        #    环境传感器 (火焰/MQ-2/DHT22)
│   ├── crypto.h            #    AES-128-CBC 加密 + SHA-256 (mbedtls)
│   └── wifi_manager.h      #    WiFi 连接 + NTP 对时
├── src/                    # C++ 源文件 (与 include 一一对应)
├── server/                 # Python 后端
│   ├── web_app.py          #    Flask Web 服务器 (核心, HTTPS)
│   ├── database.py         #    SQLite 数据库初始化
│   ├── crypto_helper.py    #    AES 加解密 (PyCryptodome)
│   ├── smart_cmd.py        #    AI 语音意图识别引擎
│   ├── analysis_engine.py  #    环境数据分析 (国标参考)
│   ├── mqtt_listener.py    #    独立 MQTT 监听程序
│   ├── config.py           #    服务端配置 (不入 Git)
│   ├── config.example.py   #    配置模板
│   ├── cert.pem            #    HTTPS 自签证书 (不入 Git)
│   ├── key.pem             #    HTTPS 私钥 (不入 Git)
│   └── static/
│       └── spa.html        #    Vue3 单页应用
└── docs/                   # 文档/报告
```

## 快速启动

### 0. 生成 HTTPS 证书 (首次)

```bash
cd server
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout key.pem -out cert.pem -days 365 \
  -subj "//C=CN\ST=Guangdong\L=Shenzhen\O=RFID\CN=localhost"
```

### 1. 安装依赖

```bash
# Python 依赖
pip install flask paho-mqtt pycryptodome requests

# PlatformIO (ESP32 编译)
pip install platformio
```

### 2. 配置参数

在 `server/config.py` 中填写实际参数 (MQTT Broker IP、AES 密钥、DeepSeek API Key 等)。模板见 `server/config.example.py`。

在 `include/config.h` 中修改 WiFi SSID 和密码。

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

访问 **https://127.0.0.1:5000** (注意是 `https://`)

首次访问会提示证书不受信任 → 点击 **高级** → **继续前往** 即可。

```
超级管理员: zdw  / 密码自行设定
管理员:     admin / admin123
普通用户:   user  / user123
```

## 刷卡认证流程 (双重方案)

```
有人靠近 → PIR 检测 → OLED 显示 "Please Swipe Card"
    ↓
刷卡 → MFRC522 读取 UID
    ↓
┌─ 1. 查动态白名单 (服务器 MQTT 同步, 可远程增删)
│   匹配 → 开门 ✅
├─ 2. 查出厂默认名单 (4人, 断网也能用)
│   匹配 → 开门 ✅
└─ 都不匹配 → 蜂鸣器报警 ❌ + 自动锁门防尾随
    ↓
AES-128-CBC 加密 → MQTT 上报 → 服务器解密记录日志
```

- **在线模式**：管理员在网页增删卡 → 数据库写入 → MQTT 推送 → ESP32 秒级同步
- **离线模式**：自动回退到出厂默认 4 人白名单 (张三/李四/王五/ZDW)，断网也能正常刷卡
- **重启恢复**：ESP32 启动时自动向服务器请求完整白名单
- **人体检测优化**：PIR 状态变化立刻 MQTT 上报 (human_detected/human_left)，前端秒级响应，不等待 30s 心跳

## MQTT 通信协议

| Topic | 方向 | 说明 | 加密 |
|-------|------|------|------|
| `rfid/system` | ESP32→服务器 | 心跳 + 环境数据 (温度/湿度/烟雾/火焰/人体/门锁) | AES |
| `rfid/auth` | ESP32→服务器 | 身份认证结果 | AES |
| `rfid/alarm` | ESP32→服务器 | 报警信息 (非法卡/烟雾/火灾) | AES |
| `rfid/door` | ESP32→服务器 | 门状态变化 (开/关) | AES |
| `rfid/cmd` | 服务器→ESP32 | 远程控制指令 + 白名单同步指令 | 明文 |
| `rfid/users` | 服务器→ESP32 | 用户白名单 JSON 下发 | 明文 |

数据格式：JSON → AES-128-CBC 加密 → 十六进制字符串。ESP32 端使用 mbedtls 库，Python 端使用 PyCryptodome。密钥与 IV 在两端 `config.h` / `config.py` 中保持一致。

## 网页功能

### 仪表盘
今日通行统计 (总次数/通过/非法)、设备在线状态、最近通行记录表格 (事件彩色标签)、快速操作 (开门/锁门/测试报警)、环境简报

### 实时监控
环境数据 (温度/湿度/烟雾/火焰)、门锁状态、人体检测、实时事件流 (800ms 刷新)

### 远程控制
门锁控制 (🔓开门 / 🔒锁门 130px 圆形按钮)、系统控制 (测试报警/重启设备)、操作日志。仅管理员可操作

### 数据统计
7天趋势图、事件分布饼图、用户活跃度排行柱状图、日志搜索

### 智能分析
温度/湿度/烟雾三条 ECharts 实时曲线图 (自动缩放 Y 轴 + 30% 内边距)、基于国标 (GB/T 18883-2022, GB 50736-2012, GB 50325-2020) 的环境数据解读与行动建议

### 权限管理
- **账户管理**：增删 Web 登录用户，分配 admin/user 角色。ZDW 为超级管理员，不可删除、不可降级
- **刷卡录入**：点绿色按钮 → 输入持卡人姓名 → 把新卡放上读卡器 → UID 自动读取并 AES 加密上报 → 入库并同步 ESP32 白名单
- **批量删除**：点红色按钮 → 勾选要删的卡 → 确认后批量移除并同步。出厂默认卡 (张三/李四/王五/ZDW) 受保护不可删
- **录入期间**：OLED 显示 "Scan Card For XXX"，30 秒超时自动退出

### MQTT 监控
实时 MQTT 消息流量可视化：方向指示 (↑收/↓发)、Topic 彩色标签 (6 色)、事件标签、传感器数据、原始密文开关 (AES 十六进制)、自动滚动、1 秒刷新

### 语音控制
右下角麦克风按钮，基于 Web Speech API + DeepSeek AI 双引擎：
- 本地规则模式：模糊语义匹配，支持 20+ 种中文表达
- AI 增强模式：DeepSeek 自然语言理解 (不限表达方式)
- "开门" / "锁门" / "测试报警" → 直接控制硬件 (需管理员)
- "温度多少" / "湿度多少" / "空气质量" / "有人吗" → 查询免登录

## 数据库表结构

| 表名 | 用途 | 字段 |
|------|------|------|
| `accounts` | Web 登录账户 | id, username, password(SHA-256), role, created |
| `users` | RFID 卡白名单 | uid(PK), username, role |
| `logs` | 事件日志 | id, uid, username, event, access_time |

**event 事件类型：**

| 事件 | 说明 | 来源 |
|------|------|------|
| `login` | 刷卡认证通过 | ESP32 |
| `door_open` | 门锁开启 | ESP32 / 远程 |
| `door_close` | 门锁关闭 | ESP32 / 远程 / 超时 |
| `unauthorized` | 非法刷卡 | ESP32 |
| `alarm` | 报警 (烟雾/火灾) | ESP32 |
| `heartbeat` | 系统心跳 (30s, 不入库) | ESP32 |
| `system_start` | ESP32 启动 | ESP32 |
| `human_detected` | 有人靠近 (即时) | ESP32 |
| `human_left` | 人离开 (即时, 1.5s 超时) | ESP32 |
| `card_learned` | 新卡录入 | ESP32 |
| `cmd_sent` | 远程指令下发 | 服务器 |
| `users_sync` | 白名单同步 | 服务器 |

## 安全机制

| 层级 | 措施 | 技术 |
|------|------|------|
| **通信加密** | Flask HTTPS (TLS 1.3) | OpenSSL 自签证书 |
| **通信加密** | MQTT Payload AES-128-CBC 加密 | mbedtls / PyCryptodome |
| **数据完整性** | SHA-256 哈希校验 | ESP32 + Python |
| **密码安全** | SHA-256 哈希存储，前端先哈希再传输 | Web Crypto API |
| **身份认证** | Session 认证 + `@login_required` 装饰器 | Flask Session |
| **访问控制** | RBAC 三级角色 (superadmin/admin/user) | 角色装饰器 |
| **防 XSS** | Session Cookie HttpOnly | Flask 配置 |
| **防窃听** | Session Cookie Secure (仅 HTTPS) | Flask 配置 |
| **密钥保护** | `server/config.py` + `*.pem` 均加入 `.gitignore` | Git |

### 角色权限矩阵

| 操作 | superadmin (ZDW) | admin | user |
|------|:---:|:---:|:---:|
| 查看仪表盘/监控/分析 | ✅ | ✅ | ✅ |
| 远程开门/锁门 | ✅ | ✅ | ❌ |
| 管理用户 (增删改角色) | ✅ | ✅ | ❌ |
| 管理 RFID 卡 (录入/删除) | ✅ | ✅ | ❌ |
| 被其他 admin 删除/降级 | ❌ 受保护 | ✅ | ✅ |
| 删除自己的卡片 | ❌ 受保护 | - | - |
| 语音查询 | ✅ | ✅ | ✅ |
| 语音控制 (开门等) | ✅ | ✅ | ❌ |

## 环境联动自动控制

| 触发条件 | 动作 |
|----------|------|
| 合法刷卡 | 舵机开锁 + 绿灯 + 蜂鸣器成功提示 + MQTT 上报 |
| 非法刷卡 | 立即锁门 (防尾随) + 蜂鸣报警 + 红灯 + MQTT 上报 |
| 火焰检测 | 紧急开锁 + 持续蜂鸣 + 红灯闪烁 + MQTT 紧急上报 |
| 烟雾超标 (>3700) | 开锁疏散 + 蜂鸣报警 + MQTT 上报 |
| 认证超时 (10s) | 自动锁门 + MQTT 上报 |
| 人离开 (1.5s) | 自动退出刷卡态 + 自动锁门 |

## 常见问题

**Q: 浏览器提示"您的连接不是私密连接"？**
A: 使用自签证书的正常现象。点"高级" → "继续前往"即可。数据已加密传输，与付费证书加密效果相同。

**Q: RFID 刷卡没反应？**
A: 检查 MFRC522 的 7 根线是否插紧 (尤其 RST/GPIO4)。上电后用串口看启动日志中 `Firmware Version` 是否为 `0x92`。返回 `0x0` 说明 SPI 通信失败。

**Q: OLED 不亮？**
A: 确认 SDA→GPIO16, SCL→GPIO17 (不是 GPIO21/22)。背面 I2C 地址通常为 0x78。

**Q: 蜂鸣器一直响？**
A: 本系统使用低电平触发蜂鸣器 (LOW=响)。如果一直响，检查 GPIO25 是否与 GND 短路。

**Q: MQ-2 一直报烟雾报警？**
A: 阈值定义在 `include/env_sensor.h` 的 `SMOKE_THRESHOLD`，默认 3700。可根据实际环境调整。

**Q: ESP32 频繁重启？**
A: `platformio.ini` 中 `-DARDUINO_LOOP_STACK_SIZE=16384` 为必须项，删除会导致栈溢出崩溃。

**Q: 刷卡录入或远程控制点按钮没反应，网页一直转圈？**
A: 通常是 MQTT 回调中发生线程死锁。确认 `server/web_app.py` 中 `live_lock` 使用了 `threading.RLock()`（可重入锁）而非 `threading.Lock()`。重启 Flask 服务即可恢复。

**Q: 仪表盘点开门/锁门，舵机实际动了但页面显示"发送失败"？**
A: CDN 模式下 `ElMessage` 需要写成 `ElementPlus.ElMessage`。已在最新版修复，刷新 `Ctrl+Shift+F5` 强制更新缓存。

**Q: SG90 舵机不转？**
A: 舵机必须接 5V 供电（ESP32 VIN 引脚），不能接 3.3V。接线：红线→5V(VIN)、棕线→GND、橙线→GPIO14。上电瞬间舵机会微微抖动一下表示初始化成功。

## 技术栈总结

| 层级 | 技术 |
|------|------|
| **感知层** | ESP32, C++/Arduino, FreeRTOS, MFRC522(SPI), DHT22(1-Wire), SSD1306(I2C), 传感器(GPIO/ADC/LEDC PWM) |
| **网络层** | WiFi 802.11 b/g/n, MQTT (Mosquitto 1884), TCP/IP, JSON, mbedtls |
| **应用层后端** | Python 3, Flask, SQLite, PyCryptodome, Paho-MQTT, difflib, OpenSSL |
| **应用层前端** | Vue 3, Element Plus, ECharts 5, Vue Router 4, Web Speech API, Web Crypto API |
| **信息安全** | HTTPS/TLS 1.3, AES-128-CBC, SHA-256, Session Auth, RBAC, PKCS7 |
| **通信协议** | SPI, I2C, 1-Wire, UART, MQTT, HTTPS/REST |
