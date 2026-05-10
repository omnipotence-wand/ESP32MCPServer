#include <unity.h>
#include <ArduinoJson.h>
#include "MCPServer.h"

class TestMCPServer : public MCPServerBase {
public:
    using MCPServerBase::MCPServerBase;
    using MCPServerBase::handle;
    using MCPServerBase::parseRequest;
    using MCPServerBase::serializeResponse;
};

class EchoHandler : public ToolHandler {
public:
    JsonDocument call(JsonVariantConst params) override {
        JsonDocument result;
        result["echo"] = params["message"];
        result["seen"] = true;
        return result;
    }
};

static TestMCPServer* server = nullptr;

void setUp(void) {
    server = new TestMCPServer("ESP32-AC-MCP-Server", "1.0.0", "AC control server");
}

void tearDown(void) {
    delete server;
    server = nullptr;
}

void test_initialize_uses_bootstrap_protocol_response(void) {
    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_TRUE(res.hasResult());
    TEST_ASSERT_FALSE(res.hasError());
    TEST_ASSERT_EQUAL_STRING("2025-11-25", res.result()["protocolVersion"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("ESP32-AC-MCP-Server", res.result()["serverInfo"]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("1.0.0", res.result()["serverInfo"]["version"].as<const char*>());
    TEST_ASSERT_FALSE(res.result()["capabilities"]["tools"]["listChanged"].as<bool>());
}

void test_tools_list_contains_registered_tool_schema(void) {
    Tool tool;
    tool.name = "echo";
    tool.description = "Echo a message";
    tool.inputSchema.type = "object";

    Properties message;
    message.type = "string";
    message.description = "Message to echo";
    tool.inputSchema.properties["message"] = message;
    tool.inputSchema.required.push_back("message");
    tool.handler = std::make_shared<EchoHandler>();
    server->RegisterTool(tool);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"list-1","method":"tools/list"})");
    MCPResponse res = server->handle(req);

    JsonArrayConst tools = res.result()["tools"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL(1, tools.size());
    TEST_ASSERT_EQUAL_STRING("echo", tools[0]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("object", tools[0]["inputSchema"]["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("string",
        tools[0]["inputSchema"]["properties"]["message"]["type"].as<const char*>());
}

void test_tools_call_returns_text_and_structured_content(void) {
    Tool tool;
    tool.name = "echo";
    tool.description = "Echo a message";
    tool.inputSchema.type = "object";
    tool.handler = std::make_shared<EchoHandler>();
    server->RegisterTool(tool);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"message":"hello"}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_FALSE(res.hasError());
    TEST_ASSERT_EQUAL_STRING("hello", res.result()["structuredContent"]["echo"].as<const char*>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"]["seen"].as<bool>());
    TEST_ASSERT_FALSE(res.result()["isError"].as<bool>());

    JsonArrayConst content = res.result()["content"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL(1, content.size());
    TEST_ASSERT_EQUAL_STRING("text", content[0]["type"].as<const char*>());
}

void test_initialized_notification_has_no_response_body(void) {
    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(202, res.code);
    TEST_ASSERT_FALSE(res.hasBody());
    TEST_ASSERT_EQUAL_STRING("", server->serializeResponse(res).c_str());
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_initialize_uses_bootstrap_protocol_response);
    RUN_TEST(test_tools_list_contains_registered_tool_schema);
    RUN_TEST(test_tools_call_returns_text_and_structured_content);
    RUN_TEST(test_initialized_notification_has_no_response_body);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
}
#else
int main() {
    return runUnityTests();
}
#endif
