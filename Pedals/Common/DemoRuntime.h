#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace devkomodo
{
inline bool demoExpired (double sampleRate, int numSamples) noexcept
{
#if defined (DEVKOMODO_DEMO_BUILD)
    static std::atomic<int64_t> renderedSamples { 0 };
    const auto effectiveSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto limit = static_cast<int64_t> (effectiveSampleRate * 900.0);
    return renderedSamples.fetch_add (static_cast<int64_t> (juce::jmax (0, numSamples)),
                                      std::memory_order_relaxed) >= limit;
#else
    juce::ignoreUnused (sampleRate, numSamples);
    return false;
#endif
}
}
