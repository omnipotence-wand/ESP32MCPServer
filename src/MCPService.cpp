#include "MCPService.h"

#include <WiFi.h>
#include <cstring>
#include <new>

#include "ACTools.h"

namespace {
constexpr const char* BLE_SERVER_NAME = "ESP32-AC-MCP-BLE";
constexpr const char* HTTP_SERVER_NAME = "ESP32-AC-MCP-HTTP";
constexpr const char* SERVER_VERSION = "1.0.0";
constexpr const char* SERVER_INSTRUCTIONS = "Control the ESP32 air conditioner over MCP.";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_TRANSPORT_CHECK_INTERVAL_MS = 3000;

class ConfigureWiFiHandler : public ToolHandler {
public:
    explicit ConfigureWiFiHandler(MCPService& service) : service(service) {}

    JsonDocument call(JsonVariantConst params) override {
        return service.configureWiFi(params);
    }

private:
    MCPService& service;
};

class WiFiStatusHandler : public ToolHandler {
public:
    explicit WiFiStatusHandler(const MCPService& service) : service(service) {}

    JsonDocument call(JsonVariantConst params) override {
        return service.getWiFiStatus(params);
    }

private:
    const MCPService& service;
};
}  // namespace

MCPService::MCPService(AirConditioner& airConditioner, uint16_t httpPort)
    : airConditioner(airConditioner),
      httpPort(httpPort),
      bleServer(BLE_SERVER_NAME, SERVER_VERSION, SERVER_INSTRUCTIONS),
      httpServer(nullptr),
      bleStarted(false),
      bleToolsRegistered(false),
      lastWifiTransportCheck(0) {
}

MCPService::~MCPService() {
    if (httpServer) {
        delete httpServer;
        httpServer = nullptr;
    }
}

void MCPService::begin() {
    registerBleTools();

    BleServerConfig bleConfig;
    bleConfig.deviceName = BLE_SERVER_NAME;
    bleServer.setBleConfig(bleConfig);
    bleServer.begin();
    bleStarted = true;

    ensureWifiTransport();
}

void MCPService::loop() {
    if (bleStarted) {
        bleServer.loop();
    }

    unsigned long now = millis();
    if (now - lastWifiTransportCheck >= WIFI_TRANSPORT_CHECK_INTERVAL_MS) {
        lastWifiTransportCheck = now;
        ensureWifiTransport();
    }
}

void MCPService::registerBleTools() {
    if (bleToolsRegistered) {
        return;
    }

    registerACTools(bleServer, airConditioner);

    Tool configWifiTool;
    configWifiTool.name = "config_wifi";
    configWifiTool.description = "Configure WiFi with ssid and password, then start WiFi MCP on success";
    configWifiTool.inputSchema.type = "object";
    configWifiTool.inputSchema.hasAdditionalProperties = true;
    configWifiTool.inputSchema.additionalProperties = false;

    Properties ssidProp;
    ssidProp.type = "string";
    ssidProp.description = "WiFi SSID";
    configWifiTool.inputSchema.properties["ssid"] = ssidProp;
    configWifiTool.inputSchema.required.push_back("ssid");

    Properties passwordProp;
    passwordProp.type = "string";
    passwordProp.description = "WiFi password";
    configWifiTool.inputSchema.properties["password"] = passwordProp;
    configWifiTool.handler = std::make_shared<ConfigureWiFiHandler>(*this);
    bleServer.RegisterTool(configWifiTool);

    Tool statusTool;
    statusTool.name = "get_wifi_status";
    statusTool.description = "Get current WiFi status and WiFi MCP URL";
    statusTool.inputSchema.type = "object";
    statusTool.handler = std::make_shared<WiFiStatusHandler>(*this);
    bleServer.RegisterTool(statusTool);

    bleToolsRegistered = true;
}

void MCPService::registerWifiTools() {
    if (!httpServer) {
        return;
    }

    registerACTools(*httpServer, airConditioner);
}

bool MCPService::startWifiTransport() {
    if (httpServer) {
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    httpServer = new (std::nothrow) HttpMCPServer(httpPort, HTTP_SERVER_NAME, SERVER_VERSION, SERVER_INSTRUCTIONS);
    if (!httpServer) {
        Serial.println("[MCP] Failed to allocate HTTP MCP server");
        return false;
    }

    registerWifiTools();
    Serial.printf("[MCP] WiFi MCP started at %s\n", getHttpUrl().c_str());
    return true;
}

void MCPService::ensureWifiTransport() {
    if (!httpServer && WiFi.status() == WL_CONNECTED) {
        startWifiTransport();
    }
}

JsonDocument MCPService::configureWiFi(JsonVariantConst params) {
    JsonDocument result;

    const char* ssid = params["ssid"].as<const char*>();
    const char* password = params["password"].isNull() ? "" : params["password"].as<const char*>();

    if (!ssid || strlen(ssid) == 0) {
        result["status"] = "invalid";
        result["error"] = "ssid is required";
        result["http_url"] = getHttpUrl();
        return result;
    }

    Serial.printf("[MCP] BLE WiFi configuration requested for SSID: %s\n", ssid);

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(200);
    WiFi.begin(ssid, password);

    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        startWifiTransport();
        result["status"] = "connected";
        result["ssid"] = WiFi.SSID();
        result["ip"] = ipToString(WiFi.localIP());
        result["http_url"] = getHttpUrl();
    } else {
        result["status"] = "failed";
        result["ssid"] = ssid;
        result["ip"] = "";
        result["http_url"] = "";
    }

    return result;
}

JsonDocument MCPService::getWiFiStatus(JsonVariantConst params) const {
    (void)params;

    JsonDocument result;
    const bool connected = WiFi.status() == WL_CONNECTED;

    result["status"] = connected ? "connected" : "disconnected";
    result["ssid"] = connected ? WiFi.SSID() : String("");
    result["ip"] = connected ? ipToString(WiFi.localIP()) : String("");
    result["http_mcp_started"] = httpServer != nullptr;
    result["http_url"] = getHttpUrl();
    result["ble_mcp_started"] = bleStarted;

    return result;
}

String MCPService::getHttpUrl() const {
    if (!httpServer || WiFi.status() != WL_CONNECTED) {
        return String("");
    }

    return String("http://") + ipToString(WiFi.localIP()) + ":" + String(httpPort) + "/mcp";
}

String MCPService::ipToString(const IPAddress& ip) const {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}
