#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "Config.h"
#include "FlightController.h"
#include "ImuMpu6050.h"
#include "Motors.h"
#include "Safety.h"
#include "WebApi.h"

WebServer server(80);
WebSocketsServer ws = WebSocketsServer(81);

DroneConfig config;
ImuState imu;
ControlInput input;
SafetyState safety;
MotorState motors;
FlightState flight;

unsigned long lastTelem = 0;

void setup() {
    Serial.begin(115200);
    delay(300);

    loadSettingsFromNvs(config);
    resetControlInput(input);
    initSafety(safety);
    resetFlightController(flight);

    initMotors(config);
    stopMotors(config, motors);

    initIMU(config);
    calibrateIMU(imu);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid, config.wifiPass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.println("\nConnected:");
    Serial.println(WiFi.localIP());

    initWebApi(server, ws, config, imu, input, safety, motors, flight);
}

void loop() {
    server.handleClient();
    ws.loop();

    if (micros() - imu.lastUpdateUs >= 1000) {
        unsigned long nowUs = micros();
        float dt = (imu.lastUpdateUs == 0) ? 0.001f : (nowUs - imu.lastUpdateUs) / 1000000.0f;
        readIMU(imu, config, dt);
    }

    if (micros() - flight.lastControlUs >= CONTROL_PERIOD_US) {
        unsigned long nowUs = micros();
        float dt = (flight.lastControlUs == 0) ? (CONTROL_PERIOD_US / 1000000.0f) : (nowUs - flight.lastControlUs) / 1000000.0f;
        flight.lastControlUs = nowUs;
        if (dt > 0) flight.loopHz = 1.0f / dt;
        updateFlightControl(config, imu, input, safety, motors, flight, dt);
    }

    if (millis() - lastTelem > 50) {
        lastTelem = millis();
        sendTelemetry(ws, config, imu, input, safety, motors, flight);
    }
}
