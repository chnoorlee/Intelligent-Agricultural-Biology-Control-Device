/**
 * @file telemetry.cpp
 * @brief Telemetry implementation with MAVLink v1 protocol
 *
 * Handles MAVLink message serialization (packing) and deserialization
 * for air-to-ground communication over serial radio modems (3DR, SiK, etc.).
 * Supports heartbeat, attitude, GPS position, battery, status text,
 * and custom bird detection report messages.
 */

#include "telemetry.h"

// ============================================================================
// Constructor
// ============================================================================

Telemetry::Telemetry()
    : _serial(nullptr)
    , _baud(57600)
    , _sysid(1)
    , _seq(0)
    , _last_send_ms(0)
    , _last_recv_ms(0)
    , _packets_sent(0)
    , _packets_rcvd(0)
    , _rssi(0)
    , _heartbeat_rate(1)
    , _attitude_rate(10)
    , _gps_rate(5)
    , _battery_rate(1)
    , _last_heartbeat_ms(0)
    , _last_attitude_ms(0)
    , _last_gps_ms(0)
    , _last_battery_ms(0)
    , _last_command(0)
    , _rx_index(0)
    , _rx_in_progress(false)
{
    memset(_rx_buffer, 0, sizeof(_rx_buffer));
    memset(_last_cmd_params, 0, sizeof(_last_cmd_params));
}

// ============================================================================
// Initialization
// ============================================================================

void Telemetry::begin(HardwareSerial &serial, uint32_t baud, uint8_t sysid) {
    _serial = &serial;
    _baud = baud;
    _sysid = sysid;
    _serial->begin(baud);
}

// ============================================================================
// Packet Sending
// ============================================================================

void Telemetry::packAndSend(uint8_t msgid, const uint8_t *payload, uint8_t len,
                             uint8_t compid) {
    if (!_serial) return;

    // MAVLink v1 packet format:
    // STX(1) LEN(1) SEQ(1) SYS(1) COMP(1) MSG(1) PAYLOAD(0-255) CKA(1) CKB(1)
    uint8_t packet[MAVLINK_MAX_PACKET_LEN];
    uint8_t idx = 0;

    packet[idx++] = MAVLINK_STX;
    packet[idx++] = len;
    packet[idx++] = _seq++;
    packet[idx++] = _sysid;
    packet[idx++] = compid;
    packet[idx++] = msgid;

    // Copy payload
    if (len > 0 && payload != nullptr) {
        memcpy(&packet[idx], payload, len);
    }
    idx += len;

    // Compute CRC (over PAYLOAD only for MAVLink v1)
    uint16_t crc = crc16(&packet[1], idx - 1, 0x00);

    // Add CRC extra byte from the MAVLink CRC seed table lookup
    // For simplicity, we compute over the entire header+payload
    crc = crc16(packet + 1, idx - 1, 0x00);
    uint8_t extra_crc = 0;
    // MAVLink message-specific CRC extra byte (simplified)
    // In production, this uses a lookup table indexed by msgid
    // For this implementation, use a basic seed
    static const uint8_t crc_extras[] = {
        50,  // HEARTBEAT
        39,  // ATTITUDE
        35,  // GPS_RAW_INT
        39,  // ...
    };
    if (msgid < sizeof(crc_extras)) {
        extra_crc = crc_extras[msgid];
    }
    crc = crc16(packet + 1, idx - 1, extra_crc);

    packet[idx++] = crc & 0xFF;
    packet[idx++] = (crc >> 8) & 0xFF;

    _serial->write(packet, idx);
    _packets_sent++;
    _last_send_ms = millis();
}

// ============================================================================
// High-Level Message Senders
// ============================================================================

void Telemetry::sendHeartbeat(const TelemetryHeartbeat &hb) {
    uint8_t payload[9];
    memset(payload, 0, 9);

    packInt32(&payload[0], (int32_t)hb.custom_mode);
    payload[4] = hb.type;
    payload[5] = hb.autopilot;
    payload[6] = hb.base_mode;
    // uint32_t custom_mode already packed
    payload[7] = hb.system_status;
    payload[8] = 0;  // MAVLink version

    packAndSend(MAVLINK_MSG_ID_HEARTBEAT, payload, 9);
}

void Telemetry::sendAttitude(const TelemetryAttitude &att) {
    uint8_t payload[28];
    memset(payload, 0, 28);

    packInt32(&payload[0], (int32_t)att.time_boot_ms);
    packFloat(&payload[4], att.roll);
    packFloat(&payload[8], att.pitch);
    packFloat(&payload[12], att.yaw);
    packFloat(&payload[16], att.rollspeed);
    packFloat(&payload[20], att.pitchspeed);
    packFloat(&payload[24], att.yawspeed);

    packAndSend(MAVLINK_MSG_ID_ATTITUDE, payload, 28);
}

void Telemetry::sendGPS(const TelemetryGPS &gps) {
    uint8_t payload[30];
    memset(payload, 0, 30);

    packInt32(&payload[0], gps.lat);           // lat * 1e7
    packInt32(&payload[4], gps.lon);            // lon * 1e7
    packInt32(&payload[8], gps.alt);            // alt mm
    packInt32(&payload[12], gps.relative_alt);  // rel alt mm
    packUint16(&payload[16], gps.vx);           // vx cm/s
    packUint16(&payload[18], gps.vy);           // vy cm/s
    packUint16(&payload[20], gps.vz);           // vz cm/s
    packUint16(&payload[22], gps.hdg);          // heading cdeg
    // Remaining: fix_type, satellites_visible (fields 24-29)
    // These are packed in the full MAVLink GPS_RAW_INT message
    // Skipping extended fields for brevity - can be added

    packAndSend(MAVLINK_MSG_ID_GPS_RAW_INT, payload, 30);
}

void Telemetry::sendBattery(const TelemetryBattery &bat) {
    uint8_t payload[12];
    memset(payload, 0, 12);

    packFloat(&payload[0], bat.voltage_v);
    // MAVLink battery_status has more fields
    // current_consumed (int32_t), energy_consumed (int32_t)
    // temperature (int16_t)
    payload[8] = (uint8_t)bat.remaining_pct;
    // voltages[10] array...

    packAndSend(MAVLINK_MSG_ID_BATTERY_STATUS, payload, 12);
}

void Telemetry::sendStatusText(uint8_t severity, const char *text) {
    uint8_t payload[51];
    memset(payload, 0, 51);

    payload[0] = severity;
    if (text) {
        uint8_t len = min(strlen(text), (size_t)50);
        memcpy(&payload[1], text, len);
    }

    packAndSend(MAVLINK_MSG_ID_STATUSTEXT, payload, 51);
}

void Telemetry::sendBirdDetection(const BirdDetectionReport &report) {
    // Custom message format for bird detection reports
    // Using a MAVLink-like format with a custom ID (200+)
    uint8_t payload[52];
    memset(payload, 0, 52);

    packInt32(&payload[0], (int32_t)report.timestamp_ms);
    packFloat(&payload[4], report.lat);
    packFloat(&payload[8], report.lon);
    packFloat(&payload[12], report.alt);
    payload[16] = report.bird_count;
    payload[17] = report.bird_type;
    packFloat(&payload[18], report.confidence);
    packFloat(&payload[22], report.cluster_radius_m);

    // Pack species hint
    uint8_t hint_len = min(strlen(report.species_hint), (size_t)25);
    memcpy(&payload[26], report.species_hint, hint_len);

    // Use custom message ID 200 for bird detection
    packAndSend(200, payload, 52);
}

void Telemetry::sendRawPacket(const uint8_t *payload, uint8_t len,
                               uint8_t msgid, uint8_t compid) {
    packAndSend(msgid, payload, len, compid);
}

// ============================================================================
// Packet Reception
// ============================================================================

bool Telemetry::receive() {
    if (!_serial) return false;

    bool packet_ready = false;

    while (_serial->available() > 0) {
        uint8_t byte = _serial->read();

        if (!_rx_in_progress) {
            // Look for start byte
            if (byte == MAVLINK_STX || byte == MAVLINK_STX_V2) {
                _rx_in_progress = true;
                _rx_index = 0;
            }
            continue;
        }

        // Store byte
        if (_rx_index < MAVLINK_MAX_PACKET_LEN) {
            _rx_buffer[_rx_index++] = byte;
        }

        // Check if we have a complete packet
        // Minimum packet: MAGIC(1) + LEN(1) + SEQ(1) + SYS(1) + COMP(1) + MSG(1) = 6 header bytes
        if (_rx_index >= 6) {
            uint8_t expected_len = _rx_buffer[0] + 8; // payload + header(6) + crc(2)
            if (_rx_index >= expected_len) {
                // Full packet received
                packet_ready = parsePacket();
                _rx_in_progress = false;
                _rx_index = 0;
                _packets_rcvd++;
                _last_recv_ms = millis();
            }
        }
    }

    return packet_ready;
}

bool Telemetry::parsePacket() {
    // Validate checksum
    uint8_t payload_len = _rx_buffer[0];
    uint8_t total_len = payload_len + 6; // header bytes after magic

    if (_rx_index < total_len + 2) return false;

    // Extract CRC
    uint16_t received_crc = _rx_buffer[total_len] | (_rx_buffer[total_len + 1] << 8);

    // MAVLink CRC extra byte for this message
    uint8_t msgid = _rx_buffer[4];
    static const uint8_t crc_extras[] = {
        50, 39, 35, 24, 24, 155, 42, 52, 25, 33, 33, 33, 33, 33, 33,
        33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
        28, 97, 74, 0, 39, 126, 118, 76, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t extra = (msgid < sizeof(crc_extras)) ? crc_extras[msgid] : 0;

    uint16_t computed_crc = crc16(_rx_buffer, total_len, extra);

    if (computed_crc != received_crc) {
        return false;  // CRC mismatch
    }

    // Handle known commands
    if (msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
        handleCommand(&_rx_buffer[5], payload_len);  // payload starts after msgid
    }

    return true;
}

// ============================================================================
// Command Handling
// ============================================================================

void Telemetry::handleCommand(const uint8_t *payload, uint8_t len) {
    if (len < 28) return;

    // COMMAND_LONG: target_system(1), target_component(1), command(2), confirmation(1)
    // param1-7 (4 bytes each = 28)
    // Total: 33 bytes

    _last_command = payload[2] | (payload[3] << 8);

    // Extract float params
    for (int i = 0; i < 7; i++) {
        uint32_t raw;
        memcpy(&raw, &payload[5 + i * 4], 4);
        memcpy(&_last_cmd_params[i], &raw, 4);
    }
}

void Telemetry::getLastCommandParams(float &p1, float &p2, float &p3,
                                      float &p4, float &p5, float &p6,
                                      float &p7) const {
    p1 = _last_cmd_params[0];
    p2 = _last_cmd_params[1];
    p3 = _last_cmd_params[2];
    p4 = _last_cmd_params[3];
    p5 = _last_cmd_params[4];
    p6 = _last_cmd_params[5];
    p7 = _last_cmd_params[6];
}

// ============================================================================
// CRC16 Computation (MAVLink variant)
// ============================================================================

uint16_t Telemetry::crc16(const uint8_t *data, uint8_t len, uint8_t extra) {
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    // Include CRC extra byte
    crc ^= (uint16_t)extra << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc = crc << 1;
        }
    }

    return crc;
}

// ============================================================================
// Telemetry Update Scheduler
// ============================================================================

void Telemetry::update() {
    // Called by main loop - send packets at appropriate rates
    if (!_serial) return;

    uint32_t now = millis();

    // Heartbeat at configured rate (default 1Hz)
    if (now - _last_heartbeat_ms >= 1000 / max(_heartbeat_rate, (uint16_t)1)) {
        TelemetryHeartbeat hb;
        hb.type = MAV_TYPE_FIXED_WING;
        hb.autopilot = MAV_AUTOPILOT_GENERIC;
        hb.base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED |
                       MAV_MODE_FLAG_GUIDED_ENABLED |
                       MAV_MODE_FLAG_AUTO_ENABLED |
                       MAV_MODE_FLAG_SAFETY_ARMED;
        hb.custom_mode = 2;  // AUTO mode
        hb.system_status = MAV_STATE_ACTIVE;
        sendHeartbeat(hb);
        _last_heartbeat_ms = now;
    }

    // Note: Attitude, GPS, Battery messages are sent by the flight controller
    // when data is updated, not on a fixed schedule here.
}

void Telemetry::setStreamRate(uint8_t msgid, uint16_t rate_hz) {
    switch (msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:
            _heartbeat_rate = rate_hz;
            break;
        case MAVLINK_MSG_ID_ATTITUDE:
            _attitude_rate = rate_hz;
            break;
        case MAVLINK_MSG_ID_GPS_RAW_INT:
            _gps_rate = rate_hz;
            break;
        case MAVLINK_MSG_ID_BATTERY_STATUS:
            _battery_rate = rate_hz;
            break;
        default:
            break;
    }
}

bool Telemetry::isLinkAlive(uint32_t timeout_ms) const {
    uint32_t now = millis();
    return (now - _last_recv_ms) < timeout_ms;
}

// ============================================================================
// Packing Helpers (Little-Endian)
// ============================================================================

void Telemetry::packFloat(uint8_t *buf, float val) {
    memcpy(buf, &val, 4);
}

void Telemetry::packInt32(uint8_t *buf, int32_t val) {
    memcpy(buf, &val, 4);
}

void Telemetry::packUint16(uint8_t *buf, uint16_t val) {
    memcpy(buf, &val, 2);
}

// ============================================================================
// Public Send (unified interface)
// ============================================================================

/**
 * @brief Update telemetry - overloaded to accept external data
 * This is called by the main flight controller update loop.
 */
// The update() method defined above is the internal scheduler.
// Data-driven sends are done via the public send methods.
