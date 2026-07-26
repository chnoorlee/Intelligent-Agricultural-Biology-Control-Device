/**
 * @file ultrasonic.cpp
 * @brief HC-SR04 超声波测距实现
 */

#include "ultrasonic.h"

// ==========================================================================
// 构造
// ==========================================================================
Ultrasonic::Ultrasonic(uint8_t trigPin, uint8_t echoPin)
    : _trigPin(trigPin)
    , _echoPin(echoPin)
    , _lastDistance(0.0)
    , _historyIndex(0)
    , _historyCount(0)
{
    memset(_distanceHistory, 0, sizeof(_distanceHistory));
}

// ==========================================================================
// 初始化
// ==========================================================================
bool Ultrasonic::begin() {
    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);

    digitalWrite(_trigPin, LOW);
    delay(100);

    Serial.printf("[Ultrasonic] 初始化 (Trig=GPIO%d, Echo=GPIO%d)\n",
                  _trigPin, _echoPin);
    return true;
}

// ==========================================================================
// 单次原始测距
// ==========================================================================
float Ultrasonic::getDistanceRaw() {
    // 确保Trig引脚为低电平
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);

    // 发送 10us 高脉冲
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    // 读取Echo高电平脉宽 (微秒)
    unsigned long duration = pulseIn(_echoPin, HIGH, ULTRASONIC_TIMEOUT_US);

    // 超时判断
    if (duration == 0) {
        return ULTRASONIC_MAX_DISTANCE;  // 超时视为超出量程
    }

    float distance = pulseToDistance(duration);

    // 超出量程限制
    if (distance > ULTRASONIC_MAX_DISTANCE) {
        distance = ULTRASONIC_MAX_DISTANCE;
    }
    if (distance < ULTRASONIC_MIN_DISTANCE && distance > 0) {
        distance = ULTRASONIC_MIN_DISTANCE;
    }

    return distance;
}

// ==========================================================================
// 滤波测距 (中值滤波)
// ==========================================================================
float Ultrasonic::getDistance() {
    float samples[ULTRASONIC_SAMPLE_COUNT];

    for (int i = 0; i < ULTRASONIC_SAMPLE_COUNT; i++) {
        samples[i] = getDistanceRaw();
        delay(ULTRASONIC_SAMPLE_INTERVAL);
    }

    // 中值滤波: 排序后取中间值
    sortArray(samples, ULTRASONIC_SAMPLE_COUNT);
    float median = samples[ULTRASONIC_SAMPLE_COUNT / 2];

    // 存入历史记录环形缓冲区
    _distanceHistory[_historyIndex] = median;
    _historyIndex = (_historyIndex + 1) % 10;
    if (_historyCount < 10) _historyCount++;

    _lastDistance = median;
    return median;
}

// ==========================================================================
// 平均距离
// ==========================================================================
float Ultrasonic::getAverageDistance(int n) {
    if (n < 1) n = 1;
    if (n > 10) n = 10;
    if (n > _historyCount) n = _historyCount;
    if (n == 0) return _lastDistance;

    float sum = 0.0;
    for (int i = 0; i < n; i++) {
        int idx = (_historyIndex - 1 - i + 10) % 10;
        sum += _distanceHistory[idx];
    }
    return sum / n;
}

// ==========================================================================
// 范围检测
// ==========================================================================
bool Ultrasonic::isTargetInRange(float minCm, float maxCm) {
    float dist = getDistance();
    return (dist >= minCm && dist <= maxCm);
}

// ==========================================================================
// 获取原始脉宽
// ==========================================================================
unsigned long Ultrasonic::getEchoPulseUs() {
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    return pulseIn(_echoPin, HIGH, ULTRASONIC_TIMEOUT_US);
}

// ==========================================================================
// 脉宽转距离
// ==========================================================================
float Ultrasonic::pulseToDistance(unsigned long durationUs) {
    // 距离 = 声速 × 时间 / 2 (往返)
    return (float)durationUs * ULTRASONIC_SPEED_OF_SOUND / 2.0f;
}

// ==========================================================================
// 冒泡排序
// ==========================================================================
void Ultrasonic::sortArray(float arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}
