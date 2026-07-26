/**
 * @file wifi_manager.h
 * @brief WiFi 连接管理器 — AP/STA 双模式
 *
 * 功能:
 *   - STA 模式连接指定 WiFi 路由器
 *   - 连接失败时自动启动 AP 模式 (用于配置与调试)
 *   - 自动重连机制
 *   - WiFi 信号强度监测
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// ==========================================================================
// 默认WiFi配置 (可通过 platformio.ini build_flags 覆盖)
// ==========================================================================
#ifndef WIFI_SSID
#define WIFI_SSID "BioControl_Farm"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "farm2024secure"
#endif

#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "BioControl_Config"
#endif

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "12345678"
#endif

// ==========================================================================
// WiFi 状态枚举
// ==========================================================================
enum WifiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_AP_MODE
};

// ==========================================================================
// WifiManager 类
// ==========================================================================
class WifiManager {
public:
    WifiManager();
    ~WifiManager() = default;

    /**
     * @brief 初始化WiFi，尝试连接路由器
     * @return true 连接成功, false 连接失败
     */
    bool begin();

    /**
     * @brief 启动AP模式 (热点)
     * @param ssid AP的SSID
     * @param password AP密码 (至少8位)
     */
    void startAP(const char* ssid, const char* password);

    /**
     * @brief 主循环: 维护连接状态，断线自动重连
     */
    void loop();

    /**
     * @brief 检查是否已连接
     */
    bool isConnected() const;

    /**
     * @brief 获取当前WiFi状态
     */
    WifiState getState() const { return _state; }

    /**
     * @brief 获取信号强度 (RSSI, dBm)
     */
    int getRSSI() const;

    /**
     * @brief 获取本机IP地址字符串
     */
    String getLocalIP() const;

    /**
     * @brief 获取MAC地址字符串
     */
    String getMacAddress() const;

private:
    WifiState _state;
    String _ssid;
    String _password;
    unsigned long _lastReconnectAttempt;
    const unsigned long RECONNECT_INTERVAL = 10000;  // 10秒重连间隔
    const unsigned long CONNECTION_TIMEOUT = 15000;  // 15秒连接超时
    int _reconnectCount;

    /**
     * @brief 尝试连接WiFi网络
     */
    bool connectToAP();
};

#endif // WIFI_MANAGER_H
