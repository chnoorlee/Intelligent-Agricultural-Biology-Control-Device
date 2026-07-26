#ifndef DRONE_WEB_API_H
#define DRONE_WEB_API_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "Config.h"
#include "FlightController.h"
#include "HTML.h"
#include "ImuMpu6050.h"
#include "Motors.h"
#include "Safety.h"

void initWebApi(WebServer& server, WebSocketsServer& ws, DroneConfig& cfg, ImuState& imu, ControlInput& input, SafetyState& safety, MotorState& motors, FlightState& flight);
void sendTelemetry(WebSocketsServer& ws, const DroneConfig& cfg, const ImuState& imu, const ControlInput& input, const SafetyState& safety, const MotorState& motors, const FlightState& flight);

#endif
