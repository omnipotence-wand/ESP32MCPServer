#include <Arduino.h>
#include <WiFi.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 为了解决 ESP32-S3 上的 ESPmDNS 兼容性问题，我们需要先定义类型
// 这是一个临时的解决方案，直到 ESPmDNS 库完全支持 ESP32-S3
#ifndef MDNS_RESULT_T_DEFINED
#define MDNS_RESULT_T_DEFINED
typedef struct mdns_result {
    struct mdns_result * next;
    char * hostname;
    char * instance_name;
    char * service_type;
    char * proto;
    uint16_t port;
    char * txt;
    size_t txt_count;
    char * txt_value;
    size_t txt_value_len;
    struct sockaddr_storage addr;
    uint32_t ttl;
} mdns_result_t;
#endif

#ifndef MDNS_TXT_ITEM_T_DEFINED
#define MDNS_TXT_ITEM_T_DEFINED
typedef struct mdns_txt_item {
    char * key;
    char * value;
    size_t value_len;
} mdns_txt_item_t;
#endif

#include <ESPmDNS.h>
#include "mDNS.h"

// 实现 MDNSServer 类的方法
MDNSServer::MDNSServer() : hostname("esp32mcp"), mcpPort(9000), initialized(false) {}

bool MDNSServer::begin(const String& deviceName, uint16_t port) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ mDNS: WiFi 未连接，无法启动 mDNS 服务");
        return false;
    }
    
    hostname = deviceName;
    mcpPort = port;
    
    // 初始化 mDNS
    if (!MDNS.begin(hostname.c_str())) {
        Serial.println("❌ mDNS: 初始化失败");
        return false;
    }
    
    Serial.printf("✅ mDNS: 服务器已启动，主机名: %s.local\n", hostname.c_str());
    
    // 设置实例名称
    MDNS.setInstanceName("midea air conditioner");
    
    // 注册服务
    registerMCPService();
    
    initialized = true;
    return true;
}

void MDNSServer::registerMCPService() {
        // 获取 WiFi 信息
    IPAddress ip = WiFi.localIP();
    String macAddress = WiFi.macAddress();
    // 注册 MCP HTTP 服务
    if (MDNS.addService("mcp", "tcp", mcpPort)) {
        Serial.printf("✅ mDNS: MCP 服务已注册在端口 %d\n", mcpPort);
        
        // 添加 MCP 服务的 TXT 记录
        MDNS.addServiceTxt("mcp", "tcp", "ip", ip.toString());
        MDNS.addServiceTxt("mcp", "tcp", "version", "2025-03-26");
        MDNS.addServiceTxt("mcp", "tcp", "protocol", "http");
        MDNS.addServiceTxt("mcp", "tcp", "capabilities", "tools,resources");
        MDNS.addServiceTxt("mcp", "tcp", "device", "ESP32-S3");
        MDNS.addServiceTxt("mcp", "tcp", "path", "/mcp");
        MDNS.addServiceTxt("mcp", "tcp", "endpoint", "http://" + ip.toString() + ":" + String(mcpPort) + "/mcp");
    } else {
        Serial.println("❌ mDNS: MCP 服务注册失败");
    }
}

bool MDNSServer::addCustomService(const String& serviceName, const String& protocol, 
                     uint16_t port, const std::vector<std::pair<String, String>>& txtRecords) {
    if (!initialized) {
        Serial.println("❌ mDNS: 服务器未初始化");
        return false;
    }
    
    if (MDNS.addService(serviceName.c_str(), protocol.c_str(), port)) {
        Serial.printf("✅ mDNS: 自定义服务 %s/%s 已注册在端口 %d\n", 
                     serviceName.c_str(), protocol.c_str(), port);
        
        // 添加 TXT 记录
        for (size_t i = 0; i < txtRecords.size(); i++) {
            MDNS.addServiceTxt(serviceName.c_str(), protocol.c_str(), 
                             txtRecords[i].first.c_str(), txtRecords[i].second.c_str());
        }
        return true;
    } else {
        Serial.printf("❌ mDNS: 自定义服务 %s/%s 注册失败\n", 
                     serviceName.c_str(), protocol.c_str());
        return false;
    }
}

void MDNSServer::update() {
    // ESP32 的 mDNS 库会自动处理，无需手动更新
    // 但我们可以检查连接状态
    if (initialized && WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ mDNS: WiFi 连接断开");
    }
}

void MDNSServer::end() {
    if (initialized) {
        MDNS.end();
        initialized = false;
        Serial.println("✅ mDNS: 服务已停止");
    }
}

String MDNSServer::getHostname() const {
    return hostname;
}

String MDNSServer::getFullAddress() const {
    return hostname + ".local";
}

void MDNSServer::printServiceInfo() {
    if (!initialized) {
        Serial.println("❌ mDNS: 服务器未初始化");
        return;
    }
    
    IPAddress ip = WiFi.localIP();
    Serial.println("\n==================================================");
    Serial.println("             mDNS 服务信息");
    Serial.println("==================================================");
    Serial.printf("主机名: %s\n", getFullAddress().c_str());
    Serial.printf("IP 地址: %s\n", ip.toString().c_str());
    Serial.printf("MCP 端口: %d\n", mcpPort);
    Serial.println("\n已注册的服务:");
    Serial.printf("  • MCP 服务: _mcp._tcp.local:%d\n", mcpPort);
    Serial.println("\n访问方式:");
    Serial.printf("  • http://%s:%d/mcp\n", getFullAddress().c_str(), mcpPort);
    Serial.println("==================================================\n");
}

bool MDNSServer::isRunning() const {
    return initialized && WiFi.status() == WL_CONNECTED;
}

// 全局 mDNS 服务器实例
static MDNSServer globalMDNSServer;

// 全局函数实现
bool initMDNS(const String& hostname, uint16_t port) {
    return globalMDNSServer.begin(hostname, port);
}

void updateMDNS() {
    globalMDNSServer.update();
}

void stopMDNS() {
    globalMDNSServer.end();
}

MDNSServer& getMDNSServer() {
    return globalMDNSServer;
}