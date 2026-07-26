/**
 * @file main.cpp
 * @brief ESP32 主控程序 — 小型农场版智能生物防控系统
 *
 * 功能:
 *   - 初始化所有硬件模块 (WiFi, 舵机云台, 超声波, 太阳能管理, MQTT)
 *   - 主循环调度各模块运行
 *   - 协调无人机通信与云台追踪逻辑
 *
 * 硬件:
 *   - ESP32 DevKit V1
 *   - PCA9685 PWM 舵机驱动板 (I2C)
 *   - HC-SR04 超声波模块
 *   - 太阳能电池 + INA226 电压电流监测 (I2C)
 *   - DRV8870/PCA9685 无人机通信 (ESP-NOW / UART)
 */

#include <Arduino.h>
#include <Wire.h>

#include "wifi_manager.h"
#include "gimbal_control.h"
#include "drone_comm.h"
#include "ultrasonic.h"
#include "solar_manager.h"
#include "mqtt_client.h"

// ==========================================================================
// 全局对象
// ==========================================================================
WifiManager    wifiManager;
GimbalControl  gimbal;
DroneComm      droneComm;
Ultrasonic     ultrasonic(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
SolarManager   solarManager;
MqttClient     mqttClient;

// ==========================================================================
// 系统状态枚举
// ==========================================================================
enum SystemState {
    STATE_INIT,
    STATE_IDLE,         // 空闲待机
    STATE_TRACKING,     // 正在追踪目标
    STATE_DRONE_DISPATCH, // 无人机已派发
    STATE_DRIVING,      // 正在驱离
    STATE_RETURNING,    // 无人机返航中
    STATE_ERROR         // 错误状态
};

SystemState currentState = STATE_INIT;
SystemState previousState = STATE_INIT;

// ==========================================================================
// 定时器与标志
// ==========================================================================
unsigned long lastSensorReportMs = 0;
const unsigned long SENSOR_REPORT_INTERVAL = 5000;  // 5秒上报一次传感器数据

unsigned long lastStateLedMs = 0;
const unsigned long STATE_LED_INTERVAL = 500;

bool targetDetected = false;
float targetPanAngle = 90.0;   // 目标水平角度 (度)
float targetTiltAngle = 90.0;  // 目标俯仰角度 (度)

// 板载LED (GPIO2 一般为内置LED)
#define STATUS_LED_PIN 2

// ==========================================================================
// 状态LED指示
// ==========================================================================
void updateStatusLed() {
    if (millis() - lastStateLedMs < STATE_LED_INTERVAL) return;
    lastStateLedMs = millis();

    static bool ledState = false;
    switch (currentState) {
        case STATE_INIT:
            // 快速闪烁: 初始化中
            digitalWrite(STATUS_LED_PIN, (millis() / 100) % 2 ? HIGH : LOW);
            break;
        case STATE_IDLE:
            // 慢速呼吸: 空闲
            ledState = !ledState;
            digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
            STATE_LED_INTERVAL == 1000 ? lastStateLedMs = millis() : 0;
            break;
        case STATE_TRACKING:
            // 双闪: 追踪中
            digitalWrite(STATUS_LED_PIN, (millis() / 200) % 2 ? HIGH : LOW);
            break;
        case STATE_DRONE_DISPATCH:
        case STATE_DRIVING:
            // 常亮: 工作中
            digitalWrite(STATUS_LED_PIN, HIGH);
            break;
        case STATE_RETURNING:
            // 三短一长: 返航
            {
                int phase = (millis() / 200) % 8;
                digitalWrite(STATUS_LED_PIN, (phase < 3 || phase == 6) ? HIGH : LOW);
            }
            break;
        case STATE_ERROR:
            // 极快闪烁: 错误
            digitalWrite(STATUS_LED_PIN, (millis() / 50) % 2 ? HIGH : LOW);
            break;
    }
}

// ==========================================================================
// 状态切换
// ==========================================================================
void setState(SystemState newState) {
    if (newState != currentState) {
        previousState = currentState;
        currentState = newState;

        Serial.print("[MAIN] State: ");
        Serial.print(previousState);
        Serial.print(" -> ");
        Serial.println(newState);

        // 状态变更时上报MQTT
        String stateStr;
        switch (newState) {
            case STATE_IDLE:           stateStr = "idle"; break;
            case STATE_TRACKING:       stateStr = "tracking"; break;
            case STATE_DRONE_DISPATCH: stateStr = "drone_dispatched"; break;
            case STATE_DRIVING:        stateStr = "driving"; break;
            case STATE_RETURNING:      stateStr = "returning"; break;
            case STATE_ERROR:          stateStr = "error"; break;
            default:                   stateStr = "unknown"; break;
        }
        mqttClient.publishState(stateStr);
    }
}

// ==========================================================================
// 初始化
// ==========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n============================================");
    Serial.println("  智能农业生物防控系统 - 小型农场版 v1.0");
    Serial.println("  ESP32 主控固件启动中...");
    Serial.println("============================================\n");

    // 初始化板载LED
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    // 1. 初始化 I2C 总线
    Wire.begin(21, 22);  // SDA=GPIO21, SCL=GPIO22
    Wire.setClock(100000);
    Serial.println("[MAIN] I2C 总线初始化完成 (SDA=21, SCL=22)");

    // 2. 初始化 WiFi
    if (!wifiManager.begin()) {
        Serial.println("[MAIN] WiFi 连接失败，启动 AP 模式作为备用");
        wifiManager.startAP("BioControl_Config", "12345678");
    }

    // 3. 初始化 PCA9685 舵机驱动 (I2C地址0x40)
    if (!gimbal.begin(0x40)) {
        Serial.println("[MAIN] 云台初始化失败! 进入错误状态");
        setState(STATE_ERROR);
    }

    // 4. 初始化超声波模块
    if (!ultrasonic.begin()) {
        Serial.println("[MAIN] 超声波初始化失败!");
    }

    // 5. 初始化太阳能管理模块
    if (!solarManager.begin()) {
        Serial.println("[MAIN] 太阳能管理初始化失败!");
    }

    // 6. 初始化 MQTT 客户端
    if (wifiManager.isConnected()) {
        mqttClient.begin(wifiManager);
    }

    // 7. 初始化无人机通信
    if (!droneComm.begin()) {
        Serial.println("[MAIN] 无人机通信初始化失败!");
    }

    // 8. 云台归中
    gimbal.center();
    delay(500);

    setState(STATE_IDLE);
    Serial.println("[MAIN] 系统初始化完成, 进入空闲状态");
    Serial.println("[MAIN] 等待摄像头端检测指令...\n");
}

// ==========================================================================
// 串口指令解析 (来自摄像头/上位机)
// ==========================================================================
void handleSerialCommand() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() == 0) return;

    Serial.print("[MAIN] Received: ");
    Serial.println(cmd);

    // 指令格式: CMD:ARG1,ARG2,...
    int colonIdx = cmd.indexOf(':');
    if (colonIdx < 0) return;

    String cmdType = cmd.substring(0, colonIdx);
    String args = cmd.substring(colonIdx + 1);

    if (cmdType == "TRACK") {
        // TRACK:pan,tilt,conf
        // 收到追踪目标指令，解析角度
        int c1 = args.indexOf(',');
        int c2 = args.lastIndexOf(',');
        if (c1 > 0 && c2 > c1) {
            float pan = args.substring(0, c1).toFloat();
            float tilt = args.substring(c1 + 1, c2).toFloat();
            float conf = args.substring(c2 + 1).toFloat();

            targetPanAngle = pan;
            targetTiltAngle = tilt;
            targetDetected = (conf > 0.5);

            if (targetDetected && currentState == STATE_IDLE) {
                setState(STATE_TRACKING);
            }

            // 移动云台到目标角度
            gimbal.setPanAngle(pan);
            gimbal.setTiltAngle(tilt);

            Serial.printf("[MAIN] Tracking: pan=%.1f tilt=%.1f conf=%.2f\n", pan, tilt, conf);
        }
    }
    else if (cmdType == "DRIVE") {
        // DRIVE:1 或 DRIVE:0
        if (args == "1") {
            // 激活驱离流程
            float distance = ultrasonic.getDistance();

            if (distance < 50.0) {
                // 距离近: 直接启动声光驱离
                setState(STATE_DRIVING);
                Serial.println("[MAIN] 目标距离<50cm，启动本地声光驱离");
            } else {
                // 距离远: 派发无人机
                setState(STATE_DRONE_DISPATCH);
                droneComm.sendDispatchCommand(targetPanAngle, targetTiltAngle, distance);
                Serial.printf("[MAIN] 目标距离=%.1fcm，派发无人机 target(pan=%.1f, tilt=%.1f)\n",
                              distance, targetPanAngle, targetTiltAngle);
            }
        } else {
            // 停止驱离
            if (currentState == STATE_DRIVING) {
                setState(STATE_IDLE);
                gimbal.center();
            }
        }
    }
    else if (cmdType == "CENTER") {
        gimbal.center();
        targetDetected = false;
        if (currentState == STATE_TRACKING) {
            setState(STATE_IDLE);
        }
    }
    else if (cmdType == "PING") {
        Serial.println("PONG");
    }
    else if (cmdType == "STATUS") {
        // 返回系统状态
        float dist = ultrasonic.getDistance();
        float voltage = solarManager.getBatteryVoltage();
        float panelV = solarManager.getPanelVoltage();
        Serial.printf("STATE:%d,DIST:%.1f,BAT:%.2f,SOLAR:%.2f\r\n",
                      currentState, dist, voltage, panelV);
    }
}

// ==========================================================================
// 处理无人机返回的消息
// ==========================================================================
void handleDroneEvents() {
    DroneEvent event = droneComm.checkEvent();
    switch (event.type) {
        case DRONE_EVENT_NONE:
            break;

        case DRONE_EVENT_ARRIVED:
            Serial.println("[MAIN] 无人机已抵达目标位置");
            if (currentState == STATE_DRONE_DISPATCH) {
                setState(STATE_DRIVING);
                // 启动超声波驱离
                droneComm.sendUltrasonicOn();
            }
            break;

        case DRONE_EVENT_DRIVE_DONE:
            Serial.println("[MAIN] 驱离完成，无人机开始返航");
            setState(STATE_RETURNING);
            droneComm.sendReturnCommand();
            break;

        case DRONE_EVENT_RETURNED:
            Serial.println("[MAIN] 无人机已返航");
            setState(STATE_IDLE);
            gimbal.center();
            // 上报驱离成功
            mqttClient.publishEvent("drive_success");
            break;

        case DRONE_EVENT_FAILED:
            Serial.println("[MAIN] 驱离失败，需要人工介入!");
            setState(STATE_ERROR);
            mqttClient.publishEvent("drive_failed");
            mqttClient.publishAlert("驱离失败，请人工检查!");
            break;

        case DRONE_EVENT_LOW_BATTERY:
            Serial.println("[MAIN] 无人机电量低，强制返航");
            setState(STATE_RETURNING);
            droneComm.sendReturnCommand();
            break;
    }
}

// ==========================================================================
// 周期性传感器数据上报
// ==========================================================================
void reportSensorData() {
    if (millis() - lastSensorReportMs < SENSOR_REPORT_INTERVAL) return;
    lastSensorReportMs = millis();

    float temperature = solarManager.getTemperature();
    float humidity = solarManager.getHumidity();
    float batteryVoltage = solarManager.getBatteryVoltage();
    float panelVoltage = solarManager.getPanelVoltage();
    int batteryPercent = solarManager.getBatteryPercent();
    float distance = ultrasonic.getDistance();

    // 串口输出 (调试)
    Serial.printf("[SENSOR] T=%.1f°C H=%.1f%% Bat=%.2fV(%d%%) Solar=%.2fV Dist=%.1fcm\n",
                  temperature, humidity, batteryVoltage, batteryPercent,
                  panelVoltage, distance);

    // MQTT 上报
    mqttClient.publishSensorData(temperature, humidity, batteryVoltage,
                                  batteryPercent, panelVoltage, distance);
}

// ==========================================================================
// 超时保护: 追踪太久无结果则回到空闲
// ==========================================================================
unsigned long trackingStartMs = 0;
const unsigned long TRACKING_TIMEOUT = 30000;  // 30秒追踪超时

void checkTrackingTimeout() {
    if (currentState == STATE_TRACKING) {
        if (millis() - trackingStartMs > TRACKING_TIMEOUT) {
            Serial.println("[MAIN] 追踪超时，回到空闲状态");
            setState(STATE_IDLE);
            gimbal.center();
            targetDetected = false;
        }
    } else if (currentState != STATE_TRACKING && targetDetected) {
        trackingStartMs = millis();
    }
}

// ==========================================================================
// 主循环
// ==========================================================================
void loop() {
    // 1. WiFi 连接维护
    wifiManager.loop();

    // 2. MQTT 连接维护
    mqttClient.loop();

    // 3. 串口指令处理
    handleSerialCommand();

    // 4. 无人机事件处理
    handleDroneEvents();

    // 5. 追踪超时检测
    checkTrackingTimeout();

    // 6. 周期性传感器数据上报
    reportSensorData();

    // 7. 太阳能管理循环
    solarManager.loop();

    // 8. 状态LED更新
    updateStatusLed();

    // 循环间隔
    delay(10);
}
