#ifndef DRONE_CONFIG_H
#define DRONE_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

struct DroneConfig {
    char wifiSsid[33];
    char wifiPass[65];

    int pinMotorFL;
    int pinMotorFR;
    int pinMotorBL;
    int pinMotorBR;
    int pwmFrequency;
    uint8_t pwmResolutionBits;

    int wireSda;
    int wireScl;

    float madgwickBeta;
    float maxTiltDeg;
    float maxYawRateDps;

    float kpRoll;
    float kiRoll;
    float kdRoll;
    float kpPitch;
    float kiPitch;
    float kdPitch;
    float kpYaw;
    float kiYaw;
    float kdYaw;
    bool pidEnabled;

    float idleThrPct;
    float minThrottlePct;
    float maxThrottlePct;
    float armThrottlePct;
    float maxArmAngleDeg;

    int yawMixSign;
    uint16_t failsafeTimeoutMs;
    uint16_t failsafeDisarmMs;
};

static const int MOTOR_FL_INDEX = 0;
static const int MOTOR_FR_INDEX = 1;
static const int MOTOR_BL_INDEX = 2;
static const int MOTOR_BR_INDEX = 3;

static const float MAX_IDLE_THR_PCT = 40.0f;
static const float ITERM_LIMIT = 120.0f;
static const float MAX_ATTITUDE_PID_PWM = 120.0f;
static const float MAX_YAW_PID_PWM = 80.0f;
static const unsigned long CONTROL_PERIOD_US = 4000;

void setDefaultConfig(DroneConfig& cfg);
void loadSettingsFromNvs(DroneConfig& cfg);
String buildConfigJson(const DroneConfig& cfg);
uint32_t pwmMaxValue(const DroneConfig& cfg);
int clampPin(int v);

#endif
