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
    float getPwmAngle();
    bool getI2CAngle(float& angle);
    float getVelocity();
    float getSpacedVelocity();
    float getRawVelocity();
    float getSmoothVelocity();
    float getAcceleration();

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

    //Frame Composition: 16 tick header (HIGH) - 4095 data period - 8 tick end signal (LOW)
    static constexpr uint32_t PWM_FRAME_LENGTH = 4119;
    static constexpr uint32_t PWM_HEADER_LENGTH = 16;

    static inline bool wireUsed = false, wire1Used=false;
    TwoWire *_wire;

    mcpwm_cap_timer_handle_t capTimer = nullptr;
    mcpwm_cap_channel_handle_t capChangePos = nullptr, capChangeNeg = nullptr;
    portMUX_TYPE capMux = portMUX_INITIALIZER_UNLOCKED;
    constexpr static float capPWMfreq = 994.4f;
    constexpr static uint32_t capPeriodMaxErrPercent = 10;
    constexpr static uint32_t capsToKeep = 4; // Power of 2
    uint32_t capTimerRes = 0,
            capExpPeriod = 0,
            capMinPeriod = 0,
            capMaxPeriod = 0;
    
    //TODO: remove default initialization of capStruct members for speed
    struct capStruct {
        uint32_t ticks = 0; // Start time of pulse in ESP timer res
        uint32_t pulseWidth = 0; // Width of high pulse in ESP timer res
        uint32_t period = 0; // Total length of pulse in ESP timer res
        uint32_t angle_pwmTicks = 0; // Length of pulse that corresponds to an angle in MT6701 ticks (0-4095)
        bool unread = true;
        capStruct() = default;
        capStruct(const volatile capStruct &other);
        capStruct& operator=(const capStruct &other) = default;
        capStruct& operator=(const volatile capStruct &other);
        volatile capStruct& operator=(const capStruct &other) volatile;
    };
    volatile capStruct recentCaps[capsToKeep];
    volatile int latestCapIndex = 0;
    volatile int waitingForPosEdge = -1; //-1 = uinitialized, 0 = false, 1 = true
    volatile int capsAvailable = 0;
    volatile uint32_t capPosEdge = 0;
    volatile uint32_t capNegEdge = 0;
    volatile int32_t capRawVelocity = 0; //TODO: checkBounds
    volatile int32_t capSmoothVelocity = 0;


    Logger::LogMessage logMsg; 
    
    static bool onPosEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg);
    static bool onNegEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg);

    bool onPosEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata);
    bool onNegEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata);
    void initI2C();
    void initPWM();
    constexpr float pwmToAngle(float pulseWidth, float period);
    constexpr uint32_t pwmToAngleWidth(uint32_t pulseWidth, uint32_t period);
    constexpr int32_t capStructToVelocity(const capStruct &prev, const capStruct &cur);
    constexpr int32_t EMAFilter(int32_t raw, int32_t smooth);
};
