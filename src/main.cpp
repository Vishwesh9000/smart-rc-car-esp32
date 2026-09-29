#include "Arduino.h"
#include "Logger.h"
#include "Timer.h"
#include "DRV8833_Driver.h"
#include "MT6701.h"
#include <string_view>
#define M1_F 4
#define M1_B 5
#define M2_F 6
#define M2_B 7

//Log Tags
constexpr std::string_view MAIN = "MAIN", MOTOR1 = "MOTOR1", MOTOR2 = "MOTOR2", MT6701_1 = "MT6701_1";

DRV8833_DCMotor motor1(M1_F, M1_B, MOTOR1, LogLevel::VERBOSE), motor2(M2_F, M2_B, MOTOR2, LogLevel::VERBOSE);
MT6701 mt6701({.pwmPin = 10, .sdaPin = 8, .sclPin = 9, .enablePWM = true, .enableI2C = true}, MT6701_1);

void setup() {
    Logger::init();
    Timer::init();
    Timer t1 = Timer("Setup", LogLevel::DEBUG);
    Serial.begin(115200);
    LOG_SET_LEVEL(LogLevel::VERBOSE, Logger::tag);
    LOG_ADD_TAG(MAIN, LogLevel::VERBOSE);
    LOG_I(MAIN, "Sketch Started");
    motor1.init();
    motor2.init();
    motor1.setSpeed(100);
    motor2.setSpeed(100);
    mt6701.init();
    LOG_E(MAIN, "EXAMPLE ERROR");
    LOG_MEM_D(MAIN, MemFlag::ALL, "End of setup");
    LOG_I(MAIN, "Finished Setup");
}

void loop() {
    // LOG_D(MAIN, "LOOP");
    // motor1.setDirection(1);
    // motor2.setDirection(1);
    // delay(5000);
    // motor1.setDirection(0);
    // motor2.setDirection(0);
    // delay(2000);
    // motor1.setDirection(-1);
    // motor2.setDirection(-1);
    // delay(5000);
    // motor1.setDirection(0);
    // motor2.setDirection(0);
    // delay(2000);
    
    static float i2cAngle{0.0f}, pwmAngle{0.0f};
    mt6701.getI2CAngle(i2cAngle);
    mt6701.getPwmAngle(pwmAngle);
    LOG_I(MT6701_1, "I2C Angle: %f\tPWM Angle: %f", i2cAngle, pwmAngle);
    vTaskDelay(pdMS_TO_TICKS(100));
}
