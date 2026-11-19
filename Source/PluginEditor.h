#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace Lilac
{
    const juce::Colour bg       { 0xffefe7ff };  // light purple
    const juce::Colour panel    { 0xffe2d4ff };
    const juce::Colour soft     { 0xffc9b3ff };
    const juce::Colour accent   { 0xff9a7bf0 };
    const juce::Colour deep     { 0xff4a2f86 };
    const juce::Colour ink      { 0xff3a2a5c };
}

class LilacLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class LilacTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LilacTuneEditor (LilacTuneProcessor&);
    ~LilacTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void selectKey (int choiceIndex);

    LilacTuneProcessor& proc;
    LilacLook look;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachment;

    std::array<juce::Rectangle<float>, 24> keyRects;
    std::array<float, 24> glow {};
    juce::Rectangle<float> redetectRect, modeRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LilacTuneEditor)
};
