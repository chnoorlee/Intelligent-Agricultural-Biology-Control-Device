/**
 * @file fail_safe.cpp
 * @brief Failsafe system implementation
 *
 * Comprehensive failsafe monitoring for fixed-wing UAV operations.
 * Prioritizes actions: DISARM > LAND > RTL > LOITER > WARN_ONLY.
 * Battery estimation uses Coulomb counting with voltage fallback.
 */

#include "fail_safe.h"

// ============================================================================
// Constructor
// ============================================================================

FailSafe::FailSafe()
    : _home_lat(0), _home_lon(0), _home_alt(0)
{
}

// ============================================================================
// Initialization
// ============================================================================

void FailSafe::begin(const FailsafeConfig &config) {
    _config = config;
    _state = FailsafeState();
}

// ============================================================================
// Main Update
// ============================================================================

FailsafeAction FailSafe::update(
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
) {
    // Clear previous cycle's active flags
    _state.low_battery_warn_active = false;
    _state.low_battery_crit_active = false;
    _state.rc_lost_active = false;
    _state.gps_lost_active = false;
    _state.geofence_breach_active = false;
    _state.imu_failure_active = false;
    _state.vibration_warn_active = false;
    _state.vibration_crit_active = false;
    _state.altitude_limit_active = false;

    // Run all checks
    bool battery_ok = checkBattery(battery_v, battery_current_a,
                                    battery_capacity_mah, now_ms);
    bool rc_ok = checkRCSignal(rc_last_update_ms, now_ms);
    bool gps_ok = checkGPS(gps_has_fix, num_satellites, now_ms);
    bool fence_ok = checkGeofence(distance_from_home, current_altitude);
    bool vib_ok = checkVibration(vibration_g);
    bool speed_ok = checkAirspeed(airspeed_ms);
    bool alt_ok = checkAltitude(current_altitude);
    bool imu_ok = checkIMU(imu_healthy, now_ms);

    // Estimate remaining flight time
    _state.remaining_flight_time_s = estimateFlightTime(
        battery_v, battery_current_a, battery_capacity_mah, now_ms
    );

    // Determine action and set state
    _state.active_action = determineAction();

    bool was_active = _state.failsafe_active;
    _state.failsafe_active = (_state.active_action != FailsafeAction::NONE &&
                              _state.active_action != FailsafeAction::WARN_ONLY);

    if (_state.failsafe_active && !was_active) {
        _state.trigger_time = now_ms;
    }

    return _state.active_action;
}

// ============================================================================
// Individual Checks
// ============================================================================

bool FailSafe::checkBattery(float voltage, float current_a,
                             float capacity_mah, uint32_t now_ms) {
    // Critical battery - land immediately
    if (voltage <= _config.battery_min_voltage && voltage > 0) {
        _state.low_battery_crit_active = true;
        _state.active_trigger = FailsafeTrigger::LOW_BATTERY_CRIT;
        _state.highest_severity = FailsafeTrigger::LOW_BATTERY_CRIT;
        return false;
    }

    // Low battery critical - RTL
    if (voltage <= _config.battery_crit_voltage && voltage > 0) {
        _state.low_battery_crit_active = true;
        _state.active_trigger = FailsafeTrigger::LOW_BATTERY_CRIT;
        if (triggerSeverity(FailsafeTrigger::LOW_BATTERY_CRIT) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::LOW_BATTERY_CRIT;
        }
        return false;
    }

    // Low battery warning
    if (voltage <= _config.battery_warn_voltage && voltage > 0) {
        _state.low_battery_warn_active = true;
        _state.active_trigger = FailsafeTrigger::LOW_BATTERY_WARN;
        return true;  // Warning only, not yet critical
    }

    return true;
}

bool FailSafe::checkRCSignal(uint32_t rc_last_update_ms, uint32_t now_ms) {
    if (rc_last_update_ms == 0) return true;  // No RC configured

    uint32_t elapsed = now_ms - rc_last_update_ms;

    if (_state.rc_lost_active) {
        // Recovery: signal must be stable for recovery_ms
        if (elapsed < _config.rc_timeout_ms) {
            static uint32_t recovery_start = 0;
            if (recovery_start == 0) recovery_start = now_ms;
            if (now_ms - recovery_start > _config.rc_recovery_ms) {
                _state.rc_lost_active = false;
                recovery_start = 0;
                return true;
            }
        }
        return false;
    }

    if (elapsed > _config.rc_timeout_ms) {
        _state.rc_lost_active = true;
        _state.rc_lost_time = now_ms;
        _state.active_trigger = FailsafeTrigger::RC_SIGNAL_LOST;
        if (triggerSeverity(FailsafeTrigger::RC_SIGNAL_LOST) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::RC_SIGNAL_LOST;
        }
        return false;
    }

    return true;
}

bool FailSafe::checkGPS(bool has_fix, uint8_t satellites, uint32_t now_ms) {
    // GPS not required for STABILIZE/MANUAL mode
    // Only critical in AUTO/RTL modes
    if (!has_fix || satellites < _config.min_satellites) {
        if (_state.gps_lost_active) return false;

        if (_state.gps_lost_time == 0) {
            _state.gps_lost_time = now_ms;
        }

        uint32_t elapsed = now_ms - _state.gps_lost_time;
        if (elapsed > _config.gps_loss_timeout_ms) {
            _state.gps_lost_active = true;
            _state.active_trigger = FailsafeTrigger::GPS_LOST;
            if (triggerSeverity(FailsafeTrigger::GPS_LOST) >
                triggerSeverity(_state.highest_severity)) {
                _state.highest_severity = FailsafeTrigger::GPS_LOST;
            }
            return false;
        }
    } else {
        _state.gps_lost_time = 0;
        _state.gps_lost_active = false;
    }

    return true;
}

bool FailSafe::checkGeofence(float distance_from_home, float alt_agl) {
    if (!_config.geofence_enabled) return true;

    bool breach = false;

    // Check horizontal fence
    if (distance_from_home > (_config.geofence_radius_m + _config.geofence_action_dist)) {
        breach = true;
    } else if (distance_from_home > _config.geofence_radius_m) {
        _state.geofence_breach_active = true;
        _state.active_trigger = FailsafeTrigger::GEOFENCE_BREACH;
    }

    // Check altitude fence
    if (alt_agl > (_config.geofence_max_alt_m + 20.0f)) {
        breach = true;
    } else if (alt_agl > _config.geofence_max_alt_m) {
        _state.altitude_limit_active = true;
        _state.active_trigger = FailsafeTrigger::ALTITUDE_LIMIT;
    }

    if (breach) {
        _state.geofence_breach_active = true;
        _state.active_trigger = FailsafeTrigger::GEOFENCE_BREACH;
        if (triggerSeverity(FailsafeTrigger::GEOFENCE_BREACH) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::GEOFENCE_BREACH;
        }
        return false;
    }

    return true;
}

bool FailSafe::checkVibration(float vibration_g) {
    if (vibration_g > _config.vibration_crit_g) {
        _state.vibration_crit_active = true;
        _state.active_trigger = FailsafeTrigger::HIGH_VIBRATION;
        if (triggerSeverity(FailsafeTrigger::HIGH_VIBRATION) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::HIGH_VIBRATION;
        }
        return false;
    }

    if (vibration_g > _config.vibration_warn_g) {
        _state.vibration_warn_active = true;
    }

    return true;
}

bool FailSafe::checkAirspeed(float airspeed_ms) {
    if (airspeed_ms > _config.max_airspeed_ms && airspeed_ms > 0) {
        _state.active_trigger = FailsafeTrigger::OVER_SPEED;
        return false;
    }
    return true;
}

bool FailSafe::checkAltitude(float alt_agl) {
    if (alt_agl > _config.geofence_max_alt_m + 50.0f) {
        _state.altitude_limit_active = true;
        _state.active_trigger = FailsafeTrigger::ALTITUDE_LIMIT;
        if (triggerSeverity(FailsafeTrigger::ALTITUDE_LIMIT) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::ALTITUDE_LIMIT;
        }
        return false;
    }
    return true;
}

bool FailSafe::checkIMU(bool imu_healthy, uint32_t now_ms) {
    if (!imu_healthy) {
        _state.imu_failure_active = true;
        _state.active_trigger = FailsafeTrigger::IMU_FAILURE;
        if (triggerSeverity(FailsafeTrigger::IMU_FAILURE) >
            triggerSeverity(_state.highest_severity)) {
            _state.highest_severity = FailsafeTrigger::IMU_FAILURE;
        }
        return false;
    }
    return true;
}

// ============================================================================
// Action Determination
// ============================================================================

FailsafeAction FailSafe::determineAction() {
    // Priority hierarchy: more severe action overrides less severe
    // IMU failure -> DISARM (on ground) or LAND
    if (_state.imu_failure_active) {
        return FailsafeAction::LAND;  // Can't safely RTL without IMU
    }

    // Critical battery at min voltage -> LAND immediately
    if (_state.low_battery_crit_active) {
        return FailsafeAction::LAND;
    }

    // High vibration critical -> LAND
    if (_state.vibration_crit_active) {
        return FailsafeAction::LAND;
    }

    // Geofence breach -> RTL (if auto_rth enabled)
    if (_state.geofence_breach_active && _config.auto_rth_enabled) {
        return FailsafeAction::RTL;
    }

    // RC signal lost -> RTL
    if (_state.rc_lost_active && _config.auto_rth_enabled) {
        return FailsafeAction::RTL;
    }

    // GPS lost (and in nav mode) -> LOITER
    if (_state.gps_lost_active) {
        return FailsafeAction::LOITER;  // Hold position until GPS recovers
    }

    // Low battery warning -> WARN_ONLY (pilot decides)
    if (_state.low_battery_warn_active) {
        return FailsafeAction::WARN_ONLY;
    }

    // Altitude limit -> WARN_ONLY (pilot should descend)
    if (_state.altitude_limit_active) {
        return FailsafeAction::WARN_ONLY;
    }

    // Vibration warning -> WARN_ONLY
    if (_state.vibration_warn_active) {
        return FailsafeAction::WARN_ONLY;
    }

    // Over-speed -> WARN_ONLY
    if (_state.active_trigger == FailsafeTrigger::OVER_SPEED) {
        return FailsafeAction::WARN_ONLY;
    }

    return FailsafeAction::NONE;
}

// ============================================================================
// Flight Mode Mapping
// ============================================================================

uint8_t FailSafe::getRecommendedMode() const {
    switch (_state.active_action) {
        case FailsafeAction::RTL:
            return 3;    // RTL
        case FailsafeAction::LAND:
            return 5;    // LAND
        case FailsafeAction::LOITER:
            return 4;    // LOITER
        case FailsafeAction::HOLD:
            return 4;    // LOITER (fixed-wing hold = loiter)
        case FailsafeAction::DISARM:
            return 0;    // DISARM (mode doesn't matter)
        case FailsafeAction::WARN_ONLY:
        case FailsafeAction::NONE:
        default:
            return 255;  // No mode change needed
    }
}

// ============================================================================
// Management
// ============================================================================

void FailSafe::clearTrigger(FailsafeTrigger trigger) {
    switch (trigger) {
        case FailsafeTrigger::LOW_BATTERY_WARN:
            _state.low_battery_warn_active = false;
            break;
        case FailsafeTrigger::LOW_BATTERY_CRIT:
            _state.low_battery_crit_active = false;
            break;
        case FailsafeTrigger::RC_SIGNAL_LOST:
            _state.rc_lost_active = false;
            _state.rc_lost_time = 0;
            break;
        case FailsafeTrigger::GPS_LOST:
            _state.gps_lost_active = false;
            _state.gps_lost_time = 0;
            break;
        case FailsafeTrigger::GEOFENCE_BREACH:
            _state.geofence_breach_active = false;
            break;
        case FailsafeTrigger::IMU_FAILURE:
            _state.imu_failure_active = false;
            break;
        case FailsafeTrigger::HIGH_VIBRATION:
            _state.vibration_crit_active = false;
            break;
        default:
            break;
    }

    // Re-evaluate state
    _state.active_action = determineAction();
    _state.failsafe_active = (_state.active_action != FailsafeAction::NONE &&
                              _state.active_action != FailsafeAction::WARN_ONLY);
}

void FailSafe::clearAll() {
    _state = FailsafeState();
}

void FailSafe::triggerFailsafe(FailsafeTrigger trigger) {
    _state.active_trigger = trigger;
    if (triggerSeverity(trigger) > triggerSeverity(_state.highest_severity)) {
        _state.highest_severity = trigger;
    }

    switch (trigger) {
        case FailsafeTrigger::RC_SIGNAL_LOST:
            _state.rc_lost_active = true;
            break;
        case FailsafeTrigger::LOW_BATTERY_CRIT:
            _state.low_battery_crit_active = true;
            break;
        case FailsafeTrigger::GEOFENCE_BREACH:
            _state.geofence_breach_active = true;
            break;
        case FailsafeTrigger::MANUAL_OVERRIDE:
            _state.active_action = FailsafeAction::RTL;
            _state.failsafe_active = true;
            break;
        default:
            break;
    }

    _state.active_action = determineAction();
    _state.failsafe_active = true;
}

void FailSafe::setConfig(const FailsafeConfig &config) {
    _config = config;
}

void FailSafe::setHomePosition(float lat, float lon, float alt) {
    _home_lat = lat;
    _home_lon = lon;
    _home_alt = alt;
}

// ============================================================================
// Status & Estimation
// ============================================================================

const char* FailSafe::getStatusMessage() const {
    if (_state.imu_failure_active) return "IMU FAILURE - Landing immediately";
    if (_state.low_battery_crit_active) return "BATTERY CRITICAL - Landing";
    if (_state.vibration_crit_active) return "HIGH VIBRATION - Landing";
    if (_state.geofence_breach_active) return "GEOFENCE BREACH - RTL";
    if (_state.rc_lost_active) return "RC LOST - RTL";
    if (_state.gps_lost_active) return "GPS LOST - Loitering";
    if (_state.low_battery_warn_active) return "Battery Low - Return Home";
    if (_state.altitude_limit_active) return "Altitude Limit - Descend";
    if (_state.vibration_warn_active) return "Vibration Warning";
    return "All Systems Nominal";
}

float FailSafe::estimateFlightTime(float voltage, float current_a,
                                    float capacity_mah, uint32_t now_ms) {
    if (current_a <= 0.05f || capacity_mah <= 0) {
        // Can't estimate with no current draw
        return -1.0f;
    }

    // Estimate remaining capacity using voltage-based state of charge
    // 3S LiPo: 12.6V = 100%, 9.9V = 0%
    float soc = (voltage - 9.9f) / (12.6f - 9.9f);
    soc = constrain(soc, 0.0f, 1.0f);

    float remaining_capacity_mah = capacity_mah * soc;
    float remaining_time_h = remaining_capacity_mah / (current_a * 1000.0f);
    return remaining_time_h * 3600.0f;
}

// ============================================================================
// Severity Mapping
// ============================================================================

uint8_t FailSafe::triggerSeverity(FailsafeTrigger trigger) {
    // Higher number = more severe
    switch (trigger) {
        case FailsafeTrigger::IMU_FAILURE:          return 100;
        case FailsafeTrigger::LOW_BATTERY_CRIT:     return 90;
        case FailsafeTrigger::HIGH_VIBRATION:       return 80;
        case FailsafeTrigger::GEOFENCE_BREACH:      return 70;
        case FailsafeTrigger::RC_SIGNAL_LOST:       return 60;
        case FailsafeTrigger::GPS_LOST:             return 50;
        case FailsafeTrigger::ALTITUDE_LIMIT:       return 40;
        case FailsafeTrigger::LOW_BATTERY_WARN:     return 30;
        case FailsafeTrigger::OVER_SPEED:           return 20;
        case FailsafeTrigger::MANUAL_OVERRIDE:      return 110;
        case FailsafeTrigger::NONE:                 return 0;
        default:                                    return 0;
    }
}
