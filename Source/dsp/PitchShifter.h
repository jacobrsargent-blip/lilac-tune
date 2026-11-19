#pragma once
// Pitch-synchronous two-head delay-line shifter with a fixed look-ahead.
// Grain length is set to a whole number of input periods, so the two read heads
// stay in phase with the voice and don't comb-filter it.
#include <vector>
#include <cmath>
#include <algorithm>

class PitchShifter
{
public:
    void prepare (int lookaheadSamples)
    {
        L = lookaheadSamples;
        int size = 1;
        while (size < 2 * L + 16) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0; phase = 0.0f; grain = (float) L;
    }

    // ratio: output/input frequency. wetMix 0 = untouched (latency-aligned) dry.
    float process (float x, float ratio, float grainTarget, float wetMix)
    {
        buf[(size_t) w] = x;
        const float dry = buf[(size_t) ((w - L) & mask)];

        grain += (grainTarget - grain) * 0.002f;
        const float g = std::clamp (grain, 32.0f, 2.0f * (float) (L - 4));

        phase += (1.0f - ratio) / g;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;

        const float base = (float) L - 0.5f * g;
        const float s  = std::sin (3.14159265f * phase);
        const float w1 = s * s, w2 = 1.0f - w1;   // Hann pair, sums to 1
        const float wet = w1 * read (base + phase * g) + w2 * read (base + p2 * g);

        w = (w + 1) & mask;
        return dry + (wet - dry) * wetMix;
    }

private:
    float read (float delay) const
    {
        const float pos = (float) w - delay;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    std::vector<float> buf;
    int L = 1024, mask = 0, w = 0;
    float phase = 0.0f, grain = 1024.0f;
};
