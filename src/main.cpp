#include "Arduino.h"
#include "Logger.h"
#include "Timer.h"
#include "DRV8833_Driver.h"
#include <string_view>
#define M1_F 4
#define M1_B 5
#define M2_F 6
#define M2_B 7

//Log Tags
constexpr std::string_view MAIN = "MAIN", MOTOR1 = "MOTOR1", MOTOR2 = "MOTOR2";

DRV8833_DCMotor motor1(M1_F, M1_B, MOTOR1, LogLevel::VERBOSE), motor2(M2_F, M2_B, MOTOR2, LogLevel::VERBOSE);

void setup() {
    //Set Pin Modes
    Timer::init();
    Timer t1 = Timer("Setup", LogLevel::DEBUG);
    Serial.begin(115200);
    LOG_SET_LEVEL(LogLevel::VERBOSE, Logger::tag);
    LOG_ADD_TAG(MAIN, LogLevel::VERBOSE);
    LOG_I(MAIN, "Sketch Started");
    motor1.begin();
    motor2.begin();
    motor1.setSpeed(100);
    motor2.setSpeed(100);
    LOG_E(MAIN, "EXAMPLE ERROR");
    LOG_MEM_D(MAIN, MemFlag::ALL, "End of setup");
    LOG_I(MAIN, "Finished Setup");
}

void loop() {
    LOG_D(MAIN, "LOOP");
    // motor1.setDirection(1);
    // delay(5000);

    // motor1.setDirection(0);
    // motor2.setDirection(1);
    // delay(5000);

    // motor2.setDirection(0);
    // delay(5000);

    // motor1.setDirection(-1);
    // delay(5000);

    // motor1.setDirection(0);
    // motor2.setDirection(-1);
    // delay(5000);

    // motor2.setDirection(0);
    // delay(5000);
    motor1.setDirection(1);
    motor2.setDirection(1);
    delay(5000);
    motor1.setDirection(0);
    motor2.setDirection(0);
    delay(2000);
    motor1.setDirection(-1);
    motor2.setDirection(-1);
    delay(5000);
    motor1.setDirection(0);
    motor2.setDirection(0);
    delay(2000);
    
}
