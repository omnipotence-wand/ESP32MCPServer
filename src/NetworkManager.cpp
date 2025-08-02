#include "NetworkManager.h"
#include <esp_random.h>
#include <ArduinoJson.h>


NetworkManager::NetworkManager() 
    : state(NetworkState::INIT),
      configFilePath("/wifi_config.json"),
      server(80),
      ws("/ws"),
      connectAttempts(0),
      lastConnectAttempt(0) {
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
    ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleRoot(request);
    });
    
    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSave(request);
    });
    
    server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleStatus(request);
    });

    server.begin();
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

void NetworkManager::onWebSocketEvent(AsyncWebSocket* server, 
                                    AsyncWebSocketClient* client,
                                    AwsEventType type, 
                                    void* arg, 
                                    uint8_t* data, 
                                    size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            client->text(getNetworkStatusJson(state, getSSID(), getIPAddress()));
            break;
        case WS_EVT_DISCONNECT:
            break;
        case WS_EVT_ERROR:
            break;
        case WS_EVT_DATA:
            break;
    }
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
            ws.textAll(getNetworkStatusJson(state, getSSID(), getIPAddress()));
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
    
    ws.textAll(getNetworkStatusJson(state, apSSID, WiFi.softAPIP().toString()));
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
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        Serial.print("Failed to parse config file: ");
        Serial.println(error.c_str());
        return false;
    }
    
    Serial.println("Configuration file loaded successfully");
    
    // Check if WiFi is enabled and has valid credentials
    if (doc["wifi"]["enabled"].as<bool>()) {
        credentials.ssid = doc["wifi"]["ssid"].as<String>();
        credentials.password = doc["wifi"]["password"].as<String>();
        credentials.valid = !credentials.ssid.isEmpty();
        
        if (credentials.valid) {
            Serial.printf("WiFi configuration found - SSID: %s\n", credentials.ssid.c_str());
            Serial.println("WiFi connection will be attempted");
        } else {
            Serial.println("WiFi enabled but SSID is empty");
        }
        return credentials.valid;
    } else {
        Serial.println("WiFi is disabled in configuration");
    }
    
    credentials.ssid = "";
    credentials.password = "";
    credentials.valid = false;
    return false;
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