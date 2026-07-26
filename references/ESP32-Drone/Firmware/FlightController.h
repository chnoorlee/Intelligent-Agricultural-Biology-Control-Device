#ifndef FLIGHT_CONTROLLER_H
#define FLIGHT_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"
#include "ImuMpu6050.h"
#include "Motors.h"
#include "Safety.h"

struct FlightState {
    float iRoll;
    float iPitch;
    float iYaw;
    float loopHz;
    unsigned long lastControlUs;
};

void resetFlightController(FlightState& flight);
void updateFlightControl(const DroneConfig& cfg, const ImuState& imu, ControlInput& input, SafetyState& safety, MotorState& motors, FlightState& flight, float dt);

#endif
