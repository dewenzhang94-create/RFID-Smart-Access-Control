/**
 * buzzer.cpp - 蜂鸣器 + LED 控制模块
 * 有源蜂鸣器 + 红/绿双色 LED 指示灯
 */

#include "buzzer.h"

// --- 引脚定义 ---
#define BUZZER_PIN  25
#define GREEN_LED   32
#define RED_LED     33

// ============================================
// 初始化
// ============================================

void buzzerInit() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);

    Serial.println(F("BUZZER: Buzzer + LEDs initialized"));
}

// ============================================
// 报警: 红灯闪烁 + 蜂鸣器三短鸣
// ============================================

void alarm() {
    digitalWrite(RED_LED, HIGH);
    for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(150);
        digitalWrite(BUZZER_PIN, LOW);
        delay(80);
    }
    digitalWrite(RED_LED, LOW);
}

// ============================================
// 成功提示: 绿灯亮 + 蜂鸣器一短鸣
// ============================================

void successBeep() {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(GREEN_LED, LOW);
}

// ============================================
// LED 独立控制
// ============================================

void ledGreen(bool on) {
    digitalWrite(GREEN_LED, on ? HIGH : LOW);
}

void ledRed(bool on) {
    digitalWrite(RED_LED, on ? HIGH : LOW);
}