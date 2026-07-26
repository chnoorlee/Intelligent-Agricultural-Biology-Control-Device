/**
 * @file flight_controller.cpp
 * @brief Fixed-wing UAV flight controller implementation
 *
 * Cascaded PID control architecture:
 *   Navigation (position) -> Attitude (roll/pitch/yaw) -> Rate (angular vel) -> Servos
 *
 * Mode switching: AUTO / MANUAL / STABILIZE / RTL / LOITER / LAND / FAILSAFE
 */

#include "flight_controller.h"
#include <math.h>

// Earth's radius for navigation calculations
#define EARTH_RADIUS_M 6371000.0f
#define DEG_TO_RAD    0.01745329252f
#define RAD_TO_DEG    57.2957795131f

// ============================================================================
// PID Controller Implementation
// ============================================================================

void PIDController::setGains(float p, float i, float d, float min_out, float max_out) {
    kp = p;
    ki = i;
    kd = d;
    out_min = min_out;
    out_max = max_out;
    integrator_min = min_out * 0.3f;
    integrator_max = max_out * 0.3f;
}

float PIDController::compute(float setpoint, float measurement) {
    float error = setpoint - measurement;

    // Proportional term
    float p_term = kp * error;

    // Integral term with anti-windup (clamping)
    integral += ki * error * dt;
    if (integral > integrator_max) integral = integrator_max;
    if (integral < integrator_min) integral = integrator_min;

    // Derivative term on measurement (reduces derivative kick)
    float deriv = -(measurement - prev_meas) / dt;

    // Low-pass filter the derivative
    if (tau > 0.0f) {
        float alpha = dt / (tau + dt);
        deriv_filtered = alpha * deriv + (1.0f - alpha) * deriv_filtered;
    } else {
        deriv_filtered = deriv;
    }
    float d_term = kd * deriv_filtered;

    // Compute output
    float output = p_term + integral + d_term;

    // Clamp output
    if (output > out_max) output = out_max;
    if (output < out_min) output = out_min;

    // Store state for next iteration
    prev_error = error;
    prev_meas = measurement;

    return output;
}

void PIDController::reset() {
    integral = 0.0f;
    prev_error = 0.0f;
    prev_meas = 0.0f;
    deriv_filtered = 0.0f;
}

// ============================================================================
// FlightController Implementation
// ============================================================================

FlightController::FlightController()
    : _initialised(false)
    , _last_update_ms(0)
    , _nav_target_heading(0.0f)
{
    // Zero all state
    memset(&_state, 0, sizeof(_state));
    memset(_rc_channels, 0, sizeof(_rc_channels));
    memset(_servo_outputs, 0, sizeof(_servo_outputs));

    _state.mode = FlightMode::MANUAL;
    _state.prev_mode = FlightMode::MANUAL;
    _state.armed = false;

    // Default navigation target (will be overwritten)
    _nav_target = Vector3f(0, 0, 100.0f);
}

void FlightController::begin() {
    // Configure rate PIDs
    _roll_rate_pid.setGains(0.15f, 0.02f, 0.01f, -1.0f, 1.0f);
    _roll_rate_pid.dt = 0.01f;
    _roll_rate_pid.tau = 0.05f;

    _pitch_rate_pid.setGains(0.15f, 0.02f, 0.01f, -1.0f, 1.0f);
    _pitch_rate_pid.dt = 0.01f;
    _pitch_rate_pid.tau = 0.05f;

    _yaw_rate_pid.setGains(0.20f, 0.01f, 0.005f, -1.0f, 1.0f);
    _yaw_rate_pid.dt = 0.01f;
    _yaw_rate_pid.tau = 0.05f;

    // Configure angle PIDs (outer attitude loop)
    _roll_angle_pid.setGains(5.0f, 0.1f, 0.05f, -45.0f, 45.0f);
    _roll_angle_pid.dt = 0.01f;
    _roll_angle_pid.tau = 0.1f;

    _pitch_angle_pid.setGains(4.0f, 0.1f, 0.05f, -30.0f, 30.0f);
    _pitch_angle_pid.dt = 0.01f;
    _pitch_angle_pid.tau = 0.1f;

    // Configure navigation PIDs
    _heading_pid.setGains(3.0f, 0.05f, 0.1f, -30.0f, 30.0f);
    _heading_pid.dt = 0.01f;
    _heading_pid.tau = 0.2f;

    _altitude_pid.setGains(2.0f, 0.1f, 0.5f, -15.0f, 15.0f);
    _altitude_pid.dt = 0.01f;
    _altitude_pid.tau = 0.2f;

    _airspeed_pid.setGains(10.0f, 2.0f, 1.0f, 0.0f, 1.0f);
    _airspeed_pid.dt = 0.01f;
    _airspeed_pid.tau = 0.1f;

    // Set default servo outputs (center/mid)
    // Ch1: Aileron (R), Ch2: Elevator, Ch3: Throttle, Ch4: Rudder
    // Ch5: Aileron (L) - for dual aileron config, Ch6: Flaps, Ch7: Mode, Ch8: Aux
    _servo_outputs[0] = 1500; // Aileron R center
    _servo_outputs[1] = 1500; // Elevator center
    _servo_outputs[2] = 1000; // Throttle min (idle)
    _servo_outputs[3] = 1500; // Rudder center
    _servo_outputs[4] = 1500; // Aileron L center
    _servo_outputs[5] = 1000; // Flaps retracted
    _servo_outputs[6] = 1500; // Mode indicator
    _servo_outputs[7] = 1000; // Aux

    _initialised = true;
}

void FlightController::update(float dt) {
    if (!_initialised) return;

    // Update PID sample times
    _roll_rate_pid.dt = dt;
    _pitch_rate_pid.dt = dt;
    _yaw_rate_pid.dt = dt;
    _roll_angle_pid.dt = dt;
    _pitch_angle_pid.dt = dt;
    _heading_pid.dt = dt;
    _altitude_pid.dt = dt;
    _airspeed_pid.dt = dt;

    switch (_state.mode) {
        case FlightMode::MANUAL:
            manualPassthrough();
            break;

        case FlightMode::STABILIZE:
            // Angle stabilization with RC input mixing
            updateAttitudeControl(dt);
            controlMixer();
            break;

        case FlightMode::AUTO:
            updateNavigationControl(dt);
            updateAttitudeControl(dt);
            controlMixer();
            break;

        case FlightMode::RTL:
            // Navigate to home position
            updateNavigationControl(dt);
            updateAttitudeControl(dt);
            controlMixer();
            break;

        case FlightMode::LOITER:
            // Circle at current position
            _state.attitude_target.roll = constrainAngle(
                _state.attitude.roll + 15.0f * dt
            );
            _state.altitude_target = _state.altitude;
            updateAttitudeControl(dt);
            controlMixer();
            break;

        case FlightMode::LAND:
            // Descend at controlled rate
            _state.altitude_target = constrain(
                _state.altitude - 2.0f * dt,
                0.0f, 500.0f
            );
            _state.pitch_angle_pid.setGains(2.0f, 0.1f, 0.3f, -10.0f, 10.0f);
            updateNavigationControl(dt);
            updateAttitudeControl(dt);
            controlMixer();
            break;

        case FlightMode::FAILSAFE:
            // Circle and descend to safe altitude
            _state.attitude_target.roll = -30.0f;  // Spiraling descent
            _state.altitude_target = 30.0f;
            updateAttitudeControl(dt);
            controlMixer();
            break;
    }

    applyOutputLimits();

    // Track mode transitions
    _state.prev_mode = _state.mode;
}

// ============================================================================
// Attitude Control (Inner Loop)
// ============================================================================

void FlightController::updateAttitudeControl(float dt) {
    // Compute angle errors
    float roll_error = constrainAngle(
        _state.attitude_target.roll - _state.attitude.roll
    );
    float pitch_error = constrainAngle(
        _state.attitude_target.pitch - _state.attitude.pitch
    );

    // Outer angle loop -> desired angular rate
    float roll_rate_target = _roll_angle_pid.compute(
        _state.attitude_target.roll, _state.attitude.roll
    );
    float pitch_rate_target = _pitch_angle_pid.compute(
        _state.attitude_target.pitch, _state.attitude.pitch
    );

    // For yaw, use heading PID in AUTO mode
    float yaw_rate_target;
    if (_state.mode == FlightMode::AUTO || _state.mode == FlightMode::RTL) {
        _nav_target_heading = computeBearingToTarget();
        float heading_error = constrainAngle(
            _nav_target_heading - _state.attitude.yaw
        );
        yaw_rate_target = _heading_pid.compute(_nav_target_heading, _state.attitude.yaw);
    } else {
        yaw_rate_target = _state.angular_rate_target.z;
    }

    // Store targets
    _state.angular_rate_target.x = roll_rate_target;
    _state.angular_rate_target.y = pitch_rate_target;
    _state.angular_rate_target.z = yaw_rate_target;

    // Inner rate loop -> control surface demands
    _state.angular_rate_target.x = _roll_rate_pid.compute(
        roll_rate_target, _state.angular_rate.x
    );
    _state.angular_rate_target.y = _pitch_rate_pid.compute(
        pitch_rate_target, _state.angular_rate.y
    );
    _state.angular_rate_target.z = _yaw_rate_pid.compute(
        yaw_rate_target, _state.angular_rate.z
    );
}

// ============================================================================
// Navigation Control (Outer Loop)
// ============================================================================

void FlightController::updateNavigationControl(float dt) {
    // Altitude hold using pitch/elevator
    float altitude_error = _state.altitude_target - _state.altitude;
    _state.attitude_target.pitch = _altitude_pid.compute(
        _state.altitude_target, _state.altitude
    );

    // Clamp pitch demand
    _state.attitude_target.pitch = constrain(
        _state.attitude_target.pitch, -25.0f, 25.0f
    );

    // Heading to waypoint
    _nav_target_heading = computeBearingToTarget();
    float heading_error = constrainAngle(
        _nav_target_heading - _state.attitude.yaw
    );
    _state.attitude_target.roll = _heading_pid.compute(
        _nav_target_heading, _state.attitude.yaw
    );

    // Clamp roll demand
    _state.attitude_target.roll = constrain(
        _state.attitude_target.roll, -35.0f, 35.0f
    );

    // Airspeed/throttle control
    float target_airspeed = 18.0f; // Cruise airspeed (m/s)
    float throttle_output = _airspeed_pid.compute(target_airspeed, _state.airspeed);
    // Throttle will be applied in mixer
}

// ============================================================================
// Control Mixer - Fixed-Wing Mixing
// ============================================================================

void FlightController::controlMixer() {
    float roll_cmd  = _state.angular_rate_target.x;  // Normalized -1..1
    float pitch_cmd = _state.angular_rate_target.y;  // Normalized -1..1
    float yaw_cmd   = _state.angular_rate_target.z;  // Normalized -1..1

    // Throttle from manual or auto
    float throttle_cmd = (_state.mode == FlightMode::MANUAL)
        ? (float)(_rc_channels[2] - 1000) / 1000.0f  // RC throttle 0..1
        : constrain(_airspeed_pid.compute(18.0f, _state.airspeed), 0.0f, 1.0f);

    // Fixed-wing mixing matrix
    // Standard airplane layout:
    //   Ch0: Aileron R = roll * 0.5 + pitch * 0.0 (differential aileron)
    //   Ch1: Elevator   = roll * 0.0 + pitch * 1.0
    //   Ch2: Throttle   = throttle
    //   Ch3: Rudder     = roll * 0.1 + yaw * 1.0 (coordinated turn)

    float ail_r =  roll_cmd * 0.5f;
    float elev  = -pitch_cmd * 1.0f;  // Negative: pull up = elevator up
    float rud   =  yaw_cmd * 1.0f + roll_cmd * 0.15f; // Rudder mix for coordinated turn
    float ail_l = -roll_cmd * 0.5f;  // Differential: left goes opposite

    // Convert normalized commands [-1..1] to PWM [1000..2000]
    // Center = 1500, range = +/- 500
    _servo_outputs[0] = (uint16_t)constrain(1500 + ail_r * 500, 1000, 2000);
    _servo_outputs[1] = (uint16_t)constrain(1500 + elev * 500, 1000, 2000);
    _servo_outputs[2] = (uint16_t)constrain(1000 + throttle_cmd * 1000, 1000, 2000);
    _servo_outputs[3] = (uint16_t)constrain(1500 + rud * 500, 1000, 2000);
    _servo_outputs[4] = (uint16_t)constrain(1500 + ail_l * 500, 1000, 2000);

    // Flaps - deployed during landing / loiter
    float flap_demand = 0.0f;
    if (_state.mode == FlightMode::LAND) {
        flap_demand = 0.6f; // 60% flaps for landing
    } else if (_state.mode == FlightMode::LOITER) {
        flap_demand = 0.2f; // 20% flaps for slow flight
    }
    _servo_outputs[5] = (uint16_t)constrain(1000 + flap_demand * 1000, 1000, 2000);
}

// ============================================================================
// Manual Passthrough Mode
// ============================================================================

void FlightController::manualPassthrough() {
    // Direct RC passthrough to servos
    for (int i = 0; i < 6; i++) {
        _servo_outputs[i] = _rc_channels[i];
    }

    // Reset integrators when in manual mode
    _roll_rate_pid.reset();
    _pitch_rate_pid.reset();
    _yaw_rate_pid.reset();
    _roll_angle_pid.reset();
    _pitch_angle_pid.reset();
    _heading_pid.reset();
    _altitude_pid.reset();
    _airspeed_pid.reset();
}

// ============================================================================
// Flight Mode Management
// ============================================================================

void FlightController::setMode(FlightMode mode) {
    if (_state.mode == mode) return;

    _state.prev_mode = _state.mode;
    _state.mode = mode;

    // Reset PIDs on mode transition
    _roll_angle_pid.reset();
    _pitch_angle_pid.reset();
    _heading_pid.reset();
    _altitude_pid.reset();

    // Set servo output for mode indication (Ch7)
    _servo_outputs[6] = 1000 + (uint8_t)mode * 142; // 1000-1994 range
}

void FlightController::arm() {
    if (_state.battery_voltage < 10.5f) {
        // Battery too low to arm - 3S minimum
        return;
    }
    _state.armed = true;
    _servo_outputs[2] = 1000; // Throttle to idle
}

void FlightController::disarm() {
    _state.armed = false;
    _servo_outputs[2] = 1000; // Kill throttle
    // Set all surfaces to neutral
    for (int i = 0; i < 8; i++) {
        if (i != 2) _servo_outputs[i] = 1500;
    }
}

void FlightController::setRCInput(const uint16_t channels[8]) {
    for (int i = 0; i < 8; i++) {
        _rc_channels[i] = constrain(channels[i], 1000U, 2000U);
    }
    _state.last_rx_time = millis();
}

void FlightController::setNavTarget(float lat, float lon, float alt) {
    _nav_target.x = lat;
    _nav_target.y = lon;
    _nav_target.z = alt;
}

void FlightController::getServoOutputs(uint16_t outputs[8]) const {
    for (int i = 0; i < 8; i++) {
        outputs[i] = _servo_outputs[i];
    }
}

bool FlightController::isFailsafe() const {
    return _state.mode == FlightMode::FAILSAFE;
}

// ============================================================================
// Navigation Helpers
// ============================================================================

float FlightController::computeBearingToTarget() {
    float lat1 = _state.position.x * DEG_TO_RAD;
    float lon1 = _state.position.y * DEG_TO_RAD;
    float lat2 = _nav_target.x * DEG_TO_RAD;
    float lon2 = _nav_target.y * DEG_TO_RAD;

    float dlon = lon2 - lon1;

    float y = sin(dlon) * cos(lat2);
    float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);

    float bearing = atan2(y, x) * RAD_TO_DEG;

    // Normalize to 0-360
    if (bearing < 0) bearing += 360.0f;
    return bearing;
}

float FlightController::computeDistanceToTarget() {
    float lat1 = _state.position.x * DEG_TO_RAD;
    float lon1 = _state.position.y * DEG_TO_RAD;
    float lat2 = _nav_target.x * DEG_TO_RAD;
    float lon2 = _nav_target.y * DEG_TO_RAD;

    float dlat = lat2 - lat1;
    float dlon = lon2 - lon1;

    float a = sin(dlat / 2) * sin(dlat / 2) +
              cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS_M * c;
}

// ============================================================================
// Safety & Output Limits
// ============================================================================

void FlightController::applyOutputLimits() {
    // If not armed, kill throttle
    if (!_state.armed) {
        _servo_outputs[2] = 1000;
    }

    // Failsafe throttle cutoff
    if (_state.mode == FlightMode::FAILSAFE) {
        _servo_outputs[2] = 1000;
        // Hold min throttle for glide
        if (_state.altitude < 10.0f) {
            // Near ground - cut completely
            for (int i = 0; i < 8; i++) {
                _servo_outputs[i] = (i == 2) ? 1000 : 1500;
            }
        }
    }

    // All outputs clamped to valid PWM range
    for (int i = 0; i < 8; i++) {
        _servo_outputs[i] = (uint16_t)constrain(
            (int32_t)_servo_outputs[i], 1000L, 2000L
        );
    }
}

// ============================================================================
// Utility: Constrain angle to [-180, 180]
// ============================================================================

extern "C" float constrainAngle(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}
