/**
 * @file gimbal_control.cpp
 * @brief 双轴云台舵机控制实现
 */

#include "gimbal_control.h"

// ==========================================================================
// 构造与析构
// ==========================================================================
GimbalControl::GimbalControl()
    : _pwm(nullptr)
    , _initialized(false)
    , _currentPan(CENTER_PAN_ANGLE)
    , _currentTilt(CENTER_TILT_ANGLE)
    , _targetPan(CENTER_PAN_ANGLE)
    , _targetTilt(CENTER_TILT_ANGLE)
    , _moveSpeed(DEFAULT_MOVE_SPEED)
    , _scanning(false)
    , _scanStep(2.0f)
    , _scanDirection(1)
{
}

GimbalControl::~GimbalControl() {
    if (_pwm) {
        delete _pwm;
        _pwm = nullptr;
    }
}

// ==========================================================================
// 初始化
// ==========================================================================
bool GimbalControl::begin(uint8_t i2cAddr) {
    Serial.printf("[Gimbal] 初始化 PCA9685 @ 0x%02X ...\n", i2cAddr);

    _pwm = new Adafruit_PWMServoDriver(i2cAddr);

    if (!_pwm->begin()) {
        Serial.println("[Gimbal] PCA9685 初始化失败!");
        return false;
    }

    _pwm->setPWMFreq(SERVO_FREQUENCY);
    _initialized = true;

    Serial.println("[Gimbal] PCA9685 初始化成功, PWM频率=50Hz");
    Serial.printf("[Gimbal] Pan通道=%d, Tilt通道=%d\n", PAN_CHANNEL, TILT_CHANNEL);

    // 初始归中
    jumpToTarget();

    return true;
}

// ==========================================================================
// 角度转PWM脉冲值
// ==========================================================================
uint16_t GimbalControl::angleToPulse(float angle) {
    // PCA9685 12位分辨率: 0-4095
    // 50Hz → 周期20ms → 每计数约4.88us
    // 标准舵机: 0°→1ms脉冲(约205计数), 180°→2ms脉冲(约410计数)
    float range = SERVO_MAX_PULSE - SERVO_MIN_PULSE;
    float pulse = SERVO_MIN_PULSE + (angle / 180.0f) * range;
    return (uint16_t)pulse;
}

// ==========================================================================
// 角度限位
// ==========================================================================
float GimbalControl::clampAngle(float angle, float minAng, float maxAng) {
    if (angle < minAng) return minAng;
    if (angle > maxAng) return maxAng;
    return angle;
}

// ==========================================================================
// 设置目标角度
// ==========================================================================
void GimbalControl::setPanAngle(float angle) {
    _targetPan = clampAngle(angle, PAN_MIN_ANGLE, PAN_MAX_ANGLE);
}

void GimbalControl::setTiltAngle(float angle) {
    _targetTilt = clampAngle(angle, TILT_MIN_ANGLE, TILT_MAX_ANGLE);
}

void GimbalControl::setAngles(float pan, float tilt) {
    setPanAngle(pan);
    setTiltAngle(tilt);
}

// ==========================================================================
// 归中
// ==========================================================================
void GimbalControl::center() {
    _targetPan = CENTER_PAN_ANGLE;
    _targetTilt = CENTER_TILT_ANGLE;
    _scanning = false;
}

// ==========================================================================
// 设置速度
// ==========================================================================
void GimbalControl::setSpeed(float speed) {
    if (speed < 0.1f) speed = 0.1f;
    if (speed > MAX_MOVE_SPEED) speed = MAX_MOVE_SPEED;
    _moveSpeed = speed;
}

// ==========================================================================
// 平滑移动更新 (在主循环中调用)
// ==========================================================================
void GimbalControl::update() {
    if (!_initialized) return;

    // 扫描模式
    if (_scanning) {
        _currentPan += _scanStep * _scanDirection;
        if (_currentPan >= PAN_MAX_ANGLE) {
            _currentPan = PAN_MAX_ANGLE;
            _scanDirection = -1;
        } else if (_currentPan <= PAN_MIN_ANGLE) {
            _currentPan = PAN_MIN_ANGLE;
            _scanDirection = 1;
        }
        // 输出 PWM
        _pwm->setPWM(PAN_CHANNEL, 0, angleToPulse(_currentPan));
        _pwm->setPWM(TILT_CHANNEL, 0, angleToPulse(_currentTilt));
        return;
    }

    // 平滑移动到目标
    bool moved = false;

    if (fabs(_currentPan - _targetPan) > 0.5f) {
        if (_currentPan < _targetPan) {
            _currentPan += _moveSpeed;
            if (_currentPan > _targetPan) _currentPan = _targetPan;
        } else {
            _currentPan -= _moveSpeed;
            if (_currentPan < _targetPan) _currentPan = _targetPan;
        }
        moved = true;
    }

    if (fabs(_currentTilt - _targetTilt) > 0.5f) {
        if (_currentTilt < _targetTilt) {
            _currentTilt += _moveSpeed;
            if (_currentTilt > _targetTilt) _currentTilt = _targetTilt;
        } else {
            _currentTilt -= _moveSpeed;
            if (_currentTilt < _targetTilt) _currentTilt = _targetTilt;
        }
        moved = true;
    }

    if (moved) {
        _pwm->setPWM(PAN_CHANNEL, 0, angleToPulse(_currentPan));
        _pwm->setPWM(TILT_CHANNEL, 0, angleToPulse(_currentTilt));
    }
}

// ==========================================================================
// 跳转到目标
// ==========================================================================
void GimbalControl::jumpToTarget() {
    if (!_initialized) return;

    _currentPan = _targetPan;
    _currentTilt = _targetTilt;

    _pwm->setPWM(PAN_CHANNEL, 0, angleToPulse(_currentPan));
    _pwm->setPWM(TILT_CHANNEL, 0, angleToPulse(_currentTilt));
}

// ==========================================================================
// 是否到达目标
// ==========================================================================
bool GimbalControl::isAtTarget() const {
    return (fabs(_currentPan - _targetPan) < 0.5f) &&
           (fabs(_currentTilt - _targetTilt) < 0.5f);
}

// ==========================================================================
// 扫描模式
// ==========================================================================
void GimbalControl::scan(float stepSize) {
    _scanning = true;
    _scanStep = stepSize;
    // 保持当前俯仰角度
    _targetTilt = _currentTilt;
}

void GimbalControl::stopScan() {
    _scanning = false;
    _targetPan = _currentPan;
}
