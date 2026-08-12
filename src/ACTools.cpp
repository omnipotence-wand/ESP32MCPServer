#include "ACTools.h"

#include <ArduinoJson.h>
#include <functional>
#include <initializer_list>
#include <memory>

class SimpleToolHandler : public ToolHandler {
public:
    using HandlerFunc = std::function<JsonDocument(JsonVariantConst, bool&)>;
    explicit SimpleToolHandler(HandlerFunc func) : func_(func) {}
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

static Properties primitive(const String& type, const String& description) {
    Properties p;
    p.type = type;
    p.description = description;
    return p;
}

static Properties stringEnum(const String& description, std::initializer_list<const char*> values) {
    Properties p = primitive("string", description);
    for (const char* value : values) p.enumValues.push_back(value);
    return p;
}

static Properties objectSchema() {
    Properties p;
    p.type = "object";
    p.hasAdditionalProperties = true;
    p.additionalProperties = false;
    return p;
}

static Properties featuresSchema() {
    Properties p = objectSchema();
    const char* names[] = {"sleep", "eco", "turbo", "quiet", "displayLight", "beep",
                           "childLock", "antiDirectBlow", "auxiliaryHeat", "mildewProof", "selfClean"};
    for (const char* name : names) {
        p.properties[name] = primitive("boolean", String("Whether ") + name + " is enabled");
        p.required.push_back(name);
    }
    return p;
}

static void addStatusProperties(Properties& schema) {
    schema = objectSchema();
    schema.properties["running"] = primitive("boolean", "True when the AC is powered on");
    schema.properties["mode"] = primitive("integer", "0 Auto, 1 Cool, 2 Heat, 3 Dehumidify, 4 Fan");
    schema.properties["modeString"] = stringEnum("Current operating mode",
        {"auto", "cool", "heat", "dehumidify", "fan", "unknown"});
    schema.properties["temperature"] = primitive("integer", "Target temperature in Celsius, 16-30");
    schema.properties["fanSpeed"] = primitive("integer", "0 Auto, 1 Low, 2 Medium, 3 High, 4 Turbo, 5 Quiet");
    schema.properties["fanSpeedString"] = stringEnum("Current fan speed",
        {"auto", "low", "medium", "high", "turbo", "quiet", "unknown"});
    schema.properties["verticalSwing"] = primitive("boolean", "Vertical swing is active");
    schema.properties["horizontalSwing"] = primitive("boolean", "Horizontal swing is active");
    schema.properties["verticalDirection"] = primitive("integer", "Fixed vertical direction, 0-4");
    schema.properties["horizontalDirection"] = primitive("integer", "Fixed horizontal direction, 0-4");
    schema.properties["features"] = featuresSchema();

    Properties environment = objectSchema();
    environment.properties["roomTemperature"] = primitive("number", "Simulated room temperature in Celsius");
    environment.properties["roomHumidity"] = primitive("number", "Simulated relative humidity percentage");
    environment.required = {"roomTemperature", "roomHumidity"};
    schema.properties["environment"] = environment;

    Properties operation = objectSchema();
    operation.properties["compressorRunning"] = primitive("boolean", "Whether the simulated compressor is active");
    operation.properties["powerWatts"] = primitive("integer", "Estimated current electrical power in watts");
    operation.properties["runtimeSeconds"] = primitive("integer", "Accumulated powered-on runtime in seconds");
    operation.required = {"compressorRunning", "powerWatts", "runtimeSeconds"};
    schema.properties["operation"] = operation;

    Properties timers = objectSchema();
    timers.properties["turnOnActive"] = primitive("boolean", "Delayed turn-on timer is active");
    timers.properties["turnOnRemainingMinutes"] = primitive("integer", "Minutes remaining before turn-on");
    timers.properties["turnOffActive"] = primitive("boolean", "Delayed turn-off timer is active");
    timers.properties["turnOffRemainingMinutes"] = primitive("integer", "Minutes remaining before turn-off");
    timers.required = {"turnOnActive", "turnOnRemainingMinutes", "turnOffActive", "turnOffRemainingMinutes"};
    schema.properties["timers"] = timers;

    Properties maintenance = objectSchema();
    maintenance.properties["filterLifePercent"] = primitive("integer", "Remaining simulated filter life percentage");
    maintenance.properties["faultCode"] = primitive("integer", "0 None, 1 Temperature sensor, 2 Compressor, 3 Overheat");
    maintenance.properties["fault"] = stringEnum("Current simulated fault",
        {"none", "temperature_sensor", "compressor", "overheat"});
    maintenance.required = {"filterLifePercent", "faultCode", "fault"};
    schema.properties["maintenance"] = maintenance;

    schema.required = {"running", "mode", "modeString", "temperature", "fanSpeed", "fanSpeedString",
                       "verticalSwing", "horizontalSwing", "verticalDirection", "horizontalDirection",
                       "features", "environment", "operation", "timers", "maintenance"};
}

static JsonDocument statusResult(AirConditioner& ac, const String& error, bool& isError) {
    JsonDocument result;
    deserializeJson(result, ac.getStatusJSON());
    isError = !error.isEmpty();
    if (isError) result["error"] = error;
    return result;
}

// State-changing tools use a compact, exact acknowledgement. Returning the full
// state from every command required storing the same large output schema 16
// times and exhausted the ESP32 heap while HTTP MCP was registered. Clients can
// call getStatus when they need the complete snapshot.
static JsonDocument commandResult(AirConditioner& ac, const String& error, bool& isError) {
    JsonDocument result;
    isError = !error.isEmpty();
    if (isError) {
        result["error"] = error;
    } else {
        result["success"] = true;
        result["running"] = ac.getRunningStatus();
    }
    return result;
}

static Tool statusTool(const char* name, const char* description) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.inputSchema = objectSchema();
    addStatusProperties(tool.outputSchema);
    return tool;
}

static Tool commandTool(const char* name, const char* description) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.inputSchema = objectSchema();
    tool.outputSchema = objectSchema();
    tool.outputSchema.properties["success"] = primitive("boolean", "True when the command completed");
    tool.outputSchema.properties["running"] = primitive("boolean", "Power state after the command");
    tool.outputSchema.required = {"success", "running"};
    return tool;
}

static bool requireInt(JsonVariantConst params, const char* name, int& value) {
    if (!params[name].is<int>()) return false;
    value = params[name].as<int>();
    return true;
}

void registerACTools(MCPServerBase& server, AirConditioner& ac) {
    // Keep every Tool in its own scope so the compiler can reuse the same stack
    // slot. Retaining all 16 locals previously created a ~7 KiB frame and
    // tripped the Arduino loopTask stack canary during Wi-Fi MCP registration.
    {
        Tool turnOn = commandTool("turnOn", "Turn on the air conditioner");
        turnOn.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            ac.turnOn();
            return commandResult(ac, "", isError);
        });
        server.RegisterTool(std::move(turnOn));
    }

    {
        Tool turnOff = commandTool("turnOff", "Turn off the air conditioner");
        turnOff.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            ac.turnOff();
            return commandResult(ac, "", isError);
        });
        server.RegisterTool(std::move(turnOff));
    }

    {
        Tool setMode = commandTool("setMode", "Set operating mode: 0 Auto, 1 Cool, 2 Heat, 3 Dehumidify, 4 Fan");
        setMode.inputSchema.properties["mode"] = primitive("integer", "Mode number from 0 through 4");
        setMode.inputSchema.required.push_back("mode");
        setMode.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            int value;
            if (!requireInt(params, "mode", value)) return commandResult(ac, "missing required integer: mode", isError);
            return commandResult(ac, ac.setMode(value), isError);
        });
        server.RegisterTool(std::move(setMode));
    }

    {
        Tool setTemperature = commandTool("setTemperature", "Set target temperature in Celsius");
        setTemperature.inputSchema.properties["temperature"] = primitive("integer", "Target temperature from 16 through 30 Celsius");
        setTemperature.inputSchema.required.push_back("temperature");
        setTemperature.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            int value;
            if (!requireInt(params, "temperature", value)) return commandResult(ac, "missing required integer: temperature", isError);
            return commandResult(ac, ac.setTemperature(value), isError);
        });
        server.RegisterTool(std::move(setTemperature));
    }

    {
        Tool setFan = commandTool("setFanSpeed", "Set fan speed independently from the operating mode");
        setFan.inputSchema.properties["speed"] = stringEnum("Requested fan speed", {"auto", "low", "medium", "high", "turbo", "quiet"});
        setFan.inputSchema.required.push_back("speed");
        setFan.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            if (!params["speed"].is<const char*>()) return commandResult(ac, "missing required string: speed", isError);
            String speed = params["speed"].as<const char*>();
            int value = speed == "auto" ? 0 : speed == "low" ? 1 : speed == "medium" ? 2 :
                        speed == "high" ? 3 : speed == "turbo" ? 4 : speed == "quiet" ? 5 : -1;
            return commandResult(ac, ac.setFanSpeed(value), isError);
        });
        server.RegisterTool(std::move(setFan));
    }

    {
        Tool setSwing = commandTool("setSwing", "Enable or disable vertical and horizontal swing");
        setSwing.inputSchema.properties["vertical"] = primitive("boolean", "Enable vertical swing");
        setSwing.inputSchema.properties["horizontal"] = primitive("boolean", "Enable horizontal swing");
        setSwing.inputSchema.required = {"vertical", "horizontal"};
        setSwing.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            if (!params["vertical"].is<bool>() || !params["horizontal"].is<bool>())
                return commandResult(ac, "vertical and horizontal booleans are required", isError);
            return commandResult(ac, ac.setSwing(params["vertical"].as<bool>(), params["horizontal"].as<bool>()), isError);
        });
        server.RegisterTool(std::move(setSwing));
    }

    {
        Tool setDirection = commandTool("setAirDirection", "Set fixed vertical and horizontal louver positions; this stops swing");
        setDirection.inputSchema.properties["vertical"] = primitive("integer", "Vertical position from 0 through 4");
        setDirection.inputSchema.properties["horizontal"] = primitive("integer", "Horizontal position from 0 through 4");
        setDirection.inputSchema.required = {"vertical", "horizontal"};
        setDirection.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            int vertical, horizontal;
            if (!requireInt(params, "vertical", vertical) || !requireInt(params, "horizontal", horizontal))
                return commandResult(ac, "vertical and horizontal integers are required", isError);
            return commandResult(ac, ac.setAirDirection(vertical, horizontal), isError);
        });
        server.RegisterTool(std::move(setDirection));
    }

    {
        Tool setFeature = commandTool("setFeature", "Enable or disable an AC convenience feature");
        setFeature.inputSchema.properties["feature"] = stringEnum("Feature to change",
            {"sleep", "eco", "turbo", "quiet", "display_light", "beep", "child_lock",
             "anti_direct_blow", "auxiliary_heat", "mildew_proof", "self_clean"});
        setFeature.inputSchema.properties["enabled"] = primitive("boolean", "Desired feature state");
        setFeature.inputSchema.required = {"feature", "enabled"};
        setFeature.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            if (!params["feature"].is<const char*>() || !params["enabled"].is<bool>())
                return commandResult(ac, "feature string and enabled boolean are required", isError);
            return commandResult(ac, ac.setFeature(params["feature"].as<const char*>(), params["enabled"].as<bool>()), isError);
        });
        server.RegisterTool(std::move(setFeature));
    }

    {
        Tool setTimer = commandTool("setTimer", "Schedule delayed power on or power off");
        setTimer.inputSchema.properties["action"] = stringEnum("Power action to schedule", {"turn_on", "turn_off"});
        setTimer.inputSchema.properties["delayMinutes"] = primitive("integer", "Delay from 1 through 1440 minutes");
        setTimer.inputSchema.required = {"action", "delayMinutes"};
        setTimer.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            int minutes;
            if (!params["action"].is<const char*>() || !requireInt(params, "delayMinutes", minutes))
                return commandResult(ac, "action string and delayMinutes integer are required", isError);
            String action = params["action"].as<const char*>();
            if (action != "turn_on" && action != "turn_off") return commandResult(ac, "invalid timer action", isError);
            return commandResult(ac, ac.setTimer(action == "turn_on", minutes), isError);
        });
        server.RegisterTool(std::move(setTimer));
    }

    {
        Tool cancelTimer = commandTool("cancelTimer", "Cancel a delayed power action");
        cancelTimer.inputSchema.properties["action"] = stringEnum("Timer to cancel", {"turn_on", "turn_off"});
        cancelTimer.inputSchema.required.push_back("action");
        cancelTimer.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            if (!params["action"].is<const char*>()) return commandResult(ac, "missing required string: action", isError);
            String action = params["action"].as<const char*>();
            if (action != "turn_on" && action != "turn_off") return commandResult(ac, "invalid timer action", isError);
            return commandResult(ac, ac.cancelTimer(action == "turn_on"), isError);
        });
        server.RegisterTool(std::move(cancelTimer));
    }

    {
        Tool setEnvironment = commandTool("setEnvironment", "Set simulated room sensor values for demos and tests");
        setEnvironment.inputSchema.properties["roomTemperature"] = primitive("number", "Room temperature from -20 through 60 Celsius");
        setEnvironment.inputSchema.properties["roomHumidity"] = primitive("number", "Relative humidity from 0 through 100 percent");
        setEnvironment.inputSchema.required = {"roomTemperature", "roomHumidity"};
        setEnvironment.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            if (!params["roomTemperature"].is<float>() || !params["roomHumidity"].is<float>())
                return commandResult(ac, "roomTemperature and roomHumidity numbers are required", isError);
            return commandResult(ac, ac.setEnvironment(params["roomTemperature"].as<float>(), params["roomHumidity"].as<float>()), isError);
        });
        server.RegisterTool(std::move(setEnvironment));
    }

    {
        Tool injectFault = commandTool("injectFault", "Inject a simulated device fault");
        injectFault.inputSchema.properties["code"] = primitive("integer", "1 Temperature sensor, 2 Compressor, 3 Overheat");
        injectFault.inputSchema.required.push_back("code");
        injectFault.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst params, bool& isError) {
            int code;
            if (!requireInt(params, "code", code)) return commandResult(ac, "missing required integer: code", isError);
            return commandResult(ac, ac.injectFault(code), isError);
        });
        server.RegisterTool(std::move(injectFault));
    }

    {
        Tool clearFault = commandTool("clearFault", "Clear the current simulated fault");
        clearFault.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            ac.clearFault();
            return commandResult(ac, "", isError);
        });
        server.RegisterTool(std::move(clearFault));
    }

    {
        Tool resetFilter = commandTool("resetFilter", "Reset filter life after simulated cleaning or replacement");
        resetFilter.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            ac.resetFilter();
            return commandResult(ac, "", isError);
        });
        server.RegisterTool(std::move(resetFilter));
    }

    {
        Tool resetDevice = commandTool("reset", "Reset the simulator to default AC settings");
        resetDevice.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            ac.reset();
            return commandResult(ac, "", isError);
        });
        server.RegisterTool(std::move(resetDevice));
    }

    {
        Tool getStatus = statusTool("getStatus", "Get complete simulated AC state");
        getStatus.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
            return statusResult(ac, "", isError);
        });
        server.RegisterTool(std::move(getStatus));
    }

    registerDescriptionTool(server, ac);
}

void registerDescriptionTool(MCPServerBase& server, AirConditioner& ac) {
    Tool tool;
    tool.name = "get_description";
    tool.description = "Get AC device identity metadata";
    tool.inputSchema = objectSchema();
    tool.outputSchema = objectSchema();
    tool.outputSchema.properties["device_type"] = primitive("string", "Device category");
    tool.outputSchema.properties["brand"] = primitive("string", "Manufacturer brand");
    tool.outputSchema.properties["model"] = primitive("string", "Model identifier");
    tool.outputSchema.properties["description"] = primitive("string", "Device description");
    tool.outputSchema.properties["alias"] = primitive("string", "User-defined alias");
    tool.outputSchema.required = {"device_type", "brand", "model", "description", "alias"};
    tool.handler = std::make_shared<SimpleToolHandler>([&ac](JsonVariantConst, bool& isError) {
        JsonDocument result;
        DeserializationError error = deserializeJson(result, ac.description());
        isError = static_cast<bool>(error);
        if (isError) result["error"] = error.c_str();
        return result;
    });
    server.RegisterTool(std::move(tool));
}
