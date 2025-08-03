#include "ac.h"
#include <Arduino.h>
#include <Wire.h>
#include "uart.h"
#include "xl9555.h"
#include "spilcd.h"

// 构造函数
AirConditioner::AirConditioner() {
    mode = AC_MODE_DEHUMIDIFY;
    temperature = 25;
    isRunning = true;
    lcdEnabled = false;
    lastUpdate = 0;
    Serial.println("空调系统初始化完成");
}

// 设置工作模式
bool AirConditioner::setMode(int newMode) {
    if (newMode < AC_MODE_AUTO || newMode > AC_MODE_DEHUMIDIFY) {
        Serial.printf("无效的工作模式: %d\n", newMode);
        return false;
    }
    
    mode = newMode;
    Serial.printf("空调模式已设置为: %s\n", getModeString().c_str());
    forceLCDUpdate(); // 立即更新LCD显示
    return true;
}

// 获取工作模式
int AirConditioner::getMode() const {
    return mode;
}

// 获取工作模式字符串
String AirConditioner::getModeString() const {
    switch (mode) {
        case AC_MODE_AUTO:
            return "auto";
        case AC_MODE_COOL:
            return "cool";
        case AC_MODE_HEAT:
            return "heat";
        case AC_MODE_DEHUMIDIFY:
            return "humdify";
        default:
            return "unknown";
    }
}

// 设置温度
bool AirConditioner::setTemperature(int temp) {
    if (temp < MIN_TEMPERATURE || temp > MAX_TEMPERATURE) {
        Serial.printf("温度超出范围: %d (范围: %d-%d)\n", temp, MIN_TEMPERATURE, MAX_TEMPERATURE);
        return false;
    }
    
    temperature = temp;
    Serial.printf("空调温度已设置为: %d°C\n", temperature);
    forceLCDUpdate(); // 立即更新LCD显示
    return true;
}

// 获取温度
int AirConditioner::getTemperature() const {
    return temperature;
}

// 开启空调
bool AirConditioner::turnOn() {
    if (isRunning) {
        Serial.println("空调已经在运行中");
        return true;
    }
    
    isRunning = true;
    Serial.printf("空调已开启 - 模式: %s, 温度: %d°C\n", getModeString().c_str(), temperature);
    forceLCDUpdate(); // 立即更新LCD显示
    return true;
}

// 关闭空调
bool AirConditioner::turnOff() {
    if (!isRunning) {
        Serial.println("空调已经关闭");
        return true;
    }
    
    isRunning = false;
    Serial.println("空调已关闭");
    forceLCDUpdate(); // 立即更新LCD显示
    return true;
}

// 获取工作状态
bool AirConditioner::getRunningStatus() const {
    return isRunning;
}

// 获取状态字符串
String AirConditioner::getStatusString() const {
    return isRunning ? "运行中" : "已关闭";
}

// 获取完整状态信息
String AirConditioner::getFullStatus() const {
    String status = "空调状态:\n";
    status += "  工作状态: " + getStatusString() + "\n";
    status += "  工作模式: " + getModeString() + "\n";
    status += "  设定温度: " + String(temperature) + "°C\n";
    return status;
}

// 重置为默认设置
void AirConditioner::reset() {
    mode = AC_MODE_AUTO;
    temperature = 25;
    isRunning = false;
    Serial.println("空调已重置为默认设置");
}

// 获取JSON格式的状态信息
String AirConditioner::getStatusJSON() const {
    String json = "{";
    json += "\"running\":" + String(isRunning ? "true" : "false") + ",";
    json += "\"mode\":" + String(mode) + ",";
    json += "\"modeString\":\"" + getModeString() + "\",";
    json += "\"temperature\":" + String(temperature);
    json += "}";
    return json;
}

// 从JSON字符串设置状态
bool AirConditioner::setFromJSON(const String& jsonStr) {
    // 这里可以实现JSON解析和设置状态
    // 为了简化，这里只是一个占位符
    Serial.println("从JSON设置状态: " + jsonStr);
    return true;
}

// ===== LCD显示功能实现 =====

// 初始化LCD
bool AirConditioner::initLCD() {
    Serial.println("正在初始化LCD...");
    
    try {
        uart_init(0, 115200);   /* 串口0初始化 */
        xl9555_init();          /* IO扩展芯片初始化 */
        lcd_init();             /* LCD初始化 */
    
        /* 刷屏测试 */
        lcd_clear(BLACK);
        delay(500);
        lcd_clear(WHITE);
        delay(500);

        lcdEnabled = true; // LCD启用
        lastUpdate = 0; // 重置更新时间
        return true;
        
    } catch (...) {
        Serial.println("❌ LCD初始化失败");
        lcdEnabled = false;
        return false;
    }
}

// 更新LCD显示
void AirConditioner::updateLCDDisplay() {
    if (!lcdEnabled) {
        return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastUpdate < UPDATE_INTERVAL) {
        return; // 还未到更新时间
    }
    
    // 清屏
    // 显示标题
    lcd_show_string(10, 0, 250, 32, LCD_FONT_32, "Air Conditioner", BLACK);
    
    // 显示状态和模式
    char statusStr[128];
    if (isRunning) {
        sprintf(statusStr, "Status: ON Mode: %s", getModeString().c_str());
    } else {
        sprintf(statusStr, "Status: OFF");
    }
    lcd_show_string(10, 32, 300, 24, LCD_FONT_24, statusStr, BLACK);
    
    // 显示温度
    char tempStr[32];
    sprintf(tempStr, "Temperature: %d C", temperature);
    lcd_show_string(10, 56, 200, 24, LCD_FONT_24, tempStr, BLACK);
    
    // 显示运行指示器
    if (isRunning) {
        static int animFrame = 0;
        const char* anim = "|/-\\";
        char animStr[16];
        sprintf(animStr, "Running: %c", anim[animFrame % 4]);
        lcd_show_string(10, 188, 200, 24, LCD_FONT_16, animStr, BLACK);
        animFrame++;
    }
    // 显示当前时间
    char timeStr[32];
    unsigned long currentTimeSeconds = currentTime / 1000;
    unsigned long hours = (currentTimeSeconds / 3600) % 24;
    unsigned long minutes = (currentTimeSeconds / 60) % 60;
    unsigned long seconds = currentTimeSeconds % 60;
    sprintf(timeStr, "Time: %02lu:%02lu:%02lu", hours, minutes, seconds);
    lcd_show_string(10, 212, 200, 24, LCD_FONT_16, timeStr, BLACK);
    
    lastUpdate = currentTime;
}

// 启用/禁用LCD
void AirConditioner::enableLCD(bool enable) {
    lcdEnabled = enable;
    if (enable) {
        Serial.println("LCD显示已启用");
        forceLCDUpdate();
    } else {
        lcd_clear(BLACK);
        Serial.println("LCD显示已禁用");
    }
}

// 检查LCD是否启用
bool AirConditioner::isLCDEnabled() const {
    return lcdEnabled;
}

// 强制更新LCD显示
void AirConditioner::forceLCDUpdate() {
    if (!lcdEnabled) {
        return;
    }
    
    lastUpdate = 0; // 重置更新时间，强制更新
    updateLCDDisplay();
}