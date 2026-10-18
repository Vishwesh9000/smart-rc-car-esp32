#include "Arduino.h"
#include "logger/logger.h"
#include "timer/timer.h"
#include "DRV8833_Driver.h"
#include "MT6701.h"
#include <Wifi.h>
#include <inttypes.h>
#include <string_view>
#define RIGHT_MOTOR_F_PIN 5
#define RIGHT_MOTOR_B_PIN 4
#define LEFT_MOTOR_F_PIN 6
#define LEFT_MOTOR_B_PIN 7

//Log Tags
constexpr std::string_view MAIN = "MAIN", MOTOR_R = "MOTOR_R", MOTOR_L = "MOTOR_L", MT6701_1 = "MT6701_1";

DRV8833_DCMotor rightMotor(RIGHT_MOTOR_F_PIN, RIGHT_MOTOR_B_PIN, MOTOR_R, LogLevel::VERBOSE), leftMotor(LEFT_MOTOR_F_PIN, LEFT_MOTOR_B_PIN, MOTOR_L, LogLevel::VERBOSE);
// MT6701 mt6701({.pwmPin = 10, .sdaPin = 8, .sclPin = 9, .enablePWM = true, .enableI2C = true}, MT6701_1);

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

void wifiControl(void* params) {
    static constexpr std::string_view WIFI_CTRL = "WIFI_CTRL";
    const char *ssid = "ESP_RC_CAR";
    const char *password = "31415926535";
    constexpr uint16_t WIFI_PORT = 8080;
    WiFiServer server(WIFI_PORT);
    LOG_ADD_TAG(WIFI_CTRL, LogLevel::VERBOSE);
    if (!WiFi.softAP(ssid, password)) {
        LOG_F(WIFI_CTRL, "Failed to start Soft AP");
    }

    IPAddress apIP = WiFi.softAPIP();
    server.begin();

    LOG_I(WIFI_CTRL, "Soft AP started: (SSID: %s, IP: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ", port: %" PRIu16 ")", 
          ssid, apIP[0], apIP[1], apIP[2], apIP[3], WIFI_PORT);

    WiFiClient client;
    int8_t buffer[2];
    int speed = 0;
    int turn = 0;
    int rightMotorVelocity;
    int leftMotorVelocity;
    for (;;) {
        client = server.accept();
        
        if (client) {
            LOG_I(WIFI_CTRL, "Client connected");
            while (client.connected()) {
                while (client.available() >= 2) {
                    if (client.read(reinterpret_cast<uint8_t*>(buffer), 2) != 2) {
                        LOG_W(WIFI_CTRL, "Failed to read 2 bytes from client");
                        continue;
                    }
                    LOG_I(WIFI_CTRL, "Received 2 bytes from client: %c%" PRId8, buffer[0], buffer[1]);
                    switch (buffer[0]) {
                        case 'S':
                            speed = buffer[1];
                            break;
                        case 'T':
                            turn = buffer[1];
                            break;
                        default:
                            LOG_W(WIFI_CTRL, "Unknown command received: %c", buffer[0]);
                            continue;
                            break;
                    }
                    

                    // // Turn value determines turn radius
                    // if (turn >= 0) {
                    //     leftMotorVelocity = speed;
                    //     rightMotorVelocity = speed - (speed * turn * 2 / 100);
                    // }
                    // else {
                    //     rightMotorVelocity = speed;
                    //     leftMotorVelocity = speed + (speed * turn * 2 / 100);
                    // }

                    // Standard Arcade Drive with Proportional Normalization
                    leftMotorVelocity = speed + turn;
                    rightMotorVelocity = speed - turn;
                    int16_t maxVelocity = std::max(std::abs(leftMotorVelocity), std::abs(rightMotorVelocity));
                    if (maxVelocity > 100) {
                        leftMotorVelocity = leftMotorVelocity * 100 / maxVelocity;
                        rightMotorVelocity = rightMotorVelocity * 100 / maxVelocity;
                    }

                    leftMotor.setVelocity(leftMotorVelocity);
                    rightMotor.setVelocity(rightMotorVelocity);

                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            client.stop();
            LOG_I(WIFI_CTRL, "Client disconnected");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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
    rightMotor.init();
    leftMotor.init();
    rightMotor.setSpeed(100);
    leftMotor.setSpeed(100);
    // mt6701.init();
    LOG_E(MAIN, "EXAMPLE ERROR");

    // TODO: Create function to auto start tasks based on map/array
    TaskHandle_t wifiTask;
    bool ok = pdPASS;
    ok &= (pdPASS == xTaskCreate(
        wifiControl,
        "wifi_task",
        4096,
        nullptr,
        2,
        &wifiTask
    ));
    configASSERT(ok);

    LOG_MEM_D(MAIN, MemFlag::ALL, "End of setup");
    LOG_I(MAIN, "Finished Setup");
}

void loop() {
    static int motorDirections[] = {1, 0, -1, 0};
    static uint32_t lastMotorUpdate = 0;
    static int motorDirectionIndex = 0;
    static float i2cAngle{0.0f}, pwmAngle{0.0f}, velocity{0.0f}, velocitySpaced{0.0f}, rVel{0.0}, sVel{0.0};
    static uint32_t lastsensorUpdate = 0;

    // TODO: Offload to another task
    // if (millis() - lastsensorUpdate >= 100) {
    //     mt6701.getI2CAngle(i2cAngle);
    //     pwmAngle = mt6701.getPwmAngle();
    //     velocity = mt6701.getVelocity();
    //     velocitySpaced = mt6701.getSpacedVelocity();
    //     rVel = mt6701.getRawVelocity();
    //     sVel = mt6701.getSmoothVelocity();
    //     static int logCounter = 0;
    //     if (logCounter == 5) {
    //         LOG_I(MT6701_1, "Angle: %7.2f FI_Velocity:%7.2f FA_Velocity:%7.2f IA_Velocity:%7.2f IS_Velocity:%7.2f", pwmAngle, velocity, velocitySpaced, rVel, sVel);
    //         // LOG_I(MT6701_1, "reg Print");`
    //         logCounter = 0;
    //     }
    //     else {
    //         LOG_I(MT6701_1, "Angle: %7.2f FI_Velocity:%7.2f FA_Velocity:%7.2f IA_Velocity:%7.2f IS_Velocity:%7.2f", pwmAngle, velocity, velocitySpaced, rVel, sVel);
    //         logCounter++;
    //     }
    //     lastsensorUpdate = millis();
    // }

    vTaskDelay(pdMS_TO_TICKS(10));
}
