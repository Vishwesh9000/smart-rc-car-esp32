#include "Arduino.h"
#include "Logger.h"
#include "Wire.h"
#include "driver/mcpwm_cap.h"
#include <string_view>

//TODO: Add rotation count
//TODO: Find velocity/acceleration

struct MT6701_Config {
    int pwmPin = -1, sdaPin = -1, sclPin = -1;
    bool enablePWM = true;
    bool enableI2C = true;
};

class MT6701 {
    public:
    MT6701_Config config;
    const std::string_view tag;
    MT6701(MT6701_Config config, std::string_view tag = "");
    bool init();
    bool getPwmAngle(float& angle);
    bool getI2CAngle(float& angle);

    private:
    static constexpr uint8_t MT6701_ADDR = 0x06;      // 7-bit I2C address
    static constexpr uint8_t REG_ANGLE_H = 0x03;      // ANGLE[13:6]
    static constexpr uint8_t REG_ANGLE_L = 0x04;      // ANGLE[5:0] in bits 7:2

    // Output configuration register (bit positions per open-source MT6701 drivers;
    // other bits in this register may be reserved, so always read-modify-write)
    static constexpr uint8_t REG_OUT_CFG = 0x38;
    static constexpr uint8_t OUT_MODE_BIT = 5;        // 0 = analog, 1 = PWM
    static constexpr uint8_t PWM_POL_BIT = 6;         // 0 = active high, 1 = inverted
    static constexpr uint8_t PWM_FREQ_BIT = 7;        // 0 = 994.4 Hz, 1 = 497.2 Hz

    static constexpr float PWM_FRAME_LENGTH = 4119.0f;
    static constexpr float PWM_HEADER_LENGTH = 16.0f;

    static inline bool wireUsed = false, wire1Used=false;
    TwoWire *_wire;

    mcpwm_cap_timer_handle_t capTimer = nullptr;
    mcpwm_cap_channel_handle_t capChangePos = nullptr, capChangeNeg = nullptr;
    portMUX_TYPE capMux = portMUX_INITIALIZER_UNLOCKED;
    constexpr static uint32_t capPWMfreq = 994.4,
            capPeriodMaxErrPercent = 10;
    uint32_t capTimerRes = 0,
            capExpPeriod = 0,
            capMinPeriod = 0,
            capMaxPeriod = 0;

    volatile uint32_t capPosEdge = 0,
            capNegEdge = 0,
            capPulseWidth = 0,
            capPeriod = 1;
    volatile bool newCapAvailable = false;

    Logger::LogMessage logMsg; 
    
    static bool onPosEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg);
    static bool onNegEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg);

    bool onPosEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata);
    bool onNegEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata);
    void initI2C();
    void initPWM();
    float pwmToAngle(float pulseWidth, float period);
};
