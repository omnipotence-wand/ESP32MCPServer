#include "MCPServer.h"
#include "MCPTypes.h"

using namespace mcp;

MCPServer::MCPServer(AirConditioner& ac, uint16_t port) : port_(port), airConditioner_(&ac), server(port) {}


void MCPServer::setupWebServer() {

    server.on("/mcp", HTTP_POST, [this](AsyncWebServerRequest *request) {
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // body 数据处理回调
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        // 打印收到的MCP请求内容
        Serial.println("========================================");
        Serial.println("收到 /mcp 请求:");
        Serial.printf("客户端IP: %s\n", request->client()->remoteIP().toString().c_str());
        Serial.printf("请求长度: %zu bytes\n", len);
        Serial.printf("请求内容: %s\n", body.c_str());
        
        // 打印请求头
        Serial.println("请求头:");
        for (size_t i = 0; i < request->headers(); i++) {
            AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("  %s: %s\n", h->name().c_str(), h->value().c_str());
        }
        Serial.println("========================================");
        
        MCPRequest mcpReq = parseRequest(body.c_str());
        MCPResponse mcpRes = handle(mcpReq);
        std::string jsonResponse = serializeResponse(mcpRes);

        // 处理会话ID
        std::string sessionId;
        if (request->hasHeader("mcp-session-id")) {
            sessionId = request->getHeader("mcp-session-id")->value().c_str();
        } else {
            sessionId = generateSessionId();
        }
        // 发送响应
        AsyncWebServerResponse *response = request->beginResponse(mcpRes.code, "application/json", jsonResponse.c_str());
        response->addHeader("mcp-session-id", sessionId.c_str());
        request->send(response);
    });
    
    server.on("/mcp", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        // 打印收到的MCP DELETE请求
        Serial.println("========================================");
        Serial.println("收到 /mcp DELETE 请求:");
        Serial.printf("客户端IP: %s\n", request->client()->remoteIP().toString().c_str());
        
        // 打印请求头
        Serial.println("请求头:");
        for (size_t i = 0; i < request->headers(); i++) {
            AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("  %s: %s\n", h->name().c_str(), h->value().c_str());
        }
        Serial.println("========================================");
        
        // DELETE 请求通常不需要请求体，直接返回成功响应
        MCPResponse res(200, 0); // 使用新的构造函数
        std::string jsonResponse = serializeResponse(res);
        
        // 处理会话ID
        std::string sessionId;
        if (request->hasHeader("mcp-session-id")) {
            sessionId = request->getHeader("mcp-session-id")->value().c_str();
        } else {
            sessionId = generateSessionId();
        }
        
        // 发送响应
        AsyncWebServerResponse *response = request->beginResponse(res.code, "application/json", jsonResponse.c_str());
        response->addHeader("mcp-session-id", sessionId.c_str());
        request->send(response);
    });
    server.onNotFound([this](AsyncWebServerRequest *request) {
        // 处理未找到的请求
        MCPResponse res = createJSONRPCError(404, -32600, 0, "Path Not Found");
        std::string jsonResponse = serializeResponse(res);
        request->send(res.code, "application/json", jsonResponse.c_str());
    });
    server.begin();
}

MCPRequest MCPServer::parseRequest(const std::string &json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    MCPRequest request;
    
    // 检查 JSON 解析是否成功
    if (error) {
        // 如果解析失败，返回一个默认的请求对象
        request.method = "";
        request.id = "";
        return request;
    }
    
    request.method = doc["method"].as<std::string>();
    request.id = doc["id"].as<std::string>();
    request.paramsDoc = doc["params"];
    return request;
}

std::string MCPServer::serializeResponse(const MCPResponse &response) {
    JsonDocument doc;
    doc["id"] = response.id;
    doc["jsonrpc"] = "2.0";

    if (response.hasResult()) {
        doc["result"] = response.result();
    }
    if (response.hasError()) {
        doc["error"] = response.error();
    }

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    return jsonResponse;
}



MCPResponse MCPServer::handle(MCPRequest &request) {
    
    // 检查请求方法是否为空（表示 JSON 解析失败）
    if (request.method.empty()) {
        return createJSONRPCError(400, -32700, request.id, "Parse error: Invalid JSON");
    }
    
    if (request.method == "initialize") {
        return handleInitialize(request);
    } else if (request.method == "tools/list") {
        return handleToolsList(request);
    } else if (request.method == "notifications/initialized") {
        // 处理初始化通知
        return handleInitialized(request);
    } else if (request.method == "tools/call") {
        return handleFunctionCalls(request);
    } else {
        // 未知方法
        return createJSONRPCError(200, -32601, request.id, "Method not found: " + request.method);
    }
}

MCPResponse MCPServer::handleInitialize(MCPRequest &request) {
    MCPResponse response(200, request.id);
    JsonObject result = response.resultDoc.to<JsonObject>();
    
    result["protocolVersion"] = "2025-03-26";
    
    // 设置capabilities
    JsonObject capabilities = result["capabilities"].to<JsonObject>();
    JsonObject experimental = capabilities["experimental"].to<JsonObject>();
    
    
    JsonObject tools = capabilities["tools"].to<JsonObject>();
    tools["listChanged"] = false;
    
    // 设置serverInfo
    JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
    serverInfo["name"] = "midea_ac";
    serverInfo["version"] = "1.9.0";
    
    result["instructions"] = "media ac";
    
    return response;
}

MCPResponse MCPServer::handleInitialized(MCPRequest &request){
    return MCPResponse(202, request.id);
}

MCPResponse MCPServer::handleToolsList(MCPRequest &request) {
    MCPResponse response(200, request.id);
    String toolsJson = airConditioner_->listTools();
    DeserializationError error = deserializeJson(response.resultDoc, toolsJson);
    
    return response;
}

MCPResponse MCPServer::handleFunctionCalls(MCPRequest &request) {
    MCPResponse mcpResponse(200, request.id);
    JsonVariantConst params = request.params();

    // 检查必需的参数
    if (!params["name"].is<std::string>()) {
        return createJSONRPCError(200, -32602, request.id, "Missing or invalid 'name' parameter");
    }
    
    std::string functionName = params["name"];
    JsonVariantConst arguments = params["arguments"];
    
    JsonObject result = mcpResponse.resultDoc.to<JsonObject>();
    JsonArray content = result["content"].to<JsonArray>();
    
    if (functionName == "turnOn") {
        if (airConditioner_ != nullptr) {
            airConditioner_->turnOn();
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = "空调已成功开启";
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if (functionName == "turnOff") {
        if (airConditioner_ != nullptr) {
            airConditioner_->turnOff();
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = "空调已成功关闭";
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if (functionName == "setTemperature") {
        if (airConditioner_ != nullptr) {
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = airConditioner_->setTemperature(arguments["temperature"].as<int>());
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if (functionName=="setMode"){
        if (airConditioner_ != nullptr) {
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = airConditioner_->setMode(arguments["mode"].as<int>());
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if (functionName=="getModeString"){
        if (airConditioner_ != nullptr) {
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = airConditioner_->getModeString();
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if (functionName=="get_description"){
        if (airConditioner_ != nullptr) {
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = airConditioner_->description();
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else if(functionName=="getStatusJSON"){
        if (airConditioner_ != nullptr) {
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = airConditioner_->getStatusJSON();
        } else {
            return createJSONRPCError(200,-32603, request.id,"Air Conditioner not initialized");
        }
    } else {
        // 未知的函数名称
        return createJSONRPCError(200,-32601, request.id,"Method not supported: " + functionName);
    }
    return mcpResponse;
}

// JSON-RPC 2.0 成功响应
std::string MCPServer::createJSONRPCResponse(const JsonVariant &id, const JsonVariant &result) {
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    doc["result"] = result;
    
    std::string response;
    serializeJson(doc, response);
    return response;
}

// JSON-RPC 2.0 错误响应
MCPResponse MCPServer::createJSONRPCError(int httpCode,int code, RequestId id, const std::string &message) {
    MCPResponse response(httpCode, id);
    
    response.errorDoc["code"] = code;
    response.errorDoc["message"] = message;

    return response;
}

// 保留旧方法以保持兼容性
std::string MCPServer::createHTTPResponse(bool success, const std::string &message, const JsonVariant &data) {
    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = message;
    
    if (!data.isNull()) {
        doc["data"] = data;
    }
    
    std::string response;
    serializeJson(doc, response);
    return response;
}


std::string MCPServer::generateSessionId() {
    // 生成一个简单的UUID格式的会话ID
    std::string sessionId = "";
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