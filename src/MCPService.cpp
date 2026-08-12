#include "MCPService.h"

#include <WiFi.h>
#include <cstring>
#include <initializer_list>
#include <new>

#include "ACTools.h"

namespace {
constexpr const char* BLE_SERVER_NAME = "ESP32-AC-MCP-BLE";
constexpr const char* HTTP_SERVER_NAME = "ESP32-AC-MCP-HTTP";
constexpr const char* SERVER_VERSION = "1.0.0";
constexpr const char* SERVER_INSTRUCTIONS = "Control the ESP32 air conditioner over MCP.";
constexpr uint32_t WIFI_TRANSPORT_CHECK_INTERVAL_MS = 3000;

// Schema helpers — keep outputSchema declarations readable.
Properties primitive(const String& type, const String& description) {
    Properties p;
    p.type = type;
    p.description = description;
    return p;
}

Properties stringEnum(const String& description, std::initializer_list<const char*> values) {
    Properties p;
    p.type = "string";
    p.description = description;
    for (auto v : values) {
        p.enumValues.push_back(v);
    }
    return p;
}

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

MCPService::MCPService(AirConditioner& airConditioner, NetworkManager& networkManager, uint16_t httpPort)
    : airConditioner(airConditioner),
      networkManager(networkManager),
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
    bleConfig.txPower = ESP_PWR_LVL_P9;
    bleConfig.advTxPower = ESP_PWR_LVL_P9;
    bleConfig.advMinInterval = 0x20;
    bleConfig.advMaxInterval = 0x30;
    bleServer.setBleConfig(bleConfig);
    bleServer.begin();
    bleStarted = true;
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

    /* BLE 仅保留配网工具 + get_description(app 设备身份协议要求, 缺失会导致连接失败) */
    registerDescriptionTool(bleServer, airConditioner);

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

    configWifiTool.outputSchema.type = "object";
    configWifiTool.outputSchema.properties["status"] = stringEnum(
        "Result of the configuration attempt", {"connecting", "invalid"});
    configWifiTool.outputSchema.properties["ssid"] = primitive(
        "string", "SSID the connection was attempted with (present on success)");
    configWifiTool.outputSchema.properties["ip"] = primitive(
        "string", "Local IP, empty until the connection completes");
    configWifiTool.outputSchema.properties["error"] = primitive(
        "string", "Error message when status is 'invalid'");
    configWifiTool.outputSchema.properties["http_url"] = primitive(
        "string", "HTTP MCP URL once WiFi is up, empty otherwise");
    configWifiTool.outputSchema.required.push_back("status");
    configWifiTool.outputSchema.required.push_back("http_url");

    configWifiTool.handler = std::make_shared<ConfigureWiFiHandler>(*this);
    bleServer.RegisterTool(configWifiTool);

    Tool statusTool;
    statusTool.name = "get_wifi_status";
    statusTool.description = "Get current WiFi status and WiFi MCP URL";
    statusTool.inputSchema.type = "object";

    statusTool.outputSchema.type = "object";
    statusTool.outputSchema.properties["status"] = stringEnum(
        "WiFi link state", {"connected", "disconnected"});
    statusTool.outputSchema.properties["ssid"] = primitive(
        "string", "Connected SSID, empty when disconnected");
    statusTool.outputSchema.properties["ip"] = primitive(
        "string", "Local IP address, empty when disconnected");
    statusTool.outputSchema.properties["network_state"] = stringEnum(
        "NetworkManager state",
        {"idle", "connecting", "connected", "failed", "ap_mode", "unknown"});
    statusTool.outputSchema.properties["http_mcp_started"] = primitive(
        "boolean", "True when the HTTP MCP transport is running");
    statusTool.outputSchema.properties["http_url"] = primitive(
        "string", "HTTP MCP URL, empty when not started");
    statusTool.outputSchema.properties["ble_mcp_started"] = primitive(
        "boolean", "True when the BLE MCP transport is running");
    statusTool.outputSchema.required.push_back("status");
    statusTool.outputSchema.required.push_back("ssid");
    statusTool.outputSchema.required.push_back("ip");
    statusTool.outputSchema.required.push_back("network_state");
    statusTool.outputSchema.required.push_back("http_mcp_started");
    statusTool.outputSchema.required.push_back("http_url");
    statusTool.outputSchema.required.push_back("ble_mcp_started");

    statusTool.handler = std::make_shared<WiFiStatusHandler>(*this);
    bleServer.RegisterTool(statusTool);

    bleToolsRegistered = true;
}

void MCPService::registerWifiTools() {
    if (!httpServer) {
        return;
    }

    const uint32_t heapBefore = ESP.getFreeHeap();
    registerACTools(*httpServer, airConditioner);
    const uint32_t heapAfter = ESP.getFreeHeap();
    Serial.printf("[MCP] HTTP tool schemas: heap %u -> %u bytes (%u bytes used)\n",
                  heapBefore, heapAfter, heapBefore - heapAfter);
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

    /* 0.4.0 起 HttpMCPServer 构造后不再自动监听: 工具注册完成后显式 begin()，
     * 避免请求处理与工具注册竞争。 */
    if (!httpServer->begin()) {
        Serial.println("[MCP] Failed to start HTTP MCP server");
        delete httpServer;
        httpServer = nullptr;
        return false;
    }

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

    if (!networkManager.connect(ssid, password)) {
        result["status"] = "invalid";
        result["error"] = "failed to start WiFi connection";
        result["http_url"] = getHttpUrl();
        return result;
    }

    result["status"] = "connecting";
    result["ssid"] = ssid;
    result["ip"] = "";
    result["http_url"] = "";

    return result;
}

JsonDocument MCPService::getWiFiStatus(JsonVariantConst params) const {
    (void)params;

    JsonDocument result;
    const bool connected = WiFi.status() == WL_CONNECTED;

    result["status"] = connected ? "connected" : "disconnected";
    result["ssid"] = connected ? WiFi.SSID() : String("");
    result["ip"] = connected ? ipToString(WiFi.localIP()) : String("");
    result["network_state"] = networkStateToString(networkManager.getState());
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

String MCPService::networkStateToString(NetworkState state) const {
    switch (state) {
        case NetworkState::INIT:
            return "idle";
        case NetworkState::CONNECTING:
            return "connecting";
        case NetworkState::CONNECTED:
            return "connected";
        case NetworkState::CONNECTION_FAILED:
            return "failed";
        case NetworkState::AP_MODE:
            return "ap_mode";
    }

    return "unknown";
}

String MCPService::ipToString(const IPAddress& ip) const {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}
