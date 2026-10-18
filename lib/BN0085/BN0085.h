#include "logger/log_types.h"
#include <string_view>
#include <SPI.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

struct ImuData {
    float x;
    float y;
    float z;
    uint32_t time;
};

class BNO085_Sensor {
    public:
    const std::string_view tag;
    const int sckPin, misoPin, mosiPin, csPin, intPin, rstPin;

    BNO085_Sensor(std::string_view tag = "", int clk, int miso, int mosi, int cs, int interrupt, int rst);
    taskHandle_t task;
    void init();
    float getAcceleration();

    private:
    BNO085 imu;
    sh2_SensorValue_t sensorValue;
    volatile ImuData acceleration = {0,0,0};
    volatile uint32_t isrTimestamp = 0;

    SemaphoreHandle_t dataAvailable;
    portMUX_TYPE isrMux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE taskMux = portMUX_INITIALIZER_UNLOCKED;
    mcpwm_cap_timer_handle_t capTimer = nullptr;
    mcpwm_cap_channel_handle_t capChannel = nullptr,
    uint32_t capTimerRes = 0;
    void BNO085::initInterrupt();
    static bool IRAM_ATTR sensorInterruptEntry(void *arg);
    bool IRAM_ATTR onSensorInterrupt();
    static void sensorTaskLaunch(void *arg);
    void sensorTask();
};