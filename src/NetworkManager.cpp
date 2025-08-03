#include "NetworkManager.h"
#include <esp_random.h>
#include <ArduinoJson.h>
#include "ESP32SSDP.h"

String ssid = "snowwolf";
String password = "13651726212zzy";

NetworkManager::NetworkManager() :
      connectAttempts(0),
      lastConnectAttempt(0),
      mcpServer(nullptr) {
}

NetworkManager::NetworkManager(mcp::MCPServer& mcpServer) :
      connectAttempts(0),
      lastConnectAttempt(0),
      mcpServer(&mcpServer) {
}

void NetworkManager::begin() {
    // Initialize LittleFS if not already initialized
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    // waiting for connect
    int attempts = 0;
    const int maxAttempts = 20; // 最多尝试20次
    
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi");
        return;
    }
    // Small delay to allow task to start
    delay(100);
    this->printConnectionStatus();


    // 启动 mDNS 服务
    if (initMDNS("midea_ac_lan", HTTP_PORT)) {
        getMDNSServer().printServiceInfo();
    }
    
    // 启动 MCP 服务
    mcpServer->setupWebServer();
}

void NetworkManager::printConnectionStatus() {
    Serial.println("======================================");
    Serial.println("           WiFi CONNECTION SUCCESS");
    Serial.println("======================================");
    
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("DNS Server: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("Channel: %d\n", WiFi.channel());
    
    // 连接质量评估
    int rssi = WiFi.RSSI();
    String quality;
    if (rssi > -50) {
        quality = "Excellent";
    } else if (rssi > -60) {
        quality = "Good";
    } else if (rssi > -70) {
        quality = "Fair";
    } else {
        quality = "Weak";
    }
    Serial.printf("Connection Quality: %s\n", quality.c_str());
    
    Serial.printf("Connection Time: %lu ms\n", millis() - lastConnectAttempt);
    Serial.printf("Connect Attempts: %d\n", connectAttempts + 1);
    
    Serial.println("======================================");
    Serial.println("Device is ready for MCP connections!");
    Serial.println("======================================");
}

void NetworkManager::initializeSSDP() {
    // 设置SSDP设备信息
    SSDP.setDeviceType("ssdp:mcp:device");
    SSDP.setHTTPPort(9000);
    SSDP.setName("Media Air Conditioner");
    SSDP.setURL("/mcp");
    // 启动SSDP服务
    if (SSDP.begin()) {
        Serial.println("SSDP服务启动成功");
    } else {
        Serial.println("SSDP服务启动失败");
    }
}
