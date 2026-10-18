#include "MT6701.h"
#include "logger/logger.h"
#include <inttypes.h>


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

float MT6701::getPwmAngle() {
    portENTER_CRITICAL(&capMux);
    uint32_t pulseWidth = recentCaps[latestCapIndex].pulseWidth;
    uint32_t period = recentCaps[latestCapIndex].period;
    bool newDataAvailable = recentCaps[latestCapIndex].unread;
    recentCaps[latestCapIndex].unread = false;
    portEXIT_CRITICAL(&capMux);
    
    return pwmToAngle(static_cast<float>(pulseWidth), static_cast<float>(period));
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

float MT6701::getVelocity() {
    capStruct latestCap, previousCap;
    portENTER_CRITICAL(&capMux);
    latestCap = recentCaps[latestCapIndex];
    previousCap = recentCaps[(latestCapIndex - 1 + capsToKeep) % capsToKeep];
    portEXIT_CRITICAL(&capMux);
    
    float angleDelta = ( pwmToAngle(static_cast<float>(latestCap.pulseWidth),   static_cast<float>(latestCap.period)) 
              -  pwmToAngle(static_cast<float>(previousCap.pulseWidth), static_cast<float>(previousCap.period)) );
    if (angleDelta > 180.0f) {
        angleDelta -= 360.0f;
    } else if (angleDelta < -180.0f) {
        angleDelta += 360.0f;
    }

    return angleDelta
            / (static_cast<float>(latestCap.ticks - previousCap.ticks))
            * capTimerRes;
}

float MT6701::getSpacedVelocity() {
    capStruct latestCap, previousCap;
    portENTER_CRITICAL(&capMux);
    latestCap = recentCaps[latestCapIndex];
    previousCap = recentCaps[(latestCapIndex + 1) % capsToKeep];
    portEXIT_CRITICAL(&capMux);
    
    float velocity = ( pwmToAngle(static_cast<float>(latestCap.pulseWidth),   static_cast<float>(latestCap.period)) 
              -  pwmToAngle(static_cast<float>(previousCap.pulseWidth), static_cast<float>(previousCap.period)) );
    if (velocity > 180.0f) {
        velocity -= 360.0f;
    } else if (velocity < -180.0f) {
        velocity += 360.0f;
    }
    velocity = velocity / ( static_cast<float>(latestCap.ticks - previousCap.ticks) )
                * capTimerRes;
    return velocity;
}

float MT6701::getRawVelocity() {
    float velocity;
    portENTER_CRITICAL(&capMux);
    velocity = capRawVelocity;
    portEXIT_CRITICAL(&capMux);
    return velocity / 1'000;
}

float MT6701::getSmoothVelocity() {
    float velocity;
    portENTER_CRITICAL(&capMux);
    velocity = capSmoothVelocity;
    portEXIT_CRITICAL(&capMux);
    return velocity / 1'000;
}

MT6701::capStruct::capStruct(const volatile capStruct &other): 
        ticks(other.ticks), pulseWidth(other.pulseWidth), period(other.period),
        angle_pwmTicks(other.angle_pwmTicks), unread(other.unread) {
}

MT6701::capStruct& MT6701::capStruct::operator=(const volatile capStruct &other) {
    ticks = other.ticks;
    pulseWidth = other.pulseWidth;
    period = other.period;
    angle_pwmTicks = other.angle_pwmTicks;
    unread = other.unread;
    return *this;
}

volatile MT6701::capStruct& MT6701::capStruct::operator=(const capStruct &other) volatile {
    ticks = other.ticks;
    pulseWidth = other.pulseWidth;
    period = other.period;
    angle_pwmTicks = other.angle_pwmTicks;
    unread = other.unread;
    return *this;
}

bool IRAM_ATTR MT6701::onPosEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg) {
        return static_cast<MT6701*>(arg)->onPosEdge(capChan, edata);
}

bool IRAM_ATTR MT6701::onNegEdgeStatic(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata, void *arg) {
    return static_cast<MT6701*>(arg)->onNegEdge(capChan, edata);
}

bool IRAM_ATTR MT6701::onPosEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    constexpr int indexOffset = capsToKeep-1;
    portENTER_CRITICAL_ISR(&capMux);
    const int expectingPosEdge = waitingForPosEdge;
    int available = capsAvailable;
    const uint32_t lastPosEdge = capPosEdge;
    const uint32_t lastNegEdge = capNegEdge;
    int32_t rawVelocity = capRawVelocity;
    int32_t smoothVelocity = capSmoothVelocity;
    const int lastCapIndex = latestCapIndex;
    const int newCapIndex = (lastCapIndex + 1) % capsToKeep;
    const int capIndexForVelocityCalc = (newCapIndex - indexOffset + capsToKeep) % capsToKeep;
    const capStruct lastCap = recentCaps[lastCapIndex];
    const capStruct capForVelocityCalc = recentCaps[capIndexForVelocityCalc];
    portEXIT_CRITICAL_ISR(&capMux);

    const uint32_t curPosEdge = edata->cap_value;
    const uint32_t period = curPosEdge-lastPosEdge;

    bool validCap = true;
    if (lastPosEdge == 0) {
        // First positive edge
        validCap = false;
    }
    else if (period > capMaxPeriod
            || period < capMinPeriod) {
        //Error: period out of bounds
        logMsg.assignTagAndMessage(nullptr, "Invalid PWM: Period out of Bounds");
        LOG_ISR_E(logMsg, higherPriorityTaskWoken);
        validCap = false;
    } 
    else if (expectingPosEdge != 1) {
        //ERROR: double positive edges
        logMsg.assignTagAndMessage(nullptr, "Invalid PWM: Double positive edge");
        LOG_ISR_E(logMsg, higherPriorityTaskWoken);
        validCap = false;
    }

    capStruct newCap;

    if (validCap) {
        newCap.ticks = curPosEdge;
        newCap.pulseWidth = lastNegEdge - lastPosEdge;
        newCap.period = period;
        newCap.angle_pwmTicks = pwmToAngleWidth(newCap.pulseWidth, newCap.period);
        newCap.unread = true;
        if (available < capsToKeep) available++;
        if (available >= capsToKeep) {
            rawVelocity = capStructToVelocity(capForVelocityCalc, newCap);
            smoothVelocity = EMAFilter(rawVelocity, smoothVelocity);   
        }
    }
    portENTER_CRITICAL_ISR(&capMux);
    if (validCap) {
        capsAvailable = available;
        recentCaps[newCapIndex] = newCap;
        latestCapIndex = newCapIndex;
        capRawVelocity = rawVelocity;
        capSmoothVelocity = smoothVelocity;
    }
    capPosEdge = curPosEdge;
    waitingForPosEdge = 0;
    portEXIT_CRITICAL_ISR(&capMux);

    return (higherPriorityTaskWoken == pdTRUE);
}

bool IRAM_ATTR MT6701::onNegEdge(mcpwm_cap_channel_handle_t capChan, const mcpwm_capture_event_data_t *edata) {
    portENTER_CRITICAL_ISR(&capMux);
    capNegEdge = edata->cap_value;
    waitingForPosEdge = 1;
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

constexpr float MT6701::pwmToAngle(float pulseWidth, float period) {
    return constrain(pulseWidth / period
            * PWM_FRAME_LENGTH - PWM_HEADER_LENGTH, 0.0f, 4095.0f)
            *360.0f/4095.0f;
}

constexpr uint32_t MT6701::pwmToAngleWidth(uint32_t pulseWidth, uint32_t period) {
    return constrain(pulseWidth*PWM_FRAME_LENGTH/period, PWM_HEADER_LENGTH, 4095 + PWM_HEADER_LENGTH) - PWM_HEADER_LENGTH;
}

constexpr int32_t MT6701::capStructToVelocity(const capStruct &prev, const capStruct &cur) {
    int64_t angleDiff = static_cast<int64_t>(cur.angle_pwmTicks)
                    - static_cast<int64_t>(prev.angle_pwmTicks);
    
    if (angleDiff > 2048) angleDiff -= 4096;
    else if (angleDiff < -2048) angleDiff += 4096;

    return static_cast<int32_t>(angleDiff * capTimerRes * 360 * 1'000 / 4096 
            / static_cast<int64_t>(cur.ticks - prev.ticks));
}

//TODO: Check overflow
constexpr int32_t MT6701::EMAFilter(int32_t raw, int32_t smooth) {
    constexpr int32_t alphaPercent = 10; // EMA smoothing factor
    return (raw * alphaPercent + smooth * (100 - alphaPercent)) / 100;
}





