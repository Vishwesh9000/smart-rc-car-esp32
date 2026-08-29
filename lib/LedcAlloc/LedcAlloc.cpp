#include "LedcAlloc.h"

static Logging& logger = Logging::instance();


void LedcAlloc::init() {
    if (initDone) return;
    logger.addTag(logTag, initialLogLevel);
    pins.fill(statusUnattached);
    initDone = true;
}

bool LedcAlloc::attachPin(const uint8_t pin, const uint32_t freq, const uint8_t resolutionBits) {
    if (pin >= maxNumPins) {
        logger.log(logTag, LogLevel::FATAL, "Pin Invalid");
        return false;
    }
    int channel = findFreeChannel();
    if (channel == nullChannel) return false;
    double setFreq = ledcSetup(channel, freq, resolutionBits);
    if (setFreq == 0) return false;
    ledcAttachPin(pin, channel);
    pins[pin] = channel;
    channels[channel] = true;
    numFreeChannels--;
    logger.log(logTag, LogLevel::INFO, "Configured pin %u to channel %u", pin, channel);
    return true;
}

bool LedcAlloc::detatchPin(const uint8_t pin) {
    if (pin >= maxNumPins) {
        logger.log(logTag, LogLevel::FATAL, "Pin Invalid");
        return false;
    }

    if (pins[pin] == statusUnattached) return false;
    ledcDetachPin(pin);
    channels[pins[pin]] = false;
    pins[pin] = statusUnattached;
    numFreeChannels++;
    return true;
}

bool LedcAlloc::write(const uint8_t pin, const uint32_t duty) {
    if (pin >= maxNumPins) {
        logger.log(logTag, LogLevel::FATAL, "Pin Invalid");
        return false;
    }

    if (pins[pin] == statusUnattached) return false;
    ledcWrite(pins[pin], duty);
    return true;
}
uint8_t LedcAlloc::freeChannels() {
    return numFreeChannels;
}
uint8_t LedcAlloc::pinToChannel(const uint8_t pin) {
    if (pin >= maxNumPins) {
        logger.log(logTag, LogLevel::FATAL, "Pin Invalid");
        return false;
    }
    return pins[pin];
}

uint8_t LedcAlloc::findFreeChannel() {
    for (uint8_t channel = 0; channel < numChannels; channel++) {
        if (!channels[channel]) return channel;
    }
    return nullChannel;
}
