#include "BN0085.h"
#include "Arduino.h"


BNO085_Sensor::BNO085_Sensor(std::string_view tag = "", int sck, int miso, int mosi, int cs, int interrupt, int rst)
        : tag(tag.empty() ? "BN0085" : tag), sckPin(sck), misoPin(miso), mosiPin(mosi), csPin, intPin(interrupt), rstPin(rst) {}

void BNO085_Sensor::init() {
    dataAvailable = xSemaphoreCreateBinary(); 
    SPI.begin(sckPin, misoPin, mosiPin, csPin);

    int c = 3;
    while (!myIMU.beginSPI(csPin, intPin, rstPin, 3'000'000)) {
        c--;
        if (c==0) LOG_F("BNO085 not detected via SPI");
        LOG_W(tag, "BNO085 not detected via SPI: Trying again ...")
        vTaskDelay(pdMS_TO_TICKS(3'000));
    }

    LOG_I(tag, "BNO085 connected");

    initInterrupt();
}

float BNO085_Sensor::getAcceleration() {

}

void BNO085::initInterrupt() {
    mcpwm_capture_timer_config_t tcfg = {
        .group_id = 1, .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT
    };
    ESP_ERROR_CHECK(mcpwm_new_capture_timer(&tcfg, &capTimer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_get_resolution(capTimer, &capTimerRes));

    mcpwm_capture_channel_config_t ccfg = {};
    ccfg.gpio_num = config.pwmPin;
    ccfg.prescale = 1;
    ccfg.flags.pull_up = false;
    ccfg.flags.pull_down = false;
    ccfg.flags.pos_edge = false;
    ccfg.flags.neg_edge = true;
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(capTimer, &ccfg, &capChannel));

    mcpwm_capture_event_callbacks_t capCallBack = {};
    capCallBack.on_cap = sensorInterruptEntry();
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(capChannel, &capCallBack, this));
    
    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(capChannel));
    ESP_ERROR_CHECK(mcpwm_capture_timer_enable(capTimer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_start(capTimer));

    LOG_I(tag, "MCPWM capture running, timer resolution %" PRIu32 " Hz\n", capTimerRes);
}

bool IRAM_ATTR BNO085::sensorInterruptEntry(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg) {
    return static_cast<BNO085*>(arg)->onSensorInterrupt();
}

bool BNO085_Sensor::onSensorInterrupt(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata) {
    bool higherPriorityTaskWoken;
    portENTER_CRITICAL_ISR(&isrMux);
    isrTimeStamp = edata->value;
    portEXIT_CRITICAL_ISR(&isrMux);
    xSemaphoreGiveFromISR(dataAvailable, &higherPriorityTaskWoken);
    return higherPriorityTaskWoken;
}

void BNO085_Sensor::sensorTaskLaunch(void *arg) {
    static_cast<BNO085*>(arg)->sensorTask();
}


void BNO085_Sensor::sensorTask() {
    for (;;) {
        if (xSemaphoreTake(dataAvailable, portMAX_DELAY) == pdTRUE) {
            if (imu.getData()) {
               portENTER_CRITICAL(&taskMux);
               acceleration.x = imu.ax;
               acceleration.y = imu.ay;
               acceleration.z = imu.az;
               portEXIT_CRITICAL(&taskMux);
            }
        }
    }
}
