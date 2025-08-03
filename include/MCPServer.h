#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <ArduinoJson.h>
#include "MCPTypes.h"
#include "ac.h"
#include <unordered_map>
#include <string>
#include <functional>

namespace mcp {

struct Implementation {
    std::string name;
    std::string version;
};

struct ServerCapabilities {
    bool supportsSubscriptions;
    bool supportsResources;
};

class MCPServer {
public:
    MCPServer(uint16_t port = 9000);
    MCPServer(AirConditioner& ac, uint16_t port = 9000);

    void begin(bool isConnected);
    void handleClient();
    void handleInitialize(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleResourcesList(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleResourceRead(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleSubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void handleUnsubscribe(uint8_t clientId, const RequestId &id, const JsonObject &params);
    void unregisterResource(const std::string &uri);
    void sendResponse(uint8_t clientId, const RequestId &id, const MCPResponse &response);
    void sendError(uint8_t clientId, const RequestId &id, int code, const std::string &message);
    void broadcastResourceUpdate(const std::string &uri);
    
    // HTTP API methods
    std::string handleHTTPRequest(const std::string &jsonRequest);
    std::string handleHTTPInitialize(const JsonVariant &id);
    std::string handleHTTPToolsList(const JsonVariant &id);
    std::string handleHTTPFunctionCalls(const JsonVariant &id, const JsonObject &params);

private:
    uint16_t port_;
    Implementation serverInfo{"esp32-mcp-server", "1.0.0"};
    ServerCapabilities capabilities{true, true};
    AirConditioner* airConditioner_; // 空调对象指针

    MCPRequest parseRequest(const std::string &json);
    std::string serializeResponse(const RequestId &id, const MCPResponse &response);
    
    // JSON-RPC 2.0 响应方法
    std::string createJSONRPCResponse(const JsonVariant &id, const JsonVariant &result);
    std::string createJSONRPCError(int code, const std::string &message, const JsonVariant &id, const JsonVariant &data = JsonVariant());
    
    // 保留旧方法以保持兼容性
    std::string createHTTPResponse(bool success, const std::string &message, const JsonVariant &data = JsonVariant());
};

} // namespace mcp

#endif // MCP_SERVER_H
