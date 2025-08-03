#ifndef AC_H
#define AC_H

#include <Arduino.h>

// 空调工作模式枚举
enum ACMode {
    AC_MODE_AUTO = 0,        // 自动模式
    AC_MODE_COOL = 1,        // 制冷模式
    AC_MODE_HEAT = 2,        // 制热模式
    AC_MODE_DEHUMIDIFY = 3   // 抽湿模式
};

// 温度范围常量
const int MIN_TEMPERATURE = 16;  // 最低温度
const int MAX_TEMPERATURE = 30;  // 最高温度

/**
 * 空调模拟类
 * 提供空调的基本功能和状态管理
 */
class AirConditioner {
private:
    int mode;           // 工作模式 (使用ACMode枚举)
    int temperature;    // 设定温度
    bool isRunning;     // 工作状态 (true=运行中, false=已关闭)
    
    // LCD相关
    bool lcdEnabled;         // LCD是否启用
    unsigned long lastUpdate; // 上次更新时间
    static const unsigned long UPDATE_INTERVAL = 1000; // 更新间隔(ms)

public:
    // 构造函数
    AirConditioner();
    
    // 协议中要求的获取描述的方法
    String description();
    String listTools();
    // 模式控制
    String setMode(int newMode);          // 设置工作模式
    int getMode() const;                // 获取工作模式
    String getModeString() const;       // 获取工作模式字符串
    
    // 温度控制
    String setTemperature(int temp);      // 设置温度
    int getTemperature() const;         // 获取温度
    
    // 电源控制
    bool turnOn();                      // 开启空调
    bool turnOff();                     // 关闭空调
    bool getRunningStatus() const;      // 获取工作状态
    String getStatusString() const;     // 获取状态字符串
    
    // 状态信息
    String getFullStatus() const;       // 获取完整状态信息
    void reset();                       // 重置为默认设置
    
    // JSON接口
    String getStatusJSON() const;       // 获取JSON格式的状态信息
    bool setFromJSON(const String& jsonStr); // 从JSON字符串设置状态
    
    // LCD显示功能
    void clearLCD();
    bool initLCD(); // 初始化LCD
    void updateLCDDisplay();            // 更新LCD显示
    void enableLCD(bool enable);        // 启用/禁用LCD
    bool isLCDEnabled() const;          // 检查LCD是否启用
    void forceLCDUpdate();              // 强制更新LCD显示
};

#endif // AC_H
