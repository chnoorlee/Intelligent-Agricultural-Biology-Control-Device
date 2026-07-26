/**
 * @file solar_manager.cpp
 * @brief 太阳能电池管理实现
 */

#include "solar_manager.h"

// ==========================================================================
// 构造与析构
// ==========================================================================
SolarManager::SolarManager()
    : _dht(nullptr)
    , _panelVoltage(0.0)
    , _batteryVoltage(0.0)
    , _batteryPercent(0)
    , _powerSource(POWER_NONE)
    , _temperature(0.0)
    , _humidity(0.0)
    , _batteryLow(false)
    , _charging(false)
    , _estimatedChargeMah(0.0)
    , _lastSampleMs(0)
    , _lastDHTReadMs(0)
    , _dhtOk(false)
{
}

SolarManager::~SolarManager() {
    if (_dht) {
        delete _dht;
        _dht = nullptr;
    }
}

// ==========================================================================
// 初始化
// ==========================================================================
bool SolarManager::begin() {
    Serial.println("[Solar] 初始化太阳能管理系统...");

    // 配置 ADC 引脚
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // 0-3.3V 量程
    pinMode(SOLAR_PANEL_ADC_PIN, INPUT);
    pinMode(BATTERY_ADC_PIN, INPUT);

    // 初始化 DHT22
    _dht = new DHT(DHT22_PIN, DHT_TYPE);
    _dht->begin();
    delay(1000);

    // 首次温湿度读取
    readDHT();

    // 首次电压采样
    _panelVoltage = readADCVoltage(SOLAR_PANEL_ADC_PIN, SOLAR_VOLTAGE_DIVIDER);
    _batteryVoltage = readADCVoltage(BATTERY_ADC_PIN, BATTERY_VOLTAGE_DIVIDER);
    _batteryPercent = voltageToSOC(_batteryVoltage);
    updatePowerSource();

    Serial.printf("[Solar] 太阳能板: %.2fV | 电池: %.2fV (%d%%) | 电源: %s\n",
                  _panelVoltage, _batteryVoltage, _batteryPercent,
                  _powerSource == POWER_SOLAR ? "太阳能" :
                  _powerSource == POWER_BATTERY ? "电池" : "外部");

    Serial.printf("[Solar] 温度: %.1f°C | 湿度: %.1f%%\n",
                  _temperature, _humidity);

    return true;
}

// ==========================================================================
// 主循环
// ==========================================================================
void SolarManager::loop() {
    unsigned long now = millis();

    // 电压采样
    if (now - _lastSampleMs >= SAMPLE_INTERVAL_MS) {
        _lastSampleMs = now;

        _panelVoltage = readADCVoltage(SOLAR_PANEL_ADC_PIN, SOLAR_VOLTAGE_DIVIDER);
        _batteryVoltage = readADCVoltage(BATTERY_ADC_PIN, BATTERY_VOLTAGE_DIVIDER);
        _batteryPercent = voltageToSOC(_batteryVoltage);
        _batteryLow = (_batteryVoltage < BATTERY_LOW_THRESHOLD);

        updatePowerSource();

        // 估算充电量 (mAh)
        if (_charging && _batteryVoltage < BATTERY_FULL_VOLTAGE) {
            // 简化估算: 假设充电电流约 200mA (取决于太阳能板功率)
            float chargeCurrentA = 0.2f;  // 200mA
            float sampleHours = SAMPLE_INTERVAL_MS / 3600000.0f;
            _estimatedChargeMah += chargeCurrentA * sampleHours * 1000.0f;
        }
    }

    // 温湿度读取 (每10秒)
    if (now - _lastDHTReadMs >= 10000) {
        _lastDHTReadMs = now;
        readDHT();
    }
}

// ==========================================================================
// ADC 电压读取
// ==========================================================================
float SolarManager::readADCVoltage(uint8_t adcPin, float divider) {
    // 多次采样取平均，减少噪声
    const int samples = 16;
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(adcPin);
        delayMicroseconds(100);
    }

    float avgADC = (float)sum / samples;
    float voltage = (avgADC / ADC_RESOLUTION) * ADC_REF_VOLTAGE * divider;

    return voltage;
}

// ==========================================================================
// 电压转SOC (基于锂电池放电曲线)
// ==========================================================================
int SolarManager::voltageToSOC(float voltage) {
    // 锂电池 SOC-电压 映射表 (3.0V ~ 4.2V)
    // 近似线性 + 两端修正
    if (voltage >= BATTERY_FULL_VOLTAGE) {
        return 100;
    }
    if (voltage <= BATTERY_EMPTY_VOLTAGE) {
        return 0;
    }

    // 放电曲线分段拟合
    float normalized = (voltage - BATTERY_EMPTY_VOLTAGE) /
                       (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE);

    // 非线性补偿: 锂电池放电曲线中间段较平
    // 使用二次曲线拟合: SOC = a*x^2 + b*x + c
    // 拟合参数 (经验值)
    float a = -0.3f;
    float b = 1.3f;
    float soc = a * normalized * normalized + b * normalized;

    int percent = (int)(soc * 100.0f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

// ==========================================================================
// 电源来源判断
// ==========================================================================
void SolarManager::updatePowerSource() {
    if (_panelVoltage > SOLAR_CHARGE_THRESHOLD) {
        _powerSource = POWER_SOLAR;
        _charging = true;
    } else if (_batteryVoltage > BATTERY_EMPTY_VOLTAGE) {
        _powerSource = POWER_BATTERY;
        _charging = (_panelVoltage > _batteryVoltage + 0.3f);
    } else {
        _powerSource = POWER_NONE;
        _charging = false;
    }
}

// ==========================================================================
// DHT22 读取
// ==========================================================================
void SolarManager::readDHT() {
    if (!_dht) return;

    float t = _dht->readTemperature();
    float h = _dht->readHumidity();

    // 检查读数有效性
    if (isnan(t) || isnan(h)) {
        _dhtOk = false;
        // 保留上一次有效读数
        return;
    }

    _temperature = t;
    _humidity = h;
    _dhtOk = true;
}

// ==========================================================================
// 公共接口
// ==========================================================================
float SolarManager::getPanelVoltage() {
    return _panelVoltage;
}

float SolarManager::getBatteryVoltage() {
    return _batteryVoltage;
}

int SolarManager::getBatteryPercent() {
    return _batteryPercent;
}

PowerSource SolarManager::getPowerSource() {
    return _powerSource;
}

float SolarManager::getTemperature() {
    return _temperature;
}

float SolarManager::getHumidity() {
    return _humidity;
}

bool SolarManager::isBatteryLow() const {
    return _batteryLow;
}

bool SolarManager::isCharging() const {
    return _charging;
}
