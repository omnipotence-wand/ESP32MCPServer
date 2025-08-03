#pragma once

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "RequestQueue.h"
#include "MCPServer.h"

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
    bool isConnected();
    String getIPAddress();
    String getSSID();

private:
    static constexpr uint32_t CONNECT_TIMEOUT = 15000; // 15 seconds
    static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
    static constexpr uint16_t RECONNECT_INTERVAL = 5000; // 5 seconds
    static constexpr const char* SETUP_PAGE_PATH = "/wifi_setup.html";
    static constexpr uint16_t HTTP_PORT = 9000; // HTTP服务器端口

    NetworkState state;
    String configFilePath;
    AsyncWebServer server;
    RequestQueue<NetworkRequest> requestQueue;
    TaskHandle_t networkTaskHandle;
    mcp::MCPServer* mcpServer; // 改为指针，指向外部创建的MCPServer对象
    
    String apSSID;
    uint8_t connectAttempts;
    uint32_t lastConnectAttempt;
    NetworkCredentials credentials;
    String requestBodyData;  // 用于存储HTTP请求体数据

    void setupWebServer();
    void handleRequest(const NetworkRequest& request);
    
    void startAP();
    void connect();
    void checkConnection();
    
    bool loadConfigFromFile();
    void saveConfigToFile(const String& ssid, const String& password);
    void createDefaultConfig();
    bool parseConfigFile(const String& content);
    
    String generateUniqueSSID();
    void queueRequest(NetworkRequest::Type type, const String& data = "");
    void printConnectionStatus();
    String repeatChar(const char* ch, int count);
    String generateSessionId();

    // Web handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    
    // MCP HTTP API handlers
    void handleMCPRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleMCPRequestBody(AsyncWebServerRequest *request);
    void handleMCPInitialize(AsyncWebServerRequest *request);
    void handleMCPSSE(AsyncWebServerRequest *request);
    
    static void networkTaskCode(void* parameter);
    void networkTask();
    static String getNetworkStatusJson(NetworkState state, const String& ssid, const String& ip);
};