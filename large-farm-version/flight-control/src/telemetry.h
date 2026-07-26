/**
 * @file telemetry.h
 * @brief Telemetry system - MAVLink protocol encapsulation, data radio relay
 *
 * Provides MAVLink v1/v2 message packing/unpacking for air-to-ground telemetry.
 * Supports heartbeat, attitude, GPS, battery status, and custom messages.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>

// ============================================================================
// MAVLink Message IDs (subset)
// ============================================================================

#define MAVLINK_MSG_ID_HEARTBEAT       0
#define MAVLINK_MSG_ID_ATTITUDE        30
#define MAVLINK_MSG_ID_GPS_RAW_INT     24
#define MAVLINK_MSG_ID_GLOBAL_POS_INT  33
#define MAVLINK_MSG_ID_BATTERY_STATUS  147
#define MAVLINK_MSG_ID_RC_CHANNELS     65
#define MAVLINK_MSG_ID_COMMAND_LONG    76
#define MAVLINK_MSG_ID_STATUSTEXT      253
#define MAVLINK_MSG_ID_SYS_STATUS      1

// MAVLink component IDs
#define MAV_COMP_ID_AUTOPILOT1         1
#define MAV_COMP_ID_MISSIONPLANNER     190

// MAVLink message max length
#define MAVLINK_MAX_PACKET_LEN         280
#define MAVLINK_NUM_CHECKSUM_BYTES     2

// ============================================================================
// MAVLink Protocol Constants (v1)
// ============================================================================

#define MAVLINK_STX                   0xFE
#define MAVLINK_STX_V2                0xFD

// MAV_TYPE
#define MAV_TYPE_FIXED_WING           1

// MAV_AUTOPILOT
#define MAV_AUTOPILOT_GENERIC         0

// MAV_MODE_FLAG
#define MAV_MODE_FLAG_CUSTOM_MODE_ENABLED 0x01
#define MAV_MODE_FLAG_GUIDED_ENABLED      0x08
#define MAV_MODE_FLAG_AUTO_ENABLED        0x10
#define MAV_MODE_FLAG_SAFETY_ARMED        0x80

// MAV_STATE
#define MAV_STATE_UNINIT              0
#define MAV_STATE_STANDBY              3
#define MAV_STATE_ACTIVE               4

// MAV_SEVERITY
#define MAV_SEVERITY_INFO             6
#define MAV_SEVERITY_WARNING          4
#define MAV_SEVERITY_ERROR            3
#define MAV_SEVERITY_CRITICAL         2

// ============================================================================
// MAVLink Packet Structure
// ============================================================================

/**
 * @brief MAVLink v1 packet header (6 bytes)
 */
struct MAVLinkHeader {
    uint8_t  magic;        // 0xFE
    uint8_t  payload_len;  // 0-255
    uint8_t  seq;          // Sequence number
    uint8_t  sysid;        // System ID
    uint8_t  compid;       // Component ID
    uint8_t  msgid;        // Message ID
};

// ============================================================================
// Telemetry Data Structures
// ============================================================================

/**
 * @brief Heartbeat status
 */
struct TelemetryHeartbeat {
    uint8_t type;           // Vehicle type (MAV_TYPE)
    uint8_t autopilot;      // Autopilot type
    uint8_t base_mode;      // System mode bitmap
    uint32_t custom_mode;   // Flight mode
    uint8_t system_status;  // System state
};

/**
 * @brief Attitude telemetry
 */
struct TelemetryAttitude {
    uint32_t time_boot_ms;  // Timestamp
    float roll;             // Roll (deg)
    float pitch;            // Pitch (deg)
    float yaw;              // Yaw (deg)
    float rollspeed;        // Roll rate (deg/s)
    float pitchspeed;       // Pitch rate (deg/s)
    float yawspeed;         // Yaw rate (deg/s)
};

/**
 * @brief GPS position telemetry
 */
struct TelemetryGPS {
    uint32_t time_boot_ms;  // Timestamp
    int32_t  lat;           // Latitude * 1e7 (degE7)
    int32_t  lon;           // Longitude * 1e7 (degE7)
    int32_t  alt;           // Altitude (mm) MSL
    int32_t  relative_alt;  // Altitude above home (mm)
    uint16_t vx;            // Ground X speed (cm/s)
    uint16_t vy;            // Ground Y speed (cm/s)
    uint16_t vz;            // Ground Z speed (cm/s)
    uint16_t hdg;           // Heading (cdeg, 0-36000)
    uint8_t  fix_type;      // GPS fix type
    uint8_t  satellites;    // Satellites visible
};

/**
 * @brief Battery status telemetry
 */
struct TelemetryBattery {
    float voltage_v;           // Battery voltage
    float current_a;           // Current draw (-1 if unknown)
    int8_t remaining_pct;      // Remaining charge (%)
    uint32_t time_remaining_s; // Estimated remaining flight time (seconds)
};

/**
 * @brief Drone detection report (custom telemetry)
 */
struct BirdDetectionReport {
    uint32_t timestamp_ms;     // Detection timestamp
    float lat;                 // Detection latitude
    float lon;                 // Detection longitude
    float alt;                 // Detection altitude (m)
    uint8_t bird_count;        // Estimated bird count
    uint8_t bird_type;         // Bird classification (0=unknown, 1=sparrow, 2=pigeon...)
    float confidence;          // Detection confidence [0-1]
    float cluster_radius_m;    // Bird cluster radius
    char species_hint[32];     // Species hint string
};

// ============================================================================
// Telemetry Class
// ============================================================================

class Telemetry {
public:
    Telemetry();
    ~Telemetry() = default;

    /**
     * @brief Initialize telemetry stream
     * @param serial Reference to serial port (radio modem)
     * @param baud Radio baud rate
     * @param sysid MAVLink system ID
     */
    void begin(HardwareSerial &serial, uint32_t baud, uint8_t sysid = 1);

    /**
     * @brief Update telemetry - call at configured rate (1-10 Hz)
     */
    void update();

    /**
     * @brief Send heartbeat message
     */
    void sendHeartbeat(const TelemetryHeartbeat &hb);

    /**
     * @brief Send attitude message
     */
    void sendAttitude(const TelemetryAttitude &att);

    /**
     * @brief Send GPS position
     */
    void sendGPS(const TelemetryGPS &gps);

    /**
     * @brief Send battery status
     */
    void sendBattery(const TelemetryBattery &bat);

    /**
     * @brief Send status text message
     * @param severity MAV_SEVERITY level
     * @param text Status message (max 50 chars)
     */
    void sendStatusText(uint8_t severity, const char *text);

    /**
     * @brief Send custom bird detection report
     */
    void sendBirdDetection(const BirdDetectionReport &report);

    /**
     * @brief Send raw MAVLink packet
     */
    void sendRawPacket(const uint8_t *payload, uint8_t len,
                       uint8_t msgid, uint8_t compid);

    /**
     * @brief Process incoming MAVLink commands
     * @return true if a valid packet was received
     */
    bool receive();

    /**
     * @brief Set telemetry rates for each message type
     */
    void setStreamRate(uint8_t msgid, uint16_t rate_hz);

    /**
     * @brief Get last received command
     */
    uint16_t getLastCommand() const { return _last_command; }

    /**
     * @brief Get last command parameters
     */
    void getLastCommandParams(float &p1, float &p2, float &p3,
                               float &p4, float &p5, float &p6, float &p7) const;

    /**
     * @brief Check if radio link is alive
     * @param timeout_ms Timeout for link loss (default 5000ms)
     */
    bool isLinkAlive(uint32_t timeout_ms = 5000) const;

    /**
     * @brief Get packets sent/received stats
     */
    uint32_t getPacketsSent() const { return _packets_sent; }
    uint32_t getPacketsReceived() const { return _packets_rcvd; }

    /**
     * @brief Get RSSI estimate from radio
     */
    uint8_t getRSSI() const { return _rssi; }

    /**
     * @brief Set RSSI value (from external radio)
     */
    void setRSSI(uint8_t rssi) { _rssi = rssi; }

private:
    HardwareSerial *_serial;
    uint32_t _baud;
    uint8_t _sysid;
    uint8_t _seq;                       // Packet sequence counter
    uint32_t _last_send_ms;             // Last transmit timestamp
    uint32_t _last_recv_ms;             // Last receive timestamp
    uint32_t _packets_sent;
    uint32_t _packets_rcvd;
    uint8_t _rssi;                      // RSSI estimate

    // Stream rate scheduling
    uint16_t _heartbeat_rate;
    uint16_t _attitude_rate;
    uint16_t _gps_rate;
    uint16_t _battery_rate;
    uint32_t _last_heartbeat_ms;
    uint32_t _last_attitude_ms;
    uint32_t _last_gps_ms;
    uint32_t _last_battery_ms;

    // Last received command
    uint16_t _last_command;
    float _last_cmd_params[7];

    // Receive buffer
    uint8_t _rx_buffer[MAVLINK_MAX_PACKET_LEN];
    uint8_t _rx_index;
    bool _rx_in_progress;

    /**
     * @brief Compute MAVLink CRC16 checksum
     */
    static uint16_t crc16(const uint8_t *data, uint8_t len, uint8_t extra);

    /**
     * @brief Pack and send a MAVLink v1 packet
     */
    void packAndSend(uint8_t msgid, const uint8_t *payload, uint8_t len,
                     uint8_t compid = MAV_COMP_ID_AUTOPILOT1);

    /**
     * @brief Parse a received MAVLink packet
     */
    bool parsePacket();

    /**
     * @brief Handle received MAVLink command
     */
    void handleCommand(const uint8_t *payload, uint8_t len);

    /**
     * @brief Pack 32-bit float into bytes (little-endian)
     */
    static void packFloat(uint8_t *buf, float val);

    /**
     * @brief Pack 32-bit int into bytes (little-endian)
     */
    static void packInt32(uint8_t *buf, int32_t val);

    /**
     * @brief Pack 16-bit int into bytes (little-endian)
     */
    static void packUint16(uint8_t *buf, uint16_t val);
};

#endif // TELEMETRY_H
