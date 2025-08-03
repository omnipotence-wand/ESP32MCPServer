#include "MCPServer.h"
#include "MCPTypes.h"

using namespace mcp;

MCPServer::MCPServer(uint16_t port) : port_(port), airConditioner_(nullptr) {}

MCPServer::MCPServer(AirConditioner& ac, uint16_t port) : port_(port), airConditioner_(&ac) {
    // 注意：在全局变量初始化时，Serial可能还没有初始化，所以这里不打印调试信息
}

void MCPServer::begin(bool isConnected) {
    // 初始化LCD屏幕
}

void MCPServer::handleClient() {
    // Handle client logic
}

void MCPServer::handleInitialize(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    JsonDocument doc;
    JsonObject result = doc["result"].to<JsonObject>();
    result["serverName"] = serverInfo.name;
    result["serverVersion"] = serverInfo.version;

    sendResponse(clientId, id, MCPResponse(true, "Initialized", result));
}

void MCPServer::handleResourcesList(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    JsonDocument doc;
    JsonArray resourcesArray = doc["resources"].to<JsonArray>();

    JsonObject resObj = resourcesArray.add<JsonObject>();
    resObj["name"] = "Resource1";
    resObj["type"] = "Type1";

    sendResponse(clientId, id, MCPResponse(true, "Resources Listed", doc.as<JsonVariant>()));
}

void MCPServer::handleResourceRead(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    JsonDocument doc;
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject content = contents.add<JsonObject>();
    content["data"] = "Sample Data";

    sendResponse(clientId, id, MCPResponse(true, "Resource Read", doc.as<JsonVariant>()));
}

void MCPServer::handleSubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    sendResponse(clientId, id, MCPResponse(true, "Subscribed", JsonVariant()));
}

void MCPServer::handleUnsubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params) {
    if (!params["uri"].is<std::string>()) {
        sendError(clientId, id, 400, "Invalid URI");
        return;
    }

    sendResponse(clientId, id, MCPResponse(true, "Unsubscribed", JsonVariant()));
}

void MCPServer::unregisterResource(const std::string &uri) {
    JsonDocument doc;
    JsonObject resource = doc.to<JsonObject>();
    resource["uri"] = uri;
}

void MCPServer::sendResponse(uint8_t clientId, const RequestId &id, const MCPResponse &response) {
    JsonDocument doc;
    doc["id"] = id;
    doc["success"] = response.success;
    doc["message"] = response.message;
    doc["data"] = response.data;

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    // Transmit response
}

void MCPServer::sendError(uint8_t clientId, const RequestId &id, int code, const std::string &message) {
    JsonDocument doc;
    JsonObject error = doc["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;

    std::string jsonError;
    serializeJson(doc, jsonError);
    // Transmit error
}

void MCPServer::broadcastResourceUpdate(const std::string &uri) {
    JsonDocument doc;
    JsonObject params = doc["params"].to<JsonObject>();
    params["uri"] = uri;

    // Broadcast logic
}

MCPRequest MCPServer::parseRequest(const std::string &json) {
    JsonDocument doc;
    deserializeJson(doc, json);

    MCPRequest request;
    request.type = MCPRequestType::INITIALIZE;
    request.id = doc["id"];
    request.params = doc["params"].as<JsonObject>();
    return request;
}

std::string MCPServer::serializeResponse(const RequestId &id, const MCPResponse &response) {
    JsonDocument doc;
    doc["id"] = id;
    doc["success"] = response.success;
    doc["message"] = response.message;
    doc["data"] = response.data;

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    return jsonResponse;
}

// HTTP API methods implementation
std::string MCPServer::handleHTTPRequest(const std::string &jsonRequest) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonRequest);
    
    if (error) {
        Serial.print("JSON解析错误: ");
        Serial.println(error.c_str());
        return createJSONRPCError(-32700, "Parse error", JsonVariant(), JsonVariant());
    }
    
    // 提取JSON-RPC字段
    std::string jsonrpc = doc["jsonrpc"] | "2.0";
    JsonVariant id = doc["id"];
    std::string method = doc["method"] | "";
    JsonObject params = doc["params"];
    
    // 验证JSON-RPC版本
    if (jsonrpc != "2.0") {
        return createJSONRPCError(-32600, "Invalid Request", id, JsonVariant());
    }
    
    // 处理不同的方法
    if (method == "initialize") {
        return handleHTTPInitialize(id);
    } else if (method == "tools/list") {
        return handleHTTPToolsList(id);
    } else if (method == "notifications/initialized") {
        return "notifications/initialized";
    } else if (method=="tools/call") {
        return handleHTTPFunctionCalls(id,params);
    } else {
        return createJSONRPCError(-32601, "Method not found", id, JsonVariant());
    }
}

std::string MCPServer::handleHTTPInitialize(const JsonVariant &id) {
    JsonDocument doc;
    JsonObject result = doc.to<JsonObject>();
    
    result["protocolVersion"] = "2025-03-26";
    
    // 设置capabilities
    JsonObject capabilities = result["capabilities"].to<JsonObject>();
    JsonObject experimental = capabilities["experimental"].to<JsonObject>();
    
    JsonObject prompts = capabilities["prompts"].to<JsonObject>();
    prompts["listChanged"] = false;
    
    JsonObject resources = capabilities["resources"].to<JsonObject>();
    resources["subscribe"] = false;
    resources["listChanged"] = false;
    
    JsonObject tools = capabilities["tools"].to<JsonObject>();
    tools["listChanged"] = false;
    
    // 设置serverInfo
    JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
    serverInfo["name"] = "midea_ac";
    serverInfo["version"] = "1.9.0";
    
    result["instructions"] = "media ac";
    
    return createJSONRPCResponse(id, result);
}

std::string MCPServer::handleHTTPToolsList(const JsonVariant &id) {
    JsonDocument doc;
    JsonObject result = doc.to<JsonObject>();
    JsonArray tools = result["tools"].to<JsonArray>();
    
    // 添加一些示例工具
    JsonObject tool1 = tools.add<JsonObject>();
    tool1["name"] = "turn_on_ac";
    tool1["description"] = "Turn on the air conditioner";
    JsonObject schema1 = tool1["inputSchema"].to<JsonObject>();
    schema1["type"] = "object";
    JsonObject properties1 = schema1["properties"].to<JsonObject>();
    JsonObject temp1 = properties1["temperature"].to<JsonObject>();
    temp1["type"] = "number";
    temp1["description"] = "Target temperature";
    
    JsonObject tool2 = tools.add<JsonObject>();
    tool2["name"] = "turn_off_ac";
    tool2["description"] = "Turn off the air conditioner";
    JsonObject schema2 = tool2["inputSchema"].to<JsonObject>();
    schema2["type"] = "object";
    
    return createJSONRPCResponse(id, result);
}

std::string MCPServer::handleHTTPFunctionCalls(const JsonVariant &id, const JsonObject &params) {
    // 检查必需的参数
    if (!params["name"].is<std::string>()) {
        return createJSONRPCError(-32602, "Invalid params: missing 'name'", id, JsonVariant());
    }
    
    std::string functionName = params["name"];
    JsonObject arguments = params["arguments"];
    
    JsonDocument doc;
    JsonObject result = doc.to<JsonObject>();
    JsonArray content = result["content"].to<JsonArray>();
    
    if (functionName == "turnOn") {
        if (airConditioner_ != nullptr) {
            airConditioner_->turnOn();
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = "空调已成功开启";
        } else {
            return createJSONRPCError(-32603, "Air Conditioner not initialized", id, JsonVariant());
        }
    } else if (functionName == "turnOff") {
        if (airConditioner_ != nullptr) {
            airConditioner_->turnOff();
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = "空调已成功关闭";
        } else {
            return createJSONRPCError(-32603, "Air Conditioner not initialized", id, JsonVariant());
        }
    } else if (functionName == "setTemperature") {
        if (airConditioner_ != nullptr) {
            airConditioner_->setTemperature(arguments["temperature"]);
            JsonObject textContent = content.add<JsonObject>();
            textContent["type"] = "text";
            textContent["text"] = "ok";
        } else {
            return createJSONRPCError(-32603, "Air Conditioner not initialized", id, JsonVariant());
        }
    } else {
        // 未知的函数名称
        return createJSONRPCError(-32601, "Unknown function: " + functionName, id, JsonVariant());
    }
    
    return createJSONRPCResponse(id, result);
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
std::string MCPServer::createJSONRPCError(int code, const std::string &message, const JsonVariant &id, const JsonVariant &data) {
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    
    JsonObject error = doc["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;
    
    if (!data.isNull()) {
        error["data"] = data;
    }
    
    std::string response;
    serializeJson(doc, response);
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