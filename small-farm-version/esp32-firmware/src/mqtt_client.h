/**
 * @file mqtt_client.h
 * @brief MQTT 云平台数据上报客户端
 *
 * 功能:
 *   - 连接 MQTT Broker
 *   - 传感器数据周期性上报
 *   - 系统状态与事件发布
 *   - 告警推送
 *   - 远程指令订阅 (预留)
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>

#include "wifi_manager.h"

// ==========================================================================
// MQTT 默认配置
// ==========================================================================
#ifndef MQTT_BROKER
#define MQTT_BROKER     "broker.emqx.io"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT       1883
#endif

#define MQTT_CLIENT_ID_PREFIX   "biocontrol_farm_"
#define MQTT_TOPIC_PREFIX       "biocontrol/farm/"

// 默认发布主题
#define TOPIC_SENSOR    MQTT_TOPIC_PREFIX "sensor"
#define TOPIC_STATE     MQTT_TOPIC_PREFIX "state"
#define TOPIC_EVENT     MQTT_TOPIC_PREFIX "event"
#define TOPIC_ALERT     MQTT_TOPIC_PREFIX "alert"
#define TOPIC_STATUS    MQTT_TOPIC_PREFIX "status"

// 默认订阅主题
#define TOPIC_COMMAND   MQTT_TOPIC_PREFIX "command"

// ==========================================================================
// MqttClient 类
// ==========================================================================
class MqttClient {
public:
    MqttClient();
    ~MqttClient();

    /**
     * @brief 初始化MQTT客户端
     * @param wifiManager WiFi管理器引用 (获取连接状态)
     * @param broker MQTT Broker地址
     * @param port MQTT端口
     * @return true 初始化成功
     */
    bool begin(WifiManager& wifiManager,
               const char* broker = MQTT_BROKER,
               uint16_t port = MQTT_PORT);

    /**
     * @brief 主循环: 维护MQTT连接，处理订阅消息
     */
    void loop();

    /**
     * @brief 发布传感器数据 (JSON格式)
     * @param temperature 温度 (°C)
     * @param humidity 湿度 (%)
     * @param batteryVoltage 电池电压
     * @param batteryPercent 电池百分比
     * @param solarVoltage 太阳能板电压
     * @param distance 超声波距离 (cm)
     */
    void publishSensorData(float temperature, float humidity,
                            float batteryVoltage, int batteryPercent,
                            float solarVoltage, float distance);

    /**
     * @brief 发布系统状态
     * @param state 状态字符串
     */
    void publishState(const String& state);

    /**
     * @brief 发布事件
     * @param event 事件字符串
     */
    void publishEvent(const String& event);

    /**
     * @brief 发布告警
     * @param alert 告警内容
     */
    void publishAlert(const String& alert);

    /**
     * @brief 检查MQTT是否连接
     */
    bool isConnected() const;

private:
    WiFiClient* _wifiClient;
    PubSubClient* _mqttClient;
    WifiManager* _wifiManager;
    String _broker;
    uint16_t _port;
    String _clientId;
    unsigned long _lastReconnectAttempt;

    /**
     * @brief 重新连接MQTT Broker
     */
    bool reconnect();

    /**
     * @brief 生成唯一客户端ID
     */
    String generateClientId();

    /**
     * @brief MQTT消息回调 (静态)
     */
    static void mqttCallback(char* topic, uint8_t* payload, unsigned int length);

    /**
     * @brief 处理接收到的指令
     */
    void handleCommand(const String& topic, const String& payload);
};

#endif // MQTT_CLIENT_H
