#include "FlightController.h"

static const unsigned long MOTOR_TEST_HOLD_MS = 800;
static const float STICK_ROLL_PWM = 70.0f;
static const float STICK_PITCH_PWM = 70.0f;

static uint32_t controlToPwm(const DroneConfig& cfg, float value) {
    return (uint32_t)constrain((int)roundf(value), 0, (int)pwmMaxValue(cfg));
}

void resetFlightController(FlightState& flight) {
    flight.iRoll = 0.0f;
    flight.iPitch = 0.0f;
    flight.iYaw = 0.0f;
    flight.loopHz = 0.0f;
    flight.lastControlUs = 0;
}

void updateFlightControl(const DroneConfig& cfg, const ImuState& imu, ControlInput& input, SafetyState& safety, MotorState& motors, FlightState& flight, float dt) {
    if (!safety.armed && motors.motorTestOverride && (millis() - motors.lastMotorTestMs < MOTOR_TEST_HOLD_MS)) {
        writeMotorsRaw(cfg, motors, motors.value[MOTOR_FL_INDEX], motors.value[MOTOR_FR_INDEX], motors.value[MOTOR_BL_INDEX], motors.value[MOTOR_BR_INDEX]);
        return;
    }
    motors.motorTestOverride = false;

    updateFailsafe(safety, cfg, input);

    if (!safety.armed) {
        flight.iRoll = flight.iPitch = flight.iYaw = 0.0f;
        writeMotors(cfg, motors, false, 0, 0, 0, 0);
        return;
    }

    const float maxPwm = (float)pwmMaxValue(cfg);
    float throttlePctValue = throttlePercent(input);
    throttlePctValue = constrain(throttlePctValue, cfg.minThrottlePct, cfg.maxThrottlePct);
    float throttle = throttlePctValue * maxPwm / 100.0f;
    float idleFloor = cfg.idleThrPct * maxPwm / 100.0f;
    if (idleFloor > throttle) throttle = idleFloor;

    float scale = maxPwm / 255.0f;
    float stickRollMix = input.roll * STICK_ROLL_PWM * scale;
    float stickPitchMix = input.pitch * STICK_PITCH_PWM * scale;

    float rollOut = 0.0f, pitchOut = 0.0f, yawOut = 0.0f;
    if (cfg.pidEnabled) {
        float targetRoll = input.roll * cfg.maxTiltDeg;
        float targetPitch = input.pitch * cfg.maxTiltDeg;
        float targetYawRate = input.yaw * cfg.maxYawRateDps;

        float errRoll = targetRoll - imu.roll;
        float errPitch = targetPitch - imu.pitch;
        float errYaw = targetYawRate - imu.gz;

        flight.iRoll = constrain(flight.iRoll + errRoll * dt, -ITERM_LIMIT, ITERM_LIMIT);
        flight.iPitch = constrain(flight.iPitch + errPitch * dt, -ITERM_LIMIT, ITERM_LIMIT);
        flight.iYaw = constrain(flight.iYaw + errYaw * dt, -ITERM_LIMIT, ITERM_LIMIT);

        rollOut = (cfg.kpRoll * errRoll) + (cfg.kiRoll * flight.iRoll) - (cfg.kdRoll * imu.gx);
        pitchOut = (cfg.kpPitch * errPitch) + (cfg.kiPitch * flight.iPitch) - (cfg.kdPitch * imu.gy);
        yawOut = (cfg.kpYaw * errYaw) + (cfg.kiYaw * flight.iYaw) - (cfg.kdYaw * imu.gz);

        rollOut = constrain(rollOut, -MAX_ATTITUDE_PID_PWM, MAX_ATTITUDE_PID_PWM) * scale;
        pitchOut = constrain(pitchOut, -MAX_ATTITUDE_PID_PWM, MAX_ATTITUDE_PID_PWM) * scale;
        yawOut = constrain(yawOut, -MAX_YAW_PID_PWM, MAX_YAW_PID_PWM) * scale;
    } else {
        flight.iRoll = flight.iPitch = flight.iYaw = 0.0f;
    }

    float yawMix = yawOut * cfg.yawMixSign;
    float fl = throttle + stickPitchMix + stickRollMix + pitchOut + rollOut - yawMix;
    float fr = throttle + stickPitchMix - stickRollMix + pitchOut - rollOut + yawMix;
    float bl = throttle - stickPitchMix + stickRollMix - pitchOut + rollOut + yawMix;
    float br = throttle - stickPitchMix - stickRollMix - pitchOut - rollOut - yawMix;

    writeMotors(cfg, motors, true,
        controlToPwm(cfg, fl),
        controlToPwm(cfg, fr),
        controlToPwm(cfg, bl),
        controlToPwm(cfg, br));
}
