#include "DRV8833_Driver.h"


static Logger& logger = Logger::instance();

DRV8833_DCMotor::DRV8833_DCMotor(int f, int b, std::string_view motorID, LogLevel level): F_PIN(f), B_PIN(b), targetPower(0), power(0), direction(0), tag(motorID.empty() ? "MOTOR" : motorID), initLogLevel(level) {}

void DRV8833_DCMotor::begin() {
    logger.addTag(tag, initLogLevel);
    LedcAlloc::init();
    if (!LedcAlloc::attachPin(F_PIN, 20000, 8))  logger.log(tag, LogLevel::FATAL, "Couldn't set up ledc for forward pin");
    if (!LedcAlloc::attachPin(B_PIN, 20000, 8)) logger.log(tag, LogLevel::FATAL, "Couldn't set up ledc for backward pin");
}
void DRV8833_DCMotor::setDirection(int dir) {
    if (dir > 1 || dir < -1) {
        logger.log(tag, LogLevel::ERROR, "Invalid direction");
        return;
    }
    direction = dir;
    updateMotor();
}
void DRV8833_DCMotor::setSpeed(int speed) {
    if (speed < 0 || speed > 100) {
        logger.log(tag, LogLevel::ERROR, "Invalid speed");
        return;
    }
    targetPower = speed;
    updateMotor();
}
void DRV8833_DCMotor::setVelocity(int p) {
    if (p < -100 || p > 100) {
        logger.log(tag, LogLevel::ERROR, "Invalid velocity");
        return;
    }
    else if (p > 0) {
        targetPower = p;
        direction = 1;
        updateMotor();

    }
    else if (p < 0) {
        targetPower = -p;
        direction = -1;
        updateMotor();
    }
    else if (p == 0) {
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
    if (!LedcAlloc::write(F_PIN, forward)) logger.log(tag, LogLevel::FATAL, "ERROR: Couldn't set speed");
    if (!LedcAlloc::write(B_PIN, backward)) logger.log(tag, LogLevel::FATAL, "ERROR: Couldn't set speed");
    logger.log(tag, LogLevel::DEBUG, "F: %d B: %d", forward, backward);
}