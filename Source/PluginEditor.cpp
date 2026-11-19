#include "PluginEditor.h"

using namespace Lilac;

//==============================================================================
void LilacLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 8, r - 8, 0, startAngle, endAngle, true);
    g.setColour (panel);
    g.strokePath (track, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 8, r - 8, 0, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (soft, bounds.getBottomLeft(), deep, bounds.getTopRight(), false));
    g.strokePath (val, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body with soft shadow
    const float br = r - 26;
    auto body = juce::Rectangle<float> (br * 2, br * 2).withCentre (c);
    juce::Path bodyPath; bodyPath.addEllipse (body);
    juce::DropShadow (deep.withAlpha (0.16f), 22, { 0, 6 }).drawForPath (g, bodyPath);
    g.setColour (juce::Colours::white);
    g.fillEllipse (body);

    // pointer dot
    const auto dot = c.getPointOnCircumference (br - 16, angle);
    g.setColour (deep);
    g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (dot));
}

//==============================================================================
LilacTuneEditor::LilacTuneEditor (LilacTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    knob.setLookAndFeel (&look);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                              juce::MathConstants<float>::pi * 2.75f, true);
    knob.setMouseDragSensitivity (300);
    knob.setDoubleClickReturnValue (true, 0.5);
    addAndMakeVisible (knob);
    knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "amount", knob);

    setSize (440, 540);
    startTimerHz (30);
}

LilacTuneEditor::~LilacTuneEditor() { knob.setLookAndFeel (nullptr); }

void LilacTuneEditor::resized()
{
    knob.setBounds (getLocalBounds().withSizeKeepingCentre (270, 270).withY (84));
    modeRect = { 22, 20, 104, 24 };

    const float margin = 22, gap = 4;
    const float cellW = (getWidth() - 2 * margin - 11 * gap) / 12.0f;
    const float cellH = 30;
    const float top = 398;
    for (int k = 0; k < 24; ++k)
    {
        const int col = k % 12, row = k / 12;
        keyRects[(size_t) k] = { margin + col * (cellW + gap), top + row * (cellH + gap), cellW, cellH };
    }
    redetectRect = { getWidth() - margin - 84, (float) getHeight() - 40, 84, 24 };
}

void LilacTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    const auto names = LilacTuneProcessor::keyNames();

    // the only knob label
    g.setColour (deep);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("tune", 0, 58, getWidth(), 26, juce::Justification::centred);

    // live / studio pill
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const bool liveNow = proc.engine.isLive();
    g.setColour (liveNow ? deep : panel);
    g.fillRoundedRectangle (modeRect, 12);
    g.setColour (liveNow ? juce::Colours::white : deep);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText ((mode == 0 ? "auto: " : "") + juce::String (liveNow ? "live" : "studio"),
                modeRect, juce::Justification::centred);

    // live note readout
    const float midi = proc.engine.currentMidi.load();
    const int tgt = proc.engine.targetNote.load();
    g.setFont (juce::FontOptions (15.0f));
    g.setColour (ink.withAlpha (0.65f));
    if (midi > 0 && tgt >= 0)
    {
        const int cents = juce::roundToInt ((midi - (float) tgt) * 100.0f);
        g.drawText (names[tgt % 12 + 1] + juce::String (tgt / 12 - 1) + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + "c",
                    getWidth() - 184, 18, 160, 28, juce::Justification::centredRight);
    }

    // key section label
    g.setColour (ink.withAlpha (0.6f));
    g.drawText ("key", 22, 372, 100, 20, juce::Justification::centredLeft);

    const int selected = (int) proc.apvts.getRawParameterValue ("key")->load() - 1;
    const int best = proc.engine.keys.getBestKey();

    for (int k = 0; k < 24; ++k)
    {
        const auto r = keyRects[(size_t) k];
        const float h = glow[(size_t) k];
        const bool isSel = (k == selected);

        auto fill = juce::Colours::white.withAlpha (0.55f).interpolatedWith (accent, h * 0.85f);
        if (isSel) fill = deep;
        g.setColour (fill);
        g.fillRoundedRectangle (r, 8);

        if (! isSel && k == best && selected < 0)
        {
            g.setColour (deep.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.75f), 8, 1.5f);
        }

        g.setColour (isSel ? juce::Colours::white : (h > 0.5f ? juce::Colours::white : ink.withAlpha (0.45f + 0.5f * h)));
        g.setFont (juce::FontOptions (11.5f, h > 0.3f || isSel ? juce::Font::bold : juce::Font::plain));
        g.drawText (names[k + 1], r, juce::Justification::centred);
    }

    // status line
    juce::String status;
    const float prog = proc.engine.keys.getProgress();
    if (selected >= 0)            status = "locked to " + names[selected + 1] + "  -  tap again to unlock";
    else if (best >= 0)           status = "sounds like " + names[best + 1] + "  -  tap a key to lock it";
    else if (prog > 0.0f)         status = "listening... " + juce::String (juce::roundToInt (prog * 100)) + "%";
    else                          status = "play your vocal to detect the key";
    g.setColour (ink.withAlpha (0.6f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (status, 22, getHeight() - 40, 300, 24, juce::Justification::centredLeft);

    g.setColour (panel);
    g.fillRoundedRectangle (redetectRect, 12);
    g.setColour (deep);
    g.drawText ("re-detect", redetectRect, juce::Justification::centred);
}

void LilacTuneEditor::selectKey (int choiceIndex)
{
    if (auto* param = proc.apvts.getParameter ("key"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) choiceIndex));
        param->endChangeGesture();
    }
}

void LilacTuneEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (redetectRect.contains (p)) { proc.engine.keys.requestReset(); repaint(); return; }

    if (modeRect.contains (p))   // tap cycles auto -> live -> studio
        if (auto* param = proc.apvts.getParameter ("mode"))
        {
            const int next = ((int) proc.apvts.getRawParameterValue ("mode")->load() + 1) % 3;
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
            param->endChangeGesture();
            repaint();
            return;
        }

    const int selected = (int) proc.apvts.getRawParameterValue ("key")->load() - 1;
    for (int k = 0; k < 24; ++k)
        if (keyRects[(size_t) k].contains (p))
        {
            selectKey (k == selected ? 0 : k + 1);   // tap the locked key again -> back to Auto
            repaint();
            return;
        }
}

void LilacTuneEditor::timerCallback()
{
    for (int k = 0; k < 24; ++k)   // ease the highlights so they breathe instead of flicker
        glow[(size_t) k] += (proc.engine.keys.getHighlight (k) - glow[(size_t) k]) * 0.15f;
    repaint();
}
