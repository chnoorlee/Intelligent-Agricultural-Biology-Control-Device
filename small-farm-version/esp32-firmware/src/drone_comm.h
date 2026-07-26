/**
 * @file drone_comm.h
 * @brief 无人机通信模块 — ESP-NOW 协议 + UART 备用
 *
 * 功能:
 *   - ESP-NOW 点对点无线通信 (ESP32 ↔ Mini无人机)
 *   - UART 有线备用通信
 *   - 指令: 派发、返航、超声波开关、状态查询
 */

#ifndef DRONE_COMM_H
#define DRONE_COMM_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ==========================================================================
// 无人机通信事件
// ==========================================================================
enum DroneEventType {
    DRONE_EVENT_NONE = 0,
    DRONE_EVENT_ARRIVED,       // 已抵达目标位置
    DRONE_EVENT_DRIVE_DONE,    // 驱离完成
    DRONE_EVENT_RETURNED,      // 已返航
    DRONE_EVENT_FAILED,        // 驱离失败
    DRONE_EVENT_LOW_BATTERY    // 电量低
};

struct DroneEvent {
    DroneEventType type;
    String message;
    unsigned long timestamp;
};

// ==========================================================================
// ESP-NOW 数据包定义
// ==========================================================================
// 指令包 (地面站 → 无人机)
struct __attribute__((packed)) DroneCommandPacket {
    uint8_t cmdType;        // 指令类型
    float targetPan;        // 目标水平角度
    float targetTilt;       // 目标俯仰角度
    float targetDistance;   // 目标距离 (cm)
    uint32_t timestamp;     // 时间戳
    uint8_t checksum;       // 校验和
};

// 指令类型
enum DroneCommand : uint8_t {
    CMD_DISPATCH    = 0x01,   // 派发到目标位置
    CMD_RETURN      = 0x02,   // 返航
    CMD_ULTRASONIC_ON = 0x03, // 开启超声波
    CMD_ULTRASONIC_OFF = 0x04,// 关闭超声波
    CMD_LAND        = 0x05,   // 立即降落
    CMD_STATUS_QUERY = 0x06,  // 查询状态
    CMD_EMERGENCY   = 0xFF    // 紧急停机
};

// 状态回传包 (无人机 → 地面站)
struct __attribute__((packed)) DroneStatusPacket {
    uint8_t eventType;      // 事件类型 (对应 DroneEventType)
    float batteryVoltage;   // 电池电压
    float currentAltitude;  // 当前高度 (cm)
    float currentPan;       // 当前位置 Pan
    float currentTilt;      // 当前位置 Tilt
    uint32_t timestamp;     // 时间戳
    uint8_t checksum;       // 校验和
};

// ==========================================================================
// DroneComm 类
// ==========================================================================
class DroneComm {
public:
    DroneComm();
    ~DroneComm();

    /**
     * @brief 初始化通信模块 (ESP-NOW + UART)
     * @param uartNum UART端口号 (默认 UART_NUM_2)
     * @param rxPin RX引脚 (默认 GPIO16)
     * @param txPin TX引脚 (默认 GPIO17)
     * @return true 初始化成功
     */
    bool begin(uint8_t uartNum = 2, int rxPin = 16, int txPin = 17);

    /**
     * @brief 派发无人机到目标位置
     * @param panAngle 目标水平角度
     * @param tiltAngle 目标俯仰角度
     * @param distance 目标距离 (cm)
     */
    void sendDispatchCommand(float panAngle, float tiltAngle, float distance);

    /**
     * @brief 发送返航指令
     */
    void sendReturnCommand();

    /**
     * @brief 开启机载超声波驱离
     */
    void sendUltrasonicOn();

    /**
     * @brief 关闭机载超声波
     */
    void sendUltrasonicOff();

    /**
     * @brief 紧急停机
     */
    void sendEmergencyStop();

    /**
     * @brief 查询无人机状态
     */
    void sendStatusQuery();

    /**
     * @brief 检查是否有新事件
     * @return 最新事件 (DRONE_EVENT_NONE 表示无事件)
     */
    DroneEvent checkEvent();

    /**
     * @brief 获取最后一次收到的电压
     */
    float getLastBatteryVoltage() const { return _lastBatteryVoltage; }

private:
    bool _initialized;
    bool _espNowInitialized;
    HardwareSerial* _serial;
    uint8_t _droneMac[6];
    bool _droneMacSet;

    float _lastBatteryVoltage;
    unsigned long _lastStatusTime;

    static const int EVENT_QUEUE_SIZE = 8;
    DroneEvent _eventQueue[EVENT_QUEUE_SIZE];
    int _eventHead;
    int _eventTail;

    /**
     * @brief 计算校验和
     */
    uint8_t calcChecksum(const uint8_t* data, size_t len);

    /**
     * @brief 通过ESP-NOW发送数据包
     */
    bool sendESPNowPacket(const uint8_t* data, size_t len);

    /**
     * @brief 通过UART发送命令字符串
     */
    void sendUARTCommand(const String& cmd);

    /**
     * @brief 处理UART接收数据
     */
    void processUARTData();

    /**
     * @brief 将事件加入队列
     */
    void enqueueEvent(DroneEventType type, const String& msg);

    /**
     * @brief ESP-NOW 发送回调 (静态)
     */
    static void onESPNOWSend(const uint8_t* mac, esp_now_send_status_t status);

    /**
     * @brief ESP-NOW 接收回调 (静态)
     */
    static void onESPNOWRecv(const esp_now_recv_info_t* info,
                              const uint8_t* data, int len);

    /**
     * @brief 广播MAC地址发现对端
     */
    void discoverPeer();
};

#endif // DRONE_COMM_H
