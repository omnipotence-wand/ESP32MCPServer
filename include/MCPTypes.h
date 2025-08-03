#ifndef MCP_TYPES_H
#define MCP_TYPES_H

#include <ArduinoJson.h>
#include <string>

namespace mcp {

enum class MCPRequestMethod {
    INITIALIZE,
    RESOURCES_LIST,
    RESOURCE_READ,
    SUBSCRIBE,
    UNSUBSCRIBE
};

using RequestId = std::string;

struct MCPRequest {
    std::string method;
    RequestId id;
    JsonDocument paramsDoc;

    MCPRequest() : method(""), id("") {}
    
    // 获取 params 的 JsonVariant 引用
    JsonVariantConst params() const { return paramsDoc.as<JsonVariantConst>(); }
    
    // 检查是否为空
    bool hasParams() const { return !paramsDoc.isNull(); }
};

struct MCPResponse {
    RequestId id;
    JsonDocument resultDoc;
    JsonDocument errorDoc;
    int code; // http status code

    MCPResponse() : code(200), id("") {}
    MCPResponse(int code, RequestId id) : code(code), id(id) {}
    
    // 获取 result 和 error 的 JsonVariant 引用
    JsonVariantConst result() const { return resultDoc.as<JsonVariantConst>(); }
    JsonVariantConst error() const { return errorDoc.as<JsonVariantConst>(); }
    
    // 检查是否为空
    bool hasResult() const { return !resultDoc.isNull(); }
    bool hasError() const { return !errorDoc.isNull(); }
};

} // namespace mcp

#endif // MCP_TYPES_H
