#ifndef ESP32_MDNS_SERVER_H
#define ESP32_MDNS_SERVER_H

#include <Arduino.h>
#include <vector>
#include <utility>

/**
 * mDNS 服务器类
 * 用于在局域网中发布 ESP32 MCP 服务器的服务信息
 */
class MDNSServer {
private:
    String hostname;
    uint16_t mcpPort;
    bool initialized;
    
public:
    MDNSServer();
    
    /**
     * 初始化 mDNS 服务器
     * @param deviceName 设备名称，将作为 mDNS 主机名
     * @param port MCP 服务器端口
     * @return 初始化是否成功
     */
    bool begin(const String& deviceName = "esp32mcp", uint16_t port = 9000);
    
    /**
     * 注册 MCP 服务
     */
    void registerMCPService();
    
    /**
     * 注册 Web 服务
     */
    void registerWebService();
    
    /**
     * 添加自定义服务
     * @param serviceName 服务名称
     * @param protocol 协议类型 ("tcp" 或 "udp")
     * @param port 端口号
     * @param txtRecords TXT 记录的键值对
     */
    bool addCustomService(const String& serviceName, const String& protocol, 
                         uint16_t port, const std::vector<std::pair<String, String>>& txtRecords = {});
    
    /**
     * 更新服务信息
     */
    void update();
    
    /**
     * 停止 mDNS 服务
     */
    void end();
    
    /**
     * 获取主机名
     */
    String getHostname() const;
    
    /**
     * 获取完整的 mDNS 地址
     */
    String getFullAddress() const;
    
    /**
     * 打印服务信息
     */
    void printServiceInfo();
    
    /**
     * 检查服务是否正在运行
     */
    bool isRunning() const;
};

// 全局函数接口
/**
 * 初始化 mDNS 服务器的全局函数
 * @param hostname 主机名
 * @param port 端口号
 * @return 初始化是否成功
 */
bool initMDNS(const String& hostname = "esp32mcp", uint16_t port = 9000);

/**
 * 更新 mDNS 服务的全局函数
 */
void updateMDNS();

/**
 * 停止 mDNS 服务的全局函数
 */
void stopMDNS();

/**
 * 获取 mDNS 服务器实例的全局函数
 */
MDNSServer& getMDNSServer();

#endif // ESP32_MDNS_SERVER_H
