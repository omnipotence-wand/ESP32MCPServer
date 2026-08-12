
#include "ac.h"
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "uart.h"
#include "xl9555.h"
#include "lcd.h"

namespace {
int remainingMinutes(bool active, unsigned long deadline) {
    if (!active) return 0;
    const long remainingMs = static_cast<long>(deadline - millis());
    if (remainingMs <= 0) return 0;
    return (remainingMs + 59999L) / 60000L;
}

const char* faultName(int code) {
    switch (code) {
        case 1: return "temperature_sensor";
        case 2: return "compressor";
        case 3: return "overheat";
        default: return "none";
    }
}
}

// 构造函数
AirConditioner::AirConditioner() {
    mode = AC_MODE_AUTO; // 默认模式为自动
    temperature = 25;
    isRunning = false;
    fanSpeed = AC_FAN_AUTO;
    verticalSwing = false;
    horizontalSwing = false;
    verticalDirection = 2;
    horizontalDirection = 2;
    sleepMode = false;
    ecoMode = false;
    turboMode = false;
    quietMode = false;
    displayLight = true;
    beepEnabled = true;
    childLock = false;
    antiDirectBlow = false;
    auxiliaryHeat = false;
    mildewProof = false;
    selfClean = false;
    roomTemperatureTenths = 280;
    roomHumidityTenths = 600;
    compressorRunning = false;
    powerWatts = 0;
    runtimeSeconds = 0;
    filterLifePercent = 100;
    faultCode = 0;
    turnOnTimerActive = false;
    turnOffTimerActive = false;
    turnOnAt = 0;
    turnOffAt = 0;
    lastSimulationUpdate = millis();
    lcdEnabled = false;
    lastUpdate = 0;
    lcdPage = 0;
    lcdPageHoldUntil = 0;
    lcdHighlightVisible = false;
    lastLCDOnTimerMinutes = -1;
    lastLCDOffTimerMinutes = -1;
    lcdRefreshRequested = false;
    wifiState = WIFI_DISP_DISCONNECTED;
    wifiIpPort = "";
    lastDrawnWifiState = -1;
    Serial.println("空调系统初始化完成");
}

String AirConditioner::description() {
    return "{\"device_type\":\"空调\",\"brand\":\"美的\",\"model\":\"KFR-35GW/N8XHA1\",\"description\":\"这是一个美的空调\",\"alias\":\"次卧空调\"}";
}

/*
    设定空调工作模式
    说明：
        如果空调没有处于开机模式，需要先开机
    参数：
        mode: 空调工作模式，0自动，1制冷，2制热，3抽湿，4送风
    返回：
        错误信息字符串，空串表示设置成功
*/
String AirConditioner::setMode(int newMode) {
    if (newMode < AC_MODE_AUTO || newMode > AC_MODE_FAN) {
        return "invalid mode (expected 0-4)";
    }
    if (!isRunning) {
        return "air conditioner is off; call turnOn first";
    }
    mode = newMode;
    if (newMode != AC_MODE_HEAT) auxiliaryHeat = false;
    Serial.printf("空调模式已设置为: %s\n", getModeString().c_str());
    selectLCDPage(0);
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
            return "dehumidify";
        case AC_MODE_FAN:
            return "fan";
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
    Serial.printf("空调温度已设置为: %d°C\n", temperature.load());
    selectLCDPage(0);
    return "";
}

// 获取温度
int AirConditioner::getTemperature() const {
    return temperature;
}

String AirConditioner::setFanSpeed(int speed) {
    if (speed < AC_FAN_AUTO || speed > AC_FAN_QUIET) {
        return "invalid fan speed (expected 0-5)";
    }
    fanSpeed = speed;
    turboMode = speed == AC_FAN_TURBO;
    quietMode = speed == AC_FAN_QUIET;
    if (turboMode) ecoMode = false;
    selectLCDPage(1);
    return "";
}

String AirConditioner::getFanSpeedString() const {
    switch (fanSpeed.load()) {
        case AC_FAN_AUTO: return "auto";
        case AC_FAN_LOW: return "low";
        case AC_FAN_MEDIUM: return "medium";
        case AC_FAN_HIGH: return "high";
        case AC_FAN_TURBO: return "turbo";
        case AC_FAN_QUIET: return "quiet";
        default: return "unknown";
    }
}

String AirConditioner::setSwing(bool vertical, bool horizontal) {
    verticalSwing = vertical;
    horizontalSwing = horizontal;
    selectLCDPage(1);
    return "";
}

String AirConditioner::setAirDirection(int vertical, int horizontal) {
    if (vertical < 0 || vertical > 4 || horizontal < 0 || horizontal > 4) {
        return "air direction out of range (expected 0-4)";
    }
    verticalDirection = vertical;
    horizontalDirection = horizontal;
    verticalSwing = false;
    horizontalSwing = false;
    selectLCDPage(1);
    return "";
}

String AirConditioner::setFeature(const String& feature, bool enabled) {
    if (feature == "sleep") sleepMode = enabled;
    else if (feature == "eco") {
        ecoMode = enabled;
        if (enabled) {
            turboMode = false;
            if (fanSpeed == AC_FAN_TURBO) fanSpeed = AC_FAN_AUTO;
        }
    } else if (feature == "turbo") {
        turboMode = enabled;
        if (enabled) {
            quietMode = false;
            ecoMode = false;
            fanSpeed = AC_FAN_TURBO;
        } else if (fanSpeed == AC_FAN_TURBO) fanSpeed = AC_FAN_AUTO;
    } else if (feature == "quiet") {
        quietMode = enabled;
        if (enabled) {
            turboMode = false;
            fanSpeed = AC_FAN_QUIET;
        } else if (fanSpeed == AC_FAN_QUIET) fanSpeed = AC_FAN_AUTO;
    } else if (feature == "display_light") displayLight = enabled;
    else if (feature == "beep") beepEnabled = enabled;
    else if (feature == "child_lock") childLock = enabled;
    else if (feature == "anti_direct_blow") antiDirectBlow = enabled;
    else if (feature == "auxiliary_heat") {
        if (enabled && mode != AC_MODE_HEAT) return "auxiliary heat requires heat mode";
        auxiliaryHeat = enabled;
    } else if (feature == "mildew_proof") mildewProof = enabled;
    else if (feature == "self_clean") {
        if (enabled && isRunning) return "self clean can only start while powered off";
        selfClean = enabled;
    } else return "unknown feature";
    const int featureIcon = feature == "sleep" ? 0 : feature == "eco" ? 1 :
                            feature == "turbo" ? 2 : feature == "quiet" ? 3 :
                            feature == "display_light" ? 4 : feature == "beep" ? 5 :
                            feature == "child_lock" ? 6 : feature == "anti_direct_blow" ? 7 :
                            feature == "auxiliary_heat" ? 8 : feature == "mildew_proof" ? 9 : 10;
    selectLCDPage(10 + featureIcon);
    return "";
}

String AirConditioner::setTimer(bool onTimer, int delayMinutes) {
    if (delayMinutes < 1 || delayMinutes > 1440) {
        return "delayMinutes out of range (expected 1-1440)";
    }
    const unsigned long deadline = millis() + static_cast<unsigned long>(delayMinutes) * 60000UL;
    if (onTimer) {
        turnOnAt = deadline;
        turnOnTimerActive = true;
    } else {
        turnOffAt = deadline;
        turnOffTimerActive = true;
    }
    selectLCDPage(4);
    return "";
}

String AirConditioner::cancelTimer(bool onTimer) {
    if (onTimer) turnOnTimerActive = false;
    else turnOffTimerActive = false;
    selectLCDPage(4);
    return "";
}

String AirConditioner::setEnvironment(float roomTemperature, float roomHumidity) {
    if (roomTemperature < -20.0f || roomTemperature > 60.0f) {
        return "roomTemperature out of range (expected -20 to 60)";
    }
    if (roomHumidity < 0.0f || roomHumidity > 100.0f) {
        return "roomHumidity out of range (expected 0-100)";
    }
    roomTemperatureTenths = static_cast<int>(roomTemperature * 10.0f + (roomTemperature >= 0 ? 0.5f : -0.5f));
    roomHumidityTenths = static_cast<int>(roomHumidity * 10.0f + 0.5f);
    selectLCDPage(0);
    return "";
}

String AirConditioner::injectFault(int code) {
    if (code < 1 || code > 3) return "invalid fault code (expected 1-3)";
    faultCode = code;
    compressorRunning = false;
    powerWatts = isRunning ? 12 : 0;
    selectLCDPage(4);
    return "";
}

void AirConditioner::clearFault() {
    faultCode = 0;
    selectLCDPage(4);
}

void AirConditioner::resetFilter() {
    filterLifePercent = 100;
    runtimeSeconds = 0;
    selectLCDPage(4);
}

/*
    开启空调
*/
bool AirConditioner::turnOn() {
    if (isRunning) {
        Serial.println("空调已经在运行中");
        selectLCDPage(0);
        return true;
    }
    
    isRunning = true;
    selfClean = false;
    turnOnTimerActive = false;
    powerWatts = 35;
    Serial.printf("空调已开启 - 模式: %s, 温度: %d°C\n", getModeString().c_str(), temperature.load());
    selectLCDPage(0);
    return true;
}

/*
    关闭空调

*/
bool AirConditioner::turnOff() {
    if (!isRunning) {
        Serial.println("空调已经关闭");
        selectLCDPage(0);
        return true;
    }
    
    isRunning = false;
    compressorRunning = false;
    powerWatts = 0;
    turnOffTimerActive = false;
    Serial.println("空调已关闭");
    selectLCDPage(0);
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
    status += "  室内温度: " + String(roomTemperatureTenths.load() / 10.0f, 1) + "°C\n";
    status += "  风速: " + getFanSpeedString() + "\n";
    return status;
}

// 重置为默认设置
void AirConditioner::reset() {
    mode = AC_MODE_AUTO;
    temperature = 25;
    isRunning = false;
    fanSpeed = AC_FAN_AUTO;
    verticalSwing = false;
    horizontalSwing = false;
    verticalDirection = 2;
    horizontalDirection = 2;
    sleepMode = ecoMode = turboMode = quietMode = false;
    displayLight = beepEnabled = true;
    childLock = antiDirectBlow = auxiliaryHeat = mildewProof = selfClean = false;
    compressorRunning = false;
    powerWatts = 0;
    faultCode = 0;
    turnOnTimerActive = turnOffTimerActive = false;
    Serial.println("空调已重置为默认设置");
    selectLCDPage(0);
}

/*
    获取空调状态JSON
    返回：
        running: true 为开机状态, false 为关机状态
        mode: 空调工作模式，0自动，1制冷，2制热，3抽湿，4送风
        temperature: 表示空调目标温度
*/
String AirConditioner::getStatusJSON() const {
    JsonDocument doc;
    doc["running"] = isRunning.load();
    doc["mode"] = mode.load();
    doc["modeString"] = getModeString();
    doc["temperature"] = temperature.load();
    doc["fanSpeed"] = fanSpeed.load();
    doc["fanSpeedString"] = getFanSpeedString();
    doc["verticalSwing"] = verticalSwing.load();
    doc["horizontalSwing"] = horizontalSwing.load();
    doc["verticalDirection"] = verticalDirection.load();
    doc["horizontalDirection"] = horizontalDirection.load();

    JsonObject features = doc["features"].to<JsonObject>();
    features["sleep"] = sleepMode.load();
    features["eco"] = ecoMode.load();
    features["turbo"] = turboMode.load();
    features["quiet"] = quietMode.load();
    features["displayLight"] = displayLight.load();
    features["beep"] = beepEnabled.load();
    features["childLock"] = childLock.load();
    features["antiDirectBlow"] = antiDirectBlow.load();
    features["auxiliaryHeat"] = auxiliaryHeat.load();
    features["mildewProof"] = mildewProof.load();
    features["selfClean"] = selfClean.load();

    JsonObject environment = doc["environment"].to<JsonObject>();
    environment["roomTemperature"] = roomTemperatureTenths.load() / 10.0f;
    environment["roomHumidity"] = roomHumidityTenths.load() / 10.0f;

    JsonObject operation = doc["operation"].to<JsonObject>();
    operation["compressorRunning"] = compressorRunning.load();
    operation["powerWatts"] = powerWatts.load();
    operation["runtimeSeconds"] = runtimeSeconds.load();

    JsonObject timers = doc["timers"].to<JsonObject>();
    timers["turnOnActive"] = turnOnTimerActive.load();
    timers["turnOnRemainingMinutes"] = remainingMinutes(turnOnTimerActive.load(), turnOnAt.load());
    timers["turnOffActive"] = turnOffTimerActive.load();
    timers["turnOffRemainingMinutes"] = remainingMinutes(turnOffTimerActive.load(), turnOffAt.load());

    JsonObject maintenance = doc["maintenance"].to<JsonObject>();
    maintenance["filterLifePercent"] = filterLifePercent.load();
    maintenance["faultCode"] = faultCode.load();
    maintenance["fault"] = faultName(faultCode.load());

    String json;
    serializeJson(doc, json);
    return json;
}

// 从JSON字符串设置状态
bool AirConditioner::setFromJSON(const String& jsonStr) {
    JsonDocument doc;
    if (deserializeJson(doc, jsonStr)) return false;
    if (doc["running"].is<bool>()) isRunning = doc["running"].as<bool>();
    if (doc["mode"].is<int>()) {
        int value = doc["mode"].as<int>();
        if (value < AC_MODE_AUTO || value > AC_MODE_FAN) return false;
        mode = value;
    }
    if (doc["temperature"].is<int>()) {
        int value = doc["temperature"].as<int>();
        if (value < MIN_TEMPERATURE || value > MAX_TEMPERATURE) return false;
        temperature = value;
    }
    if (doc["fanSpeed"].is<int>()) {
        int value = doc["fanSpeed"].as<int>();
        if (value < AC_FAN_AUTO || value > AC_FAN_QUIET) return false;
        fanSpeed = value;
    }
    selectLCDPage(0);
    return true;
}

void AirConditioner::updateSimulation() {
    const unsigned long now = millis();

    if (turnOnTimerActive.load() && static_cast<long>(now - turnOnAt.load()) >= 0) {
        turnOnTimerActive = false;
        turnOn();
    }
    if (turnOffTimerActive.load() && static_cast<long>(now - turnOffAt.load()) >= 0) {
        turnOffTimerActive = false;
        turnOff();
    }

    const unsigned long elapsedSeconds = (now - lastSimulationUpdate) / 1000UL;
    if (elapsedSeconds == 0) return;
    lastSimulationUpdate += elapsedSeconds * 1000UL;

    const bool previousCompressor = compressorRunning.load();
    const int previousPower = powerWatts.load();
    const int previousFilterLife = filterLifePercent.load();
    const int current = roomTemperatureTenths.load();
    const int target = temperature.load() * 10;
    bool compressor = false;
    if (isRunning.load() && faultCode.load() == 0) {
        const int currentMode = mode.load();
        compressor = (currentMode == AC_MODE_COOL && current > target + 2) ||
                     (currentMode == AC_MODE_HEAT && current < target - 2) ||
                     currentMode == AC_MODE_DEHUMIDIFY ||
                     (currentMode == AC_MODE_AUTO && (current > target + 5 || current < target - 5));
    }
    compressorRunning = compressor;

    const unsigned long previousRuntime = runtimeSeconds.load();
    if (isRunning.load()) {
        runtimeSeconds += elapsedSeconds;
        const unsigned long hours = runtimeSeconds.load() / 3600UL;
        filterLifePercent = hours >= 100 ? 0 : 100 - static_cast<int>(hours);
    }

    // 每 5 秒推进 0.1°C/0.1%RH，演示时可以直接看到环境趋近目标。
    if (compressor && runtimeSeconds.load() / 5UL != previousRuntime / 5UL) {
        int adjusted = current;
        const int currentMode = mode.load();
        if (currentMode == AC_MODE_COOL || (currentMode == AC_MODE_AUTO && current > target)) adjusted--;
        else if (currentMode == AC_MODE_HEAT || (currentMode == AC_MODE_AUTO && current < target)) adjusted++;
        roomTemperatureTenths = adjusted;
        if (currentMode == AC_MODE_DEHUMIDIFY && roomHumidityTenths.load() > 350) roomHumidityTenths--;
        forceLCDUpdate();
    }

    int watts = 0;
    if (isRunning.load()) {
        watts = compressor ? (mode.load() == AC_MODE_HEAT ? 1100 : 850) : 35;
        if (turboMode.load()) watts += 120;
        if (ecoMode.load() && watts > 500) watts -= 180;
        if (auxiliaryHeat.load() && mode.load() == AC_MODE_HEAT) watts += 600;
    } else if (selfClean.load()) watts = 25;
    powerWatts = watts;
    if (previousCompressor != compressorRunning.load() || previousPower != watts ||
        previousFilterLife != filterLifePercent.load()) {
        forceLCDUpdate();
    }
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

void AirConditioner::selectLCDPage(int page) {
    lcdPage = page;
    lcdPageHoldUntil = millis() + 2500UL;
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
        lastUpdate = 0;
        lastLCDOnTimerMinutes = -1;
        lastLCDOffTimerMinutes = -1;
}

// 绘制右上角WiFi状态图标 (三段圆弧 + 底部圆点)
void AirConditioner::drawWifiIcon() {
    const int cx = 298;  /* 图标基准点(底部圆点圆心) */
    const int cy = 25;

    // 清除旧图标区域
    lcd.fillRect(cx - 18, cy - 18, 37, 23, 0x10A2);

    // 已连接绿色, 连接中深灰, 未连接浅灰+红色斜线
    uint16_t color;
    switch (wifiState) {
        case WIFI_DISP_CONNECTED:  color = TFT_GREEN; break;
        case WIFI_DISP_CONNECTING: color = TFT_YELLOW; break;
        default:                   color = TFT_LIGHTGREY; break;
    }

    lcd.fillCircle(cx, cy, 3, color);
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
    if (!lcdEnabled) return;

    const bool forced = lcdRefreshRequested.exchange(false, std::memory_order_relaxed);
    const unsigned long currentTime = millis();
    const bool highlightNow = static_cast<long>(lcdPageHoldUntil.load() - currentTime) > 0;
    const bool highlightExpired = lcdHighlightVisible && !highlightNow;
    const int currentOnMinutes = remainingMinutes(turnOnTimerActive.load(), turnOnAt.load());
    const int currentOffMinutes = remainingMinutes(turnOffTimerActive.load(), turnOffAt.load());
    const bool timerChanged = currentOnMinutes != lastLCDOnTimerMinutes ||
                              currentOffMinutes != lastLCDOffTimerMinutes;
    if (!forced && !highlightExpired && !timerChanged) return;

    // 整屏清除只允许出现在首次绘制或显式 clearLCD() 之后。常规刷新直接覆盖
    // 各卡片，避免 fillScreen -> 重绘之间的空白帧造成肉眼可见的闪烁。
    const bool fullRedraw = lastUpdate == 0;

    const uint16_t bg = 0x0841;
    const uint16_t header = 0x10A2;
    const uint16_t panel = 0x2124;
    const uint16_t panelOn = 0x1945;
    const uint16_t muted = 0x9CF3;
    const uint16_t iconOff = 0x528A;
    const uint16_t accent = 0x05FF;
    const uint16_t good = 0x05E8;
    const uint16_t warning = 0xFD20;

    const bool highlightActive = highlightNow;
    const int highlight = lcdPage.load();

    lcd.startWrite();
    if (fullRedraw) lcd.fillScreen(bg);
    lcd.setTextSize(1);
    lcd.setTextPadding(0);

    // Header: mode, active timer, power, and Wi-Fi.
    lcd.fillRect(0, 0, 320, 38, header);
    String modeLabel = getModeString();
    modeLabel.toUpperCase();
    if (modeLabel == "DEHUMIDIFY") modeLabel = "DRY";
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(TFT_WHITE, header);
    lcd.drawString(modeLabel, 10, 5);

    const int onMinutes = currentOnMinutes;
    const int offMinutes = currentOffMinutes;
    if (onMinutes > 0 || offMinutes > 0) {
        char timerText[18];
        snprintf(timerText, sizeof(timerText), "%s %dm", onMinutes > 0 ? "ON" : "OFF",
                 onMinutes > 0 ? onMinutes : offMinutes);
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(TFT_YELLOW, header);
        lcd.drawString(timerText, 126, 12);
    } else if (filterLifePercent.load() <= 20) {
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(warning, header);
        lcd.drawString("FILTER", 126, 12);
    }

    lcd.fillRoundRect(216, 5, 57, 28, 7, isRunning.load() ? good : TFT_DARKGREY);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(TFT_WHITE, isRunning.load() ? good : TFT_DARKGREY);
    lcd.drawCentreString(isRunning.load() ? "ON" : "OFF", 244, 6);
    drawWifiIcon();

    // Primary information: target temperature is always the visual anchor.
    lcd.fillRoundRect(8, 45, 122, 100, 11, panel);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(accent, panel);
    lcd.drawString("TARGET", 18, 53);
    lcd.setFont(&fonts::Font7);
    lcd.setTextColor(TFT_WHITE, panel);
    char text[64];
    snprintf(text, sizeof(text), "%d", temperature.load());
    lcd.drawString(text, 18, 72);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(accent, panel);
    lcd.drawString("C", 102, 79);

    lcd.fillRoundRect(138, 45, 174, 47, 10, panel);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(muted, panel);
    lcd.drawString("ROOM", 150, 51);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(TFT_WHITE, panel);
    snprintf(text, sizeof(text), "%.1f C", roomTemperatureTenths.load() / 10.0f);
    lcd.drawRightString(text, 300, 61);

    lcd.fillRoundRect(138, 98, 174, 47, 10, panel);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(muted, panel);
    lcd.drawString("HUMIDITY", 150, 104);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(TFT_WHITE, panel);
    snprintf(text, sizeof(text), "%.0f %%", roomHumidityTenths.load() / 10.0f);
    lcd.drawRightString(text, 300, 114);

    if (highlightActive && highlight == 0) {
        lcd.drawRoundRect(8, 45, 122, 100, 11, accent);
        lcd.drawRoundRect(138, 45, 174, 100, 10, accent);
    }

    // Operational strip: fan, actual compressor state, and estimated power.
    const int fault = faultCode.load();
    if (fault != 0) {
        lcd.fillRoundRect(8, 152, 304, 42, 9, TFT_RED);
        lcd.setFont(&fonts::Font4);
        lcd.setTextColor(TFT_WHITE, TFT_RED);
        snprintf(text, sizeof(text), "FAULT E%d", fault);
        lcd.drawString(text, 18, 160);
        lcd.setFont(&fonts::Font2);
        lcd.drawRightString(faultName(fault), 300, 166);
    } else {
        lcd.fillRoundRect(8, 152, 304, 42, 9, panel);
        String fanLabel = getFanSpeedString();
        fanLabel.toUpperCase();
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(muted, panel);
        lcd.drawString("FAN", 18, 157);
        lcd.setFont(&fonts::Font4);
        lcd.setTextColor(TFT_WHITE, panel);
        lcd.drawString(fanLabel, 18, 169);

        // Airflow pictograms: lit arrows mean swing; the digit is fixed direction.
        const uint16_t vColor = verticalSwing.load() ? accent : iconOff;
        lcd.drawLine(116, 161, 116, 183, vColor);
        lcd.fillTriangle(116, 158, 112, 164, 120, 164, vColor);
        lcd.fillTriangle(116, 186, 112, 180, 120, 180, vColor);
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(vColor, panel);
        snprintf(text, sizeof(text), "%d", verticalDirection.load());
        lcd.drawString(text, 122, 166);

        const uint16_t hColor = horizontalSwing.load() ? accent : iconOff;
        lcd.drawLine(139, 173, 157, 173, hColor);
        lcd.fillTriangle(136, 173, 142, 169, 142, 177, hColor);
        lcd.fillTriangle(160, 173, 154, 169, 154, 177, hColor);
        lcd.setTextColor(hColor, panel);
        snprintf(text, sizeof(text), "%d", horizontalDirection.load());
        lcd.drawCentreString(text, 148, 154);

        const uint16_t runColor = compressorRunning.load() ? good : iconOff;
        lcd.fillCircle(174, 173, 5, runColor);
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(muted, panel);
        lcd.drawString(compressorRunning.load() ? "CMP" :
                       (isRunning.load() ? "IDLE" : "OFF"), 183, 166);

        lcd.setFont(&fonts::Font4);
        lcd.setTextColor(TFT_WHITE, panel);
        snprintf(text, sizeof(text), "%dW", powerWatts.load());
        lcd.drawRightString(text, 300, 161);
    }
    if (highlightActive && (highlight == 1 || highlight == 4)) {
        lcd.drawRoundRect(8, 152, 304, 42, 9, accent);
    }

    // Compact feature ribbon. Color means enabled; the pictograms remain visible
    // when disabled, avoiding tiny labels while keeping every feature observable.
    auto drawFeatureIcon = [&](int index, bool enabled) {
        const int x = 5 + index * 28;
        const int y = 201;
        const int cx = x + 13;
        const int cy = y + 17;
        const uint16_t cell = enabled ? panelOn : panel;
        const uint16_t ink = enabled ? accent : iconOff;
        lcd.fillRoundRect(x, y, 26, 35, 6, cell);

        switch (index) {
            case 0: // sleep: crescent moon
                lcd.fillCircle(cx - 1, cy, 8, ink);
                lcd.fillCircle(cx + 3, cy - 3, 7, cell);
                break;
            case 1: // eco: leaf
                lcd.fillTriangle(cx - 8, cy + 5, cx + 8, cy - 8, cx + 5, cy + 7, ink);
                lcd.drawLine(cx - 5, cy + 5, cx + 5, cy - 4, cell);
                break;
            case 2: // turbo: lightning
                lcd.fillTriangle(cx + 1, cy - 11, cx - 7, cy + 1, cx + 1, cy + 1, ink);
                lcd.fillTriangle(cx - 1, cy + 11, cx + 8, cy - 2, cx - 1, cy - 2, ink);
                break;
            case 3: // quiet: muted speaker
                lcd.fillRect(cx - 8, cy - 3, 5, 7, ink);
                lcd.fillTriangle(cx - 3, cy - 4, cx + 4, cy - 9, cx + 4, cy + 9, ink);
                lcd.drawLine(cx - 8, cy - 10, cx + 8, cy + 10, ink);
                break;
            case 4: // display light
                lcd.fillCircle(cx, cy, 5, ink);
                lcd.drawCircle(cx, cy, 9, ink);
                break;
            case 5: // beep
                lcd.fillCircle(cx - 3, cy - 2, 6, ink);
                lcd.fillRect(cx - 9, cy - 2, 12, 8, ink);
                lcd.drawLine(cx + 5, cy - 6, cx + 9, cy - 9, ink);
                lcd.drawLine(cx + 6, cy, cx + 11, cy, ink);
                break;
            case 6: // child lock
                lcd.drawRoundRect(cx - 7, cy - 2, 14, 12, 3, ink);
                lcd.drawCircle(cx, cy - 4, 6, ink);
                break;
            case 7: // anti-direct blow
                lcd.drawLine(cx - 9, cy - 6, cx + 7, cy - 6, ink);
                lcd.drawLine(cx - 9, cy, cx + 7, cy, ink);
                lcd.drawLine(cx - 9, cy + 6, cx + 7, cy + 6, ink);
                lcd.drawLine(cx - 8, cy - 10, cx + 9, cy + 10, ink);
                break;
            case 8: // auxiliary heat
                lcd.fillCircle(cx, cy, 5, ink);
                lcd.drawLine(cx, cy - 11, cx, cy - 7, ink);
                lcd.drawLine(cx, cy + 7, cx, cy + 11, ink);
                lcd.drawLine(cx - 11, cy, cx - 7, cy, ink);
                lcd.drawLine(cx + 7, cy, cx + 11, cy, ink);
                break;
            case 9: // mildew-proof shield
                lcd.drawRoundRect(cx - 8, cy - 10, 16, 18, 5, ink);
                lcd.drawLine(cx - 5, cy, cx - 1, cy + 4, ink);
                lcd.drawLine(cx - 1, cy + 4, cx + 6, cy - 5, ink);
                break;
            case 10: // self clean: water drop
                lcd.fillTriangle(cx, cy - 11, cx - 8, cy + 3, cx + 8, cy + 3, ink);
                lcd.fillCircle(cx, cy + 3, 8, ink);
                lcd.fillCircle(cx + 3, cy, 3, cell);
                break;
        }
        if (enabled) lcd.fillCircle(x + 21, y + 6, 2, good);
    };

    drawFeatureIcon(0, sleepMode.load());
    drawFeatureIcon(1, ecoMode.load());
    drawFeatureIcon(2, turboMode.load());
    drawFeatureIcon(3, quietMode.load());
    drawFeatureIcon(4, displayLight.load());
    drawFeatureIcon(5, beepEnabled.load());
    drawFeatureIcon(6, childLock.load());
    drawFeatureIcon(7, antiDirectBlow.load());
    drawFeatureIcon(8, auxiliaryHeat.load());
    drawFeatureIcon(9, mildewProof.load());
    drawFeatureIcon(10, selfClean.load());

    if (highlightActive && highlight >= 10 && highlight <= 20) {
        const int iconX = 5 + (highlight - 10) * 28;
        lcd.drawRoundRect(iconX, 201, 26, 35, 6, TFT_WHITE);
        lcd.drawRoundRect(iconX + 1, 202, 24, 33, 5, accent);
    }

    lcd.endWrite();
    lcdHighlightVisible = highlightActive;
    lastLCDOnTimerMinutes = currentOnMinutes;
    lastLCDOffTimerMinutes = currentOffMinutes;
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
