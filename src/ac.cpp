#include "ac.h"
#include <Arduino.h>
#include <Wire.h>

// 构造函数
AirConditioner::AirConditioner() {
    mode = AC_MODE_AUTO;
    temperature = 25;
    isRunning = false;
    lcd = nullptr;
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
            return "自动";
        case AC_MODE_COOL:
            return "制冷";
        case AC_MODE_HEAT:
            return "制热";
        case AC_MODE_DEHUMIDIFY:
            return "抽湿";
        default:
            return "未知";
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
    if (lcd != nullptr) {
        delete lcd; // 清理之前的LCD实例
    }
    uint8_t address = 0x20; // 默认I2C地址
    int cols = 240; // 默认列数     
    int rows = 320; // 默认行数
    Serial.printf("正在初始化LCD - 地址: 0x%02X, 尺寸: %dx%d\n", address, cols, rows);
    
    // 首先扫描I2C设备
    Wire.begin();
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("❌ I2C设备未找到，地址: 0x%02X (错误代码: %d)\n", address, error);
        Serial.println("请检查LCD连接:");
        Serial.println("  VCC → 5V 或 3.3V");
        Serial.println("  GND → GND");
        Serial.println("  SDA → GPIO21");
        Serial.println("  SCL → GPIO22");
        return false;
    }
    
    Serial.printf("✅ I2C设备已找到，地址: 0x%02X\n", address);
    
    // 创建LCD实例
    lcd = new LiquidCrystal_I2C(address, cols, rows);
    
    // 初始化LCD
    lcd->init();
    delay(100);
    
    // 开启背光
    lcd->backlight();
    delay(100);
    
    // 测试LCD显示
    lcd->clear();
    delay(50);
    
    lcd->setCursor(0, 0);
    lcd->print("AC System Init");
    lcd->setCursor(0, 1);
    lcd->print("Please wait...");
    
    Serial.println("✅ LCD显示测试完成");
    delay(2000);
    
    lcdEnabled = true;
    lastUpdate = 0; // 强制立即更新
    
    Serial.printf("✅ LCD初始化完成 - 地址: 0x%02X, 尺寸: %dx%d\n", address, cols, rows);
    
    // 显示初始状态
    forceLCDUpdate();
    return true;
}

// 更新LCD显示
void AirConditioner::updateLCDDisplay() {
    if (!lcdEnabled || lcd == nullptr) {
        return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastUpdate < UPDATE_INTERVAL) {
        return; // 还未到更新时间
    }
    
    lcd->clear();
    
    // 第一行：状态和模式
    lcd->setCursor(0, 0);
    if (isRunning) {
        lcd->print("ON ");
        // 显示模式简写
        switch (mode) {
            case AC_MODE_AUTO:
                lcd->print("AUTO");
                break;
            case AC_MODE_COOL:
                lcd->print("COOL");
                break;
            case AC_MODE_HEAT:
                lcd->print("HEAT");
                break;
            case AC_MODE_DEHUMIDIFY:
                lcd->print("DRY ");
                break;
        }
    } else {
        lcd->print("OFF     ");
    }
    
    // 显示当前时间（可选）
    lcd->setCursor(10, 0);
    unsigned long seconds = currentTime / 1000;
    lcd->printf("%02d:%02d", (int)((seconds / 60) % 60), (int)(seconds % 60));
    
    // 第二行：温度设置
    lcd->setCursor(0, 1);
    lcd->printf("Temp: %d", temperature);
    lcd->print((char)223); // 度数符号
    lcd->print("C");
    
    // 显示运行指示器
    if (isRunning) {
        lcd->setCursor(12, 1);
        // 简单的动画效果
        static int animFrame = 0;
        const char* anim = "|/-\\";
        lcd->print(anim[animFrame % 4]);
        animFrame++;
    }
    
    lastUpdate = currentTime;
}

// 启用/禁用LCD
void AirConditioner::enableLCD(bool enable) {
    lcdEnabled = enable;
    if (lcd != nullptr) {
        if (enable) {
            lcd->backlight();
            Serial.println("LCD已启用");
            forceLCDUpdate();
        } else {
            lcd->noBacklight();
            lcd->clear();
            Serial.println("LCD已禁用");
        }
    }
}

// 检查LCD是否启用
bool AirConditioner::isLCDEnabled() const {
    return lcdEnabled && (lcd != nullptr);
}

// 强制更新LCD显示
void AirConditioner::forceLCDUpdate() {
    if (!lcdEnabled || lcd == nullptr) {
        return;
    }
    
    lastUpdate = 0; // 重置更新时间，强制更新
    updateLCDDisplay();
}