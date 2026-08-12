#include "NetworkManager.h"

NetworkManager::NetworkManager() :
      connectAttempts(0),
      lastConnectAttempt(0),
      state(NetworkState::INIT),
      requestedSSID(""),
      requestedPassword(""),
      statusPrinted(false) {
}

void NetworkManager::begin() {
    Serial.println("[Network] Initializing WiFi manager");
    // WiFi.SSID() reports the currently associated AP, not the station
    // credentials saved in NVS. Checking it before WiFi.begin() therefore
    // makes every reboot look unconfigured and prevents HTTP MCP from coming
    // back. Let the SDK load and connect with its persisted station config;
    // BLE configuration remains available if that attempt eventually fails.
    startConnection();
}

void NetworkManager::loop() {
    if (state != NetworkState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED && !statusPrinted) {
            state = NetworkState::CONNECTED;
            printConnectionStatus();
            statusPrinted = true;
        }
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastConnectAttempt < CONNECT_TIMEOUT) {
            return;
        }

        connectAttempts++;
        Serial.printf("[Network] WiFi connection attempt %u failed\n", connectAttempts);
        if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
            state = NetworkState::CONNECTION_FAILED;
            Serial.println("[Network] WiFi connection failed; keeping BLE configuration available");
            return;
        }

        if (millis() - lastConnectAttempt >= RECONNECT_INTERVAL) {
            startConnection(requestedSSID.length() > 0 ? requestedSSID.c_str() : nullptr,
                            requestedSSID.length() > 0 ? requestedPassword.c_str() : nullptr);
        }
        return;
    }

    state = NetworkState::CONNECTED;
    printConnectionStatus();
    statusPrinted = true;
}

bool NetworkManager::connect(const String& ssid, const String& password) {
    if (ssid.length() == 0) {
        Serial.println("[Network] Refusing WiFi connection with empty SSID");
        return false;
    }

    requestedSSID = ssid;
    requestedPassword = password;
    connectAttempts = 0;
    startConnection(requestedSSID.c_str(), requestedPassword.c_str());
    return true;
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkManager::getSSID() const {
    return isConnected() ? WiFi.SSID() : requestedSSID;
}

String NetworkManager::getIPAddress() const {
    return isConnected() ? WiFi.localIP().toString() : String("");
}

NetworkState NetworkManager::getState() const {
    return state;
}

void NetworkManager::startConnection(const char* ssid, const char* password) {
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);

    statusPrinted = false;
    state = NetworkState::CONNECTING;
    lastConnectAttempt = millis();

    if (ssid && strlen(ssid) > 0) {
        Serial.printf("[Network] Connecting to WiFi SSID: %s\n", ssid);
        WiFi.begin(ssid, password ? password : "");
    } else {
        Serial.println("[Network] Connecting with saved WiFi credentials");
        WiFi.begin();
    }
}

void NetworkManager::printConnectionStatus() {
    Serial.println("======================================");
    Serial.println("           WiFi CONNECTION SUCCESS");
    Serial.println("======================================");
    
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
    
    Serial.println("======================================");
    Serial.println("Device is ready for MCP connections!");
    Serial.println("======================================");
}
