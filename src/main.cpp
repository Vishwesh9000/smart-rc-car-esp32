#include "Arduino.h"
#include "Logger.h"
#include "Timer.h"
#include "DRV8833_Driver.h"
#include "MT6701.h"
#include <string_view>
#define M1_F 5
#define M1_B 4
#define M2_F 6
#define M2_B 7

//Log Tags
constexpr std::string_view MAIN = "MAIN", MOTOR1 = "MOTOR1", MOTOR2 = "MOTOR2", MT6701_1 = "MT6701_1";

DRV8833_DCMotor motor1(M1_F, M1_B, MOTOR1, LogLevel::VERBOSE), motor2(M2_F, M2_B, MOTOR2, LogLevel::VERBOSE);
MT6701 mt6701({.pwmPin = 10, .sdaPin = 8, .sclPin = 9, .enablePWM = true, .enableI2C = true}, MT6701_1);

void gradientSpeed(void* pvParameters) {
    DRV8833_DCMotor* motor = static_cast<DRV8833_DCMotor*>(pvParameters);
    while (true) {
        for (int velocity = 0; velocity <= 100; velocity += 5) {
            motor->setVelocity(velocity);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        for (int velocity = 100; velocity >= -100; velocity -= 5) {
            motor->setVelocity(velocity);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        for (int velocity = -100; velocity <= 0; velocity += 5) {
            motor->setVelocity(velocity);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}


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
    TaskHandle_t motorTask;
    bool ok = pdPASS;
    ok &= (pdPASS == xTaskCreate(
        gradientSpeed,
        "motor_task",
        4096,
        &motor1,
        4,
        &motorTask
    ));

    configASSERT(ok);
}

void loop() {
    static int motorDirections[] = {1, 0, -1, 0};
    static uint32_t lastMotorUpdate = 0;
    static int motorDirectionIndex = 0;
    static float i2cAngle{0.0f}, pwmAngle{0.0f}, velocity{0.0f}, velocitySpaced{0.0f}, rVel{0.0}, sVel{0.0};
    static uint32_t lastsensorUpdate = 0;

    if (millis() - lastsensorUpdate >= 100) {
        mt6701.getI2CAngle(i2cAngle);
        pwmAngle = mt6701.getPwmAngle();
        velocity = mt6701.getVelocity()/60;
        velocitySpaced = mt6701.getSpacedVelocity()/60;
        rVel = mt6701.getRawVelocity()/60;
        sVel = mt6701.getSmoothVelocity()/60;
        static int logCounter = 0;
        if (logCounter == 5) {
            LOG_I(MT6701_1, "iA: %7.2f pA: %7.2f D:%7.2f V:%7.2f aV:%7.2f rV:%7.2f sV:%7.2f", i2cAngle, pwmAngle, i2cAngle - pwmAngle, velocity, velocitySpaced, rVel, sVel);
            logCounter = 0;
        }
        else {
            LOG_I(MT6701_1, "iA: %7.2f pA: %7.2f D:%7.2f V:%7.2f aV:%7.2f rV:%7.2f sV:%7.2f\r", i2cAngle, pwmAngle, i2cAngle - pwmAngle, velocity, velocitySpaced, rVel, sVel);
            logCounter++;
        }
        lastsensorUpdate = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}
