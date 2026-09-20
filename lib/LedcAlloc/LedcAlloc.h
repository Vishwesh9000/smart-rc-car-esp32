#pragma once
#include "Arduino.h"
#include "LogLevels.h"
#include <array>
#include <string_view>

class LedcAlloc {
    public:
        static constexpr std::string_view logTag = "LEDC";
        LedcAlloc() = delete;
        static void init();
        static bool attachPin(const uint8_t pin, const uint32_t freq, const uint8_t resolutionBits);
        static bool detatchPin(const uint8_t pin);
        static bool write(const uint8_t pin, const uint32_t duty);
        static uint8_t freeChannels();
        static uint8_t pinToChannel(const uint8_t pin);
    private:
        static constexpr LogLevel initialLogLevel = LogLevel::INFO;
        static inline bool initDone = false;
        static constexpr uint8_t maxNumPins = 60;
        static constexpr uint8_t numChannels = 8;
        static constexpr uint8_t statusUnattached = 255;
        static constexpr uint8_t nullChannel = 255;
        static inline uint8_t numFreeChannels = numChannels;
        static inline std::array<uint8_t, maxNumPins> pins{};
        static inline bool channels[numChannels] = {false};
        static uint8_t findFreeChannel();

};