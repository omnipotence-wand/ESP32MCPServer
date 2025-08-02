#include <Arduino.h>
#include <LittleFS.h>
#include "NetworkManager.h"
#include "MCPServer.h"
#include "ac.h"

using namespace mcp;
// Global instances
NetworkManager networkManager;
MCPServer mcpServer;
AirConditioner airConditioner;

// Task handles
TaskHandle_t mcpTaskHandle = nullptr;

// MCP task function
void mcpTask(void* parameter) {
    while (true) {
        mcpServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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
    while (!networkManager.isConnected() && networkManager.getIPAddress().isEmpty()) {
        if (millis() - startTime > 100) {
            Serial.print(".");
            startTime = millis();
        }
        delay(10);
    }
    
    if (!networkManager.getIPAddress().isEmpty()) {
        Serial.println(" Done!");
    }

    // Initialize MCP server
    Serial.println("Initializing MCP server...");
    mcpServer.begin(networkManager.isConnected());

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
    xTaskCreatePinnedToCore(
        mcpTask,
        "MCPTask",
        8192,
        nullptr,
        1,
        &mcpTaskHandle,
        1  // Run on core 1
    );
    
    Serial.println(repeatChar("*", 60));
    Serial.println("              SYSTEM INITIALIZATION COMPLETE");
    Serial.println(repeatChar("*", 60) + "\n");
}

void loop() {
    // Main loop can be used for other tasks
    // Network and MCP handling is done in their respective tasks
    delay(1000);
}