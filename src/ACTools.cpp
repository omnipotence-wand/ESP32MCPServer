#include "ACTools.h"
#include <ArduinoJson.h>
#include <functional>
#include <initializer_list>
#include <memory>

// Helper class to simplify tool creation.
// handler 通过 isError 回报本次执行是否失败: 置 true 时框架会把 MCP result.isError
// 设为 true, 客户端据此把结果当成错误而非正常返回。不会失败的工具留空该参数即可。
class SimpleToolHandler : public ToolHandler {
public:
    using HandlerFunc = std::function<JsonDocument(JsonVariantConst, bool&)>;
    SimpleToolHandler(HandlerFunc func) : func_(func) {}
    JsonDocument call(JsonVariantConst params) override {
        bool ignored = false;
        return call(params, ignored);
    }
    JsonDocument call(JsonVariantConst params, bool& isError) override {
        return func_(params, isError);
    }
private:
    HandlerFunc func_;
};

// Schema helpers — small, file-local, used to keep outputSchema declarations readable.
static Properties primitive(const String& type, const String& description) {
    Properties p;
    p.type = type;
    p.description = description;
    return p;
}

static Properties stringEnum(const String& description, std::initializer_list<const char*> values) {
    Properties p;
    p.type = "string";
    p.description = description;
    for (auto v : values) {
        p.enumValues.push_back(v);
    }
    return p;
}

// getStatus / setMode / setTemperature 共用的设备状态 schema:
// 设置类工具直接返回调用后的完整状态, 而不是 code/msg 包装。
// 开关机工具不用这套: 它们只回报本次操作的结果状态, 设备详情由 getStatus 提供。
// 这里不声明 error: outputSchema 只描述 structuredContent, 而失败结果
// (isError=true) 不再附带 structuredContent, 失败原因通过 content 文本返回。
static void addStatusProperties(Properties& schema) {
    schema.type = "object";
    schema.properties["running"] = primitive("boolean", "True when AC is powered on");
    schema.properties["mode"] = primitive("integer", "0: Auto, 1: Cool, 2: Heat, 3: Dehumidify");
    schema.properties["modeString"] = stringEnum(
        "Current mode label", {"auto", "cool", "heat", "humdify", "unknown"});
    schema.properties["temperature"] = primitive("integer", "Target temperature in Celsius");
    schema.required.push_back("running");
    schema.required.push_back("mode");
    schema.required.push_back("modeString");
    schema.required.push_back("temperature");
}

// 当前设备状态 + 可选错误信息 (error 为空串时省略, 表示成功)。
// error 非空即代表本次工具调用失败, 同步置起 isError 让框架回报 MCP 层面的执行失败。
static JsonDocument statusResult(AirConditioner& ac, const String& error, bool& isError) {
    JsonDocument result;
    deserializeJson(result, ac.getStatusJSON());
    isError = error.length() > 0;
    if (isError) {
        result["error"] = error;
    }
    return result;
}

// 开关机结果: 成功时只回报操作后的电源状态, 失败时置起 isError 并把原因放进
// content 文本(失败结果不带 structuredContent, 所以 error 不写进 outputSchema)。
static JsonDocument powerResult(bool ok, const char* state, const char* error, bool& isError) {
    JsonDocument result;
    isError = !ok;
    if (isError) {
        result["error"] = error;
    } else {
        result["status"] = state;
    }
    return result;
}

void registerACTools(MCPServerBase& server, AirConditioner& ac) {
    // 1. turnOn Tool
    Tool turnOnTool;
    turnOnTool.name = "turnOn";
    turnOnTool.description = "Turn on the air conditioner";
    turnOnTool.inputSchema.type = "object";

    turnOnTool.outputSchema.type = "object";
    turnOnTool.outputSchema.properties["status"] = stringEnum("Power state after the call", {"on"});
    turnOnTool.outputSchema.required.push_back("status");

    turnOnTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
        (void)params;
        return powerResult(ac.turnOn(), "on", "failed to turn on the air conditioner", isError);
    });
    server.RegisterTool(turnOnTool);

    // 2. turnOff Tool
    Tool turnOffTool;
    turnOffTool.name = "turnOff";
    turnOffTool.description = "Turn off the air conditioner";
    turnOffTool.inputSchema.type = "object";

    turnOffTool.outputSchema.type = "object";
    turnOffTool.outputSchema.properties["status"] = stringEnum("Power state after the call", {"off"});
    turnOffTool.outputSchema.required.push_back("status");

    turnOffTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
        (void)params;
        return powerResult(ac.turnOff(), "off", "failed to turn off the air conditioner", isError);
    });
    server.RegisterTool(turnOffTool);

    // 3. setMode Tool
    Tool setModeTool;
    setModeTool.name = "setMode";
    setModeTool.description = "Set AC mode (0: Auto, 1: Cool, 2: Heat, 3: Dehumidify)";
    setModeTool.inputSchema.type = "object";

    Properties modeProp;
    modeProp.type = "integer";
    modeProp.description = "Mode: 0=Auto, 1=Cool, 2=Heat, 3=Dehumidify";
    setModeTool.inputSchema.properties["mode"] = modeProp;
    setModeTool.inputSchema.required.push_back("mode");

    addStatusProperties(setModeTool.outputSchema);

    setModeTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
        if (!params["mode"].is<int>()) {
            return statusResult(ac, "missing required parameter: mode", isError);
        }
        return statusResult(ac, ac.setMode(params["mode"].as<int>()), isError);
    });
    server.RegisterTool(setModeTool);

    // 4. setTemperature Tool
    Tool setTempTool;
    setTempTool.name = "setTemperature";
    setTempTool.description = "Set AC temperature (16-30)";
    setTempTool.inputSchema.type = "object";

    Properties tempProp;
    tempProp.type = "integer";
    tempProp.description = "Target temperature in Celsius (16-30)";
    setTempTool.inputSchema.properties["temperature"] = tempProp;
    setTempTool.inputSchema.required.push_back("temperature");

    addStatusProperties(setTempTool.outputSchema);

    setTempTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
        if (!params["temperature"].is<int>()) {
            return statusResult(ac, "missing required parameter: temperature", isError);
        }
        return statusResult(ac, ac.setTemperature(params["temperature"].as<int>()), isError);
    });
    server.RegisterTool(setTempTool);

    // 5. getStatus Tool
    Tool getStatusTool;
    getStatusTool.name = "getStatus";
    getStatusTool.description = "Get AC status";
    getStatusTool.inputSchema.type = "object";

    addStatusProperties(getStatusTool.outputSchema);

    getStatusTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
        (void)params;
        return statusResult(ac, "", isError);
    });
    server.RegisterTool(getStatusTool);

    // 6. get_description Tool
    registerDescriptionTool(server, ac);
}

void registerDescriptionTool(MCPServerBase& server, AirConditioner& ac) {
    Tool getDescriptionTool;
    getDescriptionTool.name = "get_description";
    getDescriptionTool.description = "Get AC device description";
    getDescriptionTool.inputSchema.type = "object";

    getDescriptionTool.outputSchema.type = "object";
    getDescriptionTool.outputSchema.properties["device_type"] = primitive("string", "Device category, e.g. 空调");
    getDescriptionTool.outputSchema.properties["brand"] = primitive("string", "Manufacturer brand");
    getDescriptionTool.outputSchema.properties["model"] = primitive("string", "Model identifier");
    getDescriptionTool.outputSchema.properties["description"] = primitive("string", "Free-form device description");
    getDescriptionTool.outputSchema.properties["alias"] = primitive("string", "User-defined alias");
    getDescriptionTool.outputSchema.required.push_back("device_type");
    getDescriptionTool.outputSchema.required.push_back("brand");
    getDescriptionTool.outputSchema.required.push_back("model");
    getDescriptionTool.outputSchema.required.push_back("description");
    getDescriptionTool.outputSchema.required.push_back("alias");

    getDescriptionTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool&) {
        (void)params;
        String json = ac.description();
        JsonDocument result;
        // description() returns a hardcoded literal; deserialization is not expected
        // to fail, so no schema-violating error fallback is needed here.
        deserializeJson(result, json);
        return result;
    });
    server.RegisterTool(getDescriptionTool);
}
