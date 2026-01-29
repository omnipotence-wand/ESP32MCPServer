#include "ACTools.h"
#include <ArduinoJson.h>
#include <functional>
#include <memory>

// Helper class to simplify tool creation
class SimpleToolHandler : public ToolHandler {
public:
    using HandlerFunc = std::function<JsonDocument(JsonDocument)>;
    SimpleToolHandler(HandlerFunc func) : func_(func) {}
    JsonDocument call(JsonDocument params) override {
        return func_(params);
    }
private:
    HandlerFunc func_;
};

void registerACTools(MCPServer& server, AirConditioner& ac) {
    // 1. turnOn Tool
    Tool turnOnTool;
    turnOnTool.name = "turnOn";
    turnOnTool.description = "Turn on the air conditioner";
    turnOnTool.inputSchema.type = "object";
    
    turnOnTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonDocument params) {
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

    turnOffTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonDocument params) {
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

    setModeTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonDocument params) {
        int mode = params["mode"];
        String res = ac.setMode(mode);
        JsonDocument result;
        DeserializationError error = deserializeJson(result, res);
        if (error) {
            result["error"] = "Internal error parsing AC response";
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

    setTempTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonDocument params) {
        int temp = params["temperature"];
        String res = ac.setTemperature(temp);
        JsonDocument result;
        deserializeJson(result, res);
        return result;
    });
    server.RegisterTool(setTempTool);

    // 5. getStatus Tool
    Tool getStatusTool;
    getStatusTool.name = "getStatus";
    getStatusTool.description = "Get AC status";
    getStatusTool.inputSchema.type = "object";

    getStatusTool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonDocument params) {
        String json = ac.getStatusJSON();
        JsonDocument result;
        deserializeJson(result, json);
        return result;
    });
    server.RegisterTool(getStatusTool);
}
