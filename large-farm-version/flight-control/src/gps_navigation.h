/**
 * @file gps_navigation.h
 * @brief GPS waypoint navigation - NMEA parsing, heading computation, distance estimation
 *
 * Provides GPS data parsing (NMEA sentences) and navigation utilities
 * for waypoint-following flight control.
 */

#ifndef GPS_NAVIGATION_H
#define GPS_NAVIGATION_H

#include <Arduino.h>
#include <TinyGPSPlus.h>

// ============================================================================
// Waypoint Structure
// ============================================================================

/**
 * @brief Waypoint definition for autonomous navigation
 */
struct Waypoint {
    uint8_t id;            // Waypoint sequence number
    float latitude;        // Latitude (degrees, decimal)
    float longitude;       // Longitude (degrees, decimal)
    float altitude;        // Target altitude (meters)
    float loiter_radius;   // Acceptance radius for waypoint reached (meters)
    float loiter_time;     // Seconds to loiter at waypoint (0 = continue)
    uint8_t action;        // Action at waypoint: 0=fly-through, 1=loiter, 2=land, 3=rtl

    Waypoint() : id(0), latitude(0), longitude(0), altitude(100),
                 loiter_radius(20), loiter_time(0), action(0) {}
};

// ============================================================================
// GPS Fix Status
// ============================================================================

enum class GPSFixType : uint8_t {
    NO_FIX      = 0,
    FIX_2D      = 1,
    FIX_3D      = 2,
    DGPS        = 3,
    RTK_FLOAT   = 4,
    RTK_FIXED   = 5
};

// ============================================================================
// GPS Navigation Class
// ============================================================================

class GPSNavigation {
public:
    GPSNavigation();
    ~GPSNavigation() = default;

    /**
     * @brief Initialize GPS module
     * @param serial Reference to hardware serial port
     * @param baud GPS baud rate (default 9600 for u-blox)
     */
    void begin(HardwareSerial &serial, uint32_t baud = 9600);

    /**
     * @brief Update GPS data - call frequently in main loop
     * @return true if new NMEA sentence was processed
     */
    bool update();

    /**
     * @brief Get current latitude (degrees)
     */
    float getLatitude() const;

    /**
     * @brief Get current longitude (degrees)
     */
    float getLongitude() const;

    /**
     * @brief Get current altitude MSL (meters)
     */
    float getAltitude() const;

    /**
     * @brief Get ground speed (m/s)
     */
    float getGroundSpeed() const;

    /**
     * @brief Get course/heading over ground (degrees, 0-360)
     */
    float getCourse() const;

    /**
     * @brief Get number of visible satellites
     */
    uint8_t getSatellites() const;

    /**
     * @brief Get GPS fix type
     */
    GPSFixType getFixType() const;

    /**
     * @brief Get horizontal dilution of precision
     */
    float getHDOP() const;

    /**
     * @brief Check if GPS has valid 3D fix
     */
    bool hasFix() const;

    /**
     * @brief Get timestamp of last valid GPS update
     */
    uint32_t getLastFixTime() const;

    /**
     * @brief Compute bearing from current position to waypoint
     * @param wp Target waypoint
     * @return Bearing in degrees (0-360)
     */
    float bearingTo(const Waypoint &wp) const;

    /**
     * @brief Compute distance from current position to waypoint
     * @param wp Target waypoint
     * @return Distance in meters
     */
    float distanceTo(const Waypoint &wp) const;

    /**
     * @brief Compute bearing between two points
     * @param lat1, lon1 Origin coordinates (degrees)
     * @param lat2, lon2 Destination coordinates (degrees)
     * @return Bearing in degrees (0-360)
     */
    static float computeBearing(float lat1, float lon1,
                                 float lat2, float lon2);

    /**
     * @brief Compute distance between two points (Haversine)
     * @param lat1, lon1 Point 1 coordinates (degrees)
     * @param lat2, lon2 Point 2 coordinates (degrees)
     * @return Distance in meters
     */
    static float computeDistance(float lat1, float lon1,
                                  float lat2, float lon2);

    /**
     * @brief Set home position (for RTL)
     */
    void setHome();

    /**
     * @brief Get home waypoint
     */
    Waypoint getHome() const { return _home; }

    /**
     * @brief Load waypoint mission
     * @param waypoints Array of waypoints
     * @param count Number of waypoints
     */
    void loadMission(const Waypoint *waypoints, uint8_t count);

    /**
     * @brief Get next active waypoint
     * @param wp Output waypoint
     * @return true if waypoint available
     */
    bool getCurrentWaypoint(Waypoint &wp) const;

    /**
     * @brief Advance to next waypoint in mission
     */
    void advanceWaypoint();

    /**
     * @brief Check if waypoint is reached
     * @param wp Waypoint to check
     * @return true if within acceptance radius
     */
    bool isWaypointReached(const Waypoint &wp) const;

    /**
     * @brief Get total mission waypoint count
     */
    uint8_t getWaypointCount() const { return _waypoint_count; }

    /**
     * @brief Get current waypoint index
     */
    uint8_t getCurrentWaypointIndex() const { return _wp_index; }

    /**
     * @brief Set geofence boundary
     * @param center_lat, center_lon Fence center
     * @param radius_m Fence radius in meters
     * @param max_alt Max altitude in meters
     */
    void setGeofence(float center_lat, float center_lon,
                     float radius_m, float max_alt);

    /**
     * @brief Check if current position is within geofence
     * @return true if inside fence
     */
    bool isInsideGeofence() const;

    /**
     * @brief Get raw NMEA buffer for debugging
     */
    const char* getLastNMEA() const { return _last_nmea; }

private:
    TinyGPSPlus _gps;                    // GPS parser
    HardwareSerial *_serial;             // GPS serial port
    uint32_t _baud;

    Waypoint _home;                      // Home/RTL position
    Waypoint _mission[50];              // Mission waypoints (max 50)
    uint8_t _waypoint_count;            // Total waypoints in mission
    uint8_t _wp_index;                  // Current waypoint index

    float _geofence_lat, _geofence_lon;
    float _geofence_radius, _geofence_max_alt;
    bool _geofence_enabled;

    uint32_t _last_fix_time;
    uint32_t _last_home_set_time;
    char _last_nmea[128];               // Last received NMEA sentence

    static const float EARTH_R;
};

#endif // GPS_NAVIGATION_H
