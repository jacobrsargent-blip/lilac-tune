#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::StringArray LilacTuneProcessor::keyNames()
{
    static const char* n[] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
    juce::StringArray s { "Auto" };
    for (auto* x : n) s.add (x);
    for (auto* x : n) s.add (juce::String (x) + "m");
    return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout LilacTuneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "Amount",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key", keyNames(), 0));
    // Auto = live (low latency) while the DAW is recording, studio (look-ahead) otherwise
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", modeNames(), 0));
    return l;
}

LilacTuneProcessor::LilacTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    amountParam = apvts.getRawParameterValue ("amount");
    keyParam    = apvts.getRawParameterValue ("key");
    modeParam   = apvts.getRawParameterValue ("mode");
}

bool LilacTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void LilacTuneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    const bool live = decideLive();
    engine.setLive (live);
    setLatencySamples (engine.getLatencySamples (live));
}

bool LilacTuneProcessor::decideLive()
{
    const int mode = (int) modeParam->load();
    if (mode == 1) return true;
    if (mode == 2 || isNonRealtime()) return false;   // bounces/exports always use studio quality

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            return pos->getIsRecording();
    return false;
}

void LilacTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.setAmount (amountParam->load());
    engine.setKey ((int) keyParam->load() - 1);   // choice 0 = Auto -> -1

    const bool live = decideLive();
    if (live != engine.isLive())
    {
        engine.setLive (live);
        setLatencySamples (engine.getLatencySamples (live));  // host re-aligns tracks
    }
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void LilacTuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void LilacTuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LilacTuneProcessor::createEditor() { return new LilacTuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LilacTuneProcessor(); }
