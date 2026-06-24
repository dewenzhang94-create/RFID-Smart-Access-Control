/**
 * oled.cpp - OLED 显示模块
 * SSD1306 128x64 I2C 单色显示屏
 */

#include "oled.h"

// --- 硬件定义 ---
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_ADDR      0x3C
#define OLED_SDA       21
#define OLED_SCL       22

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
// 单行显示
// ============================================

void showMessage(String msg) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println(msg);
    display.display();
}

// ============================================
// 两行显示
// ============================================

void showMessage(String line1, String line2) {
    display.clearDisplay();

    // 第一行：小字居中
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w1, h1;
    display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    display.setCursor((SCREEN_WIDTH - w1) / 2, 8);
    display.println(line1);

    // 第二行：大字居中
    display.setTextSize(2);
    int16_t x2, y2;
    uint16_t w2, h2;
    display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
    display.setCursor((SCREEN_WIDTH - w2) / 2, 34);
    display.println(line2);

    display.display();
}

// ============================================
// 三行显示
// ============================================

void showMessage(String line1, String line2, String line3) {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println(line1);

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.println(line2);

    display.setTextSize(1);
    display.setCursor(0, 52);
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
