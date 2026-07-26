/**
 * @file mqtt_client.cpp
 * @brief MQTT 云平台数据上报实现
 */

#include "mqtt_client.h"

// ==========================================================================
// 全局静态指针 (用于MQTT回调)
// ==========================================================================
static MqttClient* _globalMqttInstance = nullptr;

// ==========================================================================
// 构造与析构
// ==========================================================================
MqttClient::MqttClient()
    : _wifiClient(nullptr)
    , _mqttClient(nullptr)
    , _wifiManager(nullptr)
    , _port(MQTT_PORT)
    , _lastReconnectAttempt(0)
{
}

MqttClient::~MqttClient() {
    if (_mqttClient) {
        _mqttClient->disconnect();
        delete _mqttClient;
        _mqttClient = nullptr;
    }
    if (_wifiClient) {
        delete _wifiClient;
        _wifiClient = nullptr;
    }
}

// ==========================================================================
// 生成客户端ID
// ==========================================================================
String MqttClient::generateClientId() {
    // 基于MAC地址生成唯一ID
    uint64_t chipId = ESP.getEfuseMac();
    char id[40];
    snprintf(id, sizeof(id), "%s%04X%08X",
             MQTT_CLIENT_ID_PREFIX,
             (uint16_t)(chipId >> 32),
             (uint32_t)(chipId & 0xFFFFFFFF));
    return String(id);
}

// ==========================================================================
// 初始化
// ==========================================================================
bool MqttClient::begin(WifiManager& wifiManager,
                        const char* broker, uint16_t port) {
    _wifiManager = &wifiManager;
    _broker = String(broker);
    _port = port;
    _clientId = generateClientId();

    Serial.printf("[MQTT] 初始化客户端 ID=%s Broker=%s:%d\n",
                  _clientId.c_str(), _broker.c_str(), _port);

    // 创建 WiFi 客户端
    _wifiClient = new WiFiClient();

    // 创建 MQTT 客户端
    _mqttClient = new PubSubClient(*_wifiClient);
    _mqttClient->setServer(_broker.c_str(), _port);
    _mqttClient->setCallback(mqttCallback);
    _mqttClient->setBufferSize(512);

    // 保存全局实例指针供回调使用
    _globalMqttInstance = this;

    // 首次连接
    if (reconnect()) {
        Serial.println("[MQTT] 初始化完成");
        return true;
    }

    Serial.println("[MQTT] 初始化: 首次连接失败，将在主循环中重试");
    return false;
}

// ==========================================================================
// 主循环
// ==========================================================================
void MqttClient::loop() {
    if (!_mqttClient) return;

    // 检查WiFi是否连接
    if (!_wifiManager || !_wifiManager->isConnected()) {
        return;
    }

    // 检查MQTT连接状态
    if (!_mqttClient->connected()) {
        if (millis() - _lastReconnectAttempt > 5000) {
            _lastReconnectAttempt = millis();
            reconnect();
        }
        return;
    }

    // 处理MQTT消息
    _mqttClient->loop();
}

// ==========================================================================
// 重连
// ==========================================================================
bool MqttClient::reconnect() {
    if (!_mqttClient) return false;

    Serial.printf("[MQTT] 正在连接 Broker: %s:%d ...\n",
                  _broker.c_str(), _port);

    // 设置 Last Will Testament (遗嘱消息) - 断线时自动发布
    String willTopic = TOPIC_STATUS;
    String willMsg = "{\"status\":\"offline\",\"client\":\"" + _clientId + "\"}";

    bool connected = _mqttClient->connect(
        _clientId.c_str(),
        nullptr, nullptr,              // 用户名/密码 (可后续添加)
        willTopic.c_str(), 1, true,    // 遗嘱主题
        willMsg.c_str()                // 遗嘱消息
    );

    if (connected) {
        Serial.println("[MQTT] 连接成功!");

        // 发布上线消息
        String onlineMsg = "{\"status\":\"online\",\"client\":\"" +
                           _clientId + "\",\"ip\":\"" +
                           _wifiManager->getLocalIP() + "\"}";
        _mqttClient->publish(TOPIC_STATUS, onlineMsg.c_str());

        // 订阅指令主题
        _mqttClient->subscribe(TOPIC_COMMAND);
        Serial.printf("[MQTT] 已订阅: %s\n", TOPIC_COMMAND);

        return true;
    }

    int state = _mqttClient->state();
    Serial.printf("[MQTT] 连接失败, 状态码: %d\n", state);
    return false;
}

// ==========================================================================
// 发布传感器数据
// ==========================================================================
void MqttClient::publishSensorData(float temperature, float humidity,
                                    float batteryVoltage, int batteryPercent,
                                    float solarVoltage, float distance) {
    if (!_mqttClient || !_mqttClient->connected()) return;

    // 使用 ArduinoJson 构建 JSON
    StaticJsonDocument<256> doc;
    doc["t"] = round(temperature * 10) / 10.0f;    // 温度, 保留1位小数
    doc["h"] = round(humidity * 10) / 10.0f;        // 湿度
    doc["bat_v"] = round(batteryVoltage * 100) / 100.0f;  // 电池电压
    doc["bat_pct"] = batteryPercent;                 // 电池百分比
    doc["solar_v"] = round(solarVoltage * 100) / 100.0f;  // 太阳能电压
    doc["dist"] = round(distance * 10) / 10.0f;      // 距离
    doc["ts"] = millis();                            // 时间戳

    String payload;
    serializeJson(doc, payload);

    bool result = _mqttClient->publish(TOPIC_SENSOR, payload.c_str());
    if (!result) {
        Serial.println("[MQTT] 传感器数据发布失败");
    }
}

// ==========================================================================
// 发布系统状态
// ==========================================================================
void MqttClient::publishState(const String& state) {
    if (!_mqttClient || !_mqttClient->connected()) return;

    StaticJsonDocument<128> doc;
    doc["state"] = state;
    doc["ts"] = millis();

    String payload;
    serializeJson(doc, payload);

    _mqttClient->publish(TOPIC_STATE, payload.c_str());
    Serial.printf("[MQTT] 状态发布: %s\n", state.c_str());
}

// ==========================================================================
// 发布事件
// ==========================================================================
void MqttClient::publishEvent(const String& event) {
    if (!_mqttClient || !_mqttClient->connected()) return;

    StaticJsonDocument<128> doc;
    doc["event"] = event;
    doc["ts"] = millis();

    String payload;
    serializeJson(doc, payload);

    _mqttClient->publish(TOPIC_EVENT, payload.c_str());
}

// ==========================================================================
// 发布告警
// ==========================================================================
void MqttClient::publishAlert(const String& alert) {
    if (!_mqttClient || !_mqttClient->connected()) return;

    StaticJsonDocument<200> doc;
    doc["alert"] = alert;
    doc["severity"] = "warning";
    doc["ts"] = millis();

    String payload;
    serializeJson(doc, payload);

    _mqttClient->publish(TOPIC_ALERT, payload.c_str());
    Serial.printf("[MQTT] 告警发布: %s\n", alert.c_str());
}

// ==========================================================================
// MQTT消息回调 (静态)
// ==========================================================================
void MqttClient::mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    // 将 payload 转换为字符串
    char buf[256];
    unsigned int copyLen = length < 255 ? length : 255;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';
    String payloadStr = String(buf);

    Serial.printf("[MQTT] 收到消息 Topic=%s Payload=%s\n", topic, payloadStr);

    // 通过全局实例处理指令
    if (_globalMqttInstance) {
        _globalMqttInstance->handleCommand(String(topic), payloadStr);
    }
}

// ==========================================================================
// 处理指令
// ==========================================================================
void MqttClient::handleCommand(const String& topic, const String& payload) {
    // 只处理 command 主题
    if (topic != TOPIC_COMMAND) return;

    // 解析 JSON 指令
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.printf("[MQTT] JSON解析失败: %s\n", error.c_str());
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) {
        Serial.println("[MQTT] 指令缺少 'cmd' 字段");
        return;
    }

    String command = String(cmd);
    Serial.printf("[MQTT] 执行指令: %s\n", command.c_str());

    // 指令处理 — 通过串口转发给 main 主控逻辑
    if (command == "drive_start") {
        Serial.println("CMD:DRIVE:1");   // 让main.cpp处理
        publishEvent("remote_drive_start");
    }
    else if (command == "drive_stop") {
        Serial.println("CMD:DRIVE:0");
        publishEvent("remote_drive_stop");
    }
    else if (command == "center") {
        Serial.println("CMD:CENTER");
    }
    else if (command == "status_query") {
        Serial.println("CMD:STATUS");
    }
    else if (command == "reboot") {
        Serial.println("[MQTT] 收到重启指令, 5秒后重启...");
        publishEvent("rebooting");
        delay(5000);
        ESP.restart();
    }
    else {
        Serial.printf("[MQTT] 未知指令: %s\n", command.c_str());
    }
}

// ==========================================================================
// 连接状态查询
// ==========================================================================
bool MqttClient::isConnected() const {
    return _mqttClient && _mqttClient->connected();
}
