<div align="center">

<div align="center">
  <img src="https://github.com/user-attachments/assets/a30bde48-e39d-4b11-8749-3401bcb82a68" width="150">
  <h1><span>ESP32-Drone</span></h1>
</div>

  <p>
    ESP32-Drone - Wi‑Fi quadcopter flight stack with browser UI
  </p>
   
<!-- Badges -->
<a href="https://github.com/cifertech/ESP32-Drone" title="Go to GitHub repo"><img src="https://img.shields.io/static/v1?label=cifertech&message=ESP32-Drone&color=purple&logo=github" alt="cifertech - ESP32-Drone"></a>
<a href="https://github.com/cifertech/ESP32-Drone"><img src="https://img.shields.io/github/stars/cifertech/ESP32-Drone?style=social" alt="stars - ESP32-Drone"></a>
<a href="https://github.com/cifertech/ESP32-Drone"><img src="https://img.shields.io/github/forks/cifertech/ESP32-Drone?style=social" alt="forks - ESP32-Drone"></a>
[![Platform](https://img.shields.io/badge/Platform-ESP32-FF6600?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
   
<h4>
    <a href="https://twitter.com/cifertech1">TWITTER</a>
  <span> · </span>
    <a href="https://www.instagram.com/cifertech/">INSTAGRAM</a>
  <span> · </span>
    <a href="https://www.youtube.com/c/techcifer">YOUTUBE</a>
  <span> · </span>
    <a href="https://cifertech.net/">WEBSITE</a>
  </h4>
</div> 
 
<br />
<div>&nbsp;</div>

## 🌟 About the Project

Welcome to **ESP32-Drone**, an open-source flight controller firmware for ESP32-based mini quads. It pairs **MPU6050** attitude sensing with a **Madgwick** filter, **attitude PID** stabilization at **250 Hz**, and a **dark-themed web control panel** served over Wi‑Fi. Joystick input, tuning, motor checks, and configuration all run in the browser via **HTTP** and **WebSockets**.

> ⚠️ **Safety:** Flying multirotors can cause injury or property damage. Use only where permitted, with propellers removed during bench testing when appropriate, and never arm unless you understand the controls. This project is for **education and hobby use** — you are responsible for compliance with local laws and safe operation.

> Replace factory Wi‑Fi credentials in `Firmware.ino` (or save new ones in **Settings**) before deploying.

<img width="1920" height="1080" alt="MVI_8465 00_05_06_05 Still029" src="https://github.com/user-attachments/assets/3e421a6b-8b1e-4d59-a7c7-864fd76ae00a" />



<div>&nbsp;</div>

## 🎯 Features

#### 🎮 Flight & control

- **Virtual sticks** — Roll, pitch, yaw, throttle over WebSocket (`JOY:`)
- **ARM / DISARM** — Props enabled only when armed; instant **ESTOP**
- **IMU calibrate** — `CALIBRATE` over WebSocket (drone still on a level surface)
- **Angle mode** — Sticks command target tilt (within configurable **max tilt**); optional **direct mix** when PID is off

#### 🧠 Stabilization & sensors

- **MPU6050** on I2C (default SDA **21**, SCL **22**; NVS configurable)
- **Madgwick AHRS** with adjustable **beta**
- **PID loop** — Live **Kp / Ki / Kd** from the **PID** tab; enable/disable stabilization without reflash
- **Idle throttle %** — Optional minimum throttle at zero stick when armed (0–40%)

#### 📡 Connectivity & UI

- **Wi‑Fi STA** — Connects using SSID/password from NVS or sketch defaults
- **Web UI** (`/`) — Single-page app: **Flight**, **Telemetry**, **PID**, **Motor Test**, **Settings**
- **Telemetry** — ~20 Hz JSON: attitude, motor PWM, arm state, PID gains, loop rate, link packet stats
- **Motor test** — Per-motor PWM sliders when **disarmed** (with short hold after release)

#### ⚙️ Configuration (NVS `dronecfg`)

- Wi‑Fi SSID and password (optional clear password)
- Motor GPIO pins, **PWM frequency** and **resolution**
- I2C pins, Madgwick **beta**, **max tilt** degrees
- **`/restart`** POST for reboot after pin changes

<img width="1920" height="913" alt="MVI_8465 00_05_30_09 Still031" src="https://github.com/user-attachments/assets/5ef644b7-b239-4e07-9f86-670e4a253c43" />

<div>&nbsp;</div>

## 🔧 Hardware Overview

> ⚠️ **PCB / hardware risk:** This board is a **custom, experimental PCB** — it may have layout or wiring issues and is **not guaranteed** for any use case. **Check polarity, power, and grounds** before applying voltage or flying. Firmware cannot fix bad hardware. **No warranty — use at your own risk.**

Typical build:

| Block | Notes |
|--------|--------|
| **MCU** | ESP32 (Arduino-ESP32 core) |
| **IMU** | MPU6050 @ `0x68`, I2C |
| **Motors** | Four brushed (or ESC) outputs — **X quad** mix: FL, FR, BL, BR |
| **Wi‑Fi** | Antenna as per your module; set credentials for your network |

Default GPIO pins in firmware (change in sketch or **Settings** + restart):

| Motor | Default pin |
|--------|-------------|
| Front left | 33 |
| Front right | 4 |
| Back left | 25 |
| Back right | 32 |

<img width="1919" height="1080" alt="IMG_0341" src="https://github.com/user-attachments/assets/d097c4f1-2983-4809-a97f-a6025d825d13" />


<div>&nbsp;</div>


## 🛠️ Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) or **Arduino CLI** with [esp32 board support](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- Libraries (install via Library Manager where possible):

  - **WiFi**, **WebServer**, **WebSocketsServer** (by Markus Sattler / links2004), **Wire**, **Preferences** — as used in `Drone.ino`
<div>&nbsp;</div>

## 📥 Flashing

1. Open `Drone.ino` in Arduino IDE (or add this folder as a sketch).
2. Select your **ESP32 board** and correct **COM port**.
3. Set `DEF_WIFI_SSID` / `DEF_WIFI_PASS` (or configure later in the web **Settings** tab).
4. **Upload** the sketch.
5. Open **Serial Monitor** @ **115200** — note the **IP address** after Wi‑Fi connects.
6. On your phone or PC browser: `http://<IP>/` — use **Flight** to connect WebSocket (port **81**); the page handles this when served from the device.

Motor / I2C / PWM changes from **Settings** require **Save** then **Restart** when prompted.
<div>&nbsp;</div>

<!-- License --> 
## :warning: License
 
Distributed under the MIT License. See LICENSE.txt for more information.


<!-- Contact -->
## :handshake: Contact 

▶ Support me on Patreon [patreon.com/cifertech](https://www.patreon.com/cifertech)

CiferTech - [@twitter](https://twitter.com/techcifer) - CiferTech@gmali.com

Project Link: [https://github.com/cifertech/ESP32-DIV](https://github.com/cifertech/ESP32-Drone)


<!-- Support & Contributions -->
## 💬 Support & Contributions

- 💬 Found a bug or have a feature request? Open an [Issue](https://github.com/cifertech/ESP32-Drone/issues)
- ⭐ Like the project? Star the repo!
- 🛠 Want to contribute? Fork it and submit a pull request.
