#include "MT6701.h"
#include "Logger.h"
#include <cinttypes>


MT6701::MT6701(MT6701_Config cfg, std::string_view tag): config(cfg), tag(tag.empty() ? "MT6701" : tag) {
    if (config.pwmPin == -1) config.enablePWM = false;
    if (config.sdaPin == -1 || config.sclPin == -1) config.enableI2C = false;
}

bool MT6701::init() {
    Logger::init();
    LOG_ADD_TAG(tag, LogLevel::VERBOSE); //TODO: Add log Parameter to mt6701 constructor
    logMsg.assignTagAndMessage(tag.data(), nullptr);
    if (config.enableI2C) {
        initI2C();
    }
    if (config.enablePWM) {
        //Launch interrupt
        initPWM();
    }
    return true;
}

bool MT6701::getPwmAngle(float& angle) {
    portENTER_CRITICAL(&capMux);
    uint32_t pulseWidth = capPulseWidth;
    uint32_t period = capPeriod;
    bool newDataAvailable = newCapAvailable;
    newCapAvailable = false;
    portEXIT_CRITICAL(&capMux);
    
    angle = pwmToAngle(static_cast<float>(pulseWidth), static_cast<float>(period));
    if (newDataAvailable) {
        return true;
    }
    return false;
}

bool MT6701::getI2CAngle(float& angle) {
    _wire->beginTransmission(MT6701_ADDR);
    _wire->write(REG_ANGLE_H);
    if (_wire->endTransmission(false) != 0) {
        LOG_E(tag, "getI2CAngle() Failed: endTransmission() Errored");
        return false;
    }
    if (_wire->requestFrom(MT6701_ADDR, 2) != 2) {
        LOG_E(tag, "getI2CAngle() Failed: requestFailed() Errored");
        return false;
    }
    uint16_t h = _wire->read();
    uint16_t l = _wire->read();
    uint16_t a = (h<<6) | (l>>2);
    angle = a*360.0f/16384.0f;
    return true;
}
bool IRAM_ATTR MT6701::onPosEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg) {
        return static_cast<MT6701*>(arg)->onPosEdge(capChan, edata);
}

bool IRAM_ATTR MT6701::onNegEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg) {
    return static_cast<MT6701*>(arg)->onNegEdge(capChan, edata);
}

bool IRAM_ATTR MT6701::onPosEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    portENTER_CRITICAL_ISR(&capMux);
    const uint32_t capCurPosEdge = edata->cap_value, capCurPeriod = capCurPosEdge-capPosEdge;
    if (capCurPeriod > capMaxPeriod
            || capCurPeriod < capMinPeriod) {
        //Error: period out of bounds
        portEXIT_CRITICAL_ISR(&capMux);
        logMsg.assignTagAndMessage(nullptr, "Invalid PWM: Period out of Bounds");
        LOG_ISR_E(logMsg, higherPriorityTaskWoken);
        portENTER_CRITICAL_ISR(&capMux);
    } 
    else if (capPosEdge > capNegEdge) {
        //ERROR: double positive edges
        portEXIT_CRITICAL_ISR(&capMux);
        logMsg.assignTagAndMessage(nullptr, "Invalid PWM: Double positive edge");
        LOG_ISR_E(logMsg, higherPriorityTaskWoken);
        portENTER_CRITICAL_ISR(&capMux);
    } 
    else {
        capPulseWidth = capNegEdge-capPosEdge;
        capPeriod = capCurPosEdge-capPosEdge;
        newCapAvailable = true;
    }
    capPosEdge = capCurPosEdge;

    portEXIT_CRITICAL_ISR(&capMux);
    return (higherPriorityTaskWoken == pdTRUE);
}

bool IRAM_ATTR MT6701::onNegEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata) {
    portENTER_CRITICAL_ISR(&capMux);
    capNegEdge = edata->cap_value;
    portEXIT_CRITICAL_ISR(&capMux);
    return false;
}

void MT6701::initI2C() {
    //Check if chip is there and check register values

    // Check if sensor is there:
    // Note: ESP32 doesn't actually send byes during .endTransmission(false);
    //TODO: Should wire usage be decided internally or passsed to user?
    if (!wireUsed) {
        _wire = &Wire;
        wireUsed = true;
    } else if (!wire1Used) {
        _wire = &Wire1;
        wire1Used = true;
    } else {
        LOG_F(tag, "No available I2C buses");
        return;
    }
    //TODO: Calling begin could mess up other devices on the bus
    _wire->begin(config.sdaPin, config.sclPin);
    bool i2cInitialized = false;
    for (int i = 0; i < 3; i++) {
        _wire->beginTransmission(MT6701_ADDR);
        if (_wire->endTransmission(true) == 0) {
            i2cInitialized = true;
            break;
        }
        LOG_W(tag, "Failed to initialize I2C bus. Retrying ...");
        vTaskDelay(pdMS_TO_TICKS(3'000)); //TODO: Excessive wait?
    }
    if (!i2cInitialized) {
        LOG_F(tag, "Failed to Initialize I2C bus");
        return;
    }
    LOG_I(tag, "I2C initialization successful");

    if (!config.enablePWM) return;
    //Check registers:
    LOG_D(tag, "PWM requested, verifying config using I2C");
    _wire->beginTransmission(MT6701_ADDR);
    _wire->write(REG_OUT_CFG);
    _wire->endTransmission(false);
    if (_wire->requestFrom(MT6701_ADDR, 1) != 1) {
        LOG_W(tag, "Failed to verify MT6701 configuration: requestFrom() Failed");
        return;
    }
    
    uint8_t val = _wire->read();
    bool registerValuesOk = true;
    if (((val >> OUT_MODE_BIT) & 1) != 1) {
        LOG_E(tag, "Can't enable PWM: Output mode is analog");
        registerValuesOk = false;
    }
    if (((val >> PWM_POL_BIT) & 1) != 0) {
        LOG_E(tag, "Can't enable PWM: PWM signal is inverted"); //TODO: Add support for inverted signal
        registerValuesOk = false;
    }
    if (((val >> PWM_FREQ_BIT) & 1) != 0) {
        LOG_E(tag, "Can't enable PWM: Frequency set to 497.2Hz"); //TODO: Add support for 497.2Hz
        registerValuesOk = false;
    }
    if (!registerValuesOk) {
        config.enablePWM = false;
        LOG_F(tag, "Couldn't enable PWM");
        return;
    }
    LOG_I(tag, "Verified MT6701 config");
}

void MT6701::initPWM() {
    mcpwm_capture_timer_config_t tcfg = {
        .group_id = 0, .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT
    };
    ESP_ERROR_CHECK(mcpwm_new_capture_timer(&tcfg, &capTimer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_get_resolution(capTimer, &capTimerRes));

    capExpPeriod = capTimerRes/capPWMfreq;
    capMaxPeriod = static_cast<uint32_t>(static_cast<uint64_t>(capExpPeriod)*capPeriodMaxErrPercent/100 + capExpPeriod);
    capMinPeriod = capExpPeriod*2-capMaxPeriod;


    mcpwm_capture_channel_config_t ccfg = {};
    ccfg.gpio_num = config.pwmPin;
    ccfg.prescale = 1;
    ccfg.flags.pull_up = false;
    ccfg.flags.pull_down = false;

    ccfg.flags.pos_edge = true;
    ccfg.flags.neg_edge = false;
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(capTimer, &ccfg, &capChangePos));

    ccfg.flags.pos_edge = false;
    ccfg.flags.neg_edge = true;
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(capTimer, &ccfg, &capChangeNeg));

    mcpwm_capture_event_callbacks_t cbPos = {};
    cbPos.on_cap = onPosEdgeStatic;
    mcpwm_capture_event_callbacks_t cbNeg = {};
    cbNeg.on_cap = onNegEdgeStatic;
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(capChangePos, &cbPos, this));
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(capChangeNeg, &cbNeg, this));
    
    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(capChangePos));
    ESP_ERROR_CHECK(mcpwm_capture_channel_enable(capChangeNeg));
    ESP_ERROR_CHECK(mcpwm_capture_timer_enable(capTimer));
    ESP_ERROR_CHECK(mcpwm_capture_timer_start(capTimer));



    LOG_I(tag, "   MCPWM capture running, timer resolution %" PRIu32 " Hz\n", capTimerRes);
}

float MT6701::pwmToAngle(float pulseWidth, float period) {
    return constrain(static_cast<float>(pulseWidth) / static_cast<float>(period)
            * PWM_FRAME_LENGTH - PWM_HEADER_LENGTH, 0.0f, 4095.0f)
            *360.0f/4095.0f;
}









