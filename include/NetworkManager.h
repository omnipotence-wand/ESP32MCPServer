#pragma once

#include <WiFi.h>

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
    void loop();
    bool connect(const String& ssid, const String& password);
    bool isConnected() const;
    String getSSID() const;
    String getIPAddress() const;
    NetworkState getState() const;

private:
    static constexpr uint32_t CONNECT_TIMEOUT = 15000; // 15 seconds
    static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;
    static constexpr uint16_t RECONNECT_INTERVAL = 5000; // 5 seconds

    uint8_t connectAttempts;
    uint32_t lastConnectAttempt;
    NetworkState state;
    String requestedSSID;
    String requestedPassword;
    bool statusPrinted;

    void startConnection(const char* ssid = nullptr, const char* password = nullptr);
    void setupWebServer();
    
    String generateUniqueSSID();
    void printConnectionStatus();
    String generateSessionId();
};
