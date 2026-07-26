#ifndef DRONE_SAFETY_H
#define DRONE_SAFETY_H

#include <Arduino.h>
#include "Config.h"
#include "ImuMpu6050.h"

struct ControlInput {
    float yaw;
    float pitch;
    float roll;
    float throttle;
    unsigned long lastPacketMs;
    unsigned long packetCount;
    unsigned long packetGapMisses;
};

struct SafetyState {
    bool armed;
    bool failsafeActive;
    char armBlockedReason[40];
    unsigned long failsafeStartMs;
    unsigned long lastMissTickMs;
};

void initSafety(SafetyState& safety);
void resetControlInput(ControlInput& input);
float throttlePercent(const ControlInput& input);
bool tryArm(SafetyState& safety, const DroneConfig& cfg, const ImuState& imu, const ControlInput& input);
void disarm(SafetyState& safety);
void emergencyStop(SafetyState& safety, ControlInput& input);
void updateFailsafe(SafetyState& safety, const DroneConfig& cfg, ControlInput& input);
unsigned long linkAgeMs(const ControlInput& input);

#endif
