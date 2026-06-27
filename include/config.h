#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// 系统配置文件 - 根据实际环境修改以下参数
// ============================================

// --- WiFi 配置 ---
#define WIFI_SSID      "125"
#define WIFI_PASSWORD  "88888888"

// --- MQTT Broker 配置 ---
#define MQTT_BROKER    "10.30.106.101"
#define MQTT_PORT      1884
#define DEVICE_ID      "ESP32_GATE_001"

// --- MQTT Topics ---
#define TOPIC_AUTH     "rfid/auth"
#define TOPIC_ALARM    "rfid/alarm"
#define TOPIC_DOOR     "rfid/door"
#define TOPIC_SYSTEM   "rfid/system"
#define TOPIC_CMD      "rfid/cmd"      // 远程指令
#define TOPIC_USERS    "rfid/users"    // 用户白名单同步

// --- AES-128 密钥 (16 字节，生产环境中需保密) ---
#define AES_KEY_BYTES  \
    'R','F','I','D',   \
    '2','0','2','6',   \
    'S','e','c','r',   \
    'e','t','K','1'

// --- 人体检测超时 (ms) ---
#define HUMAN_TIMEOUT     1500


// --- 认证超时 (ms) ---
#define AUTH_TIMEOUT      10000

// --- 系统心跳间隔 (ms) ---
#define HEARTBEAT_INTERVAL 30000

// --- NTP 时间服务器 (东八区) ---
#define NTP_SERVER1  "ntp.aliyun.com"
#define NTP_SERVER2  "pool.ntp.org"
#define GMT_OFFSET   (8 * 3600)
#define DAYLIGHT_OFFSET 0

#endif
