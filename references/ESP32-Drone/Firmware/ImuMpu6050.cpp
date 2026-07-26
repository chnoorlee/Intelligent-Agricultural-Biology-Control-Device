#include "ImuMpu6050.h"

static float axBias = 0.0f, ayBias = 0.0f, azBias = 0.0f;
static float gxBias = 0.0f, gyBias = 0.0f, gzBias = 0.0f;
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

static bool readRaw(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(0x68, 14, true);
    if (Wire.available() < 14) return false;

    ax = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0f;
    ay = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0f;
    az = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0f;
    (void)(int16_t)(Wire.read() << 8 | Wire.read());
    gx = (int16_t)(Wire.read() << 8 | Wire.read()) / 131.0f;
    gy = (int16_t)(Wire.read() << 8 | Wire.read()) / 131.0f;
    gz = (int16_t)(Wire.read() << 8 | Wire.read()) / 131.0f;
    return true;
}

static void madgwickUpdate(ImuState& imu, float beta, float dt) {
    float gx = imu.gx * 0.0174533f;
    float gy = imu.gy * 0.0174533f;
    float gz = imu.gz * 0.0174533f;
    float ax = imu.ax;
    float ay = imu.ay;
    float az = imu.az;

    float accelNorm = sqrtf(ax * ax + ay * ay + az * az);
    if (accelNorm < 1e-6f) return;
    float recipNorm = 1.0f / accelNorm;
    ax *= recipNorm;
    ay *= recipNorm;
    az *= recipNorm;

    float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    float f1 = 2.0f * (q1 * q3 - q0 * q2) - ax;
    float f2 = 2.0f * (q0 * q1 + q2 * q3) - ay;
    float f3 = 2.0f * (0.5f - q1 * q1 - q2 * q2) - az;

    float s0 = -2.0f * q2 * f1 + 2.0f * q1 * f2;
    float s1 = 2.0f * q3 * f1 + 2.0f * q0 * f2 - 4.0f * q1 * f3;
    float s2 = -2.0f * q0 * f1 + 2.0f * q3 * f2 - 4.0f * q2 * f3;
    float s3 = 2.0f * q1 * f1 + 2.0f * q2 * f2;

    float stepNorm = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (stepNorm >= 1e-6f) {
        recipNorm = 1.0f / stepNorm;
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;
        qDot1 -= beta * s0;
        qDot2 -= beta * s1;
        qDot3 -= beta * s2;
        qDot4 -= beta * s3;
    }

    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;

    imu.roll = atan2f(2.0f * (q0 * q1 + q2 * q3), 1.0f - 2.0f * (q1 * q1 + q2 * q2)) * 57.2958f;
    float pitchArg = constrain(2.0f * (q0 * q2 - q3 * q1), -1.0f, 1.0f);
    imu.pitch = asinf(pitchArg) * 57.2958f;
    imu.yaw = atan2f(2.0f * (q0 * q3 + q1 * q2), 1.0f - 2.0f * (q2 * q2 + q3 * q3)) * 57.2958f;
}

void initIMU(const DroneConfig& cfg) {
    Wire.begin(cfg.wireSda, cfg.wireScl);
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(100);
    Serial.println("MPU6050 Initialized.");
}

bool calibrateIMU(ImuState& imu) {
    const int samples = 700;
    int good = 0;
    float axSum = 0.0f, aySum = 0.0f, azSum = 0.0f;
    float gxSum = 0.0f, gySum = 0.0f, gzSum = 0.0f;

    Serial.println("Calibrating IMU: keep drone still...");
    delay(300);

    for (int i = 0; i < samples; i++) {
        float rax, ray, raz, rgx, rgy, rgz;
        if (readRaw(rax, ray, raz, rgx, rgy, rgz)) {
            axSum += rax;
            aySum += ray;
            azSum += raz;
            gxSum += rgx;
            gySum += rgy;
            gzSum += rgz;
            good++;
        }
        delay(2);
        if ((i & 0x1F) == 0) yield();
    }

    if (good < samples / 2) {
        imu.calibrated = false;
        Serial.println("IMU calibration failed.");
        return false;
    }

    axBias = axSum / good;
    ayBias = aySum / good;
    azBias = (azSum / good) - 1.0f;
    gxBias = gxSum / good;
    gyBias = gySum / good;
    gzBias = gzSum / good;
    q0 = 1.0f;
    q1 = q2 = q3 = 0.0f;
    imu.calibrated = true;
    Serial.println("IMU calibration done.");
    return true;
}

bool readIMU(ImuState& imu, const DroneConfig& cfg, float dt) {
    if (!readRaw(imu.ax, imu.ay, imu.az, imu.gx, imu.gy, imu.gz)) return false;

    imu.ax -= axBias;
    imu.ay -= ayBias;
    imu.az -= azBias;
    imu.gx -= gxBias;
    imu.gy -= gyBias;
    imu.gz -= gzBias;

    madgwickUpdate(imu, cfg.madgwickBeta, dt);
    imu.lastUpdateUs = micros();
    return true;
}
