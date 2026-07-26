#include "Safety.h"

static void setReason(SafetyState& safety, const char* reason) {
    strncpy(safety.armBlockedReason, reason, sizeof(safety.armBlockedReason) - 1);
    safety.armBlockedReason[sizeof(safety.armBlockedReason) - 1] = 0;
}

void initSafety(SafetyState& safety) {
    safety.armed = false;
    safety.failsafeActive = false;
    safety.failsafeStartMs = 0;
    safety.lastMissTickMs = 0;
    setReason(safety, "disarmed");
}

void resetControlInput(ControlInput& input) {
    input.yaw = 0.0f;
    input.pitch = 0.0f;
    input.roll = 0.0f;
    input.throttle = -1.0f;
    input.lastPacketMs = 0;
    input.packetCount = 0;
    input.packetGapMisses = 0;
}

float throttlePercent(const ControlInput& input) {
    return constrain((input.throttle + 1.0f) * 50.0f, 0.0f, 100.0f);
}

bool tryArm(SafetyState& safety, const DroneConfig& cfg, const ImuState& imu, const ControlInput& input) {
    if (input.lastPacketMs == 0 || linkAgeMs(input) > cfg.failsafeTimeoutMs) {
        setReason(safety, "no control link");
        safety.armed = false;
        return false;
    }
    if (!imu.calibrated) {
        setReason(safety, "calibrate imu");
        safety.armed = false;
        return false;
    }
    if (fabsf(imu.roll) > cfg.maxArmAngleDeg || fabsf(imu.pitch) > cfg.maxArmAngleDeg) {
        setReason(safety, "not level");
        safety.armed = false;
        return false;
    }
    if (throttlePercent(input) > cfg.armThrottlePct) {
        setReason(safety, "lower throttle");
        safety.armed = false;
        return false;
    }

    safety.armed = true;
    safety.failsafeActive = false;
    safety.failsafeStartMs = 0;
    setReason(safety, "armed");
    return true;
}

void disarm(SafetyState& safety) {
    safety.armed = false;
    safety.failsafeActive = false;
    safety.failsafeStartMs = 0;
    setReason(safety, "disarmed");
}

void emergencyStop(SafetyState& safety, ControlInput& input) {
    resetControlInput(input);
    safety.armed = false;
    safety.failsafeActive = false;
    safety.failsafeStartMs = 0;
    setReason(safety, "estop");
}

unsigned long linkAgeMs(const ControlInput& input) {
    if (input.lastPacketMs == 0) return 0xFFFFFFFFUL;
    return millis() - input.lastPacketMs;
}

void updateFailsafe(SafetyState& safety, const DroneConfig& cfg, ControlInput& input) {
    if (!safety.armed) {
        safety.failsafeActive = false;
        return;
    }

    unsigned long age = linkAgeMs(input);
    if (age <= cfg.failsafeTimeoutMs) {
        if (safety.failsafeActive) setReason(safety, "armed");
        safety.failsafeActive = false;
        safety.failsafeStartMs = 0;
        return;
    }

    if (millis() - safety.lastMissTickMs > 200) {
        input.packetGapMisses++;
        safety.lastMissTickMs = millis();
    }

    input.yaw = 0.0f;
    input.roll = 0.0f;
    input.pitch = 0.0f;
    input.throttle = -1.0f;

    if (!safety.failsafeActive) {
        safety.failsafeActive = true;
        safety.failsafeStartMs = millis();
        setReason(safety, "link failsafe");
    }

    if (millis() - safety.failsafeStartMs >= cfg.failsafeDisarmMs) {
        safety.armed = false;
        safety.failsafeActive = false;
        setReason(safety, "failsafe disarm");
    }
}
