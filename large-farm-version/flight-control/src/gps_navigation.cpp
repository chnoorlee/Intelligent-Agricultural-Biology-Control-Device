/**
 * @file gps_navigation.cpp
 * @brief GPS waypoint navigation implementation
 *
 * Parses NMEA sentences from u-blox GPS modules (NEO-6M, NEO-M8N, etc.)
 * Provides waypoint management, heading/distance computation, geofencing.
 */

#include "gps_navigation.h"
#include <math.h>

const float GPSNavigation::EARTH_R = 6371000.0f;

// ============================================================================
// Constructor
// ============================================================================

GPSNavigation::GPSNavigation()
    : _serial(nullptr)
    , _baud(9600)
    , _waypoint_count(0)
    , _wp_index(0)
    , _geofence_lat(0), _geofence_lon(0)
    , _geofence_radius(0), _geofence_max_alt(0)
    , _geofence_enabled(false)
    , _last_fix_time(0)
    , _last_home_set_time(0)
{
    memset(_last_nmea, 0, sizeof(_last_nmea));
}

// ============================================================================
// Initialization
// ============================================================================

void GPSNavigation::begin(HardwareSerial &serial, uint32_t baud) {
    _serial = &serial;
    _baud = baud;
    _serial->begin(baud);

    // Configure u-blox GPS for optimal performance
    // Disable unnecessary NMEA sentences, keep only GGA and RMC
    // $PUBX,40,GLL,0,0,0,0,0,0*5C - disable GLL
    // $PUBX,40,GSA,0,0,0,0,0,0*4E - disable GSA
    // $PUBX,40,GSV,0,0,0,0,0,0*59 - disable GSV
    // $PUBX,40,VTG,0,0,0,0,0,0*5E - disable VTG
    // Set update rate to 5Hz: $PUBX,40,GSV,0,0,0,0,0,0*59
    _serial->println("$PUBX,40,GLL,0,0,0,0,0,0*5C");
    _serial->println("$PUBX,40,GSA,0,0,0,0,0,0*4E");
    _serial->println("$PUBX,40,GSV,0,0,0,0,0,0*59");
    _serial->println("$PUBX,40,VTG,0,0,0,0,0,0*5E");

    // Set 5Hz navigation rate
    _serial->println("$PUBX,41,1,0007,0003,200,0*1C");
}

// ============================================================================
// Update
// ============================================================================

bool GPSNavigation::update() {
    if (!_serial) return false;

    bool new_data = false;
    while (_serial->available() > 0) {
        char c = _serial->read();
        if (_gps.encode(c)) {
            new_data = true;

            // Store last NMEA sentence for telemetry
            // (simplified - would store full sentence in production)
            if (_gps.location.isValid() && _gps.location.isUpdated()) {
                snprintf(_last_nmea, sizeof(_last_nmea),
                    "$GPRMC,%.6f,%.6f,%.1f,%.1f",
                    _gps.location.lat(),
                    _gps.location.lng(),
                    _gps.speed.kmph(),
                    _gps.course.deg()
                );
                _last_fix_time = millis();
            }
        }
    }
    return new_data;
}

// ============================================================================
// Data Accessors
// ============================================================================

float GPSNavigation::getLatitude() const {
    return _gps.location.isValid() ? (float)_gps.location.lat() : 0.0f;
}

float GPSNavigation::getLongitude() const {
    return _gps.location.isValid() ? (float)_gps.location.lng() : 0.0f;
}

float GPSNavigation::getAltitude() const {
    return _gps.altitude.isValid() ? (float)_gps.altitude.meters() : 0.0f;
}

float GPSNavigation::getGroundSpeed() const {
    return _gps.speed.isValid() ? (float)_gps.speed.mps() : 0.0f;
}

float GPSNavigation::getCourse() const {
    return _gps.course.isValid() ? (float)_gps.course.deg() : 0.0f;
}

uint8_t GPSNavigation::getSatellites() const {
    return _gps.satellites.isValid() ? (uint8_t)_gps.satellites.value() : 0;
}

GPSFixType GPSNavigation::getFixType() const {
    if (_gps.location.isValid() && _gps.altitude.isValid()) {
        return GPSFixType::FIX_3D;
    } else if (_gps.location.isValid()) {
        return GPSFixType::FIX_2D;
    }
    return GPSFixType::NO_FIX;
}

float GPSNavigation::getHDOP() const {
    return _gps.hdop.isValid() ? (float)_gps.hdop.hdop() : 99.99f;
}

bool GPSNavigation::hasFix() const {
    return _gps.location.isValid() && _gps.location.age() < 2000;
}

uint32_t GPSNavigation::getLastFixTime() const {
    return _last_fix_time;
}

// ============================================================================
// Navigation Computation
// ============================================================================

float GPSNavigation::bearingTo(const Waypoint &wp) const {
    return computeBearing(
        getLatitude(), getLongitude(),
        wp.latitude, wp.longitude
    );
}

float GPSNavigation::distanceTo(const Waypoint &wp) const {
    return computeDistance(
        getLatitude(), getLongitude(),
        wp.latitude, wp.longitude
    );
}

float GPSNavigation::computeBearing(float lat1, float lon1,
                                     float lat2, float lon2) {
    float lat1r = lat1 * 0.01745329252f;  // DEG_TO_RAD
    float lon1r = lon1 * 0.01745329252f;
    float lat2r = lat2 * 0.01745329252f;
    float lon2r = lon2 * 0.01745329252f;

    float dlon = lon2r - lon1r;
    float y = sin(dlon) * cos(lat2r);
    float x = cos(lat1r) * sin(lat2r) - sin(lat1r) * cos(lat2r) * cos(dlon);
    float bearing = atan2(y, x) * 57.2957795131f;  // RAD_TO_DEG

    if (bearing < 0) bearing += 360.0f;
    return bearing;
}

float GPSNavigation::computeDistance(float lat1, float lon1,
                                      float lat2, float lon2) {
    float lat1r = lat1 * 0.01745329252f;
    float lon1r = lon1 * 0.01745329252f;
    float lat2r = lat2 * 0.01745329252f;
    float lon2r = lon2 * 0.01745329252f;

    float dlat = lat2r - lat1r;
    float dlon = lon2r - lon1r;

    float a = sin(dlat / 2) * sin(dlat / 2) +
              cos(lat1r) * cos(lat2r) * sin(dlon / 2) * sin(dlon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_R * c;
}

// ============================================================================
// Home Position
// ============================================================================

void GPSNavigation::setHome() {
    _home.id = 0;
    _home.latitude = getLatitude();
    _home.longitude = getLongitude();
    _home.altitude = getAltitude();
    _home.loiter_radius = 20.0f;
    _home.loiter_time = 0;
    _home.action = 0;
    _last_home_set_time = millis();
}

// ============================================================================
// Mission Management
// ============================================================================

void GPSNavigation::loadMission(const Waypoint *waypoints, uint8_t count) {
    _waypoint_count = min(count, (uint8_t)50);
    _wp_index = 0;

    for (uint8_t i = 0; i < _waypoint_count; i++) {
        _mission[i] = waypoints[i];
    }
}

bool GPSNavigation::getCurrentWaypoint(Waypoint &wp) const {
    if (_wp_index >= _waypoint_count) return false;
    wp = _mission[_wp_index];
    return true;
}

void GPSNavigation::advanceWaypoint() {
    if (_wp_index < _waypoint_count - 1) {
        _wp_index++;
    } else {
        // Mission complete - could trigger RTL or loiter at last waypoint
        _wp_index = 0; // Loop mission (in production, would trigger RTL)
    }
}

bool GPSNavigation::isWaypointReached(const Waypoint &wp) const {
    float dist = distanceTo(wp);
    return dist <= wp.loiter_radius;
}

// ============================================================================
// Geofencing
// ============================================================================

void GPSNavigation::setGeofence(float center_lat, float center_lon,
                                 float radius_m, float max_alt) {
    _geofence_lat = center_lat;
    _geofence_lon = center_lon;
    _geofence_radius = radius_m;
    _geofence_max_alt = max_alt;
    _geofence_enabled = true;
}

bool GPSNavigation::isInsideGeofence() const {
    if (!_geofence_enabled) return true;

    // Check horizontal boundary
    float dist = computeDistance(
        getLatitude(), getLongitude(),
        _geofence_lat, _geofence_lon
    );
    if (dist > _geofence_radius) return false;

    // Check altitude boundary
    if (getAltitude() > _geofence_max_alt) return false;

    return true;
}
