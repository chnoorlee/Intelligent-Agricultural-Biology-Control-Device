#include "WebApi.h"
#include <WiFi.h>

static WebServer* httpServer = nullptr;
static WebSocketsServer* webSocket = nullptr;
static DroneConfig* config = nullptr;
static ImuState* imuState = nullptr;
static ControlInput* controlInput = nullptr;
static SafetyState* safetyState = nullptr;
static MotorState* motorState = nullptr;
static FlightState* flightState = nullptr;

static void handleRoot() {
    httpServer->send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
}

static void handleApiCfg() {
    httpServer->send(200, "application/json", buildConfigJson(*config));
}

static void reconnectWifiIfNeeded(const char* prevSsid, bool passUpdated, bool passCleared) {
    bool ssidChanged = strcmp(prevSsid, config->wifiSsid) != 0;
    bool passChanged = passUpdated || passCleared;
    if (!ssidChanged && !passChanged)
        return;
    WiFi.disconnect(true, false);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config->wifiSsid, config->wifiPass);
}

static void handleSettingsSave() {
    if (!httpServer->hasArg("ssid")) {
        httpServer->send(400, "text/plain", "missing ssid");
        return;
    }

    char prevSsid[33];
    strncpy(prevSsid, config->wifiSsid, sizeof(prevSsid));
    prevSsid[sizeof(prevSsid) - 1] = 0;

    const int omfl = config->pinMotorFL, omfr = config->pinMotorFR, ombl = config->pinMotorBL, ombr = config->pinMotorBR;
    const int opwmf = config->pwmFrequency, opwmr = (int)config->pwmResolutionBits;
    const int osda = config->wireSda, oscl = config->wireScl;

    Preferences prefs;
    if (!prefs.begin("dronecfg", false)) {
        httpServer->send(500, "text/plain", "NVS open failed");
        return;
    }

    String newSsid = httpServer->arg("ssid");
    newSsid.trim();
    if (newSsid.length() == 0) {
        prefs.end();
        httpServer->send(400, "text/plain", "SSID cannot be empty");
        return;
    }
    if (newSsid.length() > 32) newSsid = newSsid.substring(0, 32);
    prefs.putString("ssid", newSsid);
    strncpy(config->wifiSsid, newSsid.c_str(), sizeof(config->wifiSsid) - 1);
    config->wifiSsid[sizeof(config->wifiSsid) - 1] = 0;

    bool passUpdated = false;
    bool passCleared = false;
    if (httpServer->hasArg("clear_pw") && httpServer->arg("clear_pw") == "1") {
        prefs.putString("pass", "");
        config->wifiPass[0] = 0;
        passCleared = true;
    } else if (httpServer->hasArg("password") && httpServer->arg("password").length() > 0) {
        String np = httpServer->arg("password");
        if (np.length() > 64) np = np.substring(0, 64);
        prefs.putString("pass", np);
        strncpy(config->wifiPass, np.c_str(), sizeof(config->wifiPass) - 1);
        config->wifiPass[sizeof(config->wifiPass) - 1] = 0;
        passUpdated = true;
    }

    config->pinMotorFL = clampPin(httpServer->arg("mfl").toInt());
    config->pinMotorFR = clampPin(httpServer->arg("mfr").toInt());
    config->pinMotorBL = clampPin(httpServer->arg("mbl").toInt());
    config->pinMotorBR = clampPin(httpServer->arg("mbr").toInt());
    prefs.putInt("mfl", config->pinMotorFL);
    prefs.putInt("mfr", config->pinMotorFR);
    prefs.putInt("mbl", config->pinMotorBL);
    prefs.putInt("mbr", config->pinMotorBR);

    config->pwmFrequency = constrain(httpServer->arg("pwmf").toInt(), 1000, 200000);
    config->pwmResolutionBits = (uint8_t)constrain(httpServer->arg("pwmr").toInt(), 8, 13);
    prefs.putInt("pwmf", config->pwmFrequency);
    prefs.putInt("pwmr", config->pwmResolutionBits);

    config->wireSda = clampPin(httpServer->arg("sda").toInt());
    config->wireScl = clampPin(httpServer->arg("scl").toInt());
    prefs.putInt("sda", config->wireSda);
    prefs.putInt("scl", config->wireScl);

    config->madgwickBeta = constrain(httpServer->arg("beta").toFloat(), 0.01f, 1.0f);
    config->maxTiltDeg = constrain(httpServer->arg("maxtilt").toFloat(), 5.0f, 55.0f);
    config->maxYawRateDps = constrain(httpServer->arg("maxyaw").toFloat(), 30.0f, 360.0f);
    prefs.putFloat("beta", config->madgwickBeta);
    prefs.putFloat("maxtilt", config->maxTiltDeg);
    prefs.putFloat("maxyaw", config->maxYawRateDps);

    config->kpRoll = constrain(httpServer->arg("kpr").toFloat(), 0.0f, 20.0f);
    config->kiRoll = constrain(httpServer->arg("kir").toFloat(), 0.0f, 5.0f);
    config->kdRoll = constrain(httpServer->arg("kdr").toFloat(), 0.0f, 20.0f);
    config->kpPitch = constrain(httpServer->arg("kpp").toFloat(), 0.0f, 20.0f);
    config->kiPitch = constrain(httpServer->arg("kip").toFloat(), 0.0f, 5.0f);
    config->kdPitch = constrain(httpServer->arg("kdp").toFloat(), 0.0f, 20.0f);
    config->kpYaw = constrain(httpServer->arg("kpy").toFloat(), 0.0f, 20.0f);
    config->kiYaw = constrain(httpServer->arg("kiy").toFloat(), 0.0f, 5.0f);
    config->kdYaw = constrain(httpServer->arg("kdy").toFloat(), 0.0f, 20.0f);
    prefs.putFloat("kpr", config->kpRoll);
    prefs.putFloat("kir", config->kiRoll);
    prefs.putFloat("kdr", config->kdRoll);
    prefs.putFloat("kpp", config->kpPitch);
    prefs.putFloat("kip", config->kiPitch);
    prefs.putFloat("kdp", config->kdPitch);
    prefs.putFloat("kpy", config->kpYaw);
    prefs.putFloat("kiy", config->kiYaw);
    prefs.putFloat("kdy", config->kdYaw);

    config->pidEnabled = httpServer->arg("piden").toInt() != 0;
    config->idleThrPct = constrain(httpServer->arg("idle").toFloat(), 0.0f, MAX_IDLE_THR_PCT);
    config->minThrottlePct = constrain(httpServer->arg("minthr").toFloat(), 0.0f, 40.0f);
    config->maxThrottlePct = constrain(httpServer->arg("maxthr").toFloat(), 40.0f, 100.0f);
    if (config->maxThrottlePct < config->minThrottlePct) config->maxThrottlePct = config->minThrottlePct;
    config->armThrottlePct = constrain(httpServer->arg("armthr").toFloat(), 0.0f, 20.0f);
    config->maxArmAngleDeg = constrain(httpServer->arg("armang").toFloat(), 5.0f, 45.0f);
    config->yawMixSign = httpServer->arg("yawsign").toInt() < 0 ? -1 : 1;
    config->failsafeTimeoutMs = (uint16_t)constrain(httpServer->arg("fstime").toInt(), 100, 5000);
    config->failsafeDisarmMs = (uint16_t)constrain(httpServer->arg("fsdis").toInt(), 100, 10000);

    prefs.putBool("piden", config->pidEnabled);
    prefs.putFloat("idle", config->idleThrPct);
    prefs.putFloat("minthr", config->minThrottlePct);
    prefs.putFloat("maxthr", config->maxThrottlePct);
    prefs.putFloat("armthr", config->armThrottlePct);
    prefs.putFloat("armang", config->maxArmAngleDeg);
    prefs.putInt("yawsign", config->yawMixSign);
    prefs.putInt("fstime", config->failsafeTimeoutMs);
    prefs.putInt("fsdis", config->failsafeDisarmMs);
    prefs.end();

    reconnectWifiIfNeeded(prevSsid, passUpdated, passCleared);

    String msg = "Saved.";
    if (config->pinMotorFL != omfl || config->pinMotorFR != omfr || config->pinMotorBL != ombl || config->pinMotorBR != ombr ||
        config->pwmFrequency != opwmf || config->pwmResolutionBits != opwmr ||
        config->wireSda != osda || config->wireScl != oscl) {
        msg += " Restart the board for motor pins, PWM frequency/resolution, or I2C pins to take effect.";
    }
    httpServer->send(200, "text/plain", msg);
}

static void handleRestart() {
    httpServer->send(200, "text/plain", "Restarting...");
    delay(50);
    httpServer->client().stop();
    delay(100);
    ESP.restart();
}

static void handleWS(uint8_t num, uint8_t* payload, size_t len) {
    String msg = String((char*)payload).substring(0, len);

    if (msg == "ARM") {
        if (tryArm(*safetyState, *config, *imuState, *controlInput)) {
            motorState->motorTestOverride = false;
        }
        return;
    }

    if (msg == "DISARM") {
        disarm(*safetyState);
        motorState->motorTestOverride = false;
        stopMotors(*config, *motorState);
        resetFlightController(*flightState);
        return;
    }

    if (msg == "CALIBRATE") {
        disarm(*safetyState);
        motorState->motorTestOverride = false;
        resetControlInput(*controlInput);
        resetFlightController(*flightState);
        stopMotors(*config, *motorState);
        bool ok = calibrateIMU(*imuState);
        webSocket->sendTXT(num, ok ? "{\"type\":\"cal\",\"ok\":1}" : "{\"type\":\"cal\",\"ok\":0}");
        return;
    }

    if (msg.startsWith("JOY:")) {
        msg.remove(0, 4);
        int a = msg.indexOf(',');
        int b = msg.indexOf(',', a + 1);
        int c = msg.indexOf(',', b + 1);
        if (a < 0 || b < 0 || c < 0) return;

        controlInput->yaw = constrain(msg.substring(0, a).toFloat(), -1.0f, 1.0f);
        controlInput->throttle = constrain(msg.substring(a + 1, b).toFloat(), -1.0f, 1.0f);
        controlInput->roll = constrain(msg.substring(b + 1, c).toFloat(), -1.0f, 1.0f);
        controlInput->pitch = constrain(msg.substring(c + 1).toFloat(), -1.0f, 1.0f);
        controlInput->lastPacketMs = millis();
        controlInput->packetCount++;
        return;
    }

    if (msg.startsWith("PID:")) {
        msg.remove(0, 4);
        int a = msg.indexOf(',');
        int b = msg.indexOf(',', a + 1);
        if (a < 0 || b < 0) return;

        float newKp = msg.substring(0, a).toFloat();
        float newKi = msg.substring(a + 1, b).toFloat();
        float newKd = msg.substring(b + 1).toFloat();

        if (isfinite(newKp) && isfinite(newKi) && isfinite(newKd)) {
            config->kpRoll = config->kpPitch = constrain(newKp, 0.0f, 20.0f);
            config->kiRoll = config->kiPitch = constrain(newKi, 0.0f, 5.0f);
            config->kdRoll = config->kdPitch = constrain(newKd, 0.0f, 20.0f);
        }
        return;
    }

    if (msg.startsWith("USE_PID:")) {
        String tail = msg.substring(8);
        tail.trim();
        config->pidEnabled = (tail.toInt() != 0);
        if (!config->pidEnabled) {
            flightState->iRoll = flightState->iPitch = flightState->iYaw = 0.0f;
        }
        return;
    }

    if (msg.startsWith("IDLE:")) {
        float v = msg.substring(5).toFloat();
        if (isfinite(v)) config->idleThrPct = constrain(v, 0.0f, MAX_IDLE_THR_PCT);
        return;
    }

    if (msg == "ESTOP") {
        emergencyStop(*safetyState, *controlInput);
        motorState->motorTestOverride = false;
        resetFlightController(*flightState);
        stopMotors(*config, *motorState);
        return;
    }

    if (msg.startsWith("MOTORS:")) {
        msg.remove(0, 8);
        int a = msg.indexOf(',');
        int b = msg.indexOf(',', a + 1);
        int c = msg.indexOf(',', b + 1);
        if (a < 0 || b < 0 || c < 0) return;

        uint32_t maxPwm = pwmMaxValue(*config);
        uint32_t m1 = constrain(msg.substring(0, a).toInt(), 0, (int)maxPwm);
        uint32_t m2 = constrain(msg.substring(a + 1, b).toInt(), 0, (int)maxPwm);
        uint32_t m3 = constrain(msg.substring(b + 1, c).toInt(), 0, (int)maxPwm);
        uint32_t m4 = constrain(msg.substring(c + 1).toInt(), 0, (int)maxPwm);
        motorState->motorTestOverride = !safetyState->armed;
        motorState->lastMotorTestMs = millis();
        writeMotorsRaw(*config, *motorState, m1, m2, m3, m4);
        return;
    }
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (type == WStype_TEXT)
        handleWS(num, payload, len);
}

void initWebApi(WebServer& server, WebSocketsServer& ws, DroneConfig& cfg, ImuState& imu, ControlInput& input, SafetyState& safety, MotorState& motors, FlightState& flight) {
    httpServer = &server;
    webSocket = &ws;
    config = &cfg;
    imuState = &imu;
    controlInput = &input;
    safetyState = &safety;
    motorState = &motors;
    flightState = &flight;

    server.on("/", handleRoot);
    server.on("/api/cfg", HTTP_GET, handleApiCfg);
    server.on("/settings", HTTP_POST, handleSettingsSave);
    server.on("/restart", HTTP_POST, handleRestart);
    server.begin();

    ws.begin();
    ws.onEvent(onWsEvent);
}

void sendTelemetry(WebSocketsServer& ws, const DroneConfig& cfg, const ImuState& imu, const ControlInput& input, const SafetyState& safety, const MotorState& motors, const FlightState& flight) {
    String json = "{";
    json += "\"type\":\"telem\",";
    json += "\"ax\":" + String(imu.roll, 2) + ",";
    json += "\"ay\":" + String(imu.pitch, 2) + ",";
    json += "\"az\":" + String(imu.yaw, 2) + ",";
    json += "\"m1\":" + String(motors.value[MOTOR_FL_INDEX]) + ",";
    json += "\"m2\":" + String(motors.value[MOTOR_FR_INDEX]) + ",";
    json += "\"m3\":" + String(motors.value[MOTOR_BL_INDEX]) + ",";
    json += "\"m4\":" + String(motors.value[MOTOR_BR_INDEX]) + ",";
    json += "\"pwmMax\":" + String(pwmMaxValue(cfg)) + ",";
    json += "\"arm\":" + String(safety.armed ? 1 : 0) + ",";
    json += "\"failsafe\":" + String(safety.failsafeActive ? 1 : 0) + ",";
    json += "\"armReason\":\"" + String(safety.armBlockedReason) + "\",";
    json += "\"linkAge\":" + String(linkAgeMs(input)) + ",";
    json += "\"cal\":" + String(imu.calibrated ? 1 : 0) + ",";
    json += "\"kp\":" + String(cfg.kpRoll, 2) + ",";
    json += "\"ki\":" + String(cfg.kiRoll, 2) + ",";
    json += "\"kd\":" + String(cfg.kdRoll, 2) + ",";
    json += "\"pidEn\":" + String(cfg.pidEnabled ? 1 : 0) + ",";
    json += "\"idlePct\":" + String(cfg.idleThrPct, 1) + ",";
    json += "\"loopHz\":" + String(flight.loopHz, 1) + ",";
    json += "\"pkt\":" + String(input.packetCount) + ",";
    json += "\"pktMiss\":" + String(input.packetGapMisses);
    json += "}";

    ws.broadcastTXT(json);
}
