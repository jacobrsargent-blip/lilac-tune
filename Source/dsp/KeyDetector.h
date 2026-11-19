#pragma once
// Builds a pitch-class histogram from the sung notes and matches it against
// Krumhansl-Schmuckler key profiles. Keys 0..11 = C..B major, 12..23 = C..B minor.
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

class KeyDetector
{
public:
    static constexpr int numKeys = 24;

    void prepare (double analysesPerSecond)
    {
        decay     = (float) std::pow (0.5, 1.0 / (40.0 * analysesPerSecond)); // 40 s memory half-life
        minWeight = (float) (2.5 * analysesPerSecond * 0.85);                  // ~2.5 s of singing
        clear();
    }

    void requestReset()
    {
        resetRequested.store (true);
        for (auto& h : highlight) h.store (0.0f);
        bestKey.store (-1);
    }

    // Called from the audio thread for each voiced analysis frame.
    void addNote (float midi, float weight)
    {
        if (resetRequested.exchange (false)) clear();

        const int pc = (((int) std::lround (midi)) % 12 + 12) % 12;
        for (auto& c : chroma) c *= decay;
        total *= decay;
        chroma[(size_t) pc] += weight;
        total += weight;

        if (++counter >= 16) { counter = 0; evaluate(); }
    }

    int   getBestKey() const        { return bestKey.load(); }
    float getHighlight (int k) const { return highlight[(size_t) k].load(); }
    float getProgress() const        { return progress.load(); } // 0..1 until enough data

    static bool inScale (int key, int midiNote)
    {
        static constexpr bool major[12] = { 1,0,1,0,1,1,0,1,0,1,0,1 };
        static constexpr bool minor[12] = { 1,0,1,1,0,1,0,1,1,0,1,0 };
        const int root = key % 12;
        const int iv = ((midiNote - root) % 12 + 12) % 12;
        return key < 12 ? major[iv] : minor[iv];
    }

private:
    void clear()
    {
        chroma.fill (0.0f);
        total = 0.0f; counter = 0;
        for (auto& h : highlight) h.store (0.0f);
        bestKey.store (-1);
        progress.store (0.0f);
    }

    void evaluate()
    {
        progress.store (std::min (1.0f, total / minWeight));
        if (total < minWeight) return;

        static constexpr float majorP[12] = { 6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f };
        static constexpr float minorP[12] = { 6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f };

        std::array<float, numKeys> r {};
        float rBest = -2.0f; int kBest = -1;

        for (int k = 0; k < numKeys; ++k)
        {
            const float* p = k < 12 ? majorP : minorP;
            const int root = k % 12;
            float mx = 0, mp = 0;
            for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) ((root + i) % 12)]; mp += p[i]; }
            mx /= 12; mp /= 12;
            float sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const float dx = chroma[(size_t) ((root + i) % 12)] - mx, dy = p[i] - mp;
                sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
            }
            r[(size_t) k] = (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0f;
            if (r[(size_t) k] > rBest) { rBest = r[(size_t) k]; kBest = k; }
        }

        const float spread = 0.12f; // keys within this correlation distance of the best get highlighted
        for (int k = 0; k < numKeys; ++k)
        {
            float h = (r[(size_t) k] - (rBest - spread)) / spread;
            h = std::clamp (h, 0.0f, 1.0f);
            highlight[(size_t) k].store (h * h);
        }
        bestKey.store (kBest);
    }

    std::array<float, 12> chroma {};
    float total = 0, decay = 0.9999f, minWeight = 300.0f;
    int counter = 0;

    std::array<std::atomic<float>, numKeys> highlight {};
    std::atomic<int>   bestKey { -1 };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool>  resetRequested { false };
};
