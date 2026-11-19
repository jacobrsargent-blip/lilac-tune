#pragma once
// YIN pitch detector (de Cheveigné & Kawahara, 2002). No JUCE dependency.
#include <vector>
#include <cmath>
#include <algorithm>

class PitchDetector
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        windowSize = sr > 100000.0 ? 8192 : (sr > 50000.0 ? 4096 : 2048);
        hopSize    = windowSize / 8;
        ring.assign  ((size_t) windowSize, 0.0f);
        frame.assign ((size_t) windowSize, 0.0f);
        yin.assign   ((size_t) windowSize / 2, 1.0f);
        writePos = hopCounter = 0;
        frequency = confidence = rms = 0.0f;
    }

    int getWindowSize() const { return windowSize; }
    int getHopSize() const    { return hopSize; }

    // Returns true when a fresh estimate is available.
    bool push (float x)
    {
        ring[(size_t) writePos] = x;
        if (++writePos == windowSize) writePos = 0;
        if (++hopCounter >= hopSize) { hopCounter = 0; analyse(); return true; }
        return false;
    }

    float getFrequency() const  { return frequency; }   // 0 = unvoiced / silent
    float getConfidence() const { return confidence; }  // 0..1

private:
    void analyse()
    {
        for (int i = 0; i < windowSize; ++i)
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % windowSize)];

        double e = 0.0;
        for (float s : frame) e += (double) s * s;
        rms = (float) std::sqrt (e / windowSize);

        frequency = confidence = 0.0f;
        if (rms < gate) return;

        const int W      = windowSize / 2;
        const int tauMin = std::max (2, (int) (sampleRate / maxFreq));
        const int tauMax = std::min (W - 2, (int) (sampleRate / minFreq));

        yin[0] = 1.0f;
        double running = 0.0;
        const float* x = frame.data();
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0.0;
            for (int i = 0; i < W; ++i) { const float df = x[i] - x[i + tau]; d += (double) df * df; }
            running += d;
            yin[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (yin[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && yin[(size_t) tau + 1] < yin[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0) return;

        float refined = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = yin[(size_t) best - 1], s1 = yin[(size_t) best], s2 = yin[(size_t) best + 1];
            const float denom = s0 - 2.0f * s1 + s2;
            if (std::abs (denom) > 1e-9f) refined = best + 0.5f * (s0 - s2) / denom;
        }

        frequency  = (float) (sampleRate / refined);
        confidence = 1.0f - yin[(size_t) best];
    }

    double sampleRate = 44100.0;
    int windowSize = 2048, hopSize = 256, writePos = 0, hopCounter = 0;
    std::vector<float> ring, frame, yin;
    float frequency = 0, confidence = 0, rms = 0;

    static constexpr float minFreq = 70.0f, maxFreq = 1100.0f;
    static constexpr float threshold = 0.15f, gate = 0.004f;
};
