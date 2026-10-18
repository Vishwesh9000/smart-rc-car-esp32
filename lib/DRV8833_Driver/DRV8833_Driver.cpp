#include "DRV8833_Driver.h"
#include "Logger.h"

DRV8833_DCMotor::DRV8833_DCMotor(int f, int b, std::string_view motorID, LogLevel level):
        F_PIN(f), B_PIN(b), targetPower(0), power(0), direction(0),
        tag(motorID.empty() ? "MOTOR" : motorID), initLogLevel(level) {}

void DRV8833_DCMotor::init() {
    Logger::init();
    LOG_ADD_TAG(tag, initLogLevel);
    if (!ledcAttach(F_PIN, 20000, 8))  LOG_F(tag, "Couldn't set up ledc for forward pin");
    if (!ledcAttach(B_PIN, 20000, 8)) LOG_F(tag, "Couldn't set up ledc for backward pin");
}
void DRV8833_DCMotor::setDirection(int dir) {
    if (dir > 1 || dir < -1) {
        LOG_E(tag, "Invalid direction");
        return;
    }
    direction = dir;
    updateMotor();
}
void DRV8833_DCMotor::setSpeed(int speed) {
    if (speed < 0 || speed > 100) {
        LOG_E(tag, "Invalid speed");
        return;
    }
    targetPower = speed;
    updateMotor();
}
void DRV8833_DCMotor::setVelocity(int v) {
    if (v < -100 || v > 100) {
        LOG_E(tag, "Invalid velocity: %d", v);
        return;
    }
    else if (v > 0) {
        targetPower = v;
        direction = 1;
        updateMotor();

    }
    else if (v < 0) {
        targetPower = -v;
        direction = -1;
        updateMotor();
    }
    else if (v == 0) {
        targetPower = 0;
        direction = 0;
        updateMotor();
    }
}

void DRV8833_DCMotor::updateMotor() {
    int forward, backward;
    if (direction == 1) {
        forward = targetPower*255/100;
        backward = 0;
        power = targetPower;
    }
    else if (direction == -1) {
        forward = 0;
        backward = targetPower*255/100;
        power = -targetPower;
    }
    else if (direction == 0) {
        forward = 0;
        backward = 0;
        power = 0;
    }
    if (!ledcWrite(F_PIN, forward)) LOG_F(tag, "ERROR: Couldn't set speed");
    if (!ledcWrite(B_PIN, backward)) LOG_F(tag, "ERROR: Couldn't set speed");
    LOG_D(tag, "F: %d B: %d", forward, backward);
}