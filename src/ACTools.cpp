#include "ACTools.h"
#include <ArduinoJson.h>
#include <functional>
#include <initializer_list>
#include <memory>

// Helper class to simplify tool creation
class SimpleToolHandler : public ToolHandler {
public:
    using HandlerFunc = std::function<JsonDocument(JsonVariantConst)>;
    SimpleToolHandler(HandlerFunc func) : func_(func) {}
    JsonDocument call(JsonVariantConst params) override {
        return func_(params);
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

void registerACTools(MCPServerBase& server, AirConditioner& ac) {
    // 1. turnOn Tool
    Tool turnOnTool;
    turnOnTool.name = "turnOn";
    turnOnTool.description = "Turn on the air conditioner";
    turnOnTool.inputSchema.type = "object";

    turnOnTool.outputSchema.type = "object";
    turnOnTool.outputSchema.properties["status"] = stringEnum("Power state after the call", {"on"});
    turnOnTool.outputSchema.required.push_back("status");

    turnOnTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
        (void)params;
        ac.turnOn();
        JsonDocument result;
        result["status"] = "on";
        return result;
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

    turnOffTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
        (void)params;
        ac.turnOff();
        JsonDocument result;
        result["status"] = "off";
        return result;
    });
    server.RegisterTool(turnOffTool);

    // 3. setMode Tool
    Tool setModeTool;
    setModeTool.name = "setMode";
    setModeTool.description = "Set AC mode (0: Auto, 1: Cool, 2: Heat, 3: Dehumidify)";
    setModeTool.inputSchema.type = "object";

    Properties modeProp;
    modeProp.type = "integer";
    modeProp.description = "Mode value";
    setModeTool.inputSchema.properties["mode"] = modeProp;
    setModeTool.inputSchema.required.push_back("mode");

    setModeTool.outputSchema.type = "object";
    setModeTool.outputSchema.properties["code"] = primitive("integer", "0 = success, 1 = failure");
    setModeTool.outputSchema.properties["msg"] = primitive("string", "Human-readable result message");
    setModeTool.outputSchema.required.push_back("code");
    setModeTool.outputSchema.required.push_back("msg");

    setModeTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
        int mode = params["mode"].as<int>();
        String res = ac.setMode(mode);
        JsonDocument result;
        DeserializationError error = deserializeJson(result, res);
        if (error) {
            result.clear();
            result["code"] = 1;
            result["msg"] = "Internal error parsing AC response";
        }
        return result;
    });
    server.RegisterTool(setModeTool);

    // 4. setTemperature Tool
    Tool setTempTool;
    setTempTool.name = "setTemperature";
    setTempTool.description = "Set AC temperature (16-30)";
    setTempTool.inputSchema.type = "object";

    Properties tempProp;
    tempProp.type = "integer";
    tempProp.description = "Temperature value";
    setTempTool.inputSchema.properties["temperature"] = tempProp;
    setTempTool.inputSchema.required.push_back("temperature");

    setTempTool.outputSchema.type = "object";
    setTempTool.outputSchema.properties["code"] = primitive("integer", "0 = success, 1 = failure");
    setTempTool.outputSchema.properties["msg"] = primitive("string", "Human-readable result message");
    setTempTool.outputSchema.required.push_back("code");
    setTempTool.outputSchema.required.push_back("msg");

    setTempTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
        int temp = params["temperature"].as<int>();
        String res = ac.setTemperature(temp);
        JsonDocument result;
        DeserializationError error = deserializeJson(result, res);
        if (error) {
            result.clear();
            result["code"] = 1;
            result["msg"] = "Internal error parsing AC response";
        }
        return result;
    });
    server.RegisterTool(setTempTool);

    // 5. getStatus Tool
    Tool getStatusTool;
    getStatusTool.name = "getStatus";
    getStatusTool.description = "Get AC status";
    getStatusTool.inputSchema.type = "object";

    getStatusTool.outputSchema.type = "object";
    getStatusTool.outputSchema.properties["running"] = primitive("boolean", "True when AC is powered on");
    getStatusTool.outputSchema.properties["mode"] = primitive("integer", "0: Auto, 1: Cool, 2: Heat, 3: Dehumidify");
    getStatusTool.outputSchema.properties["modeString"] = stringEnum(
        "Current mode label", {"auto", "cool", "heat", "humdify", "unknown"});
    getStatusTool.outputSchema.properties["temperature"] = primitive("integer", "Target temperature in Celsius");
    getStatusTool.outputSchema.required.push_back("running");
    getStatusTool.outputSchema.required.push_back("mode");
    getStatusTool.outputSchema.required.push_back("modeString");
    getStatusTool.outputSchema.required.push_back("temperature");

    getStatusTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
        (void)params;
        String json = ac.getStatusJSON();
        JsonDocument result;
        deserializeJson(result, json);
        return result;
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

    getDescriptionTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params) {
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
