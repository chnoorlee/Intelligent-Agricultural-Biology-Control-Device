/**
 * @file flight_controller.h
 * @brief Fixed-wing UAV flight controller - PID attitude control, mode switching
 *
 * Implements cascaded PID control loops for fixed-wing aircraft:
 *   - Outer loop: Navigation (position/heading) -> desired attitude
 *   - Inner loop: Attitude (roll/pitch/yaw) -> servo/PWM outputs
 * Supports Auto (waypoint) and Manual (RC) flight modes.
 */

#ifndef FLIGHT_CONTROLLER_H
#define FLIGHT_CONTROLLER_H

#include <Arduino.h>

// ============================================================================
// PID Controller Structure
// ============================================================================

/**
 * @brief PID controller with integral anti-windup and derivative filtering
 */
struct PIDController {
    float kp;           // Proportional gain
    float ki;           // Integral gain
    float kd;           // Derivative gain
    float integral;     // Accumulated integral term
    float prev_error;   // Previous error for derivative calculation
    float prev_meas;    // Previous measurement (for derivative-on-measurement)
    float out_min;      // Output saturation minimum
    float out_max;      // Output saturation maximum
    float integrator_min;  // Integrator anti-windup min
    float integrator_max;  // Integrator anti-windup max
    float tau;          // Derivative low-pass filter time constant (seconds)
    float deriv_filtered;  // Filtered derivative term
    float dt;           // Sample time (seconds)

    /**
     * @brief Compute PID output
     * @param setpoint Desired value
     * @param measurement Current measured value
     * @return Control output
     */
    float compute(float setpoint, float measurement);

    /**
     * @brief Reset integral and filter states
     */
    void reset();

    /**
     * @brief Configure PID gains
     */
    void setGains(float p, float i, float d, float min_out, float max_out);
};

// ============================================================================
// Flight Modes
// ============================================================================

enum class FlightMode : uint8_t {
    MANUAL      = 0,  // Direct RC passthrough
    STABILIZE   = 1,  // Attitude stabilization (rate mode)
    AUTO        = 2,  // Full autonomous waypoint navigation
    RTL         = 3,  // Return to Launch
    LOITER      = 4,  // Circle around current position
    LAND        = 5,  // Auto-landing sequence
    FAILSAFE    = 6   // Failsafe triggered
};

// ============================================================================
// Vehicle State
// ============================================================================

/**
 * @brief 3D vector (float)
 */
struct Vector3f {
    float x, y, z;
    Vector3f() : x(0), y(0), z(0) {}
    Vector3f(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

/**
 * @brief Attitude in Euler angles (degrees)
 */
struct Attitude {
    float roll;     // Roll angle (deg), positive right-wing-down
    float pitch;    // Pitch angle (deg), positive nose-up
    float yaw;      // Yaw angle (deg), 0-360 from true North
};

/**
 * @brief Full vehicle state structure
 */
struct VehicleState {
    Attitude attitude;          // Current attitude (deg)
    Attitude attitude_target;   // Desired attitude (deg)
    Vector3f angular_rate;      // Angular rates (deg/s)
    Vector3f angular_rate_target; // Target angular rates (deg/s)

    Vector3f position;          // GPS position (lat, lon, alt) - degrees/meters
    Vector3f velocity;          // Ground velocity (N, E, D) - m/s
    float airspeed;             // Airspeed (m/s)
    float ground_speed;         // Ground speed (m/s)
    float heading;              // GPS heading (deg)
    float altitude;             // Barometric altitude (m)
    float altitude_target;      // Target altitude (m)

    float battery_voltage;      // Battery voltage (V)
    float battery_current;      // Current draw (A)
    uint8_t battery_percent;    // Remaining battery (%)

    uint32_t gps_fix;           // GPS fix type (0=none, 2=2D, 3=3D)
    uint8_t satellites;         // Number of satellites

    uint32_t last_rx_time;      // Last RC receiver update (ms)
    FlightMode mode;            // Current flight mode
    FlightMode prev_mode;       // Previous flight mode
    bool armed;                 // Motors armed flag
};

// ============================================================================
// Flight Controller Class
// ============================================================================

class FlightController {
public:
    FlightController();
    ~FlightController() = default;

    /**
     * @brief Initialize the flight controller
     */
    void begin();

    /**
     * @brief Main control loop - call at 100Hz
     * @param dt Time delta since last call (seconds)
     */
    void update(float dt);

    /**
     * @brief Set flight mode
     * @param mode New flight mode
     */
    void setMode(FlightMode mode);

    /**
     * @brief Get current flight mode
     */
    FlightMode getMode() const { return _state.mode; }

    /**
     * @brief Get vehicle state (const ref)
     */
    const VehicleState& getState() const { return _state; }

    /**
     * @brief Set RC input channels (from receiver)
     * @param channels Array of 8 channel values [1000-2000]
     */
    void setRCInput(const uint16_t channels[8]);

    /**
     * @brief Update navigation target
     * @param lat Target latitude (degrees)
     * @param lon Target longitude (degrees)
     * @param alt Target altitude (m)
     */
    void setNavTarget(float lat, float lon, float alt);

    /**
     * @brief Arm/disarm motors
     */
    void arm();
    void disarm();

    /**
     * @brief Get computed servo outputs
     * @param outputs Array of 8 channel values [1000-2000]
     */
    void getServoOutputs(uint16_t outputs[8]) const;

    /**
     * @brief Check if vehicle is in a failsafe condition
     */
    bool isFailsafe() const;

private:
    VehicleState _state;                // Vehicle state
    PIDController _roll_rate_pid;      // Roll rate PID
    PIDController _pitch_rate_pid;     // Pitch rate PID
    PIDController _yaw_rate_pid;       // Yaw rate PID
    PIDController _roll_angle_pid;     // Roll angle PID (outer loop)
    PIDController _pitch_angle_pid;    // Pitch angle PID (outer loop)
    PIDController _heading_pid;        // Heading PID (navigation outer)
    PIDController _altitude_pid;       // Altitude PID
    PIDController _airspeed_pid;       // Airspeed/throttle PID

    uint16_t _rc_channels[8];          // Raw RC inputs
    uint16_t _servo_outputs[8];        // Computed servo outputs
    Vector3f _nav_target;              // Navigation waypoint target
    float _nav_target_heading;         // Computed heading to waypoint

    uint32_t _last_update_ms;          // Last update timestamp
    bool _initialised;

    /**
     * @brief Update attitude controller (inner loop)
     * @param dt Time delta (seconds)
     */
    void updateAttitudeControl(float dt);

    /**
     * @brief Update navigation controller (outer loop)
     * @param dt Time delta (seconds)
     */
    void updateNavigationControl(float dt);

    /**
     * @brief Perform control mixing - convert PID outputs to servo channels
     */
    void controlMixer();

    /**
     * @brief RC override in manual mode
     */
    void manualPassthrough();

    /**
     * @brief Compute heading to waypoint
     * @return Bearing in degrees (0-360)
     */
    float computeBearingToTarget();

    /**
     * @brief Compute distance to waypoint
     * @return Distance in meters
     */
    float computeDistanceToTarget();

    /**
     * @brief Apply servo limits and safety checks
     */
    void applyOutputLimits();
};

#endif // FLIGHT_CONTROLLER_H
