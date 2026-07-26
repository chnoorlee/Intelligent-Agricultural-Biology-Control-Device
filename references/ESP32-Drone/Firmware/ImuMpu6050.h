#ifndef IMU_MPU6050_H
#define IMU_MPU6050_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

struct ImuState {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float roll;
    float pitch;
    float yaw;
    bool calibrated;
    unsigned long lastUpdateUs;
};

void initIMU(const DroneConfig& cfg);
bool calibrateIMU(ImuState& imu);
bool readIMU(ImuState& imu, const DroneConfig& cfg, float dt);

#endif
