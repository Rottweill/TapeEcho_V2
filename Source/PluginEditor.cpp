#include "PluginEditor.h"

using namespace TapeColours;

//==============================================================================
// LookAndFeel
//==============================================================================
void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c      = bounds.getCentre();
    const float knobR  = radius * 0.70f;
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const auto  accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    // tor
    const float arcR = radius - 3.0f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff3b362f));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // wartość
    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // cień + korpus
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - knobR, c.y - knobR + 3.0f, knobR * 2.0f, knobR * 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7b7268), c.x - knobR * 0.5f, c.y - knobR * 0.6f,
                                             juce::Colour (0xff211e1b), c.x + knobR * 0.6f, c.y + knobR * 0.8f, true));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

    // wskaźnik
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.8f, -knobR + 4.0f, 3.6f, knobR * 0.45f, 1.5f);
    g.setColour (cream);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? accent.withAlpha (0.18f) : juce::Colour (0xff1c1917));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    g.setColour (on ? accent : (highlighted ? juce::Colour (0xff7a7268) : juce::Colour (0xff57514a)));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.4f);

    auto led = juce::Rectangle<float> (r.getX() + 9.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? accent : juce::Colour (0xff4a443d));
    g.fillEllipse (led);

    g.setColour (on ? cream : muted);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (on ? "ON" : "OFF", r.withTrimmedLeft (22.0f).toNearestInt(), juce::Justification::centred);
}

juce::Label* TapeLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (12.5f)));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, cream);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
// Editor
//==============================================================================
TapeChorusEchoEditor::TapeChorusEchoEditor (TapeChorusEchoProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    echoP   = { "ECHO",     echo,   {} };
    ageP    = { "TAPE AGE", age,    {} };
    duckP   = { "DUCKING",  duck,   {} };
    chaosP  = { "CHAOS",    chaos,  {} };
    chorusP = { "CHORUS",   chorus, {} };

    initKnob (time,    "time",    "Time",          echo);
    initKnob (repeats, "repeats", "Repeats",       echo);
    initKnob (mix,     "mix",     "Dry/Wet",       echo);
    initKnob (tone,    "tone",    "Tone",          echo);
    initKnob (sat,     "sat",     "Saturation",    echo);
    initKnob (wow,     "wow",     "Wow & Flutter", echo);

    initKnob (degrade, "degrade", "Degrade",       age);

    initKnob (duckAmt, "duckAmt", "Amount",        duck);
    initKnob (duckAtk, "duckAtk", "Attack",        duck);
    initKnob (duckRel, "duckRel", "Release",       duck);
    initToggle (duckT, "duckOn", duck);

    initKnob (chaosAmt,  "chaosAmt",  "Amount",    chaos);
    initKnob (chaosRate, "chaosRate", "Rate",      chaos);
    initToggle (chaosT, "chaosOn", chaos);

    initKnob (cRate,  "cRate",  "Rate",            chorus);
    initKnob (cDepth, "cDepth", "Depth",           chorus);
    initKnob (cMix,   "cMix",   "Dry/Wet",         chorus);

    setSize (940, 464);
    startTimerHz (10);
    timerCallback();
}

TapeChorusEchoEditor::~TapeChorusEchoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TapeChorusEchoEditor::initKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                                     juce::Colour accent)
{
    addAndMakeVisible (k.slider);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setColour (juce::Slider::textBoxTextColourId, cream);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    // ponowne utworzenie pola tekstowego już z naszym LookAndFeel
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);

    k.label.setText (name.toUpperCase(), juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, muted);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.att = std::make_unique<SliderAtt> (proc.apvts, paramId, k.slider);
}

void TapeChorusEchoEditor::initToggle (Toggle& t, const juce::String& paramId, juce::Colour accent)
{
    t.button.setColour (juce::ToggleButton::tickColourId, accent);
    t.button.setClickingTogglesState (true);
    addAndMakeVisible (t.button);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, paramId, t.button);
}

void TapeChorusEchoEditor::layoutPanel (Panel& panel, juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
{
    panel.bounds = r;

    auto inner = r.reduced (12, 0);
    inner.removeFromTop (36);
    inner.removeFromBottom (8);

    const int w = inner.getWidth() / (int) knobs.size();
    for (auto* k : knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void TapeChorusEchoEditor::placeToggle (Toggle& t, const Panel& p)
{
    t.button.setBounds (p.bounds.getRight() - 12 - 66, p.bounds.getY() + 9, 66, 24);
}

void TapeChorusEchoEditor::resized()
{
    const int m = 16, headerH = 58, gap = 10;

    auto area = getLocalBounds().reduced (m, 0);
    area.removeFromTop (headerH);
    area.removeFromBottom (m);

    const int rowH = (area.getHeight() - gap) / 2;
    auto rowA = area.removeFromTop (rowH);
    area.removeFromTop (gap);
    auto rowB = area.removeFromTop (rowH);

    // rząd A: ECHO + TAPE AGE
    auto echoR = rowA.removeFromLeft (juce::roundToInt ((float) rowA.getWidth() * 0.74f));
    rowA.removeFromLeft (gap);
    layoutPanel (echoP, echoR, { &time, &repeats, &mix, &tone, &sat, &wow });
    layoutPanel (ageP,  rowA,  { &degrade });

    // rząd B: DUCKING + CHAOS + CHORUS
    const int wB = rowB.getWidth() - 2 * gap;
    auto duckR = rowB.removeFromLeft (juce::roundToInt ((float) wB * 0.36f));
    rowB.removeFromLeft (gap);
    auto chaosR = rowB.removeFromLeft (juce::roundToInt ((float) wB * 0.28f));
    rowB.removeFromLeft (gap);
    auto chorusR = rowB;

    layoutPanel (duckP,   duckR,   { &duckAmt, &duckAtk, &duckRel });
    layoutPanel (chaosP,  chaosR,  { &chaosAmt, &chaosRate });
    layoutPanel (chorusP, chorusR, { &cRate, &cDepth, &cMix });

    placeToggle (duckT,  duckP);
    placeToggle (chaosT, chaosP);
}

void TapeChorusEchoEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBot, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // nagłówek
    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("TAPE CHORUS ECHO", 22, 10, 420, 38, juce::Justification::centredLeft);

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("TAPE DELAY  /  CHORUS", getWidth() - 340, 10, 318, 38, juce::Justification::centredRight);

    g.setColour (echo.withAlpha (0.8f));
    g.fillRect (22, 50, 56, 2);

    // panele
    for (auto* p : { &echoP, &ageP, &duckP, &chaosP, &chorusP })
    {
        auto r = p->bounds.toFloat();

        g.setColour (panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (panelLn);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (p->colour);
        g.fillRoundedRectangle (r.getX() + 14.0f, r.getY() + 15.0f, 4.0f, 14.0f, 2.0f);

        g.setColour (cream.withAlpha (0.92f));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (p->title, (int) r.getX() + 25, (int) r.getY() + 9, 200, 26, juce::Justification::centredLeft);
    }
}

void TapeChorusEchoEditor::timerCallback()
{
    // przyciemnia sekcje, które są wyłączone (nadal można nimi kręcić)
    auto setDim = [] (std::initializer_list<Knob*> ks, bool active)
    {
        for (auto* k : ks)
        {
            k->slider.setAlpha (active ? 1.0f : 0.45f);
            k->label.setAlpha  (active ? 1.0f : 0.45f);
        }
    };

    setDim ({ &duckAmt, &duckAtk, &duckRel }, proc.apvts.getRawParameterValue ("duckOn")->load()  > 0.5f);
    setDim ({ &chaosAmt, &chaosRate },        proc.apvts.getRawParameterValue ("chaosOn")->load() > 0.5f);
}
