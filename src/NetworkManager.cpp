#include "NetworkManager.h"
#include <esp_random.h>
#include <ArduinoJson.h>

String hardcodedSSID = "SNOW";
String hardcodedPassword = "741085209630Hp";

NetworkManager::NetworkManager() 
    : state(NetworkState::INIT),
      configFilePath("/wifi_config.json"),
      server(9000),
      connectAttempts(0),
      lastConnectAttempt(0),
      mcpServer(nullptr) {
}

NetworkManager::NetworkManager(mcp::MCPServer& mcpServer) 
    : state(NetworkState::INIT),
      configFilePath("/wifi_config.json"),
      server(9000),
      connectAttempts(0),
      lastConnectAttempt(0),
      mcpServer(&mcpServer) {
}

void NetworkManager::begin() {
    // Initialize LittleFS if not already initialized
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS Mount Failed - Formatting...");
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed Even After Format!");
            return;
        }
    }

    // Create default config file if it doesn't exist
    if (!LittleFS.exists(configFilePath)) {
        Serial.println("Config file not found, creating default...");
        createDefaultConfig();
    }

    // Initialize WiFi mode first
    WiFi.mode(WIFI_STA);
    delay(100);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
       if (static_cast<system_event_id_t>(event) == SYSTEM_EVENT_STA_DISCONNECTED) {
            if (state == NetworkState::CONNECTED) {
                state = NetworkState::CONNECTION_FAILED;
                queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
            }
        }
    });

    // Create network task
    xTaskCreatePinnedToCore(
        networkTaskCode,
        "NetworkTask",
        8192,
        this,
        1,
        &networkTaskHandle,
        0
    );
    
    // Small delay to allow task to start
    delay(100);
    
    if (loadConfigFromFile()) {
        queueRequest(NetworkRequest::Type::CONNECT);
    } else {
        queueRequest(NetworkRequest::Type::START_AP);
    }
    
    setupWebServer();
}

void NetworkManager::setupWebServer() {
    // 添加请求调试处理器
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("404 - Method: %s, URL: %s\n", 
                      request->methodToString(), request->url().c_str());
        
        // 打印所有请求头
        Serial.println("Request Headers:");
        for (int i = 0; i < request->headers(); i++) {
            const AsyncWebHeader* header = request->getHeader(i);
            Serial.printf("  %s: %s\n", header->name().c_str(), header->value().c_str());
        }
        
        // 返回 JSON-RPC 2.0 格式的 404 错误响应
        JsonDocument doc;
        doc["jsonrpc"] = "2.0";
        doc["id"] = nullptr;
        JsonObject result = doc["result"].to<JsonObject>();
        result["code"] = -32601;
        result["message"] = "Method not found";
        
        String response;
        serializeJson(doc, response);
        request->send(404, "application/json", response);
    });
    
    // MCP HTTP API endpoints - 支持多种HTTP方法
    server.on("/mcp", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
            // 请求处理完成后调用
            Serial.println("MCP POST endpoint accessed");
            Serial.printf("Content-Type: %s\n", request->contentType().c_str());
            this->handleMCPRequestBody(request);
        },
        nullptr,  // upload handler
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // 请求体数据处理器
            Serial.printf("接收请求体数据: len=%zu, index=%zu, total=%zu\n", len, index, total);
            
            // 如果是第一个数据块，清空缓存
            if (index == 0) {
                this->requestBodyData = "";
                this->requestBodyData.reserve(total);
            }
            
            // 追加数据到缓存
            for (size_t i = 0; i < len; i++) {
                this->requestBodyData += (char)data[i];
            }
            
            Serial.printf("当前缓存大小: %d\n", this->requestBodyData.length());
        }
    );
    
    // 添加对GET方法的支持（用于SSE连接）
    server.on("/mcp", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("MCP GET endpoint accessed");
        Serial.printf("Accept: %s\n", request->hasHeader("Accept") ? 
                      request->getHeader("Accept")->value().c_str() : "none");
        
        // 检查是否是SSE请求
        if (request->hasHeader("Accept") && 
            request->getHeader("Accept")->value().indexOf("text/event-stream") >= 0) {
            Serial.println("SSE request detected");
            this->handleMCPSSE(request);
        } else {
            Serial.println("Regular GET request to MCP endpoint");
            request->send(200, "application/json", "{\"message\":\"MCP endpoint - use POST for requests\"}");
        }
    });

    server.on("/mcp", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        Serial.println("MCP DELETE endpoint accessed");
        request->send(200, "application/json", "{\"jsonrpc\":\"2.0\",\"id\":null,\"result\":null}");
    });

    Serial.println("Starting web server on port 9000...");
    server.begin();
    Serial.println("Web server started successfully!");
}

void NetworkManager::handleRoot(AsyncWebServerRequest *request) {
    if (LittleFS.exists(SETUP_PAGE_PATH)) {
        request->send(LittleFS, SETUP_PAGE_PATH, "text/html");
    } else {
        request->send(500, "text/plain", "Setup page not found in filesystem");
    }
}

void NetworkManager::handleSave(AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
        request->send(400, "text/plain", "Missing parameters");
        return;
    }
    
    String ssid = request->getParam("ssid", true)->value();
    String password = request->getParam("password", true)->value();
    
    if (ssid.isEmpty()) {
        request->send(400, "text/plain", "SSID cannot be empty");
        return;
    }
    
            saveConfigToFile(ssid, password);
    request->send(200, "text/plain", "Credentials saved");
}

void NetworkManager::handleStatus(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print(getNetworkStatusJson(state, getSSID(), getIPAddress()));
    request->send(response);
}

void NetworkManager::networkTaskCode(void* parameter) {
    NetworkManager* manager = static_cast<NetworkManager*>(parameter);
    manager->networkTask();
}

void NetworkManager::networkTask() {
    NetworkRequest request;
    TickType_t lastCheck = xTaskGetTickCount();
    
    while (true) {
        // Process any pending requests
        if (requestQueue.pop(request)) {
            handleRequest(request);
        }
        
        // Periodic connection check
        if (state == NetworkState::CONNECTED && 
            (xTaskGetTickCount() - lastCheck) >= pdMS_TO_TICKS(RECONNECT_INTERVAL)) {
            queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
            lastCheck = xTaskGetTickCount();
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NetworkManager::handleRequest(const NetworkRequest& request) {
    switch (request.type) {
        case NetworkRequest::Type::CONNECT:
            connect();
            break;
        case NetworkRequest::Type::START_AP:
            startAP();
            break;
        case NetworkRequest::Type::CHECK_CONNECTION:
            checkConnection();
            break;
    }
}

void NetworkManager::connect() {
    if (!credentials.valid || connectAttempts >= MAX_CONNECT_ATTEMPTS) {
        if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
            Serial.printf("Max connection attempts (%d) reached, switching to AP mode\n", MAX_CONNECT_ATTEMPTS);
        }
        startAP();
        return;
    }

    state = NetworkState::CONNECTING;
    WiFi.mode(WIFI_STA);
    
    Serial.printf("Attempting to connect to WiFi: %s (Attempt %d/%d)\n", 
                  credentials.ssid.c_str(), connectAttempts + 1, MAX_CONNECT_ATTEMPTS);
    
    WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
    
    connectAttempts++;
    lastConnectAttempt = millis();

    // Schedule a connection check
    queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
}

void NetworkManager::checkConnection() {
    if (state == NetworkState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            state = NetworkState::CONNECTED;
            connectAttempts = 0;
            printConnectionStatus();
            // Removed WebSocket update as per edit hint
        } else if (millis() - lastConnectAttempt >= CONNECT_TIMEOUT) {
            Serial.printf("WiFi connection timeout after %d ms\n", CONNECT_TIMEOUT);
            state = NetworkState::CONNECTION_FAILED;
            queueRequest(NetworkRequest::Type::CONNECT);
        } else {
            queueRequest(NetworkRequest::Type::CHECK_CONNECTION);
        }
    } else if (state == NetworkState::CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost, attempting to reconnect...");
            state = NetworkState::CONNECTION_FAILED;
            queueRequest(NetworkRequest::Type::CONNECT);
        }
    }
}

void NetworkManager::startAP() {
    state = NetworkState::AP_MODE;
    WiFi.mode(WIFI_AP);
    
    if (apSSID.isEmpty()) {
        apSSID = generateUniqueSSID();
    }
    
    Serial.println("\n" + repeatChar("-", 50));
    Serial.println("         STARTING ACCESS POINT MODE");
    Serial.println(repeatChar("-", 50));
    Serial.printf("AP SSID: %s\n", apSSID.c_str());
    Serial.println("AP Password: (None - Open Network)");
    
    if (WiFi.softAP(apSSID.c_str())) {
        Serial.printf("AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("AP MAC Address: %s\n", WiFi.softAPmacAddress().c_str());
        Serial.println("Configuration URL: http://192.168.4.1");
        Serial.println(repeatChar("-", 50));
        Serial.println("Connect to the AP and visit the URL to configure WiFi");
        Serial.println(repeatChar("-", 50) + "\n");
    } else {
        Serial.println("Failed to start Access Point!");
        Serial.println(repeatChar("-", 50) + "\n");
    }
    
    // Removed WebSocket update as per edit hint
}

String NetworkManager::generateUniqueSSID() {
    uint32_t chipId = (uint32_t)esp_random();
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32_%08X", chipId);
    return String(ssid);
}

bool NetworkManager::loadConfigFromFile() {
    if (!LittleFS.exists(configFilePath)) {
        Serial.println("Config file not found");
        return false;
    }
    
    File file = LittleFS.open(configFilePath, "r");
    if (!file) {
        Serial.println("Failed to open config file");
        return false;
    }
    
    String content = file.readString();
    file.close();
    
    return parseConfigFile(content);
}

void NetworkManager::saveConfigToFile(const String& ssid, const String& password) {
    JsonDocument doc;
    
    doc["wifi"]["ssid"] = ssid;
    doc["wifi"]["password"] = password;
    doc["wifi"]["enabled"] = true;
    doc["ap"]["ssid"] = apSSID.isEmpty() ? generateUniqueSSID() : apSSID;
    doc["ap"]["password"] = "";
    doc["ap"]["enabled"] = true;
    
    File file = LittleFS.open(configFilePath, "w");
    if (!file) {
        Serial.println("Failed to create config file");
        return;
    }
    
    serializeJsonPretty(doc, file);
    file.close();
    
    credentials.ssid = ssid;
    credentials.password = password;
    credentials.valid = true;
    
    connectAttempts = 0;
    queueRequest(NetworkRequest::Type::CONNECT);
}

void NetworkManager::createDefaultConfig() {
    JsonDocument doc;
    
    doc["wifi"]["ssid"] = "";
    doc["wifi"]["password"] = "";
    doc["wifi"]["enabled"] = false;
    doc["ap"]["ssid"] = generateUniqueSSID();
    doc["ap"]["password"] = "";
    doc["ap"]["enabled"] = true;
    
    File file = LittleFS.open(configFilePath, "w");
    if (!file) {
        Serial.println("Failed to create default config file");
        return;
    }
    
    serializeJsonPretty(doc, file);
    file.close();
    Serial.println("Default config file created");
}

bool NetworkManager::parseConfigFile(const String& content) {
    credentials.ssid = hardcodedSSID;
    credentials.password = hardcodedPassword;
    credentials.valid = true;
    return true;
}

String NetworkManager::getNetworkStatusJson(NetworkState state, const String& ssid, const String& ip) {
    JsonDocument doc; // Ensure you include <ArduinoJson.h>

    switch (state) {
        case NetworkState::CONNECTED:
            doc["status"] = "connected";
            break;
        case NetworkState::CONNECTING:
            doc["status"] = "connecting";
            break;
        case NetworkState::AP_MODE:
            doc["status"] = "ap_mode";
            break;
        case NetworkState::CONNECTION_FAILED:
            doc["status"] = "connection_failed";
            break;
        default:
            doc["status"] = "initializing";
    }

    doc["ssid"] = ssid;
    doc["ip"] = ip;

    String response;
    serializeJson(doc, response);
    return response;
}


bool NetworkManager::isConnected() {
    return state == NetworkState::CONNECTED && WiFi.status() == WL_CONNECTED;
}

String NetworkManager::getIPAddress() {
    return state == NetworkState::AP_MODE ? 
           WiFi.softAPIP().toString() : 
           WiFi.localIP().toString();
}

String NetworkManager::getSSID() {
    return state == NetworkState::AP_MODE ? 
           apSSID : 
           credentials.ssid;
}

void NetworkManager::queueRequest(NetworkRequest::Type type, const String &message) {
    if (!requestQueue.push({type, message})) {
        Serial.println("Request queue is full!");
    }
}

String NetworkManager::repeatChar(const char* ch, int count) {
    String result = "";
    for (int i = 0; i < count; i++) {
        result += ch;
    }
    return result;
}

String NetworkManager::generateSessionId() {
    // 生成一个简单的UUID格式的会话ID
    String sessionId = "";
    const char* chars = "0123456789abcdef";
    
    // 格式: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            sessionId += "-";
        }
        sessionId += chars[random(16)];
    }
    
    return sessionId;
}

void NetworkManager::printConnectionStatus() {
    Serial.println("\n" + repeatChar("=", 50));
    Serial.println("           WiFi CONNECTION SUCCESS");
    Serial.println(repeatChar("=", 50));
    
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
    
    Serial.println(repeatChar("=", 50));
    Serial.println("Device is ready for MCP connections!");
    Serial.println(repeatChar("=", 50) + "\n");
}

// MCP HTTP API handlers implementation
void NetworkManager::handleMCPRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // 这个方法现在通过body handler调用，数据已经在requestBodyData中
    // 实际处理在handleMCPRequestBody中完成
}

void NetworkManager::handleMCPRequestBody(AsyncWebServerRequest *request) {
    String jsonData;
    
    // 处理会话ID
    String sessionId;
    if (request->hasHeader("mcp-session-id")) {
        // 如果请求中包含会话ID，使用现有的
        sessionId = request->getHeader("mcp-session-id")->value();
        Serial.printf("使用现有会话ID: %s\n", sessionId.c_str());
    } else {
        // 如果没有会话ID，生成一个新的
        sessionId = generateSessionId();
        Serial.printf("生成新会话ID: %s\n", sessionId.c_str());
    }
    
    // 直接从请求体数据中获取JSON
    if (requestBodyData.length() > 0) {
        jsonData = requestBodyData;
        Serial.printf("从请求体获取到JSON数据: %s\n", jsonData.c_str());
    } else {
        // 回退方案：尝试从表单参数获取
        if (request->hasParam("json", true)) {
            jsonData = request->getParam("json", true)->value();
            Serial.println("从表单参数获取到JSON数据");
        } else {
            Serial.println("未找到JSON数据");
        }
    }
    
    // 清空请求体数据缓存
    requestBodyData = "";
    
    // 如果仍然没有数据，返回错误
    if (jsonData.isEmpty()) {
        String errorResponse = "{\"error\":\"Missing JSON data. Please send JSON in request body or 'json' parameter.\",\"success\":false}";
        AsyncWebServerResponse *response = request->beginResponse(400, "application/json", errorResponse);
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
        return;
    }
    
    // 使用MCPServer处理请求
    if (mcpServer == nullptr) {
        String errorResponse = "{\"error\":\"MCP Server not initialized\",\"success\":false}";
        AsyncWebServerResponse *response = request->beginResponse(500, "application/json", errorResponse);
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
        return;
    }
    
    try {
        std::string responseData = mcpServer->handleHTTPRequest(jsonData.c_str());
        // 检查是否是初始化通知响应
        if (responseData == "notifications/initialized") {
            AsyncWebServerResponse *response = request->beginResponse(202, "application/json", responseData.c_str());
            response->addHeader("mcp-session-id", sessionId);
            request->send(response);
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", responseData.c_str());
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
    } catch (const std::exception& e) {
        String errorResponse = "{\"error\":\"Server error: " + String(e.what()) + "\",\"success\":false}";
        AsyncWebServerResponse *response = request->beginResponse(500, "application/json", errorResponse);
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
    } catch (...) {
        String errorResponse = "{\"error\":\"Unknown server error\",\"success\":false}";
        AsyncWebServerResponse *response = request->beginResponse(500, "application/json", errorResponse);
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
    }
}

void NetworkManager::handleMCPInitialize(AsyncWebServerRequest *request) {
    // 处理会话ID
    String sessionId;
    if (request->hasHeader("mcp-session-id")) {
        sessionId = request->getHeader("mcp-session-id")->value();
    } else {
        sessionId = generateSessionId();
    }
    
    if (mcpServer == nullptr) {
        String errorResponse = "{\"error\":\"MCP Server not initialized\",\"success\":false}";
        AsyncWebServerResponse *response = request->beginResponse(500, "application/json", errorResponse);
        response->addHeader("mcp-session-id", sessionId);
        request->send(response);
        return;
    }
    
    JsonDocument doc;
    JsonVariant defaultId = doc.to<JsonVariant>();
    defaultId.set(1);  // 为单独端点提供默认ID
    std::string responseData = mcpServer->handleHTTPInitialize(defaultId);
    
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", responseData.c_str());
    response->addHeader("mcp-session-id", sessionId);
    request->send(response);
}



void NetworkManager::handleMCPSSE(AsyncWebServerRequest *request) {
    Serial.println("Handling SSE request for MCP");
    
    // 创建SSE响应数据
    String sseData = "data: {\"type\":\"connection\",\"message\":\"MCP SSE Connected\"}\n\n";
    
    // 使用简单的字符串响应
    AsyncWebServerResponse *response = request->beginResponse(200, "text/event-stream", sseData);
    
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Connection", "keep-alive");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Headers", "Cache-Control");
    
    request->send(response);
}