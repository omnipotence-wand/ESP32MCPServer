#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEMCPServer.h>
#include <HttpMCPServer.h>

#include "ac.h"

class MCPService {
public:
    explicit MCPService(AirConditioner& airConditioner, uint16_t httpPort = 9000);
    ~MCPService();

    MCPService(const MCPService&) = delete;
    MCPService& operator=(const MCPService&) = delete;

    void begin();
    void loop();
    bool startWifiTransport();
    void ensureWifiTransport();
    String getHttpUrl() const;

    JsonDocument configureWiFi(JsonVariantConst params);
    JsonDocument getWiFiStatus(JsonVariantConst params) const;

private:
    void registerBleTools();
    void registerWifiTools();
    String ipToString(const IPAddress& ip) const;

    AirConditioner& airConditioner;
    uint16_t httpPort;
    BLEMCPServer bleServer;
    HttpMCPServer* httpServer;
    bool bleStarted;
    bool bleToolsRegistered;
    unsigned long lastWifiTransportCheck;
};
