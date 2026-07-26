/**
 * @file fail_safe.h
 * @brief Failsafe system - low battery RTL, signal loss protection, geofence
 *
 * Monitors critical vehicle conditions and triggers appropriate failsafe actions:
 *   - Low battery: Return to Launch (RTL) or immediate landing
 *   - RC signal loss: RTL after timeout
 *   - GPS loss: Loiter or auto-land
 *   - Geofence breach: RTL
 *   - IMU failure: Immediate land
 */

#ifndef FAIL_SAFE_H
#define FAIL_SAFE_H

#include <Arduino.h>

// ============================================================================
// Failsafe Trigger Types
// ============================================================================

enum class FailsafeTrigger : uint8_t {
    NONE                = 0,
    LOW_BATTERY_WARN    = 1,  // Battery below warning threshold
    LOW_BATTERY_CRIT    = 2,  // Battery below critical threshold
    RC_SIGNAL_LOST      = 3,  // RC receiver timeout
    GPS_LOST            = 4,  // GPS fix lost
    GEOFENCE_BREACH     = 5,  // Outside geofence boundary
    IMU_FAILURE         = 6,  // IMU sensor unhealthy
    HIGH_VIBRATION      = 7,  // Excessive vibration detected
    OVER_SPEED          = 8,  // Airspeed exceeds limit
    ALTITUDE_LIMIT      = 9,  // Above max altitude
    MANUAL_OVERRIDE     = 10  // Manual failsafe command from GCS
};

// ============================================================================
// Failsafe Action Types
// ============================================================================

enum class FailsafeAction : uint8_t {
    NONE        = 0,  // No action
    WARN_ONLY   = 1,  // Log warning, continue
    RTL         = 2,  // Return to Launch
    LAND        = 3,  // Immediate auto-land
    LOITER      = 4,  // Loiter at current position
    HOLD        = 5,  // Hold position (multi-rotor) / circle (fixed-wing)
    DISARM      = 6   // Emergency disarm (ground only)
};

// ============================================================================
// Failsafe Configuration
// ============================================================================

struct FailsafeConfig {
    // Battery thresholds (per-cell voltage for 3S LiPo)
    float battery_warn_voltage;    // Warning threshold (e.g., 10.8V for 3S)
    float battery_crit_voltage;    // Critical threshold (e.g., 10.2V for 3S)
    float battery_min_voltage;     // Emergency landing now (e.g., 9.9V for 3S)

    // RC signal timeout (milliseconds)
    uint32_t rc_timeout_ms;        // Time without RC signal to trigger failsafe
    uint32_t rc_recovery_ms;       // Time with valid signal to clear failsafe

    // GPS
    uint32_t gps_loss_timeout_ms;  // Time without GPS fix to trigger failsafe
    uint8_t  min_satellites;       // Minimum satellites for safe navigation

    // Geofence
    bool     geofence_enabled;
    float    geofence_radius_m;    // Max distance from home
    float    geofence_max_alt_m;   // Max altitude AGL
    float    geofence_action_dist; // Distance beyond fence to trigger action

    // Auto-land parameters
    float    land_descent_rate;    // Descent rate for auto-land (m/s)
    float    land_flare_alt;       // Flare altitude (m)
    float    land_touchdown_speed; // Touchdown ground speed (m/s)

    // General
    bool     auto_rth_enabled;     // Enable automatic RTH on failsafe
    uint32_t rth_min_battery_time; // Minimum flight time after RTH (seconds)

    // Vibration monitoring
    float    vibration_warn_g;     // Vibration warning threshold (g)
    float    vibration_crit_g;     // Vibration critical threshold (g)

    // Airspeed limits
    float    max_airspeed_ms;      // Maximum safe airspeed (m/s)
    float    min_airspeed_ms;      // Stall speed (m/s)

    FailsafeConfig()
        : battery_warn_voltage(10.8f)
        , battery_crit_voltage(10.2f)
        , battery_min_voltage(9.9f)
        , rc_timeout_ms(2000)
        , rc_recovery_ms(1000)
        , gps_loss_timeout_ms(5000)
        , min_satellites(6)
        , geofence_enabled(true)
        , geofence_radius_m(500.0f)
        , geofence_max_alt_m(120.0f)
        , geofence_action_dist(50.0f)
        , land_descent_rate(2.0f)
        , land_flare_alt(5.0f)
        , land_touchdown_speed(3.0f)
        , auto_rth_enabled(true)
        , rth_min_battery_time(120)
        , vibration_warn_g(2.0f)
        , vibration_crit_g(4.0f)
        , max_airspeed_ms(30.0f)
        , min_airspeed_ms(10.0f)
    {}
};

// ============================================================================
// Failsafe State
// ============================================================================

struct FailsafeState {
    FailsafeTrigger active_trigger;   // Currently active failsafe trigger
    FailsafeAction  active_action;    // Currently executing failsafe action
    FailsafeTrigger highest_severity; // Highest severity trigger since arming

    uint32_t trigger_time;            // When the failsafe was triggered (ms)
    uint32_t rc_lost_time;            // When RC signal was lost (ms)
    uint32_t gps_lost_time;           // When GPS fix was lost (ms)

    bool low_battery_warn_active;     // Low battery warning active
    bool low_battery_crit_active;     // Low battery critical active
    bool rc_lost_active;              // RC signal lost active
    bool gps_lost_active;             // GPS lost active
    bool geofence_breach_active;      // Geofence breach active
    bool imu_failure_active;          // IMU failure active
    bool vibration_warn_active;       // Vibration warning active
    bool vibration_crit_active;       // Vibration critical active
    bool altitude_limit_active;       // Altitude limit active

    bool failsafe_active;             // Any failsafe is active
    bool has_landed;                  // Auto-land sequence complete
    float rth_altitude;               // RTH cruise altitude (m)
    float remaining_flight_time_s;    // Estimated remaining flight time

    FailsafeState()
        : active_trigger(FailsafeTrigger::NONE)
        , active_action(FailsafeAction::NONE)
        , highest_severity(FailsafeTrigger::NONE)
        , trigger_time(0), rc_lost_time(0), gps_lost_time(0)
        , low_battery_warn_active(false)
        , low_battery_crit_active(false)
        , rc_lost_active(false)
        , gps_lost_active(false)
        , geofence_breach_active(false)
        , imu_failure_active(false)
        , vibration_warn_active(false)
        , vibration_crit_active(false)
        , altitude_limit_active(false)
        , failsafe_active(false)
        , has_landed(false)
        , rth_altitude(100.0f)
        , remaining_flight_time_s(0)
    {}
};

// ============================================================================
// Failsafe Class
// ============================================================================

class FailSafe {
public:
    FailSafe();
    ~FailSafe() = default;

    /**
     * @brief Initialize failsafe system with configuration
     */
    void begin(const FailsafeConfig &config);

    /**
     * @brief Update failsafe checks - call at 10Hz
     * @param now_ms Current system time (millis())
     * @param battery_v Battery voltage (V)
     * @param battery_current_a Battery current (A)
     * @param battery_capacity_mah Total battery capacity (mAh)
     * @param rc_last_update_ms Last RC signal timestamp (millis)
     * @param gps_has_fix Current GPS fix status
     * @param num_satellites Number of visible satellites
     * @param distance_from_home Distance from home (meters)
     * @param current_altitude Current altitude AGL (meters)
     * @param vibration_g Current vibration level (g)
     * @param airspeed_ms Current airspeed (m/s)
     * @param imu_healthy IMU health status
     * @return Recommended failsafe action
     */
    FailsafeAction update(
        uint32_t now_ms,
        float battery_v,
        float battery_current_a,
        float battery_capacity_mah,
        uint32_t rc_last_update_ms,
        bool gps_has_fix,
        uint8_t num_satellites,
        float distance_from_home,
        float current_altitude,
        float vibration_g,
        float airspeed_ms,
        bool imu_healthy
    );

    /**
     * @brief Get current failsafe state
     */
    const FailsafeState& getState() const { return _state; }

    /**
     * @brief Check if any failsafe is currently active
     */
    bool isActive() const { return _state.failsafe_active; }

    /**
     * @brief Get the recommended flight mode for current failsafe
     * @return 0=no change, 3=RTL, 5=LAND, 4=LOITER
     */
    uint8_t getRecommendedMode() const;

    /**
     * @brief Clear a specific failsafe trigger
     */
    void clearTrigger(FailsafeTrigger trigger);

    /**
     * @brief Clear all failsafe triggers (e.g., on mode change)
     */
    void clearAll();

    /**
     * @brief Manually trigger a failsafe (from GCS command)
     */
    void triggerFailsafe(FailsafeTrigger trigger);

    /**
     * @brief Update configuration at runtime
     */
    void setConfig(const FailsafeConfig &config);

    /**
     * @brief Get estimated remaining flight time (seconds)
     */
    float getRemainingFlightTime() const { return _state.remaining_flight_time_s; }

    /**
     * @brief Check if auto-land has completed
     */
    bool hasLanded() const { return _state.has_landed; }

    /**
     * @brief Get a human-readable status message
     */
    const char* getStatusMessage() const;

    /**
     * @brief Set the home position coordinates (for geofence center)
     */
    void setHomePosition(float lat, float lon, float alt);

private:
    FailsafeConfig _config;
    FailsafeState _state;

    float _home_lat, _home_lon, _home_alt;  // Home position

    // Internal check functions - each returns true if failsafe should trigger
    bool checkBattery(float voltage, float current_a,
                      float capacity_mah, uint32_t now_ms);
    bool checkRCSignal(uint32_t rc_last_update_ms, uint32_t now_ms);
    bool checkGPS(bool has_fix, uint8_t satellites, uint32_t now_ms);
    bool checkGeofence(float distance_from_home, float alt_agl);
    bool checkVibration(float vibration_g);
    bool checkAirspeed(float airspeed_ms);
    bool checkAltitude(float alt_agl);
    bool checkIMU(bool imu_healthy, uint32_t now_ms);

    /**
     * @brief Determine the most appropriate action based on active triggers
     */
    FailsafeAction determineAction();

    /**
     * @brief Map trigger to severity level (higher = more severe)
     */
    static uint8_t triggerSeverity(FailsafeTrigger trigger);

    /**
     * @brief Estimate remaining flight time based on battery
     */
    float estimateFlightTime(float voltage, float current_a,
                              float capacity_mah, uint32_t now_ms);
};

#endif // FAIL_SAFE_H
