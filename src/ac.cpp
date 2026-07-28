
#include "ac.h"
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "uart.h"
#include "xl9555.h"
#include "lcd.h"

// 构造函数
AirConditioner::AirConditioner() {
    mode = AC_MODE_AUTO; // 默认模式为自动
    temperature = 25;
    isRunning = false;
    lcdEnabled = false;
    lastUpdate = 0;
    lcdRefreshRequested = false;
    lcdClearRequested = false;
    wifiState = WIFI_DISP_DISCONNECTED;
    wifiIpPort = "";
    lastDrawnWifiState = -1;
    Serial.println("空调系统初始化完成");
}

String AirConditioner::description() {
    return "{\"device_type\":\"空调\",\"brand\":\"美的\",\"model\":\"KFR-35GW/N8XHA1\",\"description\":\"这是一个美的空调\",\"alias\":\"次卧空调\"}";
}

String AirConditioner::listTools() {
    JsonDocument doc;
    JsonObject result = doc.to<JsonObject>();
    JsonArray tools = result["tools"].to<JsonArray>();

    // 开启空调工具
    JsonObject turnOnTool = tools.add<JsonObject>();
    turnOnTool["name"] = "turnOn";
    turnOnTool["description"] = "开启空调";
    
    JsonObject turnOnInputSchema = turnOnTool["inputSchema"].to<JsonObject>();
    turnOnInputSchema["type"] = "object";
    turnOnInputSchema["properties"].to<JsonObject>();
    

    // 关闭空调工具
    JsonObject turnOffTool = tools.add<JsonObject>();
    turnOffTool["name"] = "turnOff";
    turnOffTool["description"] = "关闭空调";
    
    JsonObject turnOffInputSchema = turnOffTool["inputSchema"].to<JsonObject>();
    turnOffInputSchema["type"] = "object";
    turnOffInputSchema["properties"].to<JsonObject>();
    

    String output;
    serializeJson(doc, output);
    return output;
}

/*
    设定空调工作模式
    说明：
        如果空调没有处于开机模式，需要先开机
    参数：
        mode: 空调工作模式，0表示自动，1表示制冷，2表示制热，3表示抽湿
    返回：
        错误信息字符串，空串表示设置成功
*/
String AirConditioner::setMode(int newMode) {
    if (newMode < AC_MODE_AUTO || newMode > AC_MODE_DEHUMIDIFY) {
        return "invalid mode (expected 0-3)";
    }
    if (!isRunning) {
        return "air conditioner is off; call turnOn first";
    }
    mode = newMode;
    Serial.printf("空调模式已设置为: %s\n", getModeString().c_str());
    forceLCDUpdate(); // 立即更新LCD显示
    return "";
}

/*
    获取空调工作模式
    返回：
        空调工作模式，0表示自动，1表示制冷，2表示制热，3表示抽湿
*/
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

/*
    设定空调温度
    说明：
        如果空调没有处于开机模式，需要先开机
    返回：
        错误信息字符串，空串表示设置成功
*/
String AirConditioner::setTemperature(int temp) {
    if (temp < MIN_TEMPERATURE || temp > MAX_TEMPERATURE) {
        Serial.printf("温度超出范围: %d (范围: %d-%d)\n", temp, MIN_TEMPERATURE, MAX_TEMPERATURE);
        return "temperature out of range (expected 16-30)";
    }
    if (!isRunning) {
        Serial.println("空调未开启，请先开启空调");
        return "air conditioner is off; call turnOn first";
    }
    temperature = temp;
    Serial.printf("空调温度已设置为: %d°C\n", temperature);
    forceLCDUpdate(); // 立即更新LCD显示
    return "";
}

// 获取温度
int AirConditioner::getTemperature() const {
    return temperature;
}

/*
    开启空调
*/
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

/*
    关闭空调

*/
bool AirConditioner::turnOff() {
    if (!isRunning) {
        Serial.println("空调已经关闭");
        return true;
    }
    
    isRunning = false;
    lcdClearRequested.store(true, std::memory_order_relaxed); // 关机后整屏重绘, 清掉开机时的大字温度
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

/*
    获取空调状态JSON
    返回：
        running: true 为开机状态, false 为关机状态
        mode: 空调工作模式，0表示自动，1表示制冷，2表示制热，3表示抽湿
        temperature: 表示空调目标温度
*/
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

// 设置网络状态 (主循环中调用, 状态变化时立即刷新LCD)
void AirConditioner::setNetworkStatus(int state, const String& ipPort) {
    if (state == wifiState && ipPort == wifiIpPort) {
        return;
    }
    wifiState = state;
    wifiIpPort = ipPort;
    forceLCDUpdate();
}

// ===== LCD显示功能实现 =====

// 初始化LCD
bool AirConditioner::initLCD() {
    Serial.println("正在初始化LCD...");
    
    try {
        uart_init(0, 115200);   /* 串口0初始化 */
        xl9555_init();          /* IO扩展芯片初始化 */
        lcd_hw_init();          /* LCD初始化 */

        /* 刷屏测试 */
        lcd.fillScreen(TFT_BLACK);
        delay(500);
        lcd.fillScreen(TFT_WHITE);
        delay(500);

        lcdEnabled = true; // LCD启用
        lastUpdate = 0; // 重置更新时间
        lastDrawnWifiState = -1; // 屏幕为空白, 首次更新时绘制WiFi图标
        return true;
        
    } catch (...) {
        Serial.println("❌ LCD初始化失败");
        lcdEnabled = false;
        return false;
    }
}

void AirConditioner::clearLCD() {
        lcd.fillScreen(TFT_WHITE);
        lastDrawnWifiState = -1; // 全屏已清空, 下次更新时重绘WiFi图标
}

// 绘制右上角WiFi状态图标 (三段圆弧 + 底部圆点)
void AirConditioner::drawWifiIcon() {
    if (wifiState == lastDrawnWifiState) {
        return; // 状态未变化不重绘, 避免闪烁
    }

    const int cx = 298;  /* 图标基准点(底部圆点圆心) */
    const int cy = 32;

    // 清除旧图标区域
    lcd.fillRect(cx - 18, cy - 18, 37, 23, TFT_WHITE);

    // 已连接绿色, 连接中深灰, 未连接浅灰+红色斜线
    uint16_t color;
    switch (wifiState) {
        case WIFI_DISP_CONNECTED:  color = TFT_DARKGREEN; break;
        case WIFI_DISP_CONNECTING: color = TFT_DARKGREY;  break;
        default:                   color = TFT_LIGHTGREY; break;
    }

    lcd.fillCircle(cx, cy, 2, color);
    lcd.fillArc(cx, cy, 7, 5, 225, 315, color);
    lcd.fillArc(cx, cy, 12, 10, 225, 315, color);
    lcd.fillArc(cx, cy, 17, 15, 225, 315, color);

    if (wifiState == WIFI_DISP_DISCONNECTED) {
        lcd.drawLine(cx - 13, cy - 16, cx + 13, cy + 2, TFT_RED);
        lcd.drawLine(cx - 13, cy - 15, cx + 13, cy + 3, TFT_RED);
    }

    lastDrawnWifiState = wifiState;
}

// 更新LCD显示
void AirConditioner::updateLCDDisplay() {
    if (!lcdEnabled) {
        return;
    }
    
    // 其它任务(MCP worker)的状态变更在这里兑现为一次绘制, 保证只有主循环碰SPI
    const bool forced = lcdRefreshRequested.exchange(false, std::memory_order_relaxed);

    unsigned long currentTime = millis();
    if (!forced && currentTime - lastUpdate < UPDATE_INTERVAL) {
        return; // 还未到更新时间
    }

    if (lcdClearRequested.exchange(false, std::memory_order_relaxed)) {
        clearLCD();
    }

    // 文字带背景色绘制, 变长行用 setTextPadding 清除旧内容残留
    // Font7 为七段数码管字体, 仅含数字和冒号, 带字母的行用 Font2/Font4
    lcd.setTextColor(TFT_BLACK, TFT_WHITE);

    // 头部: 标题 + 右上角WiFi图标 + 分隔线
    lcd.setFont(&fonts::Orbitron_Light_24);
    lcd.drawString("Air Conditioner", 10, 8);
    lcd.drawFastHLine(10, 44, 300, TFT_BLACK);
    drawWifiIcon();

    // 主体左右分栏: 左栏状态/模式/网络地址(小标签+值), 右栏大字温度
    lcd.drawFastVLine(160, 56, 140, TFT_BLACK);

    // 左栏: 状态
    lcd.setTextPadding(145);
    lcd.setFont(&fonts::Font2);
    lcd.drawString("STATUS", 10, 58);
    lcd.setFont(&fonts::Font4);
    lcd.drawString(isRunning ? "ON" : "OFF", 10, 76);

    // 左栏: 模式 (关机显示 "--")
    lcd.setFont(&fonts::Font2);
    lcd.drawString("MODE", 10, 112);
    lcd.setFont(&fonts::Font4);
    lcd.drawString(isRunning ? getModeString() : String("--"), 10, 130);

    // 左栏: 网络地址, WiFi连接成功后显示HTTP MCP服务地址 "ip:port"
    lcd.setFont(&fonts::Font2);
    lcd.drawString("NETWORK", 10, 166);
    bool wifiReady = (wifiState == WIFI_DISP_CONNECTED && wifiIpPort.length() > 0);
    lcd.drawString(wifiReady ? wifiIpPort : String("--"), 10, 184);

    // 右栏: 设定温度(七段数码管放大2倍, 主视觉), 仅开机时显示
    if (isRunning) {
        char tempStr[16];
        sprintf(tempStr, "%d", temperature);
        lcd.setFont(&fonts::Font7);
        lcd.setTextSize(2.0f);
        lcd.setTextPadding(132);
        lcd.drawRightString(tempStr, 296, 66);
        lcd.setTextSize(1);
        lcd.setFont(&fonts::Font4);
        lcd.setTextPadding(0);
        lcd.drawString("C", 300, 68);
    } else {
        lcd.fillRect(166, 56, 154, 140, TFT_WHITE); /* 关机时清空右栏, 不碰分隔线 */
    }

    // 底部小字: 运行指示器 + 启动时间
    lcd.setFont(&fonts::Font2);
    lcd.setTextPadding(140);
    if (isRunning) {
        static int animFrame = 0;
        const char* anim = "|/-\\";
        char animStr[16];
        sprintf(animStr, "Running: %c", anim[animFrame % 4]);
        lcd.drawString(animStr, 10, 216);
        animFrame++;
    } else {
        lcd.drawString("Standby", 10, 216);
    }
    char timeStr[32];
    unsigned long currentTimeSeconds = currentTime / 1000;
    unsigned long hours = (currentTimeSeconds / 3600) % 24;
    unsigned long minutes = (currentTimeSeconds / 60) % 60;
    unsigned long seconds = currentTimeSeconds % 60;
    sprintf(timeStr, "Up %02lu:%02lu:%02lu", hours, minutes, seconds);
    lcd.drawRightString(timeStr, 310, 216);
    lcd.setTextPadding(0);

    lastUpdate = currentTime;
}

// 启用/禁用LCD
void AirConditioner::enableLCD(bool enable) {
    lcdEnabled = enable;
    if (enable) {
        Serial.println("LCD显示已启用");
        forceLCDUpdate();
    } else {
        lcd.fillScreen(TFT_BLACK);
        Serial.println("LCD显示已禁用");
    }
}

// 检查LCD是否启用
bool AirConditioner::isLCDEnabled() const {
    return lcdEnabled;
}

// 请求立即刷新LCD。可在任意任务调用: 这里只置位, 绘制由主循环的
// updateLCDDisplay() 完成, 因此 SPI 总线始终只被一个任务使用。
void AirConditioner::forceLCDUpdate() {
    if (!lcdEnabled) {
        return;
    }

    lcdRefreshRequested.store(true, std::memory_order_relaxed);
}