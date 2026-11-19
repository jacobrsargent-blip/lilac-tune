#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/TuneEngine.h"

class LilacTuneProcessor : public juce::AudioProcessor
{
public:
    LilacTuneProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lilac Tune"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::StringArray keyNames();   // "Auto", "C", ..., "B", "Cm", ..., "Bm"
    static juce::StringArray modeNames() { return { "Auto", "Live", "Studio" }; }

    juce::AudioProcessorValueTreeState apvts;
    TuneEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    bool decideLive();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LilacTuneProcessor)
};
