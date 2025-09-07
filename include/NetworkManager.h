#pragma once

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "RequestQueue.h"
#include "MCPServer.h"
#include "ESP32SSDP.h"
#include "mDNS.h"

enum class NetworkState {
    INIT,
    CONNECTING,
    CONNECTED,
    CONNECTION_FAILED,
    AP_MODE
};

struct NetworkCredentials {
    String ssid;
    String password;
    bool valid;
};

struct NetworkRequest {
    enum class Type {
        CONNECT,
        START_AP,
        CHECK_CONNECTION
    };
    
    Type type;
    String data;
};

class NetworkManager {
public:
    NetworkManager();
    NetworkManager(mcp::MCPServer& mcpServer); // 添加接受MCPServer引用的构造函数
    void begin();

private:
    static constexpr uint32_t CONNECT_TIMEOUT = 15000; // 15 seconds
    static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
    static constexpr uint16_t RECONNECT_INTERVAL = 5000; // 5 seconds
    static constexpr uint16_t HTTP_PORT = 9000; // HTTP服务器端口

    mcp::MCPServer* mcpServer; // 改为指针，指向外部创建的MCPServer对象
    
    uint8_t connectAttempts;
    uint32_t lastConnectAttempt;

    void setupWebServer();
    
    String generateUniqueSSID();
    void printConnectionStatus();
    String generateSessionId();
    
    // MCP HTTP API handlers
    void handleMCPRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleMCPInitialize(AsyncWebServerRequest *request);

    // SSDP Server
    void initializeSSDP();
};