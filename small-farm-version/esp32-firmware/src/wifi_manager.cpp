/**
 * @file wifi_manager.cpp
 * @brief WiFi 连接管理器实现
 */

#include "wifi_manager.h"

// ==========================================================================
// 构造与析构
// ==========================================================================
WifiManager::WifiManager()
    : _state(WIFI_DISCONNECTED)
    , _ssid(WIFI_SSID)
    , _password(WIFI_PASSWORD)
    , _lastReconnectAttempt(0)
    , _reconnectCount(0)
{
}

// ==========================================================================
// 初始化
// ==========================================================================
bool WifiManager::begin() {
    Serial.println("[WiFi] 初始化 WiFi...");

    // 设置WiFi模式为STA
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // 降低功耗: 设置发射功率 (dBm, 范围2-20)
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // 尝试连接
    bool connected = connectToAP();

    if (connected) {
        _state = WIFI_CONNECTED;
        _reconnectCount = 0;

        Serial.println("[WiFi] 连接成功!");
        Serial.print("[WiFi] IP地址: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] 信号强度: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        _state = WIFI_DISCONNECTED;
        Serial.println("[WiFi] 连接失败，将启动AP模式");
    }

    return connected;
}

// ==========================================================================
// 连接路由器
// ==========================================================================
bool WifiManager::connectToAP() {
    Serial.printf("[WiFi] 正在连接 %s ...\n", _ssid.c_str());

    _state = WIFI_CONNECTING;
    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startMs > CONNECTION_TIMEOUT) {
            Serial.println("[WiFi] 连接超时");
            _state = WIFI_DISCONNECTED;
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    _state = WIFI_CONNECTED;
    return true;
}

// ==========================================================================
// 启动AP模式
// ==========================================================================
void WifiManager::startAP(const char* ssid, const char* password) {
    Serial.printf("[WiFi] 启动AP模式: %s\n", ssid);

    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(ssid, password);

    if (success) {
        _state = WIFI_AP_MODE;
        Serial.print("[WiFi] AP启动成功, IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.printf("[WiFi] 已连接设备数: %d\n", WiFi.softAPgetStationNum());
    } else {
        Serial.println("[WiFi] AP启动失败!");
        _state = WIFI_DISCONNECTED;
    }
}

// ==========================================================================
// 主循环
// ==========================================================================
void WifiManager::loop() {
    switch (_state) {
        case WIFI_CONNECTED:
            // 检查连接是否还活着
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] 连接断开!");
                _state = WIFI_DISCONNECTED;
                _lastReconnectAttempt = millis();
            }
            break;

        case WIFI_DISCONNECTED:
            // 断线自动重连
            if (millis() - _lastReconnectAttempt > RECONNECT_INTERVAL) {
                _lastReconnectAttempt = millis();
                _reconnectCount++;

                Serial.printf("[WiFi] 尝试重连 #%d ...\n", _reconnectCount);

                if (connectToAP()) {
                    _reconnectCount = 0;
                    Serial.println("[WiFi] 重连成功!");
                } else if (_reconnectCount >= 5) {
                    // 重连5次失败后切换到AP模式
                    Serial.println("[WiFi] 多次重连失败，切换到AP模式");
                    startAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
                }
            }
            break;

        case WIFI_AP_MODE:
            // AP模式下定期输出连接设备数
            static unsigned long lastAPReport = 0;
            if (millis() - lastAPReport > 30000) {
                lastAPReport = millis();
                int numStations = WiFi.softAPgetStationNum();
                if (numStations > 0) {
                    Serial.printf("[WiFi] AP模式: %d 个设备已连接\n", numStations);
                }
            }
            break;

        case WIFI_CONNECTING:
            // 等待连接完成 (在 connectToAP 中同步等待, 这里不需处理)
            break;
    }
}

// ==========================================================================
// 状态查询
// ==========================================================================
bool WifiManager::isConnected() const {
    return (_state == WIFI_CONNECTED);
}

int WifiManager::getRSSI() const {
    if (_state == WIFI_CONNECTED) {
        return WiFi.RSSI();
    }
    return -100;
}

String WifiManager::getLocalIP() const {
    if (_state == WIFI_CONNECTED) {
        return WiFi.localIP().toString();
    } else if (_state == WIFI_AP_MODE) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String WifiManager::getMacAddress() const {
    return WiFi.macAddress();
}
