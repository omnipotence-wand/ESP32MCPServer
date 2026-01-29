#pragma once

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "RequestQueue.h"
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
    void begin();

private:
    static constexpr uint32_t CONNECT_TIMEOUT = 15000; // 15 seconds
    static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
    static constexpr uint16_t RECONNECT_INTERVAL = 5000; // 5 seconds
    static constexpr uint16_t HTTP_PORT = 9000; // HTTP服务器端口

    uint8_t connectAttempts;
    uint32_t lastConnectAttempt;

    void setupWebServer();
    
    String generateUniqueSSID();
    void printConnectionStatus();
    String generateSessionId();

    // SSDP Server
    void initializeSSDP();
};