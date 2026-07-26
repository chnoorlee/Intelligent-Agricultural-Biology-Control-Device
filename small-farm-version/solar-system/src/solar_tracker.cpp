#include <Arduino.h>
#include <ESP32Servo.h>

// ============== Pin Definitions ==============
#define LDR_TOP_LEFT    34    // 左上光敏电阻 (ADC)
#define LDR_TOP_RIGHT   35    // 右上光敏电阻 (ADC)
#define LDR_BOT_LEFT    32    // 左下光敏电阻 (ADC)
#define LDR_BOT_RIGHT   33    // 右下光敏电阻 (ADC)
#define SERVO_PAN_PIN   13    // 水平旋转舵机
#define SERVO_TILT_PIN  12    // 垂直倾斜舵机
#define VOLTAGE_SENSE   36    // 太阳能板电压检测 (ADC, 电压分压)

// ============== Constants ==============
#define TRACK_INTERVAL   5000   // 追光调整间隔 (ms)
#define VOLTAGE_SAMPLE   10     // 电压采样次数
#define SERVO_STEP       3      // 每步舵机角度增量
#define PAN_MIN          0
#define PAN_MAX          180
#define TILT_MIN         30
#define TILT_MAX         150

// ADC 分压比: 假设太阳能板 21V, 分压后接 3.3V ADC
// 分压比 = (R2 / (R1 + R2)) = 3.3/21 ≈ 0.157
#define VOLT_DIVIDER_RATIO  0.157f
#define ADC_REF_VOLTAGE     3.3f
#define ADC_RESOLUTION      4095.0f

// ============== Globals ==============
Servo panServo;
Servo tiltServo;
int panAngle = 90;      // 当前水平角度
int tiltAngle = 90;     // 当前垂直角度
unsigned long lastTrackTime = 0;
float solarVoltage = 0.0;

// ============== Setup ==============
void setup() {
    Serial.begin(115200);
    Serial.println("[SolarTracker] Initializing...");

    // 舵机初始化
    panServo.attach(SERVO_PAN_PIN);
    tiltServo.attach(SERVO_TILT_PIN);
    panServo.write(panAngle);
    tiltServo.write(tiltAngle);

    // ADC 初始化
    analogReadResolution(12);  // ESP32 12-bit ADC
    pinMode(VOLTAGE_SENSE, INPUT);

    // 初始校准: 转到默认位置
    delay(1000);
    Serial.println("[SolarTracker] Ready.");
}

// ============== Main Loop ==============
void loop() {
    unsigned long now = millis();

    if (now - lastTrackTime >= TRACK_INTERVAL) {
        lastTrackTime = now;
        trackSun();
    }

    // 每 10 秒读取一次太阳能电压
    static unsigned long lastVoltRead = 0;
    if (now - lastVoltRead >= 10000) {
        lastVoltRead = now;
        readSolarVoltage();
    }

    // 串口指令处理
    handleSerialCommand();
    delay(100);
}

// ============== Sun Tracking ==============
void trackSun() {
    // 读取四个方向的光敏电阻值
    int tl = analogRead(LDR_TOP_LEFT);
    int tr = analogRead(LDR_TOP_RIGHT);
    int bl = analogRead(LDR_BOT_LEFT);
    int br = analogRead(LDR_BOT_RIGHT);

    // 计算各方向平均光照
    int avgTop  = (tl + tr) / 2;
    int avgBot  = (bl + br) / 2;
    int avgLeft = (tl + bl) / 2;
    int avgRight= (tr + br) / 2;

    // 计算差值
    int diffVert = avgTop - avgBot;     // 正值: 上方更亮
    int diffHoriz = avgLeft - avgRight; // 正值: 左侧更亮

    // 光照差阈值 (避免抖动)
    const int LIGHT_THRESHOLD = 30;

    // 水平调整
    if (abs(diffHoriz) > LIGHT_THRESHOLD) {
        if (diffHoriz > 0) {
            panAngle = constrain(panAngle - SERVO_STEP, PAN_MIN, PAN_MAX);
        } else {
            panAngle = constrain(panAngle + SERVO_STEP, PAN_MIN, PAN_MAX);
        }
    }

    // 垂直调整
    if (abs(diffVert) > LIGHT_THRESHOLD) {
        if (diffVert > 0) {
            tiltAngle = constrain(tiltAngle - SERVO_STEP, TILT_MIN, TILT_MAX);
        } else {
            tiltAngle = constrain(tiltAngle + SERVO_STEP, TILT_MIN, TILT_MAX);
        }
    }

    // 更新舵机
    panServo.write(panAngle);
    tiltServo.write(tiltAngle);

    // 调试输出
    Serial.printf("[Track] LDR(TL:%d TR:%d BL:%d BR:%d) diff(V:%d H:%d) Servo(P:%d T:%d)\n",
                  tl, tr, bl, br, diffVert, diffHoriz, panAngle, tiltAngle);
}

// ============== Voltage Reading ==============
void readSolarVoltage() {
    float sum = 0;
    for (int i = 0; i < VOLTAGE_SAMPLE; i++) {
        sum += analogRead(VOLTAGE_SENSE);
        delay(5);
    }
    float avg = sum / VOLTAGE_SAMPLE;

    // ADC 值 -> 实际太阳能板电压
    float adcVoltage = (avg / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
    solarVoltage = adcVoltage / VOLT_DIVIDER_RATIO;

    Serial.printf("[Solar] Voltage: %.2fV (ADC: %.0f)\n", solarVoltage, avg);
}

// ============== Serial Commands ==============
void handleSerialCommand() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "STATUS") {
            Serial.printf("{\"pan\":%d,\"tilt\":%d,\"voltage\":%.2f}\n",
                          panAngle, tiltAngle, solarVoltage);
        } else if (cmd.startsWith("PAN=")) {
            panAngle = constrain(cmd.substring(4).toInt(), PAN_MIN, PAN_MAX);
            panServo.write(panAngle);
        } else if (cmd.startsWith("TILT=")) {
            tiltAngle = constrain(cmd.substring(5).toInt(), TILT_MIN, TILT_MAX);
            tiltServo.write(tiltAngle);
        } else if (cmd == "HOME") {
            panAngle = 90;
            tiltAngle = 90;
            panServo.write(panAngle);
            tiltServo.write(tiltAngle);
            Serial.println("{\"status\":\"homed\"}");
        }
    }
}
