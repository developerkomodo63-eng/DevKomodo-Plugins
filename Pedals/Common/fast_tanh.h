#pragma once

// Lightweight rational approximation of tanh for high-performance per-sample use.
// Good enough for audio shaping where perfect mathematical tanh isn't required.
static inline float fast_tanh(float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
