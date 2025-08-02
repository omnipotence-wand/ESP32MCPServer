/**
 * LCD测试程序
 * 用于诊断LCD连接和显示问题
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 常见的I2C地址
uint8_t i2c_addresses[] = {0x27, 0x3F, 0x26, 0x20, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E};
int num_addresses = sizeof(i2c_addresses) / sizeof(i2c_addresses[0]);

LiquidCrystal_I2C* lcd = nullptr;
uint8_t found_address = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== LCD诊断测试程序 ===");
    Serial.println();
    
    // 初始化I2C
    Wire.begin();
    Serial.println("I2C总线已初始化");
    
    // 扫描I2C设备
    Serial.println("正在扫描I2C设备...");
    scanI2CDevices();
    
    // 尝试初始化LCD
    if (found_address != 0) {
        testLCD(found_address);
    } else {
        Serial.println("❌ 未找到I2C设备，请检查连接");
        Serial.println();
        printConnectionGuide();
    }
}

void loop() {
    if (lcd != nullptr) {
        // 显示动态内容
        static unsigned long lastUpdate = 0;
        static int counter = 0;
        
        if (millis() - lastUpdate > 1000) {
            lcd->setCursor(0, 1);
            lcd->printf("Count: %04d", counter++);
            lastUpdate = millis();
        }
    }
    
    // 处理串口命令
    if (Serial.available()) {
        char cmd = Serial.read();
        handleCommand(cmd);
    }
    
    delay(100);
}

void scanI2CDevices() {
    Serial.println("扫描I2C地址范围: 0x01-0x7F");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("✅ 发现I2C设备: 0x%02X\n", address);
            if (found_address == 0) {
                found_address = address; // 记录第一个找到的设备
            }
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("❌ 未发现任何I2C设备");
    } else {
        Serial.printf("总共发现 %d 个I2C设备\n", deviceCount);
    }
    Serial.println();
}

void testLCD(uint8_t address) {
    Serial.printf("正在测试LCD，地址: 0x%02X\n", address);
    
    // 创建LCD实例
    lcd = new LiquidCrystal_I2C(address, 16, 2);
    
    // 初始化LCD
    lcd->init();
    delay(100);
    
    // 开启背光
    lcd->backlight();
    delay(500);
    
    // 清屏
    lcd->clear();
    delay(100);
    
    // 显示测试信息
    lcd->setCursor(0, 0);
    lcd->print("LCD Test OK!");
    lcd->setCursor(0, 1);
    lcd->print("Addr: 0x");
    lcd->print(address, HEX);
    
    Serial.println("✅ LCD初始化成功！");
    Serial.println("如果LCD仍然没有显示，请尝试以下操作：");
    Serial.println("  b - 切换背光");
    Serial.println("  c - 对比度测试");
    Serial.println("  t - 显示测试文本");
    Serial.println("  r - 重置LCD");
    Serial.println();
}

void handleCommand(char cmd) {
    if (lcd == nullptr) {
        Serial.println("LCD未初始化");
        return;
    }
    
    switch (cmd) {
        case 'b':
            // 切换背光
            {
                static bool backlightOn = true;
                if (backlightOn) {
                    lcd->noBacklight();
                    Serial.println("背光已关闭");
                } else {
                    lcd->backlight();
                    Serial.println("背光已开启");
                }
                backlightOn = !backlightOn;
            }
            break;
            
        case 'c':
            // 对比度测试 - 显示全黑和全白块
            lcd->clear();
            lcd->setCursor(0, 0);
            // 显示全白块字符
            for (int i = 0; i < 16; i++) {
                lcd->write(255); // 全白块
            }
            lcd->setCursor(0, 1);
            lcd->print("Contrast Test");
            Serial.println("对比度测试 - 如果看不到白块，请调整对比度电位器");
            break;
            
        case 't':
            // 显示测试文本
            lcd->clear();
            lcd->setCursor(0, 0);
            lcd->print("Hello World!");
            lcd->setCursor(0, 1);
            lcd->printf("Time: %lu", millis()/1000);
            Serial.println("显示测试文本");
            break;
            
        case 'r':
            // 重置LCD
            lcd->clear();
            delay(100);
            lcd->home();
            lcd->setCursor(0, 0);
            lcd->print("LCD Reset");
            Serial.println("LCD已重置");
            break;
            
        default:
            if (cmd != '\n' && cmd != '\r') {
                Serial.println("未知命令");
            }
            break;
    }
}

void printConnectionGuide() {
    Serial.println("=== LCD连接指南 ===");
    Serial.println();
    Serial.println("标准I2C LCD 1602连接:");
    Serial.println("  VCC  → 5V 或 3.3V");
    Serial.println("  GND  → GND");
    Serial.println("  SDA  → GPIO21 (ESP32默认SDA)");
    Serial.println("  SCL  → GPIO22 (ESP32默认SCL)");
    Serial.println();
    Serial.println("故障排除:");
    Serial.println("1. 检查电源电压 (3.3V或5V)");
    Serial.println("2. 确认I2C连线正确");
    Serial.println("3. 检查LCD背面的对比度电位器");
    Serial.println("4. 尝试不同的I2C地址");
    Serial.println("5. 检查焊接连接");
    Serial.println();
    Serial.println("常见I2C地址: 0x27, 0x3F, 0x26, 0x20");
}