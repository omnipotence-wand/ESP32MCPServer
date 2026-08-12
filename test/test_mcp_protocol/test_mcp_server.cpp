#include <unity.h>
#include <ArduinoJson.h>
#include "ACTools.h"
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
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{)"
        R"("protocolVersion":"2025-11-25","capabilities":{},)"
        R"("clientInfo":{"name":"test-client","version":"1.0.0"}}})");
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

void test_ac_description_tool_returns_device_metadata(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"desc-1","method":"tools/call","params":{"name":"get_description","arguments":{}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_FALSE(res.hasError());
    TEST_ASSERT_EQUAL_STRING("KFR-35GW/N8XHA1", res.result()["structuredContent"]["model"].as<const char*>());
}

void test_all_ac_tools_publish_input_and_output_schemas(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"schemas-1","method":"tools/list"})");
    MCPResponse res = server->handle(req);
    JsonArrayConst tools = res.result()["tools"].as<JsonArrayConst>();

    TEST_ASSERT_EQUAL(17, tools.size());
    for (JsonObjectConst tool : tools) {
        TEST_ASSERT_EQUAL_STRING("object", tool["inputSchema"]["type"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING("object", tool["outputSchema"]["type"].as<const char*>());
    }
}

// 工具执行失败必须体现为 MCP result.isError = true, 否则客户端会把带 error
// 字段的状态当成一次正常调用。失败结果不再附带 structuredContent(它由
// outputSchema 描述, 只用于成功返回), 失败原因通过 content 文本传达。
void test_set_mode_missing_argument_reports_tool_error(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"mode-1","method":"tools/call","params":{"name":"setMode","arguments":{}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_FALSE(res.hasError());
    TEST_ASSERT_TRUE(res.result()["isError"].as<bool>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"].isNull());
}

// 错误由 AirConditioner 层产生(空调未开机)时同样要置起 isError
void test_set_mode_rejected_by_device_reports_tool_error(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"mode-2","method":"tools/call","params":{"name":"setMode","arguments":{"mode":1}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_TRUE(res.result()["isError"].as<bool>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"].isNull());
}

void test_turn_on_returns_compact_acknowledgement(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"on-1","method":"tools/call","params":{"name":"turnOn","arguments":{}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_FALSE(res.result()["isError"].as<bool>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"]["success"].as<bool>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"]["running"].as<bool>());
    TEST_ASSERT_TRUE(res.result()["structuredContent"]["environment"].isNull());
}

void test_extended_controls_update_structured_state(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest onReq = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"on-2","method":"tools/call","params":{"name":"turnOn","arguments":{}}})");
    server->handle(onReq);
    MCPRequest fanReq = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"fan-1","method":"tools/call","params":{"name":"setFanSpeed","arguments":{"speed":"turbo"}}})");
    MCPResponse fanRes = server->handle(fanReq);
    TEST_ASSERT_FALSE(fanRes.result()["isError"].as<bool>());
    TEST_ASSERT_TRUE(fanRes.result()["structuredContent"]["success"].as<bool>());

    MCPRequest swingReq = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"swing-1","method":"tools/call","params":{"name":"setSwing","arguments":{"vertical":true,"horizontal":true}}})");
    MCPResponse swingRes = server->handle(swingReq);
    TEST_ASSERT_TRUE(swingRes.result()["structuredContent"]["success"].as<bool>());

    MCPRequest statusReq = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"status-2","method":"tools/call","params":{"name":"getStatus","arguments":{}}})");
    MCPResponse statusRes = server->handle(statusReq);
    TEST_ASSERT_EQUAL_STRING("turbo", statusRes.result()["structuredContent"]["fanSpeedString"].as<const char*>());
    TEST_ASSERT_TRUE(statusRes.result()["structuredContent"]["features"]["turbo"].as<bool>());
    TEST_ASSERT_TRUE(statusRes.result()["structuredContent"]["verticalSwing"].as<bool>());
    TEST_ASSERT_TRUE(statusRes.result()["structuredContent"]["horizontalSwing"].as<bool>());
}

// 成功路径保持原样: isError 为 false, structuredContent 携带设备状态
void test_get_status_success_has_structured_content_and_no_error(void) {
    AirConditioner ac;
    registerACTools(*server, ac);

    MCPRequest req = server->parseRequest(
        R"({"jsonrpc":"2.0","id":"status-1","method":"tools/call","params":{"name":"getStatus","arguments":{}}})");
    MCPResponse res = server->handle(req);

    TEST_ASSERT_EQUAL(200, res.code);
    TEST_ASSERT_FALSE(res.result()["isError"].as<bool>());
    TEST_ASSERT_FALSE(res.result()["structuredContent"].isNull());
    TEST_ASSERT_TRUE(res.result()["structuredContent"]["error"].isNull());
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
    RUN_TEST(test_ac_description_tool_returns_device_metadata);
    RUN_TEST(test_all_ac_tools_publish_input_and_output_schemas);
    RUN_TEST(test_set_mode_missing_argument_reports_tool_error);
    RUN_TEST(test_set_mode_rejected_by_device_reports_tool_error);
    RUN_TEST(test_turn_on_returns_compact_acknowledgement);
    RUN_TEST(test_extended_controls_update_structured_state);
    RUN_TEST(test_get_status_success_has_structured_content_and_no_error);
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
