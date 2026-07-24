#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "NetworkManager.h"
#include "MCPService.h"

// Global instances

// MCP Server 监听的端口
int MCP_HTTP_PORT = 9000;

AirConditioner airConditioner;
NetworkManager networkManager;
MCPService mcpService(airConditioner, networkManager, MCP_HTTP_PORT);

// Helper function to repeat characters
String repeatChar(const char* ch, int count) {
    String result = "";
    for (int i = 0; i < count; i++) {
        result += ch;
    }
    return result;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n" + repeatChar("*", 60));
    Serial.println("                ESP32 MCP SERVER STARTING");
    Serial.println(repeatChar("*", 60));
    Serial.println("Initializing system...");

    // Initialize Air Conditioner with LCD
    Serial.println("Initializing Air Conditioner...");
    
    // 验证空调系统基本状态
    Serial.printf("✅ Air Conditioner basic system initialized - Mode: %s, Temp: %d°C, Status: %s\n", 
                  airConditioner.getModeString().c_str(), 
                  airConditioner.getTemperature(), 
                  airConditioner.getStatusString().c_str());
    
    // 初始化LCD屏幕
    if (!airConditioner.initLCD()) {
        Serial.println("❌ LCD initialization failed. Air Conditioner will not display.");
    } else {
        Serial.println("✅ Air Conditioner initialized with LCD.");
    }

    // Start MCP service. BLE starts immediately; WiFi MCP starts once WiFi is connected.
    Serial.println("Starting MCP service...");
    mcpService.begin();

    // Start network manager and then give MCP service another chance to start HTTP MCP.
    Serial.println("Waiting for network initialization...");
    networkManager.begin();
    mcpService.ensureWifiTransport();

    Serial.println(repeatChar("*", 60));
    Serial.println("              SYSTEM INITIALIZATION COMPLETE");
    Serial.println(repeatChar("*", 60) + "\n");
}

void loop() {
    // 主循环处理
    static unsigned long lastLCDUpdate = 0;
    unsigned long currentTime = millis();

    networkManager.loop();
    mcpService.loop();

    // 同步WiFi状态到LCD (右上角图标 + ip:port 信息行), 状态变化时立即刷新
    if (networkManager.isConnected()) {
        airConditioner.setNetworkStatus(WIFI_DISP_CONNECTED,
                                        networkManager.getIPAddress() + ":" + String(MCP_HTTP_PORT));
    } else if (networkManager.getState() == NetworkState::CONNECTING) {
        airConditioner.setNetworkStatus(WIFI_DISP_CONNECTING, "");
    } else {
        airConditioner.setNetworkStatus(WIFI_DISP_DISCONNECTED, "");
    }


    // 定期更新LCD显示
    if (currentTime - lastLCDUpdate >= 1000) {  // 每秒更新一次LCD
        airConditioner.updateLCDDisplay();
        lastLCDUpdate = currentTime;
    }
    // 短暂延时，避免CPU占用过高
    delay(100);
}
