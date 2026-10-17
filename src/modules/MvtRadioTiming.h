#pragma once
#include <algorithm>
#include <cstdint>

// Same operational policy as portal radio-timing.js and Android SensorPollTiming.kt.
// Each window includes the complete 7-hop request AND 7-hop reply, with retry margin.
// These are application deadlines, not a guarantee under unlimited RF congestion.
inline uint32_t mvtRadioReplyMs(uint32_t base, unsigned) {
    return base>=5500?600000U:base>=3000?300000U:base>=500?120000U:60000U;
}
inline uint32_t mvtRadioSpacingMs(uint32_t base, unsigned hops) {
    return std::min(25000U, std::max(3000U, base * std::min(7U, std::max(1U, hops)) + 1500));
}
