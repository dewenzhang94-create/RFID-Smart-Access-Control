"""
config.example.py - 系统配置文件模板
复制此文件为 config.py 并填入实际参数
"""
# --- MQTT Broker ---
MQTT_BROKER = "10.30.106.101"   # Mosquitto 服务器地址
MQTT_PORT = 1884
MQTT_KEEPALIVE = 60

# --- MQTT 订阅 Topics ---
MQTT_TOPICS = [
    "rfid/auth",
    "rfid/alarm",
    "rfid/door",
    "rfid/system",
]

# --- SQLite 数据库 ---
DB_FILE = "rfid.db"

# --- AES-128 密钥 (与 ESP32 端 config.h 保持一致) ---
AES_KEY = b"RFID2026SecretK1"  # 16 bytes

# --- 日志级别 ---
LOG_LEVEL = "INFO"

# --- 是否打印原始密文 (调试用) ---
DEBUG_PRINT_CIPHER = True

# --- DeepSeek AI 语音识别 (可选, 不填则用本地规则匹配) ---
DEEPSEEK_API_KEY = ""  # 在此填入你的 API Key