/**
 * @file ultrasonic.h
 * @brief HC-SR04 超声波测距模块驱动
 *
 * 功能:
 *   - 脉宽法测距
 *   - 多次采样取中值滤波
 *   - 超时保护与异常检测
 */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <Arduino.h>

// ==========================================================================
// 默认引脚
// ==========================================================================
#ifndef ULTRASONIC_TRIG_PIN
#define ULTRASONIC_TRIG_PIN     5   // GPIO5 触发引脚
#endif

#ifndef ULTRASONIC_ECHO_PIN
#define ULTRASONIC_ECHO_PIN     18  // GPIO18 回波引脚
#endif

// ==========================================================================
// 测距参数
// ==========================================================================
#define ULTRASONIC_MAX_DISTANCE     400.0f   // 最大量程 (cm)
#define ULTRASONIC_MIN_DISTANCE     2.0f     // 最近盲区 (cm)
#define ULTRASONIC_TIMEOUT_US       23200    // 超时微秒 (对应400cm)
#define ULTRASONIC_SPEED_OF_SOUND   0.0343f  // 声速 (cm/us @ 20°C)
#define ULTRASONIC_SAMPLE_COUNT     5        // 中值滤波采样次数
#define ULTRASONIC_SAMPLE_INTERVAL  60       // 采样间隔 (ms)

// ==========================================================================
// Ultrasonic 类
// ==========================================================================
class Ultrasonic {
public:
    /**
     * @brief 构造函数
     * @param trigPin 触发引脚
     * @param echoPin 回波引脚
     */
    Ultrasonic(uint8_t trigPin = ULTRASONIC_TRIG_PIN,
               uint8_t echoPin = ULTRASONIC_ECHO_PIN);

    ~Ultrasonic() = default;

    /**
     * @brief 初始化引脚
     * @return true 初始化成功
     */
    bool begin();

    /**
     * @brief 获取单次测量距离 (阻塞式)
     * @return 距离 (cm), -1.0 表示超时/异常
     */
    float getDistanceRaw();

    /**
     * @brief 获取滤波后距离 (中值滤波, 多次采样)
     * @return 距离 (cm)
     */
    float getDistance();

    /**
     * @brief 获取最近N次平均距离
     * @param n 平均次数 (1-10)
     * @return 平均距离 (cm)
     */
    float getAverageDistance(int n = 3);

    /**
     * @brief 检测是否有目标在指定范围内
     * @param minCm 最小距离
     * @param maxCm 最大距离
     * @return true 有目标在范围内
     */
    bool isTargetInRange(float minCm, float maxCm);

    /**
     * @brief 获取原始回波脉宽 (微秒)
     * @return 脉宽 (us)
     */
    unsigned long getEchoPulseUs();

private:
    uint8_t _trigPin;
    uint8_t _echoPin;
    float _lastDistance;
    float _distanceHistory[10];
    int _historyIndex;
    int _historyCount;

    /**
     * @brief 冒泡排序 (用于中值滤波)
     */
    void sortArray(float arr[], int n);

    /**
     * @brief 将回波脉宽转换为距离
     * @param durationUs 脉宽 (微秒)
     * @return 距离 (cm)
     */
    float pulseToDistance(unsigned long durationUs);
};

#endif // ULTRASONIC_H
