/**
 * @file imu_sensor.h
 * @brief IMU attitude estimation - MPU6050/MPU9250, Madgwick filter, attitude angles
 *
 * Provides sensor fusion (accelerometer + gyroscope + optional magnetometer)
 * using the Madgwick AHRS algorithm to estimate 3D orientation.
 */

#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MadgwickAHRS.h>

// ============================================================================
// IMU Sensor Types
// ============================================================================

enum class IMUType : uint8_t {
    MPU6050 = 0,   // 6-DOF (accel + gyro)
    MPU9250 = 1,   // 9-DOF (accel + gyro + magnetometer)
    ICM20948 = 2   // 9-DOF (accel + gyro + magnetometer)
};

// ============================================================================
// IMU Configuration
// ============================================================================

struct IMUConfig {
    IMUType type;
    uint8_t i2c_address;        // I2C address (0x68 or 0x69)
    float accel_range_g;        // Accelerometer range (2, 4, 8, 16 g)
    float gyro_range_dps;       // Gyroscope range (250, 500, 1000, 2000 dps)
    uint16_t sample_rate_hz;    // Sample rate (typically 100-200 Hz)
    bool use_magnetometer;      // Enable magnetometer if available

    // Madgwick filter parameters
    float beta;                 // Filter gain (0.01-0.5, higher = faster convergence)
    float filter_rate_hz;       // Filter update rate

    // Calibration offsets
    float gyro_offset_x;        // Gyro bias X (dps)
    float gyro_offset_y;        // Gyro bias Y (dps)
    float gyro_offset_z;        // Gyro bias Z (dps)
    float accel_offset_x;       // Accelerometer bias X (g)
    float accel_offset_y;       // Accelerometer bias Y (g)
    float accel_offset_z;       // Accelerometer bias Z (g)

    // Magnetometer calibration (hard iron)
    float mag_offset_x;
    float mag_offset_y;
    float mag_offset_z;
    float mag_scale_x;
    float mag_scale_y;
    float mag_scale_z;

    IMUConfig()
        : type(IMUType::MPU6050), i2c_address(0x68)
        , accel_range_g(8.0f), gyro_range_dps(1000.0f)
        , sample_rate_hz(100), use_magnetometer(false)
        , beta(0.1f), filter_rate_hz(100.0f)
        , gyro_offset_x(0), gyro_offset_y(0), gyro_offset_z(0)
        , accel_offset_x(0), accel_offset_y(0), accel_offset_z(0)
        , mag_offset_x(0), mag_offset_y(0), mag_offset_z(0)
        , mag_scale_x(1), mag_scale_y(1), mag_scale_z(1)
    {}
};

// ============================================================================
// IMU Data Structures
// ============================================================================

/**
 * @brief Raw sensor readings
 */
struct SensorRaw {
    float accel_x, accel_y, accel_z;   // m/s^2
    float gyro_x, gyro_y, gyro_z;      // rad/s
    float mag_x, mag_y, mag_z;          // uT (microtesla)
    float temperature;                  // deg C
    uint32_t timestamp_us;
};

/**
 * @brief Attitude estimate output
 */
struct AttitudeEstimate {
    float roll;       // Roll angle (deg), positive = right wing down
    float pitch;      // Pitch angle (deg), positive = nose up
    float yaw;        // Yaw angle (deg), 0-360
    float quat_w;     // Quaternion w component
    float quat_x;     // Quaternion x component
    float quat_y;     // Quaternion y component
    float quat_z;     // Quaternion z component
    float accel_mag;  // Total acceleration magnitude (g)
    bool valid;       // Data validity flag
};

// ============================================================================
// IMU Sensor Class
// ============================================================================

class IMUSensor {
public:
    IMUSensor();
    ~IMUSensor() = default;

    /**
     * @brief Initialize IMU sensor
     * @param wire I2C bus reference
     * @param config IMU configuration
     * @return true if initialization successful
     */
    bool begin(TwoWire &wire, const IMUConfig &config);

    /**
     * @brief Update sensor readings and compute attitude
     * @return true if new data was processed
     */
    bool update();

    /**
     * @brief Perform gyroscope bias calibration
     * @param samples Number of samples to average (default 500)
     */
    void calibrateGyro(uint16_t samples = 500);

    /**
     * @brief Perform accelerometer calibration (6-point)
     */
    void calibrateAccel();

    /**
     * @brief Get latest attitude estimate
     */
    AttitudeEstimate getAttitude() const { return _attitude; }

    /**
     * @brief Get raw sensor readings
     */
    SensorRaw getRaw() const { return _raw; }

    /**
     * @brief Get roll angle (degrees)
     */
    float getRoll() const { return _attitude.roll; }

    /**
     * @brief Get pitch angle (degrees)
     */
    float getPitch() const { return _attitude.pitch; }

    /**
     * @brief Get yaw angle (degrees)
     */
    float getYaw() const { return _attitude.yaw; }

    /**
     * @brief Get roll rate (deg/s)
     */
    float getRollRate() const;

    /**
     * @brief Get pitch rate (deg/s)
     */
    float getPitchRate() const;

    /**
     * @brief Get yaw rate (deg/s)
     */
    float getYawRate() const;

    /**
     * @brief Get angular velocity vector
     */
    void getAngularRates(float &rx, float &ry, float &rz) const;

    /**
     * @brief Get acceleration vector
     */
    void getAcceleration(float &ax, float &ay, float &az) const;

    /**
     * @brief Check if sensor is healthy
     */
    bool isHealthy() const;

    /**
     * @brief Get sensor temperature
     */
    float getTemperature() const { return _raw.temperature; }

    /**
     * @brief Reset yaw to zero (or heading reference)
     */
    void resetYaw(float heading = 0.0f);

    /**
     * @brief Set Madgwick filter beta gain
     */
    void setFilterGain(float beta);

    /**
     * @brief Set accelerometer range
     */
    void setAccelRange(mpu6050_accel_range_t range);

    /**
     * @brief Set gyro range
     */
    void setGyroRange(mpu6050_gyro_range_t range);

    /**
     * @brief Get current configuration
     */
    const IMUConfig& getConfig() const { return _config; }

private:
    Adafruit_MPU6050 _mpu;         // Adafruit MPU6050 driver
    Madgwick _madgwick;            // Madgwick AHRS filter
    IMUConfig _config;             // Sensor configuration
    SensorRaw _raw;                // Raw sensor readings
    AttitudeEstimate _attitude;    // Filtered attitude output
    TwoWire *_wire;                // I2C bus
    uint32_t _last_update_us;      // Last update timestamp
    bool _initialized;

    float _gyro_offset[3];         // Gyroscope calibration offsets (rad/s)
    float _accel_offset[3];        // Accelerometer calibration offsets (m/s^2)
    float _yaw_offset;             // Yaw reference offset (rad)

    /**
     * @brief Read raw sensor data from MPU
     */
    bool readSensor();

    /**
     * @brief Run Madgwick filter update
     */
    void runFilter(float dt);

    /**
     * @brief Convert quaternion to Euler angles
     */
    void quatToEuler();

    /**
     * @brief Convert dps to rad/s
     */
    static float degToRad(float deg) { return deg * 0.01745329252f; }

    /**
     * @brief Convert rad to deg
     */
    static float radToDeg(float rad) { return rad * 57.2957795131f; }
};

#endif // IMU_SENSOR_H
