/**
 * @file servo_control.h
 * @brief Servo/ESC control - PWM output, control mixing, travel calibration
 *
 * Handles all PWM output generation for servos and ESCs (Electronic Speed Controllers).
 * Supports standard 50Hz servo signals and Oneshot125/Oneshot42 for ESCs.
 */

#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <Arduino.h>

// ============================================================================
// Servo Configuration
// ============================================================================

// PWM output channel assignments
#define CH_AILERON_R    0   // Right aileron servo
#define CH_ELEVATOR     1   // Elevator servo
#define CH_THROTTLE     2   // ESC / throttle output
#define CH_RUDDER       3   // Rudder servo
#define CH_AILERON_L    4   // Left aileron servo (dual aileron)
#define CH_FLAPS        5   // Flaps servo
#define CH_MODE_IND     6   // Flight mode indicator / aux
#define CH_AUX          7   // Auxiliary channel

// PWM timing constants
#define PWM_MIN_US      1000   // Minimum PWM pulse (microseconds)
#define PWM_MAX_US      2000   // Maximum PWM pulse (microseconds)
#define PWM_CENTER_US   1500   // Center/neutral PWM pulse
#define PWM_PERIOD_US   20000  // 50Hz period for standard servos (20ms)

// Oneshot125 timing (for ESCs)
#define ONESHOT125_MIN  125    // Minimum pulse (microseconds)
#define ONESHOT125_MAX  250    // Maximum pulse (microseconds)

// Servo travel limits (in PWM microseconds)
#define SERVO_TRAVEL_MIN    900
#define SERVO_TRAVEL_MAX    2100

// ============================================================================
// Servo Calibration Data
// ============================================================================

/**
 * @brief Per-channel calibration data for servo travel compensation
 */
struct ServoCalibration {
    uint16_t min_pwm;       // Minimum PWM for this channel
    uint16_t max_pwm;       // Maximum PWM for this channel
    uint16_t center_pwm;    // Center/neutral PWM
    int16_t  trim;          // Trim offset in microseconds
    bool     reversed;      // Reverse output direction
    float    scale;         // Output scale factor (0.5 - 2.0)

    ServoCalibration()
        : min_pwm(1000), max_pwm(2000), center_pwm(1500)
        , trim(0), reversed(false), scale(1.0f) {}

    /**
     * @brief Apply calibration to raw PWM value
     */
    uint16_t apply(uint16_t raw) const {
        // Apply trim
        int32_t val = raw + trim;

        // Apply reversal
        if (reversed) {
            val = PWM_MAX_US - (val - PWM_MIN_US);
        }

        // Apply scale around center
        if (scale != 1.0f) {
            int32_t diff = val - center_pwm;
            val = center_pwm + (int32_t)(diff * scale);
        }

        // Clamp to travel limits
        val = constrain(val, (int32_t)SERVO_TRAVEL_MIN, (int32_t)SERVO_TRAVEL_MAX);
        return (uint16_t)val;
    }
};

// ============================================================================
// Mixer Modes
// ============================================================================

/**
 * @brief Control mixer configuration types
 */
enum class MixerType : uint8_t {
    STANDARD_FIXED_WING = 0,  // Aileron, Elevator, Throttle, Rudder
    ELEVON              = 1,  // Delta wing / flying wing mixing
    VTAIL               = 2,  // V-tail mixing (rudder+elevator combined)
    DUAL_AILERON        = 3,  // Separate left/right aileron channels
    FLAPERON            = 4   // Flaps + aileron combined on same surfaces
};

// ============================================================================
// Servo Control Class
// ============================================================================

class ServoControl {
public:
    ServoControl();
    ~ServoControl() = default;

    /**
     * @brief Initialize servo outputs
     * @param pins Array of 8 output pin numbers
     */
    void begin(const uint8_t pins[8]);

    /**
     * @brief Write raw PWM values to all channels
     * @param values Array of 8 PWM values [1000-2000]
     */
    void writeChannels(const uint16_t values[8]);

    /**
     * @brief Write PWM to a single channel
     * @param channel Channel index (0-7)
     * @param pwm PWM value in microseconds
     */
    void writeChannel(uint8_t channel, uint16_t pwm);

    /**
     * @brief Set channel to safe/neutral position
     */
    void setAllNeutral();

    /**
     * @brief Emergency stop - kill throttle, center surfaces
     */
    void emergencyStop();

    /**
     * @brief Configure mixer type
     */
    void setMixerType(MixerType type);

    /**
     * @brief Perform control mixing
     * @param roll Normalized roll demand [-1.0, 1.0]
     * @param pitch Normalized pitch demand [-1.0, 1.0]
     * @param yaw Normalized yaw demand [-1.0, 1.0]
     * @param throttle Normalized throttle [0.0, 1.0]
     */
    void applyMixer(float roll, float pitch, float yaw, float throttle);

    /**
     * @brief Set calibration for a specific channel
     */
    void setCalibration(uint8_t channel, const ServoCalibration &cal);

    /**
     * @brief Get current output value for a channel
     */
    uint16_t getOutput(uint8_t channel) const;

    /**
     * @brief Enable/disable servo outputs
     */
    void enable();
    void disable();

    /**
     * @brief Check if outputs are enabled
     */
    bool isEnabled() const { return _enabled; }

    /**
     * @brief Set servo refresh rate
     * @param hz Refresh frequency (50-400 Hz, default 50 for analog servos)
     */
    void setRefreshRate(uint16_t hz);

    /**
     * @brief Perform end-point calibration for a servo channel
     * @param channel Channel to calibrate
     * @param min_pwm Measured minimum PWM
     * @param max_pwm Measured maximum PWM
     */
    void calibrateTravel(uint8_t channel, uint16_t min_pwm, uint16_t max_pwm);

    /**
     * @brief Save calibration to EEPROM
     */
    void saveCalibration();

    /**
     * @brief Load calibration from EEPROM
     */
    void loadCalibration();

private:
    uint8_t _pins[8];                    // Output pin numbers
    uint16_t _outputs[8];                // Current PWM outputs
    ServoCalibration _calibration[8];    // Per-channel calibration
    MixerType _mixer_type;
    bool _enabled;
    uint16_t _refresh_hz;
    uint32_t _last_servo_update;         // Last servo write timestamp
    uint32_t _period_us;                 // Servo update period

    /**
     * @brief Write PWM pulse to a pin using hardware timer or bit-banging
     */
    void writeServoPWM(uint8_t pin, uint16_t pulse_us);

    /**
     * @brief Standard fixed-wing mixing
     */
    void mixerStandard(float roll, float pitch, float yaw, float throttle);

    /**
     * @brief Elevon/delta wing mixing
     */
    void mixerElevon(float roll, float pitch, float yaw, float throttle);

    /**
     * @brief V-tail mixing
     */
    void mixerVTail(float roll, float pitch, float yaw, float throttle);

    /**
     * @brief Flaperon mixing
     */
    void mixerFlaperon(float roll, float pitch, float yaw, float throttle);
};

#endif // SERVO_CONTROL_H
