#pragma once
// The whole auto-tune brain, independent of JUCE so it can be unit-tested.
#include "PitchDetector.h"
#include "KeyDetector.h"
#include "PitchShifter.h"
#include <atomic>
#include <vector>

class TuneEngine
{
public:
    static constexpr int maxChannels = 8;

    void prepare (double sr)
    {
        sampleRate = sr;
        detector.prepare (sr);
        lookahead = detector.getWindowSize() / 2;                       // studio: ~23 ms
        liveLookahead = std::max (64, (int) std::lround (0.0065 * sr));   // live:   ~6.5 ms
        studioShifters.assign (maxChannels, PitchShifter{});
        liveShifters.assign (maxChannels, PitchShifter{});
        for (auto& s : studioShifters) s.prepare (lookahead);
        for (auto& s : liveShifters)   s.prepare (liveLookahead);
        keys.prepare (sr / detector.getHopSize());
        smoothShift = desiredShift = overshoot = 0.0f;
        wet = wetTarget = 0.0f;
        grainTarget = (float) lookahead;
        liveGrainTarget = (float) liveLookahead;
        prevTarget = noNote;
    }

    int getLatencySamples (bool live) const { return live ? liveLookahead : lookahead; }

    // Live = low-latency preview for recording/monitoring. Studio = look-ahead, better quality.
    // Both paths run all the time so switching never plays stale audio.
    void setLive (bool shouldBeLive) { live.store (shouldBeLive); }
    bool isLive() const { return live.load(); }

    void setAmount (float a) { amount.store (std::clamp (a, 0.0f, 1.0f)); }
    void setKey (int k)      { selectedKey.store (k); }  // -1 = auto, 0..23

    void process (float* const* ch, int numCh, int numSamples)
    {
        numCh = std::min (numCh, maxChannels);
        if (numCh <= 0) return;

        // ---- map the single knob onto the internal controls ----
        const float a = amount.load();
        const bool useLive = live.load();
        const float strength  = a < 0.6f ? 0.2f + 0.8f * (a / 0.6f) : 1.0f;     // how far to pull
        const float retuneSec = a >= 0.97f ? 0.0f : 0.3f * std::pow (1.0f - a, 2.2f); // how fast
        const float grainMult = a < 0.75f ? 4.0f : 2.0f;                          // 2 periods = buzzier
        const float hyper     = std::clamp ((a - 0.85f) / 0.15f, 0.0f, 1.0f);       // "x50" zone
        const float hyst      = 0.15f * (1.0f - hyper);

        const float coef     = retuneSec <= 0.0f ? 0.0f : std::exp (-1.0f / (retuneSec * (float) sampleRate));
        const float wetCoef  = std::exp (-1.0f / (0.015f * (float) sampleRate));
        const float overCoef = std::exp (-1.0f / (0.045f * (float) sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < numCh; ++c) mono += ch[c][i];
            mono /= (float) numCh;

            if (detector.push (mono))
                updateControl (strength, grainMult, hyper, hyst);

            smoothShift = desiredShift + coef * (smoothShift - desiredShift);
            overshoot  *= overCoef;
            wet = wetTarget + wetCoef * (wet - wetTarget);

            const float ratio = std::exp2 ((smoothShift + overshoot) / 12.0f);
            for (int c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i];
                const float studioOut = studioShifters[(size_t) c].process (x, ratio, grainTarget, wet);
                const float liveOut   = liveShifters[(size_t) c].process (x, ratio, liveGrainTarget, wet);
                ch[c][i] = useLive ? liveOut : studioOut;
            }
        }
    }

    KeyDetector keys;
    std::atomic<float> currentMidi { -1.0f }; // what's being sung (for the UI)
    std::atomic<int>   targetNote  { -1 };     // where it's being pulled to
    std::atomic<int>   activeKey   { -1 };     // key actually in use

private:
    static constexpr int noNote = -1000;

    void updateControl (float strength, float grainMult, float hyper, float hyst)
    {
        const float f = detector.getFrequency();
        const float conf = detector.getConfidence();
        if (f <= 0.0f || conf < 0.8f)
        {
            wetTarget = 0.0f; desiredShift = 0.0f; prevTarget = noNote;
            currentMidi.store (-1.0f); targetNote.store (-1);
            return;
        }

        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        keys.addNote (midi, conf);

        const int sel = selectedKey.load();
        const int key = sel >= 0 ? sel : keys.getBestKey();  // -1 => chromatic until a key is found
        activeKey.store (key);

        const int target = nearestNote (midi, key, prevTarget, hyst);
        if (prevTarget != noNote && target != prevTarget && hyper > 0.0f)
            overshoot += hyper * 0.7f * (target > prevTarget ? 1.0f : -1.0f); // exaggerated "flip"
        prevTarget = target;

        desiredShift = ((float) target - midi) * strength;
        grainTarget     = std::clamp ((float) (sampleRate / f) * grainMult, 64.0f, 2.0f * (float) (lookahead - 4));
        liveGrainTarget = std::clamp ((float) (sampleRate / f) * 2.0f, 32.0f, 2.0f * (float) (liveLookahead - 4));
        wetTarget    = 1.0f;

        currentMidi.store (midi);
        targetNote.store (target);
    }

    static int nearestNote (float midi, int key, int prev, float hyst)
    {
        int best = (int) std::lround (midi);
        float bestD = 1e9f;
        const int base = (int) std::floor (midi);
        for (int n = base - 3; n <= base + 3; ++n)
        {
            if (key >= 0 && ! KeyDetector::inScale (key, n)) continue;
            const float d = std::abs (midi - (float) n);
            if (d < bestD) { bestD = d; best = n; }
        }
        if (prev != noNote && (key < 0 || KeyDetector::inScale (key, prev))
            && std::abs (midi - (float) prev) < bestD + hyst)
            return prev;
        return best;
    }

    double sampleRate = 44100.0;
    int lookahead = 1024, liveLookahead = 288;
    PitchDetector detector;
    std::vector<PitchShifter> studioShifters, liveShifters;
    std::atomic<bool> live { false };

    std::atomic<float> amount { 0.5f };
    std::atomic<int> selectedKey { -1 };

    float smoothShift = 0, desiredShift = 0, overshoot = 0, wet = 0, wetTarget = 0, grainTarget = 1024, liveGrainTarget = 288;
    int prevTarget = noNote;
};
