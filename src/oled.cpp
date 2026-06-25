/**
 * oled.cpp - OLED 显示模块
 * SSD1306 128x64 I2C 单色显示屏
 */

#include "oled.h"

// --- 硬件定义 ---
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_ADDR      0x3C
#define OLED_SDA       16
#define OLED_SCL       17

// --- Adafruit 显示屏实例 ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================
// 初始化
// ============================================

void oledInit() {
    Wire.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("OLED: SSD1306 allocation failed!"));
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.display();

    Serial.println(F("OLED: SSD1306 initialized"));
}

// ============================================
// 单行显示 (自动根据文字长度选择字号)
// ============================================

void showMessage(String msg) {
    display.clearDisplay();
    display.setTextWrap(false);

    int size = (msg.length() > 10) ? 1 : 2;
    display.setTextSize(size);

    int16_t x, y;
    uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.println(msg);
    display.display();
}

// ============================================
// 两行显示
// ============================================

void showMessage(String line1, String line2) {
    display.clearDisplay();
    display.setTextWrap(false);

    // 第一行: 小字顶部居中
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w1, h1;
    display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    display.setCursor((SCREEN_WIDTH - w1) / 2, 0);
    display.println(line1);

    // 第二行: 根据长度自适应
    int size2 = (line2.length() > 8) ? 1 : 2;
    display.setTextSize(size2);
    int16_t x2, y2;
    uint16_t w2, h2;
    display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
    int yPos = 8 + h1 + ((SCREEN_HEIGHT - 8 - h1 - h2) / 2);
    if (yPos < 12) yPos = 12;
    display.setCursor((SCREEN_WIDTH - w2) / 2, yPos);
    display.println(line2);

    display.display();
}

// ============================================
// 三行显示
// ============================================

void showMessage(String line1, String line2, String line3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextWrap(false);

    display.setCursor(0, 0);
    display.println(line1);

    display.setTextSize(1);
    display.setCursor(0, 22);
    display.println(line2);

    display.setTextSize(1);
    display.setCursor(0, 46);
    display.println(line3);

    display.display();
}

// ============================================
// 清屏
// ============================================

void clearDisplay() {
    display.clearDisplay();
    display.display();
}
