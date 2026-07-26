#ifndef DRONE_MOTORS_H
#define DRONE_MOTORS_H

#include <Arduino.h>
#include "Config.h"

struct MotorState {
    uint32_t value[4];
    bool motorTestOverride;
    unsigned long lastMotorTestMs;
};

void initMotors(const DroneConfig& cfg);
void writeMotors(const DroneConfig& cfg, MotorState& motors, bool armed, uint32_t fl, uint32_t fr, uint32_t bl, uint32_t br);
void writeMotorsRaw(const DroneConfig& cfg, MotorState& motors, uint32_t fl, uint32_t fr, uint32_t bl, uint32_t br);
void stopMotors(const DroneConfig& cfg, MotorState& motors);

#endif
