#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <ArduinoJson.h>
#include "MCPTypes.h"
#include "ac.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <ESPAsyncWebServer.h>

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

    // Setup the web server
    void setupWebServer();
    
    // HTTP API methods
    std::string handleHTTPRequest(const std::string &jsonRequest);


private:
    uint16_t port_;
    AirConditioner* airConditioner_;
    AsyncWebServer server;

    MCPResponse handle(MCPRequest &request);
    MCPResponse handleInitialize(MCPRequest &request);
    MCPResponse handleInitialized(MCPRequest &request);
    MCPResponse handleToolsList(MCPRequest &request);
    MCPResponse handleFunctionCalls(MCPRequest &request);
    MCPRequest parseRequest(const std::string &json);
    std::string serializeResponse(const MCPResponse &response);

    std::string generateSessionId();
    void setupMiddleware();
    
    // JSON-RPC 2.0 响应方法
    MCPResponse createJSONRPCError(int httpCode,int code, RequestId id, const std::string &message);
    std::string createJSONRPCResponse(const JsonVariant &id, const JsonVariant &result);
    
    // 保留旧方法以保持兼容性
    std::string createHTTPResponse(bool success, const std::string &message, const JsonVariant &data = JsonVariant());
};

}
#endif // MCP_SERVER_H
