/**
 * @file servo_control.cpp
 * @brief Servo/ESC PWM control implementation
 *
 * Generates servo PWM signals using ESP32 MCPWM or LEDC hardware.
 * Supports multiple mixer types for different airframe configurations.
 */

#include "servo_control.h"
#include <EEPROM.h>

// ============================================================================
// Constructor
// ============================================================================

ServoControl::ServoControl()
    : _mixer_type(MixerType::STANDARD_FIXED_WING)
    , _enabled(false)
    , _refresh_hz(50)
    , _last_servo_update(0)
    , _period_us(PWM_PERIOD_US)
{
    memset(_pins, 0, sizeof(_pins));
    memset(_outputs, 0, sizeof(_outputs));
}

// ============================================================================
// Initialization
// ============================================================================

void ServoControl::begin(const uint8_t pins[8]) {
    for (int i = 0; i < 8; i++) {
        _pins[i] = pins[i];
        pinMode(pins[i], OUTPUT);
        _outputs[i] = PWM_CENTER_US;
    }

    // Throttle defaults to minimum (motor off)
    _outputs[CH_THROTTLE] = PWM_MIN_US;

    // Configure PWM hardware
    setRefreshRate(_refresh_hz);

    // Load calibration if available
    loadCalibration();

    _enabled = true;

    // Write initial neutral positions
    setAllNeutral();
}

// ============================================================================
// Channel Output
// ============================================================================

void ServoControl::writeChannels(const uint16_t values[8]) {
    if (!_enabled) return;

    for (int i = 0; i < 8; i++) {
        // Apply calibration
        uint16_t calibrated = _calibration[i].apply(values[i]);
        _outputs[i] = calibrated;
    }
}

void ServoControl::writeChannel(uint8_t channel, uint16_t pwm) {
    if (channel >= 8 || !_enabled) return;

    uint16_t calibrated = _calibration[channel].apply(pwm);
    _outputs[channel] = calibrated;
}

void ServoControl::setAllNeutral() {
    if (!_enabled) return;
    for (int i = 0; i < 8; i++) {
        if (i == CH_THROTTLE) {
            _outputs[i] = PWM_MIN_US;  // Throttle to idle
        } else {
            _outputs[i] = _calibration[i].center_pwm;
        }
    }
}

void ServoControl::emergencyStop() {
    // Immediately kill throttle and center all surfaces
    _outputs[CH_THROTTLE] = PWM_MIN_US;
    for (int i = 0; i < 8; i++) {
        if (i != CH_THROTTLE) {
            _outputs[i] = PWM_CENTER_US;
        }
    }
    // Write outputs immediately
    for (int i = 0; i < 8; i++) {
        writeServoPWM(_pins[i], _outputs[i]);
    }
    _enabled = false;
}

// ============================================================================
// PWM Generation (ESP32 LEDC)
// ============================================================================

void ServoControl::writeServoPWM(uint8_t pin, uint16_t pulse_us) {
    // Use ESP32 LEDC for hardware PWM on arbitrary pins
    // For Arduino framework on ESP32, we use ledcWrite

    static bool ledc_initialized[8] = {false};
    static uint8_t ledc_channel_map[40] = {255};  // Map pin to LEDC channel

    // Find or allocate LEDC channel for this pin
    uint8_t ch = ledc_channel_map[pin];
    if (ch == 255) {
        // Allocate new channel
        for (uint8_t c = 0; c < 8; c++) {
            bool used = false;
            for (int p = 0; p < 40; p++) {
                if (ledc_channel_map[p] == c) { used = true; break; }
            }
            if (!used) {
                ch = c;
                ledc_channel_map[pin] = ch;
                ledcSetup(ch, _refresh_hz, 16);  // 16-bit resolution
                ledcAttachPin(pin, ch);
                break;
            }
        }
        if (ch == 255) return;  // No free channel
    }

    // Convert microseconds to duty cycle
    // With 50Hz (20ms period), 16-bit resolution:
    //   1000us -> 1000/20000 * 65535 = 3276
    //   1500us -> 1500/20000 * 65535 = 4915
    //   2000us -> 2000/20000 * 65535 = 6553
    uint32_t duty = (uint32_t)pulse_us * 65535 / PWM_PERIOD_US;
    ledcWrite(ch, (uint16_t)duty);
}

void ServoControl::setRefreshRate(uint16_t hz) {
    _refresh_hz = constrain(hz, (uint16_t)50, (uint16_t)400);
    _period_us = 1000000 / _refresh_hz;

    // Reconfigure all LEDC channels
    for (int i = 0; i < 8; i++) {
        // ledcWriteTone would be used here in ESP32 framework
        // For Arduino ESP32 core, LEDC frequency is set at setup
    }
}

// ============================================================================
// Control Mixer Implementations
// ============================================================================

void ServoControl::setMixerType(MixerType type) {
    _mixer_type = type;
}

void ServoControl::applyMixer(float roll, float pitch, float yaw, float throttle) {
    if (!_enabled) return;

    // Clamp inputs
    roll = constrain(roll, -1.0f, 1.0f);
    pitch = constrain(pitch, -1.0f, 1.0f);
    yaw = constrain(yaw, -1.0f, 1.0f);
    throttle = constrain(throttle, 0.0f, 1.0f);

    switch (_mixer_type) {
        case MixerType::STANDARD_FIXED_WING:
            mixerStandard(roll, pitch, yaw, throttle);
            break;
        case MixerType::ELEVON:
            mixerElevon(roll, pitch, yaw, throttle);
            break;
        case MixerType::VTAIL:
            mixerVTail(roll, pitch, yaw, throttle);
            break;
        case MixerType::DUAL_AILERON:
            mixerStandard(roll, pitch, yaw, throttle);
            break;
        case MixerType::FLAPERON:
            mixerFlaperon(roll, pitch, yaw, throttle);
            break;
    }
}

void ServoControl::mixerStandard(float roll, float pitch, float yaw, float throttle) {
    // Standard airplane: Aileron, Elevator, Throttle, Rudder
    // Roll: differential aileron movement
    // Pitch: elevator
    // Yaw: rudder (+ aileron-rudder coupling)

    float ruddermix = yaw * 0.7f + roll * 0.15f;

    _outputs[CH_AILERON_R] = (uint16_t)(PWM_CENTER_US + roll * 450);
    _outputs[CH_AILERON_L] = (uint16_t)(PWM_CENTER_US - roll * 450);
    _outputs[CH_ELEVATOR]  = (uint16_t)(PWM_CENTER_US - pitch * 500);
    _outputs[CH_THROTTLE]  = (uint16_t)(PWM_MIN_US + throttle * 1000);
    _outputs[CH_RUDDER]    = (uint16_t)(PWM_CENTER_US + ruddermix * 450);
}

void ServoControl::mixerElevon(float roll, float pitch, float yaw, float throttle) {
    // Delta wing / flying wing mixing
    // Left elevon:  pitch + roll
    // Right elevon: pitch - roll
    float left  = pitch + roll;
    float right = pitch - roll;

    // Clamp combined throws
    left  = constrain(left, -1.0f, 1.0f);
    right = constrain(right, -1.0f, 1.0f);

    _outputs[CH_ELEVATOR]  = (uint16_t)(PWM_CENTER_US + left * 500);
    _outputs[CH_AILERON_R] = (uint16_t)(PWM_CENTER_US + right * 500);
    _outputs[CH_AILERON_L] = (uint16_t)(PWM_CENTER_US + right * 500);  // Duplicate if needed
    _outputs[CH_THROTTLE]  = (uint16_t)(PWM_MIN_US + throttle * 1000);
    _outputs[CH_RUDDER]    = PWM_CENTER_US;  // No rudder on pure delta
}

void ServoControl::mixerVTail(float roll, float pitch, float yaw, float throttle) {
    // V-tail mixing: rudder + elevator combined
    // Left V:  pitch + yaw (or pitch - yaw depending on orientation)
    // Right V: pitch - yaw
    float left_v  = pitch + yaw * 0.7f;
    float right_v = pitch - yaw * 0.7f;

    left_v  = constrain(left_v, -1.0f, 1.0f);
    right_v = constrain(right_v, -1.0f, 1.0f);

    _outputs[CH_AILERON_R] = (uint16_t)(PWM_CENTER_US + roll * 450);
    _outputs[CH_AILERON_L] = (uint16_t)(PWM_CENTER_US - roll * 450);
    _outputs[CH_ELEVATOR]  = (uint16_t)(PWM_CENTER_US + left_v * 500);
    _outputs[CH_RUDDER]    = (uint16_t)(PWM_CENTER_US + right_v * 500);
    _outputs[CH_THROTTLE]  = (uint16_t)(PWM_MIN_US + throttle * 1000);
}

void ServoControl::mixerFlaperon(float roll, float pitch, float yaw, float throttle) {
    // Flaperon: both ailerons can droop as flaps + differential for roll
    // Assume flaps are activated via an RC switch or flight mode
    // This is a simplified implementation

    static float flap_position = 0.0f;  // 0.0 = retracted, 1.0 = full flaps

    // In a real system, flap_position would be controlled by the flight controller mode
    float right_ail = roll * 0.5f + flap_position;
    float left_ail  = -roll * 0.5f + flap_position;

    right_ail = constrain(right_ail, -1.0f, 1.5f);  // Allow more down than up
    left_ail  = constrain(left_ail, -1.0f, 1.5f);

    _outputs[CH_AILERON_R] = (uint16_t)(PWM_CENTER_US + right_ail * 400);
    _outputs[CH_AILERON_L] = (uint16_t)(PWM_CENTER_US + left_ail * 400);
    _outputs[CH_ELEVATOR]  = (uint16_t)(PWM_CENTER_US - pitch * 500);
    _outputs[CH_THROTTLE]  = (uint16_t)(PWM_MIN_US + throttle * 1000);
    _outputs[CH_RUDDER]    = (uint16_t)(PWM_CENTER_US + yaw * 450);
}

// ============================================================================
// Calibration
// ============================================================================

void ServoControl::setCalibration(uint8_t channel, const ServoCalibration &cal) {
    if (channel >= 8) return;
    _calibration[channel] = cal;
}

uint16_t ServoControl::getOutput(uint8_t channel) const {
    if (channel >= 8) return 0;
    return _outputs[channel];
}

void ServoControl::enable() {
    _enabled = true;
}

void ServoControl::disable() {
    _enabled = false;
    setAllNeutral();
    // Write neutrals immediately
    for (int i = 0; i < 8; i++) {
        writeServoPWM(_pins[i], _outputs[i]);
    }
}

void ServoControl::calibrateTravel(uint8_t channel, uint16_t min_pwm, uint16_t max_pwm) {
    if (channel >= 8) return;

    _calibration[channel].min_pwm = min_pwm;
    _calibration[channel].max_pwm = max_pwm;
    _calibration[channel].center_pwm = (min_pwm + max_pwm) / 2;
}

void ServoControl::saveCalibration() {
    // Save calibration data to EEPROM
    EEPROM.begin(sizeof(ServoCalibration) * 8 + 2);

    uint16_t magic = 0xBEEF;
    EEPROM.put(0, magic);

    for (int i = 0; i < 8; i++) {
        EEPROM.put(2 + i * sizeof(ServoCalibration), _calibration[i]);
    }

    EEPROM.commit();
}

void ServoControl::loadCalibration() {
    EEPROM.begin(sizeof(ServoCalibration) * 8 + 2);

    uint16_t magic;
    EEPROM.get(0, magic);

    if (magic == 0xBEEF) {
        // Valid calibration data exists
        for (int i = 0; i < 8; i++) {
            EEPROM.get(2 + i * sizeof(ServoCalibration), _calibration[i]);
        }
    }
}
