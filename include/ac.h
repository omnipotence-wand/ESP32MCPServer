#ifndef AC_H
#define AC_H

#include <Arduino.h>

#include <atomic>

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

// WiFi 显示状态 (用于LCD右上角状态图标)
enum WiFiDisplayState {
    WIFI_DISP_DISCONNECTED = 0,  // 未连接/连接失败
    WIFI_DISP_CONNECTING = 1,    // 连接中
    WIFI_DISP_CONNECTED = 2      // 已连接
};

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

    /* LCD 绘制只允许发生在主循环任务里。LovyanGFX 用 FreeRTOS 互斥量保护 SPI
     * 总线, 两个任务同时绘制时后结束的那个会在 spiEndTransaction 中 give 一把
     * 自己没有持有的互斥量, 触发 xQueueGenericSend 断言并复位。MCP 工具处理器
     * 跑在 ESP-MCP 的 worker 任务上(turnOn/turnOff/setMode/setTemperature 都会
     * 刷新显示), 因此它们只置位刷新请求, 由主循环完成实际绘制。 */
    std::atomic<bool> lcdRefreshRequested;  // 请求跳过更新间隔立即重绘
    std::atomic<bool> lcdClearRequested;    // 请求重绘前先清屏

    // 网络状态显示相关
    int wifiState;            // WiFi显示状态 (WiFiDisplayState)
    String wifiIpPort;        // 连接成功后的 "ip:port" 文本
    int lastDrawnWifiState;   // 上次绘制的WiFi状态, 用于避免图标重复重绘

    void drawWifiIcon();      // 绘制右上角WiFi状态图标

public:
    // 构造函数
    AirConditioner();
    
    // 协议中要求的获取描述的方法
    String description();
    String listTools();
    // 模式控制
    String setMode(int newMode);          // 设置工作模式, 返回错误信息(空串=成功)
    int getMode() const;                // 获取工作模式
    String getModeString() const;       // 获取工作模式字符串
    
    // 温度控制
    String setTemperature(int temp);      // 设置温度, 返回错误信息(空串=成功)
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
    
    // 网络状态显示 (由主循环同步, state 取 WiFiDisplayState, ipPort 形如 "192.168.1.100:9000")
    void setNetworkStatus(int state, const String& ipPort);

    // LCD显示功能 (除 forceLCDUpdate 外都会真正驱动SPI, 只能在主循环任务中调用)
    void clearLCD();
    bool initLCD(); // 初始化LCD
    void updateLCDDisplay();            // 更新LCD显示
    void enableLCD(bool enable);        // 启用/禁用LCD
    bool isLCDEnabled() const;          // 检查LCD是否启用
    void forceLCDUpdate();              // 请求立即刷新LCD (可在任意任务调用)
};

#endif // AC_H
