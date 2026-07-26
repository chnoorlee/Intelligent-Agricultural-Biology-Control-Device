# Flight Control System - Fixed-Wing UAV

## Overview

Complete flight controller firmware for fixed-wing bird deterrent UAVs. Implements cascaded PID attitude control, GPS waypoint navigation, servo/ESC PWM output, Madgwick-filtered IMU attitude estimation, MAVLink telemetry, and comprehensive failsafe monitoring.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Flight Controller                   │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐  │
│  │ Navigation   │→│ Attitude     │→│ Control   │  │
│  │ (outer loop) │  │ (inner loop) │  │ Mixer     │→ Servos
│  └──────────────┘  └──────────────┘  └───────────┘  │
│         ↑                ↑                │         │
│    ┌────────┐       ┌────────┐       ┌────────┐    │
│    │  GPS   │       │  IMU   │       │Telemetry│→ 数传
│    └────────┘       └────────┘       └────────┘    │
│                                                      │
│              ┌──────────────────┐                    │
│              │   Fail Safe      │→ Override          │
│              └──────────────────┘                    │
└─────────────────────────────────────────────────────┘
```

## Modules

| File | Description |
|------|-------------|
| `flight_controller.h/.cpp` | Main flight controller: cascaded PID, 7 flight modes, auto/manual switching |
| `gps_navigation.h/.cpp` | GPS waypoint navigation: NMEA parsing, bearing/distance, geofence, mission management |
| `servo_control.h/.cpp` | Servo/ESC PWM control: 5 mixer types (standard, elevon, V-tail, flaperon), calibration |
| `imu_sensor.h/.cpp` | IMU attitude estimation: MPU6050/9250, Madgwick AHRS, calibration routines |
| `telemetry.h/.cpp` | MAVLink v1 telemetry: heartbeat, attitude, GPS, battery, custom bird detection reports |
| `fail_safe.h/.cpp` | Failsafe system: low battery RTL, RC loss, GPS loss, geofence, vibration, IMU failure |

## Flight Modes

| Mode | Description |
|------|-------------|
| MANUAL | Direct RC passthrough to servos |
| STABILIZE | Attitude stabilization with RC input |
| AUTO | Full autonomous waypoint navigation |
| RTL | Return to Launch (home position) |
| LOITER | Circle at current position |
| LAND | Auto-landing sequence |
| FAILSAFE | Emergency failsafe descent |

## PID Control Architecture

The controller uses cascaded PID loops:

1. **Navigation (outer)**: Position/heading errors → desired roll/pitch
2. **Attitude (middle)**: Angle errors → desired angular rates
3. **Rate (inner)**: Angular rate errors → servo commands

Each PID includes:
- Integral anti-windup (clamping)
- Derivative low-pass filtering (derivative-on-measurement)
- Output saturation

## Control Mixing

Supports 5 airframe configurations:
- **Standard**: Aileron, Elevator, Throttle, Rudder
- **Elevon**: Delta wing / flying wing
- **V-Tail**: Combined rudder+elevator surfaces
- **Dual Aileron**: Independent left/right ailerons
- **Flaperon**: Flaps + aileron combined

## Hardware Requirements

- **MCU**: ESP32 (dual-core 240MHz) or STM32F405
- **IMU**: MPU6050 or MPU9250 (I2C)
- **GPS**: u-blox NEO-M8N or NEO-6M (UART, 9600-115200 baud)
- **Barometer**: BMP280 (I2C, for altitude)
- **Radio**: 3DR/SiK telemetry radio (UART, 57600 baud)
- **RC Receiver**: SBUS/IBUS/PPM (serial or timer capture)
- **Servos/ESC**: Standard 50Hz PWM servos, 2-4S ESC

## Build & Upload

```bash
# Using PlatformIO
cd large-farm-version/flight-control

# Build for ESP32
pio run -e esp32dev

# Build for STM32F405
pio run -e stm32f405

# Upload to ESP32
pio run -e esp32dev -t upload

# Monitor serial output
pio device monitor -b 115200
```

## Pin Mapping (ESP32)

| Function | GPIO | Notes |
|----------|------|-------|
| Aileron R | 13 | Servo PWM |
| Elevator | 12 | Servo PWM |
| Throttle | 14 | ESC PWM |
| Rudder | 27 | Servo PWM |
| Aileron L | 26 | Servo PWM |
| Flaps | 25 | Servo PWM |
| Mode LED | 33 | Status indicator |
| GPS TX | 17 | UART2 |
| GPS RX | 16 | UART2 |
| Radio TX | 4 | UART1 (telemetry) |
| Radio RX | 5 | UART1 (telemetry) |
| RC RX | 34 | SBUS signal |
| I2C SDA | 21 | IMU + Baro |
| I2C SCL | 22 | IMU + Baro |
| Buzzer | 32 | Warning buzzer |
| Battery ADC | 35 | Voltage divider |

## Failsafe Thresholds

| Condition | Action | Default Threshold |
|-----------|--------|-------------------|
| Low Battery Warn | Warning only | 10.8V (3S) |
| Low Battery Critical | RTL | 10.2V (3S) |
| Battery Minimum | Land immediately | 9.9V (3S) |
| RC Signal Lost | RTL (after 2s) | Timeout 2000ms |
| GPS Lost | Loiter | Timeout 5000ms |
| Geofence Breach | RTL | 500m radius / 120m alt |
| IMU Failure | Land immediately | Sensor unhealthy |
| High Vibration | Land | >4g |

## Safety Features

- **Arming check**: Battery voltage, IMU calibration, GPS fix verified before arming
- **Throttle lock**: Zero throttle when disarmed
- **Geofence**: Virtual fence with automatic RTL
- **RC failover**: Graceful transition to auto modes on signal loss
- **Battery estimation**: Coulomb counting + voltage-based SoC
- **Vibration monitoring**: Detects prop imbalance / structural issues
