/**
 * @file gimbal_control.h
 * @brief 双轴云台舵机控制 — PCA9685 PWM 驱动
 *
 * 功能:
 *   - 通过 I2C 控制 PCA9685 16通道 PWM 生成器
 *   - 水平 (Pan) 轴: 0° ~ 180°, 通道 0
 *   - 俯仰 (Tilt) 轴: 0° ~ 180°, 通道 1
 *   - 支持平滑运动和限位保护
 */

#ifndef GIMBAL_CONTROL_H
#define GIMBAL_CONTROL_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

// ==========================================================================
// 舵机参数
// ==========================================================================
#define SERVO_MIN_PULSE     150     // 最小脉冲宽度 (对应 0°)
#define SERVO_MAX_PULSE     600     // 最大脉冲宽度 (对应 180°)
#define SERVO_FREQUENCY     50      // PWM频率 50Hz (标准舵机)

#define PAN_CHANNEL         0       // 水平舵机通道
#define TILT_CHANNEL        1       // 俯仰舵机通道

#define PAN_MIN_ANGLE       0.0f    // 水平最小角度
#define PAN_MAX_ANGLE       180.0f  // 水平最大角度
#define TILT_MIN_ANGLE      30.0f   // 俯仰最小角度 (避免指向地面)
#define TILT_MAX_ANGLE      150.0f  // 俯仰最大角度

#define CENTER_PAN_ANGLE    90.0f   // 水平归中角度
#define CENTER_TILT_ANGLE   90.0f   // 俯仰归中角度

#define DEFAULT_MOVE_SPEED  2.0f    // 默认运动速度 (度/步)
#define MAX_MOVE_SPEED      5.0f    // 最大运动速度

// ==========================================================================
// GimbalControl 类
// ==========================================================================
class GimbalControl {
public:
    GimbalControl();
    ~GimbalControl();

    /**
     * @brief 初始化PCA9685驱动板
     * @param i2cAddr PCA9685 I2C地址 (默认0x40)
     * @return true 初始化成功, false 失败
     */
    bool begin(uint8_t i2cAddr = 0x40);

    /**
     * @brief 设置水平(Pan)目标角度
     * @param angle 目标角度 (0-180)
     */
    void setPanAngle(float angle);

    /**
     * @brief 设置俯仰(Tilt)目标角度
     * @param angle 目标角度 (30-150, 限位保护)
     */
    void setTiltAngle(float angle);

    /**
     * @brief 同时设置水平和俯仰角度
     */
    void setAngles(float pan, float tilt);

    /**
     * @brief 云台归中 (pan=90, tilt=90)
     */
    void center();

    /**
     * @brief 获取当前水平角度
     */
    float getPanAngle() const { return _currentPan; }

    /**
     * @brief 获取当前俯仰角度
     */
    float getTiltAngle() const { return _currentTilt; }

    /**
     * @brief 设置运动速度
     */
    void setSpeed(float speed);

    /**
     * @brief 平滑移动到目标角度 (需在loop中调用)
     */
    void update();

    /**
     * @brief 立即跳转到目标角度 (不平滑)
     */
    void jumpToTarget();

    /**
     * @brief 检查是否达到目标位置
     */
    bool isAtTarget() const;

    /**
     * @brief 云台扫描模式 (巡逻)
     * @param stepSize 每次步进步长
     */
    void scan(float stepSize = 1.0f);

    /**
     * @brief 停止扫描
     */
    void stopScan();

private:
    Adafruit_PWMServoDriver* _pwm;
    bool _initialized;

    float _currentPan;
    float _currentTilt;
    float _targetPan;
    float _targetTilt;
    float _moveSpeed;

    bool _scanning;
    float _scanStep;
    int _scanDirection;  // 1=正向, -1=反向

    /**
     * @brief 将角度转换为PCA9685 PWM脉冲值
     * @param angle 舵机角度 (0-180)
     * @return PWM脉冲值
     */
    uint16_t angleToPulse(float angle);

    /**
     * @brief 角度限位保护
     */
    float clampAngle(float angle, float minAng, float maxAng);
};

#endif // GIMBAL_CONTROL_H
