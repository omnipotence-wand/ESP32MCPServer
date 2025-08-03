/**
 * mDNS 服务器使用示例
 * 
 * 此示例展示如何在 ESP32 MCP Server 项目中使用 mDNS 服务
 */

#include <Arduino.h>
#include <WiFi.h>
#include "mDNS.h"

// WiFi 配置
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== mDNS 服务器示例 ===");
    
    // 连接 WiFi
    WiFi.begin(ssid, password);
    Serial.print("连接 WiFi");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi 连接成功!");
    Serial.printf("IP 地址: %s\n", WiFi.localIP().toString().c_str());
    
    // 初始化 mDNS 服务器
    Serial.println("\n启动 mDNS 服务器...");
    
    if (initMDNS("esp32mcp", 9000)) {
        Serial.println("mDNS 服务器启动成功!");
        
        // 打印服务信息
        getMDNSServer().printServiceInfo();
        
        // 添加自定义服务示例
        std::vector<std::pair<String, String>> customTxtRecords;
        customTxtRecords.push_back(std::make_pair("version", "1.0.0"));
        customTxtRecords.push_back(std::make_pair("author", "ESP32MCPServer"));
        customTxtRecords.push_back(std::make_pair("description", "Custom service example"));
        
        getMDNSServer().addCustomService("custom", "tcp", 8080, customTxtRecords);
        
        Serial.println("\n=== 服务发现测试 ===");
        Serial.println("在同一网络的设备上，你可以通过以下方式访问:");
        Serial.printf("• http://%s:9000/\n", getMDNSServer().getFullAddress().c_str());
        Serial.printf("• http://%s:9000/mcp\n", getMDNSServer().getFullAddress().c_str());
        Serial.printf("• http://%s:9000/status\n", getMDNSServer().getFullAddress().c_str());
        
        Serial.println("\n=== 命令行测试 ===");
        Serial.println("你可以使用以下命令测试 mDNS 服务:");
        Serial.println("# 在 macOS/Linux 上:");
        Serial.println("dns-sd -B _mcp._tcp");
        Serial.println("dns-sd -B _http._tcp");
        Serial.println("avahi-browse -t _mcp._tcp");
        Serial.println("avahi-browse -t _http._tcp");
        
        Serial.println("\n# 在 Windows 上 (需要安装 Bonjour):");
        Serial.println("dns-sd -B _mcp._tcp");
        Serial.println("dns-sd -B _http._tcp");
        
    } else {
        Serial.println("❌ mDNS 服务器启动失败!");
    }
}

void loop() {
    // 定期更新 mDNS 服务
    updateMDNS();
    
    // 每 30 秒打印一次状态
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        
        if (getMDNSServer().isRunning()) {
            Serial.printf("[%lu] mDNS 服务运行正常 - %s\n", 
                         millis(), getMDNSServer().getFullAddress().c_str());
        } else {
            Serial.printf("[%lu] ⚠️ mDNS 服务异常\n", millis());
        }
    }
    
    delay(1000);
}
