#include <Arduino.h>

// ============== Pin Definitions ==============
#define BATTERY_VOLTAGE_PIN  39    // 电池电压分压检测 (ADC)
#define CHARGE_STATUS_PIN    25    // 充电状态指示 (电池管理芯片状态引脚)
#define LOAD_SWITCH_PIN      26    // 负载开关控制
#define SOLAR_INPUT_PIN      27    // 太阳能输入检测
#define POWER_LED_PIN        2     // 电源指示灯

// ============== Constants ==============
#define VOLT_DIVIDER_RATIO  0.25f     // 电池电压分压比 (12V电池 -> 3V ADC)
#define ADC_REF_VOLTAGE     3.3f
#define ADC_RESOLUTION      4095.0f

// 锂电池电压阈值 (3S LiPo: 12.6V满电, 9.0V低压保护)
#define VOLTAGE_FULL        12.6f     // 满电电压
#define VOLTAGE_LOW         10.5f     // 低电量警告
#define VOLTAGE_CRITICAL    9.6f      // 临界低电量 (强制关机)
#define VOLTAGE_CHARGING_MIN 11.0f    // 开始充电的最低电压判断

// 电池容量 (mAh)
#define BATTERY_CAPACITY_MAH  6000    // 标称容量
#define CURRENT_DRAW_MA       500     // 系统平均功耗 (mA)

// 采样与计算间隔
#define VOLTAGE_SAMPLES     20
#define SOC_INTERVAL_MS     10000     // SOC 更新间隔 (10秒)
#define REPORT_INTERVAL_MS  30000     // 状态上报间隔 (30秒)

// ============== Globals ==============
float batteryVoltage = 0.0;
float batterySOC = 100.0;            // State of Charge (%)
bool isCharging = false;
bool loadEnabled = true;
unsigned int runTimeSeconds = 0;
unsigned long lastSOCUpdate = 0;
unsigned long lastReport = 0;
unsigned long lastSecondTick = 0;

// 电量积分用 (Coulomb counting)
float consumed_mAh = 0.0;

// ============== Setup ==============
void setup() {
    Serial.begin(115200);
    Serial.println("[BatteryManager] Initializing...");

    pinMode(CHARGE_STATUS_PIN, INPUT_PULLUP);
    pinMode(LOAD_SWITCH_PIN, OUTPUT);
    pinMode(SOLAR_INPUT_PIN, INPUT);
    pinMode(POWER_LED_PIN, OUTPUT);

    digitalWrite(LOAD_SWITCH_PIN, HIGH);  // 默认开启负载
    digitalWrite(POWER_LED_PIN, HIGH);    // 电源指示

    // 初始电压读取
    readBatteryVoltage();
    batterySOC = estimateSOC(batteryVoltage);

    lastSOCUpdate = millis();
    lastReport = millis();
    lastSecondTick = millis();

    Serial.printf("[Battery] Initial: %.2fV, SOC: %.0f%%\n", batteryVoltage, batterySOC);
}

// ============== Main Loop ==============
void loop() {
    unsigned long now = millis();

    // 秒级计时
    if (now - lastSecondTick >= 1000) {
        lastSecondTick += 1000;
        runTimeSeconds++;
    }

    // SOC 更新
    if (now - lastSOCUpdate >= SOC_INTERVAL_MS) {
        lastSOCUpdate = now;
        updateBatteryState();
    }

    // 状态上报
    if (now - lastReport >= REPORT_INTERVAL_MS) {
        lastReport = now;
        reportStatus();
    }

    // 保护检查
    checkProtection();

    // 串口指令
    handleCommand();

    delay(100);
}

// ============== Battery State ==============
void updateBatteryState() {
    readBatteryVoltage();

    // 检测充电状态
    bool solarInput = digitalRead(SOLAR_INPUT_PIN);
    bool chargeStatus = !digitalRead(CHARGE_STATUS_PIN);  // 低电平有效
    isCharging = solarInput && chargeStatus;

    // Coulomb counting 消耗累计
    if (!isCharging && loadEnabled) {
        float dt_hours = SOC_INTERVAL_MS / 3600000.0f;
        consumed_mAh += CURRENT_DRAW_MA * dt_hours;
    }

    // SOC 估计 (结合电压与库仑计数)
    batterySOC = estimateSOC(batteryVoltage);

    // 如果正在充电, 重置消耗计数
    if (isCharging) {
        consumed_mAh *= 0.95f;  // 缓慢恢复
    }

    Serial.printf("[Battery] %.2fV SOC:%.0f%% Charge:%d Load:%d Runtime:%us\n",
                  batteryVoltage, batterySOC, isCharging, loadEnabled, runTimeSeconds);
}

float estimateSOC(float voltage) {
    // 基于电压的 SOC 估计 (3S LiPo 放电曲线简化模型)
    // 12.6V = 100%, 11.1V = 50%, 9.6V = 0%
    float soc_voltage;

    if (voltage >= VOLTAGE_FULL) {
        soc_voltage = 100.0;
    } else if (voltage <= VOLTAGE_CRITICAL) {
        soc_voltage = 0.0;
    } else {
        // 线性插值 (实际锂电池曲线是非线性的)
        soc_voltage = (voltage - VOLTAGE_CRITICAL) /
                      (VOLTAGE_FULL - VOLTAGE_CRITICAL) * 100.0;
    }

    // 结合库仑计数
    float soc_coulomb = 100.0 - (consumed_mAh / BATTERY_CAPACITY_MAH * 100.0);
    soc_coulomb = constrain(soc_coulomb, 0, 100);

    // 加权融合 (电压权重 0.4, 库仑计数权重 0.6)
    float soc = soc_voltage * 0.4 + soc_coulomb * 0.6;

    return constrain(soc, 0.0, 100.0);
}

void readBatteryVoltage() {
    float sum = 0;
    for (int i = 0; i < VOLTAGE_SAMPLES; i++) {
        sum += analogRead(BATTERY_VOLTAGE_PIN);
        delay(1);
    }
    float avg = sum / VOLTAGE_SAMPLES;
    float adcVoltage = (avg / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
    batteryVoltage = adcVoltage / VOLT_DIVIDER_RATIO;
}

// ============== Protection ==============
void checkProtection() {
    if (batteryVoltage <= VOLTAGE_CRITICAL && loadEnabled) {
        Serial.println("[Battery] CRITICAL: Low voltage shutdown!");
        loadEnabled = false;
        digitalWrite(LOAD_SWITCH_PIN, LOW);
        digitalWrite(POWER_LED_PIN, LOW);
    } else if (batteryVoltage >= VOLTAGE_CHARGING_MIN && !loadEnabled && isCharging) {
        Serial.println("[Battery] Recovery: Load re-enabled");
        loadEnabled = true;
        digitalWrite(LOAD_SWITCH_PIN, HIGH);
        digitalWrite(POWER_LED_PIN, HIGH);
    }

    // 低电量 LED 闪烁警告
    if (batteryVoltage <= VOLTAGE_LOW && loadEnabled) {
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 500) {
            lastBlink = millis();
            digitalWrite(POWER_LED_PIN, !digitalRead(POWER_LED_PIN));
        }
    }
}

// ============== Status Report ==============
void reportStatus() {
    Serial.printf(
        "{\"type\":\"battery_status\","
        "\"voltage\":%.2f,"
        "\"soc\":%.1f,"
        "\"charging\":%s,"
        "\"load\":%s,"
        "\"runtime_s\":%u,"
        "\"consumed_mah\":%.1f}\n",
        batteryVoltage, batterySOC,
        isCharging ? "true" : "false",
        loadEnabled ? "true" : "false",
        runTimeSeconds, consumed_mAh
    );
}

// ============== Serial Commands ==============
void handleCommand() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "STATUS") {
            reportStatus();
        } else if (cmd == "LOAD_ON") {
            loadEnabled = true;
            digitalWrite(LOAD_SWITCH_PIN, HIGH);
            Serial.println("{\"load\":\"on\"}");
        } else if (cmd == "LOAD_OFF") {
            loadEnabled = false;
            digitalWrite(LOAD_SWITCH_PIN, LOW);
            Serial.println("{\"load\":\"off\"}");
        } else if (cmd == "RESET_COULOMB") {
            consumed_mAh = 0;
            Serial.println("{\"coulomb\":\"reset\"}");
        }
    }
}

// ============== API Functions ==============
float getBatteryVoltage() { return batteryVoltage; }
float getBatterySOC() { return batterySOC; }
bool getChargingState() { return isCharging; }
bool getLoadState() { return loadEnabled; }

void setLoad(bool enable) {
    loadEnabled = enable;
    digitalWrite(LOAD_SWITCH_PIN, enable ? HIGH : LOW);
}
