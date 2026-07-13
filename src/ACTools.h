#pragma once
#include "MCPServer.h"
#include "ac.h"

void registerACTools(MCPServerBase& server, AirConditioner& ac);

/* 单独注册 get_description: BLE 传输仅做配网, 但 app 的设备身份协议
 * 要求所有传输都能提供 get_description (逻辑设备 id 的来源) */
void registerDescriptionTool(MCPServerBase& server, AirConditioner& ac);
