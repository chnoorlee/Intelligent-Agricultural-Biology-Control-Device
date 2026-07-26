/**
 * @file imu_sensor.cpp
 * @brief IMU attitude estimation implementation
 *
 * Uses Adafruit MPU6050 library for sensor I/O and MadgwickAHRS for
 * sensor fusion. Calibration routines compute gyro bias by averaging
 * stationary samples and accelerometer by 6-point measurement.
 */

#include "imu_sensor.h"

// ============================================================================
// Constructor
// ============================================================================

IMUSensor::IMUSensor()
    : _wire(nullptr)
    , _initialized(false)
    , _last_update_us(0)
    , _yaw_offset(0.0f)
{
    memset(&_raw, 0, sizeof(_raw));
    memset(&_attitude, 0, sizeof(_attitude));
    memset(_gyro_offset, 0, sizeof(_gyro_offset));
    memset(_accel_offset, 0, sizeof(_accel_offset));
}

// ============================================================================
// Initialization
// ============================================================================

bool IMUSensor::begin(TwoWire &wire, const IMUConfig &config) {
    _wire = &wire;
    _config = config;

    // Initialize MPU6050
    if (!_mpu.begin(config.i2c_address, wire)) {
        return false;
    }

    // Configure sensor ranges
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Configure Madgwick filter
    _madgwick.begin(config.filter_rate_hz);
    _madgwick.setBeta(config.beta);

    // Start with identity quaternion
    _attitude.quat_w = 1.0f;
    _attitude.quat_x = 0.0f;
    _attitude.quat_y = 0.0f;
    _attitude.quat_z = 0.0f;

    // Load offsets from config
    _gyro_offset[0] = degToRad(config.gyro_offset_x);
    _gyro_offset[1] = degToRad(config.gyro_offset_y);
    _gyro_offset[2] = degToRad(config.gyro_offset_z);

    _accel_offset[0] = config.accel_offset_x * 9.80665f;  // g to m/s^2
    _accel_offset[1] = config.accel_offset_y * 9.80665f;
    _accel_offset[2] = config.accel_offset_z * 9.80665f;

    _initialized = true;
    _last_update_us = micros();

    return true;
}

// ============================================================================
// Update
// ============================================================================

bool IMUSensor::update() {
    if (!_initialized) return false;

    // Read raw sensor data
    if (!readSensor()) return false;

    // Compute time delta
    uint32_t now = micros();
    float dt = (now - _last_update_us) / 1000000.0f;
    _last_update_us = now;

    // Clamp dt to prevent filter blowup
    if (dt > 0.05f) dt = 0.01f;  // Max 50ms
    if (dt <= 0) dt = 0.01f;

    // Run Madgwick filter update
    runFilter(dt);

    // Convert to Euler angles
    quatToEuler();

    // Compute acceleration magnitude (for vibration/hit detection)
    _attitude.accel_mag = sqrt(
        _raw.accel_x * _raw.accel_x +
        _raw.accel_y * _raw.accel_y +
        _raw.accel_z * _raw.accel_z
    ) / 9.80665f;

    _attitude.valid = true;
    return true;
}

// ============================================================================
// Sensor Reading
// ============================================================================

bool IMUSensor::readSensor() {
    sensors_event_t a, g, temp;
    if (!_mpu.getEvent(&a, &g, &temp)) {
        return false;
    }

    // Convert to m/s^2 and rad/s, apply calibration offsets
    _raw.accel_x = a.acceleration.x - _accel_offset[0];
    _raw.accel_y = a.acceleration.y - _accel_offset[1];
    _raw.accel_z = a.acceleration.z - _accel_offset[2];

    _raw.gyro_x = g.gyro.x - _gyro_offset[0];
    _raw.gyro_y = g.gyro.y - _gyro_offset[1];
    _raw.gyro_z = g.gyro.z - _gyro_offset[2];

    _raw.temperature = temp.temperature;
    _raw.timestamp_us = micros();

    return true;
}

// ============================================================================
// Madgwick Filter
// ============================================================================

void IMUSensor::runFilter(float dt) {
    // Madgwick filter expects:
    // - Gyro in rad/s
    // - Accel in any unit (normalized internally)
    // - Mag in any unit (normalized internally)

    _madgwick.update(
        _raw.gyro_x, _raw.gyro_y, _raw.gyro_z,
        _raw.accel_x, _raw.accel_y, _raw.accel_z
    );

    // Extract quaternion
    _attitude.quat_w = _madgwick.getQuaternionW();
    _attitude.quat_x = _madgwick.getQuaternionX();
    _attitude.quat_y = _madgwick.getQuaternionY();
    _attitude.quat_z = _madgwick.getQuaternionZ();
}

// ============================================================================
// Quaternion to Euler Conversion
// ============================================================================

void IMUSensor::quatToEuler() {
    float qw = _attitude.quat_w;
    float qx = _attitude.quat_x;
    float qy = _attitude.quat_y;
    float qz = _attitude.quat_z;

    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (qw * qx + qy * qz);
    float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
    _attitude.roll = atan2(sinr_cosp, cosr_cosp) * 57.2957795131f;

    // Pitch (y-axis rotation)
    float sinp = 2.0f * (qw * qy - qz * qx);
    if (fabs(sinp) >= 1.0f) {
        _attitude.pitch = copysign(90.0f, sinp);
    } else {
        _attitude.pitch = asin(sinp) * 57.2957795131f;
    }

    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (qw * qz + qx * qy);
    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
    _attitude.yaw = atan2(siny_cosp, cosy_cosp) * 57.2957795131f;

    // Apply yaw offset correction
    _attitude.yaw -= _yaw_offset * 57.2957795131f;

    // Normalize yaw to 0-360
    while (_attitude.yaw < 0) _attitude.yaw += 360.0f;
    while (_attitude.yaw >= 360.0f) _attitude.yaw -= 360.0f;
}

// ============================================================================
// Calibration
// ============================================================================

void IMUSensor::calibrateGyro(uint16_t samples) {
    if (!_initialized) return;

    float sum_x = 0, sum_y = 0, sum_z = 0;

    // Discard first few samples
    for (uint8_t i = 0; i < 10; i++) {
        sensors_event_t a, g, temp;
        _mpu.getEvent(&a, &g, &temp);
        delay(2);
    }

    // Average stationary samples
    for (uint16_t i = 0; i < samples; i++) {
        sensors_event_t a, g, temp;
        if (_mpu.getEvent(&a, &g, &temp)) {
            sum_x += g.gyro.x;
            sum_y += g.gyro.y;
            sum_z += g.gyro.z;
        }
        delay(1);
    }

    _gyro_offset[0] = sum_x / samples;
    _gyro_offset[1] = sum_y / samples;
    _gyro_offset[2] = sum_z / samples;

    _config.gyro_offset_x = radToDeg(_gyro_offset[0]);
    _config.gyro_offset_y = radToDeg(_gyro_offset[1]);
    _config.gyro_offset_z = radToDeg(_gyro_offset[2]);
}

void IMUSensor::calibrateAccel() {
    // 6-point calibration - simplified version
    // In production, you'd guide the user through orienting the sensor
    // at each of 6 positions and measure the readings.
    // For this implementation, we use a simple level-plane calibration.

    if (!_initialized) return;

    float sum_x = 0, sum_y = 0, sum_z = 0;
    const uint16_t samples = 200;

    for (uint16_t i = 0; i < samples; i++) {
        sensors_event_t a, g, temp;
        if (_mpu.getEvent(&a, &g, &temp)) {
            sum_x += a.acceleration.x;
            sum_y += a.acceleration.y;
            sum_z += a.acceleration.z;
        }
        delay(5);
    }

    // Expected: [0, 0, 9.81] when level
    _accel_offset[0] = sum_x / samples;
    _accel_offset[1] = sum_y / samples;
    _accel_offset[2] = (sum_z / samples) - 9.80665f;

    _config.accel_offset_x = _accel_offset[0] / 9.80665f;
    _config.accel_offset_y = _accel_offset[1] / 9.80665f;
    _config.accel_offset_z = _accel_offset[2] / 9.80665f;
}

// ============================================================================
// Data Accessors
// ============================================================================

float IMUSensor::getRollRate() const {
    return radToDeg(_raw.gyro_x);
}

float IMUSensor::getPitchRate() const {
    return radToDeg(_raw.gyro_y);
}

float IMUSensor::getYawRate() const {
    return radToDeg(_raw.gyro_z);
}

void IMUSensor::getAngularRates(float &rx, float &ry, float &rz) const {
    rx = radToDeg(_raw.gyro_x);
    ry = radToDeg(_raw.gyro_y);
    rz = radToDeg(_raw.gyro_z);
}

void IMUSensor::getAcceleration(float &ax, float &ay, float &az) const {
    ax = _raw.accel_x;
    ay = _raw.accel_y;
    az = _raw.accel_z;
}

bool IMUSensor::isHealthy() const {
    if (!_initialized) return false;

    // Check if data is updating
    uint32_t age = micros() - _last_update_us;
    if (age > 500000) return false;  // No update in 500ms

    // Check for NaN values
    if (isnan(_attitude.roll) || isnan(_attitude.pitch) || isnan(_attitude.yaw)) {
        return false;
    }

    // Check acceleration magnitude sanity (should be ~1g at rest)
    float accel_mag = _attitude.accel_mag;
    if (accel_mag > 4.0f || accel_mag < 0.5f) {
        return false;  // Abnormal acceleration
    }

    return true;
}

void IMUSensor::resetYaw(float heading) {
    _yaw_offset = _attitude.yaw * 0.01745329252f - heading * 0.01745329252f;
}

void IMUSensor::setFilterGain(float beta) {
    _config.beta = beta;
    _madgwick.setBeta(beta);
}

void IMUSensor::setAccelRange(mpu6050_accel_range_t range) {
    _mpu.setAccelerometerRange(range);
}

void IMUSensor::setGyroRange(mpu6050_gyro_range_t range) {
    _mpu.setGyroRange(range);
}
