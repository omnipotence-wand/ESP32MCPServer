# ESP32 MCP Server HTTP API 使用说明

## 概述

ESP32 MCP Server 现在使用HTTP API替代WebSocket，运行在端口9000上。

## 服务器端点

### 基础URL
```
http://<ESP32_IP>:9000
```

## API端点

### 1. 服务器状态
```bash
GET /status
```

### 2. MCP API端点

#### 初始化
```bash
POST /api/mcp/initialize
```

#### 获取资源列表
```bash
GET /api/mcp/resources/list
```

#### 读取资源
```bash
POST /api/mcp/resources/read
Content-Type: application/x-www-form-urlencoded

uri=test://resource1
```

#### 订阅资源
```bash
POST /api/mcp/subscribe
Content-Type: application/x-www-form-urlencoded

uri=test://resource1
```

#### 取消订阅
```bash
POST /api/mcp/unsubscribe
Content-Type: application/x-www-form-urlencoded

uri=test://resource1
```

#### 通用MCP端点
```bash
POST /mcp
Content-Type: application/x-www-form-urlencoded

json={"method":"initialize","params":{}}
```

## 测试方法

### 使用Python测试脚本
```bash
python test/test_http_9000.py 192.168.1.100
```

### 使用curl测试脚本
```bash
./test/test_curl_9000.sh 192.168.1.100
```

### 手动curl测试
```bash
# 测试服务器状态
curl http://192.168.1.100:9000/status

# 测试MCP初始化
curl -X POST http://192.168.1.100:9000/api/mcp/initialize

# 测试资源列表
curl http://192.168.1.100:9000/api/mcp/resources/list

# 测试通用端点
curl -X POST http://192.168.1.100:9000/mcp \
  -d "json={\"method\":\"initialize\",\"params\":{}}"
```

## 故障排除

1. **无法连接**: 检查ESP32是否已连接到网络
2. **500错误**: 查看ESP32串口输出的错误信息
3. **400错误**: 检查请求参数是否正确
4. **端口问题**: 确认服务器运行在9000端口

## 响应格式

成功响应：
```json
{
  "success": true,
  "message": "操作成功",
  "data": { ... }
}
```

错误响应：
```json
{
  "success": false,
  "error": "错误描述"
}
```