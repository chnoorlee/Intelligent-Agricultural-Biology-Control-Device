#include "Config.h"

static const char* const DEF_WIFI_SSID = "CiferTech";
static const char* const DEF_WIFI_PASS = "1234567891";
static const int DEF_MOTOR_FL = 33;
static const int DEF_MOTOR_FR = 4;
static const int DEF_MOTOR_BL = 25;
static const int DEF_MOTOR_BR = 32;
static const int DEF_PWM_FREQ = 16000;
static const int DEF_PWM_RES = 8;
static const int DEF_I2C_SDA = 21;
static const int DEF_I2C_SCL = 22;
static const float DEF_BETA = 0.15f;
static const float DEF_MAX_TILT = 25.0f;
static const float DEF_MAX_YAW_RATE = 90.0f;

static String jsonEscape(const String& in) {
    String o;
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

int clampPin(int v) {
    if (v < 0) return 0;
    if (v > 39) return 39;
    return v;
}

uint32_t pwmMaxValue(const DroneConfig& cfg) {
    return (1UL << cfg.pwmResolutionBits) - 1UL;
}

void setDefaultConfig(DroneConfig& cfg) {
    strncpy(cfg.wifiSsid, DEF_WIFI_SSID, sizeof(cfg.wifiSsid) - 1);
    cfg.wifiSsid[sizeof(cfg.wifiSsid) - 1] = 0;
    strncpy(cfg.wifiPass, DEF_WIFI_PASS, sizeof(cfg.wifiPass) - 1);
    cfg.wifiPass[sizeof(cfg.wifiPass) - 1] = 0;

    cfg.pinMotorFL = DEF_MOTOR_FL;
    cfg.pinMotorFR = DEF_MOTOR_FR;
    cfg.pinMotorBL = DEF_MOTOR_BL;
    cfg.pinMotorBR = DEF_MOTOR_BR;
    cfg.pwmFrequency = DEF_PWM_FREQ;
    cfg.pwmResolutionBits = DEF_PWM_RES;
    cfg.wireSda = DEF_I2C_SDA;
    cfg.wireScl = DEF_I2C_SCL;

    cfg.madgwickBeta = DEF_BETA;
    cfg.maxTiltDeg = DEF_MAX_TILT;
    cfg.maxYawRateDps = DEF_MAX_YAW_RATE;

    cfg.kpRoll = 2.0f;
    cfg.kiRoll = 0.04f;
    cfg.kdRoll = 1.0f;
    cfg.kpPitch = 2.0f;
    cfg.kiPitch = 0.04f;
    cfg.kdPitch = 1.0f;
    cfg.kpYaw = 1.0f;
    cfg.kiYaw = 0.008f;
    cfg.kdYaw = 0.0f;
    cfg.pidEnabled = true;

    cfg.idleThrPct = 0.0f;
    cfg.minThrottlePct = 0.0f;
    cfg.maxThrottlePct = 100.0f;
    cfg.armThrottlePct = 5.0f;
    cfg.maxArmAngleDeg = 25.0f;

    cfg.yawMixSign = 1;
    cfg.failsafeTimeoutMs = 500;
    cfg.failsafeDisarmMs = 1500;
}

void loadSettingsFromNvs(DroneConfig& cfg) {
    setDefaultConfig(cfg);

    Preferences prefs;
    if (!prefs.begin("dronecfg", true))
        return;

    String s = prefs.getString("ssid", "");
    if (s.length() > 0) {
        strncpy(cfg.wifiSsid, s.c_str(), sizeof(cfg.wifiSsid) - 1);
        cfg.wifiSsid[sizeof(cfg.wifiSsid) - 1] = 0;
    }

    String pw = prefs.getString("pass", "");
    if (pw.length() > 0) {
        strncpy(cfg.wifiPass, pw.c_str(), sizeof(cfg.wifiPass) - 1);
        cfg.wifiPass[sizeof(cfg.wifiPass) - 1] = 0;
    }

    cfg.pinMotorFL = clampPin(prefs.getInt("mfl", cfg.pinMotorFL));
    cfg.pinMotorFR = clampPin(prefs.getInt("mfr", cfg.pinMotorFR));
    cfg.pinMotorBL = clampPin(prefs.getInt("mbl", cfg.pinMotorBL));
    cfg.pinMotorBR = clampPin(prefs.getInt("mbr", cfg.pinMotorBR));

    cfg.pwmFrequency = constrain(prefs.getInt("pwmf", cfg.pwmFrequency), 1000, 200000);
    cfg.pwmResolutionBits = (uint8_t)constrain(prefs.getInt("pwmr", (int)cfg.pwmResolutionBits), 8, 13);
    cfg.wireSda = clampPin(prefs.getInt("sda", cfg.wireSda));
    cfg.wireScl = clampPin(prefs.getInt("scl", cfg.wireScl));

    cfg.madgwickBeta = constrain(prefs.getFloat("beta", cfg.madgwickBeta), 0.01f, 1.0f);
    cfg.maxTiltDeg = constrain(prefs.getFloat("maxtilt", cfg.maxTiltDeg), 5.0f, 55.0f);
    cfg.maxYawRateDps = constrain(prefs.getFloat("maxyaw", cfg.maxYawRateDps), 30.0f, 360.0f);

    cfg.kpRoll = constrain(prefs.getFloat("kpr", cfg.kpRoll), 0.0f, 20.0f);
    cfg.kiRoll = constrain(prefs.getFloat("kir", cfg.kiRoll), 0.0f, 5.0f);
    cfg.kdRoll = constrain(prefs.getFloat("kdr", cfg.kdRoll), 0.0f, 20.0f);
    cfg.kpPitch = constrain(prefs.getFloat("kpp", cfg.kpPitch), 0.0f, 20.0f);
    cfg.kiPitch = constrain(prefs.getFloat("kip", cfg.kiPitch), 0.0f, 5.0f);
    cfg.kdPitch = constrain(prefs.getFloat("kdp", cfg.kdPitch), 0.0f, 20.0f);
    cfg.kpYaw = constrain(prefs.getFloat("kpy", cfg.kpYaw), 0.0f, 20.0f);
    cfg.kiYaw = constrain(prefs.getFloat("kiy", cfg.kiYaw), 0.0f, 5.0f);
    cfg.kdYaw = constrain(prefs.getFloat("kdy", cfg.kdYaw), 0.0f, 20.0f);
    cfg.pidEnabled = prefs.getBool("piden", cfg.pidEnabled);

    cfg.idleThrPct = constrain(prefs.getFloat("idle", cfg.idleThrPct), 0.0f, MAX_IDLE_THR_PCT);
    cfg.minThrottlePct = constrain(prefs.getFloat("minthr", cfg.minThrottlePct), 0.0f, 40.0f);
    cfg.maxThrottlePct = constrain(prefs.getFloat("maxthr", cfg.maxThrottlePct), 40.0f, 100.0f);
    if (cfg.maxThrottlePct < cfg.minThrottlePct) cfg.maxThrottlePct = cfg.minThrottlePct;
    cfg.armThrottlePct = constrain(prefs.getFloat("armthr", cfg.armThrottlePct), 0.0f, 20.0f);
    cfg.maxArmAngleDeg = constrain(prefs.getFloat("armang", cfg.maxArmAngleDeg), 5.0f, 45.0f);

    cfg.yawMixSign = prefs.getInt("yawsign", cfg.yawMixSign) < 0 ? -1 : 1;
    cfg.failsafeTimeoutMs = (uint16_t)constrain(prefs.getInt("fstime", cfg.failsafeTimeoutMs), 100, 5000);
    cfg.failsafeDisarmMs = (uint16_t)constrain(prefs.getInt("fsdis", cfg.failsafeDisarmMs), 100, 10000);

    prefs.end();
}

String buildConfigJson(const DroneConfig& cfg) {
    bool pwSet = false;
    Preferences p;
    if (p.begin("dronecfg", true)) {
        pwSet = p.getString("pass", "").length() > 0;
        p.end();
    }

    String j = "{";
    j += "\"ssid\":\"" + jsonEscape(String(cfg.wifiSsid)) + "\",";
    j += "\"pwSet\":" + String(pwSet ? "true" : "false") + ",";
    j += "\"mfl\":" + String(cfg.pinMotorFL) + ",";
    j += "\"mfr\":" + String(cfg.pinMotorFR) + ",";
    j += "\"mbl\":" + String(cfg.pinMotorBL) + ",";
    j += "\"mbr\":" + String(cfg.pinMotorBR) + ",";
    j += "\"pwmf\":" + String(cfg.pwmFrequency) + ",";
    j += "\"pwmr\":" + String((int)cfg.pwmResolutionBits) + ",";
    j += "\"beta\":" + String(cfg.madgwickBeta, 3) + ",";
    j += "\"maxtilt\":" + String(cfg.maxTiltDeg, 1) + ",";
    j += "\"maxyaw\":" + String(cfg.maxYawRateDps, 1) + ",";
    j += "\"sda\":" + String(cfg.wireSda) + ",";
    j += "\"scl\":" + String(cfg.wireScl) + ",";
    j += "\"kpr\":" + String(cfg.kpRoll, 3) + ",";
    j += "\"kir\":" + String(cfg.kiRoll, 3) + ",";
    j += "\"kdr\":" + String(cfg.kdRoll, 3) + ",";
    j += "\"kpp\":" + String(cfg.kpPitch, 3) + ",";
    j += "\"kip\":" + String(cfg.kiPitch, 3) + ",";
    j += "\"kdp\":" + String(cfg.kdPitch, 3) + ",";
    j += "\"kpy\":" + String(cfg.kpYaw, 3) + ",";
    j += "\"kiy\":" + String(cfg.kiYaw, 3) + ",";
    j += "\"kdy\":" + String(cfg.kdYaw, 3) + ",";
    j += "\"piden\":" + String(cfg.pidEnabled ? 1 : 0) + ",";
    j += "\"idle\":" + String(cfg.idleThrPct, 1) + ",";
    j += "\"minthr\":" + String(cfg.minThrottlePct, 1) + ",";
    j += "\"maxthr\":" + String(cfg.maxThrottlePct, 1) + ",";
    j += "\"armthr\":" + String(cfg.armThrottlePct, 1) + ",";
    j += "\"armang\":" + String(cfg.maxArmAngleDeg, 1) + ",";
    j += "\"yawsign\":" + String(cfg.yawMixSign) + ",";
    j += "\"fstime\":" + String(cfg.failsafeTimeoutMs) + ",";
    j += "\"fsdis\":" + String(cfg.failsafeDisarmMs);
    j += "}";
    return j;
}
