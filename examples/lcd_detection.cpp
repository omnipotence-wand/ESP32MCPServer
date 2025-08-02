/**
 * LCD检测和配置程序
 * 帮助确定LCD的正确参数和连接状态
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== LCD检测和配置程序 ===");
    Serial.println();
    
    // 步骤1: 检查I2C总线
    Serial.println("📡 步骤1: 初始化I2C总线");
    Wire.begin(21, 22); // SDA=21, SCL=22 (ESP32默认)
    Serial.println("✅ I2C总线已初始化 (SDA=GPIO21, SCL=GPIO22)");
    Serial.println();
    
    // 步骤2: 扫描I2C设备
    Serial.println("🔍 步骤2: 扫描I2C设备");
    scanI2CDevices();
    
    // 步骤3: 测试常见地址的LCD
    Serial.println("🖥️  步骤3: 测试LCD显示");
    testLCDAddresses();
    
    Serial.println();
    Serial.println("=== 检测完成 ===");
    Serial.println("如果找到工作的LCD，记录下地址和配置信息");
}

void loop() {
    // 空循环
    delay(1000);
}

void scanI2CDevices() {
    Serial.println("扫描I2C地址范围: 0x01 到 0x7F");
    Serial.println("地址  状态    可能的设备");
    Serial.println("----  ----    ----------");
    
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("0x%02X  找到    ", address);
            
            // 判断可能的设备类型
            switch (address) {
                case 0x27:
                case 0x3F:
                    Serial.println("LCD 1602/2004 (I2C)");
                    break;
                case 0x20:
                case 0x21:
                case 0x22:
                case 0x23:
                case 0x24:
                case 0x25:
                case 0x26:
                case 0x38:
                case 0x39:
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x3D:
                case 0x3E:
                    Serial.println("可能是LCD或其他I2C设备");
                    break;
                default:
                    Serial.println("未知I2C设备");
                    break;
            }
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("❌ 未发现任何I2C设备");
        Serial.println();
        Serial.println("请检查连接:");
        Serial.println("  1. VCC连接到3.3V或5V");
        Serial.println("  2. GND连接到GND");
        Serial.println("  3. SDA连接到GPIO21");
        Serial.println("  4. SCL连接到GPIO22");
        Serial.println("  5. 确认LCD模块是I2C接口");
    } else {
        Serial.printf("\n✅ 总共发现 %d 个I2C设备\n", deviceCount);
    }
    Serial.println();
}

void testLCDAddresses() {
    // 常见LCD I2C地址列表
    uint8_t addresses[] = {0x27, 0x3F, 0x26, 0x20, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E};
    int numAddresses = sizeof(addresses) / sizeof(addresses[0]);
    
    for (int i = 0; i < numAddresses; i++) {
        uint8_t addr = addresses[i];
        
        // 检查设备是否存在
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) {
            continue; // 设备不存在，跳过
        }
        
        Serial.printf("🧪 测试地址 0x%02X:\n", addr);
        
        // 尝试16x2配置
        if (testLCDConfiguration(addr, 16, 2)) {
            Serial.printf("✅ 地址 0x%02X 配置 16x2 - 工作正常!\n", addr);
            Serial.println("   建议使用此配置");
            Serial.println();
            continue;
        }
        
        // 尝试20x4配置
        if (testLCDConfiguration(addr, 20, 4)) {
            Serial.printf("✅ 地址 0x%02X 配置 20x4 - 工作正常!\n", addr);
            Serial.println("   建议使用此配置");
            Serial.println();
            continue;
        }
        
        Serial.printf("❌ 地址 0x%02X - LCD测试失败\n", addr);
        Serial.println();
    }
}

bool testLCDConfiguration(uint8_t address, int cols, int rows) {
    try {
        // 创建LCD实例
        LiquidCrystal_I2C lcd(address, cols, rows);
        
        // 初始化
        lcd.init();
        delay(100);
        
        // 开启背光
        lcd.backlight();
        delay(100);
        
        // 清屏
        lcd.clear();
        delay(50);
        
        // 显示测试文本
        lcd.setCursor(0, 0);
        lcd.print("LCD Test OK!");
        
        if (rows > 1) {
            lcd.setCursor(0, 1);
            lcd.printf("Addr:0x%02X %dx%d", address, cols, rows);
        }
        
        // 等待一下让用户观察
        delay(2000);
        
        // 清屏
        lcd.clear();
        
        Serial.printf("   测试文本已显示，如果LCD上显示了文字，则此配置有效\n");
        return true;
        
    } catch (...) {
        return false;
    }
}