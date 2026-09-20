#pragma once
#include <string_view>
#include "Arduino.h"
#include "LogLevels.h"
#include "LedcAlloc.h"

class DRV8833_DCMotor {
    public:
    const uint8_t F_PIN, B_PIN;
    int targetPower, power, direction;
    const std::string_view tag;

    DRV8833_DCMotor(int f, int b, std::string_view motorID = {}, LogLevel level = LogLevel::INFO);
    void begin();
    void setDirection(int dir);
    void setSpeed(int speed);
    void setVelocity(int velocity);

    private:
    const LogLevel initLogLevel;
    void updateMotor();
};