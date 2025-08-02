# JSON-RPC 2.0 格式更新

## 概述

ESP32 MCP Server 已更新为支持标准的 JSON-RPC 2.0 格式，同时保持向后兼容性。

## 主要变更

### 1. 请求格式

**旧格式:**
```json
{
  "method": "initialize",
  "params": {}
}
```

**新格式 (JSON-RPC 2.0):**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {}
}
```

### 2. 响应格式

**旧格式:**
```json
{
  "success": true,
  "message": "Initialized",
  "data": {
    "serverName": "esp32-mcp-server",
    "serverVersion": "1.0.0"
  }
}
```

**新格式 (JSON-RPC 2.0):**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "type": "initializeResponse",
    "model_id": "esp32-mcp",
    "model_version": "1.0.0",
    "mcp_version": "0.1"
  }
}
```

### 3. 错误响应格式

**新格式 (JSON-RPC 2.0):**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": -32601,
    "message": "Method not found"
  }
}
```

## 支持的方法

### 1. initialize
初始化MCP服务器连接

**请求:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {}
}
```

**响应:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "type": "initializeResponse",
    "model_id": "esp32-mcp",
    "model_version": "1.0.0",
    "mcp_version": "0.1"
  }
}
```

### 2. resources/list
获取可用资源列表

**请求:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "resources/list",
  "params": {}
}
```

**响应:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "resources": [
      {
        "uri": "esp32://sensor/temperature",
        "name": "Temperature Sensor",
        "description": "ESP32 built-in temperature sensor",
        "mimeType": "application/json"
      },
      {
        "uri": "esp32://system/info",
        "name": "System Information",
        "description": "ESP32 system information",
        "mimeType": "application/json"
      }
    ]
  }
}
```

### 3. resources/read
读取指定资源的数据

**请求:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "resources/read",
  "params": {
    "uri": "esp32://sensor/temperature"
  }
}
```

**响应:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "contents": [
      {
        "uri": "esp32://sensor/temperature",
        "mimeType": "application/json",
        "data": {
          "temperature": 25.6,
          "unit": "celsius",
          "timestamp": 123456789
        }
      }
    ]
  }
}
```

### 4. subscribe / unsubscribe
订阅/取消订阅资源更新

**请求:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "subscribe",
  "params": {
    "uri": "esp32://sensor/temperature"
  }
}
```

**响应:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "result": {
    "subscribed": true,
    "uri": "esp32://sensor/temperature"
  }
}
```

## 错误代码

| 代码 | 含义 | 描述 |
|------|------|------|
| -32700 | Parse error | JSON解析错误 |
| -32600 | Invalid Request | 无效的请求格式 |
| -32601 | Method not found | 方法不存在 |
| -32602 | Invalid params | 无效的参数 |

## 请求体解析

服务器现在支持两种方式接收JSON数据：

1. **直接从请求体解析** (推荐)
   - Content-Type: application/json
   - JSON数据直接放在HTTP请求体中

2. **表单参数方式** (向后兼容)
   - Content-Type: application/x-www-form-urlencoded
   - JSON数据作为 `json` 参数传递

## 测试

使用以下脚本测试新功能：

- `test/test_jsonrpc_format.sh` - JSON-RPC格式展示
- `test/test_request_body_curl.sh` - 完整功能测试
- `test/test_request_body.py` - Python测试脚本

## 向后兼容性

- 保留了旧的响应格式方法以确保兼容性
- 支持表单参数方式传递JSON数据
- 自动处理缺少jsonrpc字段的请求

## 实现细节

### 修改的文件

1. **src/MCPServer.cpp**
   - 添加JSON-RPC 2.0响应方法
   - 更新所有HTTP处理方法
   - 添加错误代码支持

2. **include/MCPServer.h**
   - 更新方法声明
   - 添加JSON-RPC响应方法声明

3. **src/NetworkManager.cpp**
   - 添加请求体解析功能
   - 更新端点处理器

4. **include/NetworkManager.h**
   - 添加请求体数据存储
   - 更新方法声明

### 新增功能

- 直接从HTTP请求体解析JSON
- 标准JSON-RPC 2.0格式支持
- 完整的错误处理机制
- 更丰富的资源信息返回