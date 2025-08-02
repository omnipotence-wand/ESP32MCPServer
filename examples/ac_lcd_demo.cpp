/**
 * 空调LCD显示示例
 * 演示如何使用AirConditioner类的LCD显示功能
 */

#include <Arduino.h>
#include "ac.h"

// 创建空调实例
AirConditioner ac;

void setup() {
    Serial.begin(115200);
    Serial.println("空调LCD显示示例启动");
    
    // 初始化LCD显示器
    // 参数: I2C地址(默认0x27), 列数(默认16), 行数(默认2)
    if (ac.initLCD(0x27, 16, 2)) {
        Serial.println("LCD初始化成功");
    } else {
        Serial.println("LCD初始化失败");
        return;
    }
    
    // 设置初始状态
    ac.turnOn();
    ac.setMode(AC_MODE_COOL);
    ac.setTemperature(22);
    
    Serial.println("空调LCD显示示例准备完成");
    Serial.println("LCD将显示空调状态信息");
    Serial.println();
    Serial.println("命令说明:");
    Serial.println("1 - 开启空调");
    Serial.println("0 - 关闭空调");
    Serial.println("a - 自动模式");
    Serial.println("c - 制冷模式");
    Serial.println("h - 制热模式");
    Serial.println("d - 抽湿模式");
    Serial.println("+ - 温度+1");
    Serial.println("- - 温度-1");
    Serial.println("l - 切换LCD开关");
    Serial.println("s - 显示状态");
}

void loop() {
    // 定期更新LCD显示
    ac.updateLCDDisplay();
    
    // 处理串口命令
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case '1':
                ac.turnOn();
                Serial.println("空调已开启");
                break;
                
            case '0':
                ac.turnOff();
                Serial.println("空调已关闭");
                break;
                
            case 'a':
                ac.setMode(AC_MODE_AUTO);
                Serial.println("切换到自动模式");
                break;
                
            case 'c':
                ac.setMode(AC_MODE_COOL);
                Serial.println("切换到制冷模式");
                break;
                
            case 'h':
                ac.setMode(AC_MODE_HEAT);
                Serial.println("切换到制热模式");
                break;
                
            case 'd':
                ac.setMode(AC_MODE_DEHUMIDIFY);
                Serial.println("切换到抽湿模式");
                break;
                
            case '+':
                if (ac.getTemperature() < MAX_TEMPERATURE) {
                    ac.setTemperature(ac.getTemperature() + 1);
                }
                break;
                
            case '-':
                if (ac.getTemperature() > MIN_TEMPERATURE) {
                    ac.setTemperature(ac.getTemperature() - 1);
                }
                break;
                
            case 'l':
                ac.enableLCD(!ac.isLCDEnabled());
                Serial.println(ac.isLCDEnabled() ? "LCD已启用" : "LCD已禁用");
                break;
                
            case 's':
                Serial.println(ac.getFullStatus());
                Serial.println("JSON状态: " + ac.getStatusJSON());
                break;
                
            default:
                if (cmd != '\n' && cmd != '\r') {
                    Serial.println("未知命令: " + String(cmd));
                }
                break;
        }
    }
    
    delay(100); // 避免过度占用CPU
}