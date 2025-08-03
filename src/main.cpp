#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "NetworkManager.h"
#include "MCPServer.h"
#include "ac.h"

using namespace mcp;

// Global instances

// MCP Server 监听的端口
int MCP_HTTP_PORT = 9000;

AirConditioner airConditioner;
MCPServer mcpServer(airConditioner, MCP_HTTP_PORT);
NetworkManager networkManager(mcpServer);

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

    // Start network manager (it will initialize LittleFS)
    networkManager.begin();

    // Wait for network connection or AP mode
    Serial.println("Waiting for network initialization...");
    uint32_t startTime = millis();

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

    // Create MCP task
    Serial.println("Creating MCP task on core 1...");

    Serial.println(repeatChar("*", 60));
    Serial.println("              SYSTEM INITIALIZATION COMPLETE");
    Serial.println(repeatChar("*", 60) + "\n");
}

void loop() {
    // 主循环处理
    static unsigned long lastLCDUpdate = 0;
    static unsigned long lastHeartbeat = 0;
    unsigned long currentTime = millis();
    
    // 定期更新LCD显示
    if (currentTime - lastLCDUpdate >= 1000) {  // 每秒更新一次LCD
        airConditioner.updateLCDDisplay();
        lastLCDUpdate = currentTime;
    }
    // 短暂延时，避免CPU占用过高
    delay(100);
}