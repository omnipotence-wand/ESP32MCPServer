# WiFi 配置文件说明

## wifi_config.json

这个文件用于配置ESP32的WiFi连接信息。

### 文件格式

```json
{
  "wifi": {
    "ssid": "你的WiFi名称",
    "password": "你的WiFi密码", 
    "enabled": true
  },
  "ap": {
    "ssid": "ESP32_Config",
    "password": "",
    "enabled": true
  }
}
```

### 配置说明

#### WiFi 站点模式 (wifi)
- `ssid`: WiFi网络名称
- `password`: WiFi密码
- `enabled`: 是否启用WiFi连接 (true/false)

#### 接入点模式 (ap)  
- `ssid`: ESP32热点名称
- `password`: 热点密码（空字符串表示无密码）
- `enabled`: 是否启用热点模式 (true/false)

### 使用方法

1. **方法1: 直接编辑配置文件**
   - 将 `data/wifi_config.json` 文件上传到ESP32的LittleFS文件系统
   - 修改其中的WiFi信息
   - 重启设备

2. **方法2: 通过Web界面配置**
   - 设备首次启动会进入AP模式
   - 连接到ESP32热点
   - 在浏览器中访问 192.168.4.1
   - 输入WiFi信息保存

### 注意事项

- 如果WiFi连接失败，设备会自动切换到AP模式
- 配置文件不存在时会自动创建默认配置
- 修改配置后需要重启设备才能生效