/**
 * @file drone_comm.cpp
 * @brief ESP-NOW / UART 无人机通信实现
 */

#include "drone_comm.h"

// ==========================================================================
// 广播 MAC 地址 (ESP-NOW 发现用)
// ==========================================================================
static const uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ==========================================================================
// 构造与析构
// ==========================================================================
DroneComm::DroneComm()
    : _initialized(false)
    , _espNowInitialized(false)
    , _serial(nullptr)
    , _droneMacSet(false)
    , _lastBatteryVoltage(0.0)
    , _lastStatusTime(0)
    , _eventHead(0)
    , _eventTail(0)
{
    memset(_droneMac, 0, sizeof(_droneMac));
}

DroneComm::~DroneComm() {
    if (_espNowInitialized) {
        esp_now_deinit();
    }
}

// ==========================================================================
// 初始化
// ==========================================================================
bool DroneComm::begin(uint8_t uartNum, int rxPin, int txPin) {
    Serial.println("[DroneComm] 初始化无人机通信...");

    // 初始化 UART 备用链路
    _serial = new HardwareSerial(uartNum);
    _serial->begin(115200, SERIAL_8N1, rxPin, txPin);
    Serial.printf("[DroneComm] UART%d 初始化 (RX=%d, TX=%d, 115200bps)\n",
                  uartNum, rxPin, txPin);

    // 初始化 ESP-NOW
    WiFi.mode(WIFI_MODE_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[DroneComm] ESP-NOW 初始化失败! 使用UART备用链路");
        _espNowInitialized = false;
    } else {
        _espNowInitialized = true;
        esp_now_register_send_cb(onESPNOWSend);
        esp_now_register_recv_cb(onESPNOWRecv);

        // 添加广播对端用于发现
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);

        Serial.println("[DroneComm] ESP-NOW 初始化成功");
    }

    _initialized = true;
    return true;
}

// ==========================================================================
// 校验和
// ==========================================================================
uint8_t DroneComm::calcChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

// ==========================================================================
// ESP-NOW 发送
// ==========================================================================
bool DroneComm::sendESPNowPacket(const uint8_t* data, size_t len) {
    if (!_espNowInitialized || !_droneMacSet) return false;

    esp_err_t result = esp_now_send(_droneMac, data, len);
    if (result != ESP_OK) {
        Serial.printf("[DroneComm] ESP-NOW 发送失败: %d\n", result);
        return false;
    }
    return true;
}

// ==========================================================================
// UART 发送
// ==========================================================================
void DroneComm::sendUARTCommand(const String& cmd) {
    if (!_serial) return;
    _serial->println(cmd);
    Serial.printf("[DroneComm] UART→Drone: %s\n", cmd.c_str());
}

// ==========================================================================
// 发送指令
// ==========================================================================
void DroneComm::sendDispatchCommand(float panAngle, float tiltAngle, float distance) {
    // 构建ESP-NOW数据包
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_DISPATCH;
    pkt.targetPan = panAngle;
    pkt.targetTilt = tiltAngle;
    pkt.targetDistance = distance;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (!sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt))) {
        // ESP-NOW失败，使用UART备用
        String cmd = "DISPATCH:" + String(panAngle, 1) + "," +
                     String(tiltAngle, 1) + "," + String(distance, 1);
        sendUARTCommand(cmd);
    }
}

void DroneComm::sendReturnCommand() {
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_RETURN;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (!sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt))) {
        sendUARTCommand("RETURN");
    }
}

void DroneComm::sendUltrasonicOn() {
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_ULTRASONIC_ON;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (!sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt))) {
        sendUARTCommand("ULTRASONIC:ON");
    }
}

void DroneComm::sendUltrasonicOff() {
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_ULTRASONIC_OFF;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (!sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt))) {
        sendUARTCommand("ULTRASONIC:OFF");
    }
}

void DroneComm::sendEmergencyStop() {
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_EMERGENCY;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt));
    sendUARTCommand("EMERGENCY");
}

void DroneComm::sendStatusQuery() {
    DroneCommandPacket pkt;
    pkt.cmdType = CMD_STATUS_QUERY;
    pkt.timestamp = millis();
    pkt.checksum = calcChecksum((uint8_t*)&pkt, sizeof(pkt) - 1);

    if (!sendESPNowPacket((uint8_t*)&pkt, sizeof(pkt))) {
        sendUARTCommand("STATUS?");
    }
}

// ==========================================================================
// 事件队列操作
// ==========================================================================
void DroneComm::enqueueEvent(DroneEventType type, const String& msg) {
    int next = (_eventHead + 1) % EVENT_QUEUE_SIZE;
    if (next == _eventTail) {
        // 队列满，丢弃最旧的事件
        _eventTail = (_eventTail + 1) % EVENT_QUEUE_SIZE;
    }

    _eventQueue[_eventHead].type = type;
    _eventQueue[_eventHead].message = msg;
    _eventQueue[_eventHead].timestamp = millis();
    _eventHead = next;
}

DroneEvent DroneComm::checkEvent() {
    if (_eventTail == _eventHead) {
        // 无事件
        DroneEvent empty;
        empty.type = DRONE_EVENT_NONE;
        empty.message = "";
        empty.timestamp = 0;
        return empty;
    }

    DroneEvent event = _eventQueue[_eventTail];
    _eventTail = (_eventTail + 1) % EVENT_QUEUE_SIZE;
    return event;
}

// ==========================================================================
// 处理UART接收数据
// ==========================================================================
void DroneComm::processUARTData() {
    if (!_serial || !_serial->available()) return;

    String line = _serial->readStringUntil('\n');
    line.trim();

    if (line.length() == 0) return;

    Serial.printf("[DroneComm] UART←Drone: %s\n", line.c_str());

    // 解析协议: EVENT:ARRIVED,BAT:3.85
    int colonIdx = line.indexOf(':');
    if (colonIdx < 0) return;

    String key = line.substring(0, colonIdx);
    String value = line.substring(colonIdx + 1);

    if (key == "EVENT") {
        if (value == "ARRIVED") {
            enqueueEvent(DRONE_EVENT_ARRIVED, "无人机已抵达");
        } else if (value == "DRIVE_DONE") {
            enqueueEvent(DRONE_EVENT_DRIVE_DONE, "驱离完成");
        } else if (value == "RETURNED") {
            enqueueEvent(DRONE_EVENT_RETURNED, "已返航");
        } else if (value == "FAILED") {
            enqueueEvent(DRONE_EVENT_FAILED, "驱离失败");
        }
    } else if (key == "BAT") {
        _lastBatteryVoltage = value.toFloat();
        _lastStatusTime = millis();
        if (_lastBatteryVoltage < 3.3f && _lastBatteryVoltage > 0) {
            enqueueEvent(DRONE_EVENT_LOW_BATTERY,
                        "电量低: " + String(_lastBatteryVoltage) + "V");
        }
    } else if (key == "MAC") {
        // 无人机上报MAC地址，用于ESP-NOW配对
        // 格式: MAC:AA:BB:CC:DD:EE:FF
        int macVals[6];
        if (sscanf(value.c_str(), "%x:%x:%x:%x:%x:%x",
                   &macVals[0], &macVals[1], &macVals[2],
                   &macVals[3], &macVals[4], &macVals[5]) == 6) {
            for (int i = 0; i < 6; i++) {
                _droneMac[i] = (uint8_t)macVals[i];
            }
            _droneMacSet = true;
            Serial.printf("[DroneComm] 已发现无人机 MAC: %s\n", value.c_str());
        }
    }
}

// ==========================================================================
// ESP-NOW 回调
// ==========================================================================
void DroneComm::onESPNOWSend(const uint8_t* mac, esp_now_send_status_t status) {
    // 静态回调, 这里仅记录日志
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[DroneComm] ESP-NOW 发送失败");
    }
}

void DroneComm::onESPNOWRecv(const esp_now_recv_info_t* info,
                              const uint8_t* data, int len) {
    if (len == sizeof(DroneStatusPacket)) {
        const DroneStatusPacket* pkt = (const DroneStatusPacket*)data;

        // 验证校验和
        uint8_t expected = 0;
        for (int i = 0; i < len - 1; i++) {
            expected ^= data[i];
        }

        if (expected != pkt->checksum) {
            Serial.println("[DroneComm] 校验和错误, 丢弃数据包");
            return;
        }

        // 记录无人机MAC地址
        // (通过全局实例访问, 这里用直接串口输出简化)
        Serial.printf("[DroneComm] ESP-NOW ← Drone: event=%d bat=%.2fV alt=%.1f\n",
                      pkt->eventType, pkt->batteryVoltage, pkt->currentAltitude);
    }
}

// ==========================================================================
// 发现对端
// ==========================================================================
void DroneComm::discoverPeer() {
    if (_droneMacSet) return;

    // 通过UART广播发现请求
    static unsigned long lastDiscover = 0;
    if (millis() - lastDiscover > 5000) {
        lastDiscover = millis();
        sendUARTCommand("DISCOVER");
    }
}
