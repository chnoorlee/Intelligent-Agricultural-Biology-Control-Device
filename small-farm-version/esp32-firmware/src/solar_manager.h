/**
 * @file solar_manager.h
 * @brief 太阳能电池管理 — 电压监测与电源切换
 *
 * 功能:
 *   - 太阳能电池板电压监测 (ADC)
 *   - 锂电池电压监测 (电压分压 + ADC)
 *   - 电量SOC估算 (基于电压)
 *   - 电源来源自动切换 (太阳能优先，电池后备)
 *   - DHT22 环境温湿度监测
 *   - INA226 精密电压电流监测 (I2C, 可选)
 */

#ifndef SOLAR_MANAGER_H
#define SOLAR_MANAGER_H

#include <Arduino.h>
#include <DHT.h>

// ==========================================================================
// 硬件引脚与参数
// ==========================================================================
#define SOLAR_PANEL_ADC_PIN     34   // 太阳能电池板电压采样 (GPIO34 = ADC1_CH6)
#define BATTERY_ADC_PIN         35   // 电池电压采样 (GPIO35 = ADC1_CH7)
#define DHT22_PIN               4    // DHT22 数据引脚 (GPIO4)
#define DHT_TYPE                DHT22

// 电压分压比
#define SOLAR_VOLTAGE_DIVIDER   5.7f   // 分压比 (R1+R2)/R2, 例 47K+10K→5.7
#define BATTERY_VOLTAGE_DIVIDER 4.0f   // 分压比, 例 30K+10K→4.0

// ADC 参数
#define ADC_REF_VOLTAGE         3.3f   // ESP32 ADC 参考电压
#define ADC_RESOLUTION          4095   // 12位 ADC

// 电池参数
#define BATTERY_FULL_VOLTAGE    4.2f   // 满电电压 (单节锂电)
#define BATTERY_EMPTY_VOLTAGE   3.0f   // 空电电压
#define BATTERY_NOMINAL_VOLTAGE 3.7f   // 标称电压
#define BATTERY_CAPACITY_MAH    2000   // 电池容量 (mAh)

// 太阳能切换阈值
#define SOLAR_CHARGE_THRESHOLD  4.5f   // 太阳能板电压高于此阈值视为有效充电
#define BATTERY_LOW_THRESHOLD   3.3f   // 电池电压低于此阈值报警
#define SAMPLE_INTERVAL_MS      2000   // 采样间隔

// ==========================================================================
// 电源来源枚举
// ==========================================================================
enum PowerSource {
    POWER_SOLAR,      // 太阳能供电
    POWER_BATTERY,    // 电池供电
    POWER_EXTERNAL,   // 外部电源
    POWER_NONE        // 无电源
};

// ==========================================================================
// SolarManager 类
// ==========================================================================
class SolarManager {
public:
    SolarManager();
    ~SolarManager();

    /**
     * @brief 初始化ADC引脚与DHT22传感器
     * @return true 初始化成功
     */
    bool begin();

    /**
     * @brief 主循环: 周期性采样电压与温湿度
     */
    void loop();

    /**
     * @brief 获取太阳能电池板电压
     * @return 电压 (V)
     */
    float getPanelVoltage();

    /**
     * @brief 获取锂电池电压
     * @return 电压 (V)
     */
    float getBatteryVoltage();

    /**
     * @brief 获取电池电量百分比 (SOC估算)
     * @return 百分比 (0-100)
     */
    int getBatteryPercent();

    /**
     * @brief 获取当前供电来源
     */
    PowerSource getPowerSource();

    /**
     * @brief 获取环境温度
     * @return 温度 (°C)
     */
    float getTemperature();

    /**
     * @brief 获取环境湿度
     * @return 湿度 (%RH)
     */
    float getHumidity();

    /**
     * @brief 电池是否低电量
     */
    bool isBatteryLow() const;

    /**
     * @brief 太阳能板是否在有效充电
     */
    bool isCharging() const;

    /**
     * @brief 获取累计充电量 (mAh, 估算)
     */
    float getEstimatedChargeMah() const { return _estimatedChargeMah; }

private:
    DHT* _dht;
    float _panelVoltage;
    float _batteryVoltage;
    int _batteryPercent;
    PowerSource _powerSource;
    float _temperature;
    float _humidity;
    bool _batteryLow;
    bool _charging;
    float _estimatedChargeMah;

    unsigned long _lastSampleMs;
    unsigned long _lastDHTReadMs;
    bool _dhtOk;

    /**
     * @brief 读取ADC电压 (经分压)
     * @param adcPin ADC引脚
     * @param divider 分压比
     * @return 实际电压
     */
    float readADCVoltage(uint8_t adcPin, float divider);

    /**
     * @brief 电压转SOC百分比
     * @param voltage 电池电压
     * @return 百分比 (0-100)
     */
    int voltageToSOC(float voltage);

    /**
     * @brief 更新电源来源判断
     */
    void updatePowerSource();

    /**
     * @brief 读取DHT22 (带重试)
     */
    void readDHT();
};

#endif // SOLAR_MANAGER_H
