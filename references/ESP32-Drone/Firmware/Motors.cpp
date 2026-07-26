#include "Motors.h"

static const int CH_FL = 0;
static const int CH_FR = 1;
static const int CH_BL = 2;
static const int CH_BR = 3;

static uint32_t clampPwm(const DroneConfig& cfg, uint32_t v) {
    uint32_t maxPwm = pwmMaxValue(cfg);
    return v > maxPwm ? maxPwm : v;
}

void initMotors(const DroneConfig& cfg) {
    ledcSetup(CH_FL, cfg.pwmFrequency, cfg.pwmResolutionBits);
    ledcSetup(CH_FR, cfg.pwmFrequency, cfg.pwmResolutionBits);
    ledcSetup(CH_BL, cfg.pwmFrequency, cfg.pwmResolutionBits);
    ledcSetup(CH_BR, cfg.pwmFrequency, cfg.pwmResolutionBits);

    ledcAttachPin((uint8_t)cfg.pinMotorFL, CH_FL);
    ledcAttachPin((uint8_t)cfg.pinMotorFR, CH_FR);
    ledcAttachPin((uint8_t)cfg.pinMotorBL, CH_BL);
    ledcAttachPin((uint8_t)cfg.pinMotorBR, CH_BR);
}

void writeMotorsRaw(const DroneConfig& cfg, MotorState& motors, uint32_t fl, uint32_t fr, uint32_t bl, uint32_t br) {
    motors.value[MOTOR_FL_INDEX] = clampPwm(cfg, fl);
    motors.value[MOTOR_FR_INDEX] = clampPwm(cfg, fr);
    motors.value[MOTOR_BL_INDEX] = clampPwm(cfg, bl);
    motors.value[MOTOR_BR_INDEX] = clampPwm(cfg, br);

    ledcWrite(CH_FL, motors.value[MOTOR_FL_INDEX]);
    ledcWrite(CH_FR, motors.value[MOTOR_FR_INDEX]);
    ledcWrite(CH_BL, motors.value[MOTOR_BL_INDEX]);
    ledcWrite(CH_BR, motors.value[MOTOR_BR_INDEX]);
}

void writeMotors(const DroneConfig& cfg, MotorState& motors, bool armed, uint32_t fl, uint32_t fr, uint32_t bl, uint32_t br) {
    if (!armed) {
        fl = fr = bl = br = 0;
    }
    writeMotorsRaw(cfg, motors, fl, fr, bl, br);
}

void stopMotors(const DroneConfig& cfg, MotorState& motors) {
    motors.motorTestOverride = false;
    writeMotorsRaw(cfg, motors, 0, 0, 0, 0);
}
