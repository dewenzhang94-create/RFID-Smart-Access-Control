/**
 * wifi_manager.cpp - WiFi 连接管理 + NTP 时间同步
 */

#include "wifi_manager.h"
#include <time.h>

// --- 连接重试参数 ---
#define WIFI_RETRY_MAX     40    // 最大重试次数
#define WIFI_RETRY_DELAY   500   // 重试间隔 (ms)

// ============================================
// WiFi 连接
// ============================================

void connectWifi() {
    Serial.printf("WiFi: Connecting to %s", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_MAX) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi: Connected!");
        Serial.print("WiFi: IP = ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi: Connection failed, continuing anyway...");
        return;
    }

    // === NTP 时间同步 ===
    configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
    Serial.print("NTP: Syncing time");

    struct tm timeinfo;
    int ntpRetry = 0;
    while (!getLocalTime(&timeinfo) && ntpRetry < 10) {
        delay(500);
        Serial.print(".");
        ntpRetry++;
    }

    if (getLocalTime(&timeinfo)) {
        Serial.println("\nNTP: Time synced successfully");
        char buf[30];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.printf("NTP: Current time = %s\n", buf);
    } else {
        Serial.println("\nNTP: Time sync failed, using default time");
    }
}

// ============================================
// WiFi 状态查询
// ============================================

bool isWifiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

// ============================================
// 获取当前时间戳 (ISO 8601 格式)
// ============================================

String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return String("1970-01-01T00:00:00");
    }
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(buffer);
}
