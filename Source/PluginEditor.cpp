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
    g.setColour (juce::Colour (0xff251d68));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // wartość
    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // cień + korpus (granatowo-indygowy)
    g.setColour (juce::Colour (0xff020010).withAlpha (0.55f));
    g.fillEllipse (c.x - knobR, c.y - knobR + 3.0f, knobR * 2.0f, knobR * 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6e67c4), c.x - knobR * 0.5f, c.y - knobR * 0.6f,
                                             juce::Colour (0xff120c40), c.x + knobR * 0.6f, c.y + knobR * 0.8f, true));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);

    g.setColour (juce::Colour (0xff04011c).withAlpha (0.8f));
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

    g.setColour (on ? accent.withAlpha (0.18f) : inset);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    g.setColour (on ? accent : (highlighted ? edgeHi : edge));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.4f);

    auto led = juce::Rectangle<float> (r.getX() + 9.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? accent : juce::Colour (0xff3b3585));
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
// Własne przyciski
//==============================================================================
void ModeSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = findColour (juce::ToggleButton::tickColourId);
    const bool comp = getToggleState();
    const float h = r.getHeight();

    g.setColour (inset);
    g.fillRoundedRectangle (r, h * 0.5f);

    auto left  = r.withWidth (r.getWidth() * 0.5f);
    auto right = r.withTrimmedLeft (r.getWidth() * 0.5f);

    g.setColour (accent.withAlpha (0.30f));
    g.fillRoundedRectangle ((comp ? right : left).reduced (2.0f), (h - 4.0f) * 0.5f);

    g.setColour (highlighted ? edgeHi : edge);
    g.drawRoundedRectangle (r, h * 0.5f, 1.4f);

    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.setColour (comp ? muted : cream);
    g.drawText ("DUCK", left.toNearestInt(), juce::Justification::centred);
    g.setColour (comp ? cream : muted);
    g.drawText ("COMP", right.toNearestInt(), juce::Justification::centred);
}

void ChipToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = findColour (juce::ToggleButton::tickColourId);
    const bool on = getToggleState();

    g.setColour (on ? accent.withAlpha (0.16f) : inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? accent : (highlighted ? edgeHi : juce::Colour (0xff3b3585)));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    auto led = juce::Rectangle<float> (r.getX() + 6.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (2.0f));
    }
    g.setColour (on ? accent : juce::Colour (0xff3b3585));
    g.fillEllipse (led);

    g.setColour (on ? cream : muted);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (getButtonText(), r.withTrimmedLeft (13.0f).toNearestInt(), juce::Justification::centred);
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
    initKnob (compAmt, "compAmt", "Comp",          duck);
    initButton (duckT.button, duckT.att, "duckOn",   duck);
    initButton (modeT.button, modeT.att, "duckComp", duck);

    initKnob (chaosAmt,  "chaosAmt",  "Amount",    chaos);
    initKnob (chaosRate, "chaosRate", "Rate",      chaos);
    initButton (chaosT.button, chaosT.att, "chaosOn", chaos);

    evNudge.button.setButtonText   ("NUDGE");
    evHold.button.setButtonText    ("HOLD");
    evFlutter.button.setButtonText ("FLUTTER");
    evPull.button.setButtonText    ("PULL");
    initButton (evNudge.button,   evNudge.att,   "chaosNudge",   chaos);
    initButton (evHold.button,    evHold.att,    "chaosHold",    chaos);
    initButton (evFlutter.button, evFlutter.att, "chaosFlutter", chaos);
    initButton (evPull.button,    evPull.att,    "chaosPull",    chaos);

    initKnob (cRate,  "cRate",  "Rate",            chorus);
    initKnob (cDepth, "cDepth", "Depth",           chorus);
    initKnob (cMix,   "cMix",   "Dry/Wet",         chorus);

    // gwiazdy na nocnym niebie (stałe, deterministyczne)
    juce::Random rnd (2024);
    for (int i = 0; i < 160; ++i)
    {
        const float q = rnd.nextFloat();
        stars.push_back ({ rnd.nextFloat() * 940.0f, rnd.nextFloat() * 480.0f,
                           0.5f + rnd.nextFloat() * 1.0f, 0.10f + 0.55f * q * q });
    }

    setSize (940, 480);
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

void TapeChorusEchoEditor::initButton (juce::ToggleButton& b, std::unique_ptr<ButtonAtt>& att,
                                       const juce::String& paramId, juce::Colour accent)
{
    b.setColour (juce::ToggleButton::tickColourId, accent);
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
    att = std::make_unique<ButtonAtt> (proc.apvts, paramId, b);
}

void TapeChorusEchoEditor::layoutPanel (Panel& panel, juce::Rectangle<int> r,
                                        std::initializer_list<Knob*> knobs, int extraBottom)
{
    panel.bounds = r;

    auto inner = r.reduced (12, 0);
    inner.removeFromTop (36);
    inner.removeFromBottom (8 + extraBottom);

    const int w = inner.getWidth() / (int) knobs.size();
    for (auto* k : knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
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
    auto duckR = rowB.removeFromLeft (juce::roundToInt ((float) wB * 0.38f));
    rowB.removeFromLeft (gap);
    auto chaosR = rowB.removeFromLeft (juce::roundToInt ((float) wB * 0.30f));
    rowB.removeFromLeft (gap);
    auto chorusR = rowB;

    layoutPanel (duckP,   duckR,   { &duckAmt, &duckAtk, &duckRel, &compAmt });
    layoutPanel (chaosP,  chaosR,  { &chaosAmt, &chaosRate }, 32);
    layoutPanel (chorusP, chorusR, { &cRate, &cDepth, &cMix });

    // ducking: przełącznik trybu + ON/OFF w wierszu tytułu
    duckT.button.setBounds (duckP.bounds.getRight() - 12 - 66, duckP.bounds.getY() + 9, 66, 24);
    modeT.button.setBounds (duckT.button.getX() - 8 - 92,      duckP.bounds.getY() + 9, 92, 24);

    // chaos: ON/OFF w wierszu tytułu, 4 przełączniki zdarzeń na dole panelu
    chaosT.button.setBounds (chaosP.bounds.getRight() - 12 - 66, chaosP.bounds.getY() + 9, 66, 24);

    auto chips = chaosP.bounds.reduced (12, 0);
    chips.removeFromBottom (8);
    chips = chips.removeFromBottom (24);
    const int chipGap = 4;
    const int chipW   = (chips.getWidth() - 3 * chipGap) / 4;
    evNudge.button.setBounds   (chips.removeFromLeft (chipW));  chips.removeFromLeft (chipGap);
    evHold.button.setBounds    (chips.removeFromLeft (chipW));  chips.removeFromLeft (chipGap);
    evFlutter.button.setBounds (chips.removeFromLeft (chipW));  chips.removeFromLeft (chipGap);
    evPull.button.setBounds    (chips.removeFromLeft (chipW));
}

void TapeChorusEchoEditor::paint (juce::Graphics& g)
{
    // nocne niebo
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBot, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // delikatna poświata w prawym górnym rogu
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3bd0).withAlpha (0.22f),
                                             (float) getWidth() * 0.85f, 0.0f,
                                             juce::Colours::transparentBlack,
                                             (float) getWidth() * 0.85f, 300.0f, true));
    g.fillAll();

    // gwiazdy
    for (const auto& s : stars)
    {
        if (s.r > 1.3f)
        {
            g.setColour (cream.withAlpha (s.a * 0.25f));
            g.fillEllipse (s.x - s.r * 2.0f, s.y - s.r * 2.0f, s.r * 4.0f, s.r * 4.0f);
        }
        g.setColour (cream.withAlpha (s.a));
        g.fillEllipse (s.x - s.r * 0.5f, s.y - s.r * 0.5f, s.r, s.r);
    }

    // nagłówek
    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("TAPE CHORUS ECHO", 22, 10, 420, 38, juce::Justification::centredLeft);

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("TAPE DELAY  /  CHORUS", getWidth() - 340, 10, 318, 38, juce::Justification::centredRight);

    g.setColour (echo.withAlpha (0.85f));
    g.fillRect (22, 50, 56, 2);

    // panele (lekko półprzezroczyste, więc gwiazdy delikatnie prześwitują)
    for (auto* p : { &echoP, &ageP, &duckP, &chaosP, &chorusP })
    {
        auto r = p->bounds.toFloat();

        g.setColour (panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (panelLn);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (p->colour);
        g.fillRoundedRectangle (r.getX() + 14.0f, r.getY() + 15.0f, 4.0f, 14.0f, 2.0f);

        g.setColour (cream.withAlpha (0.92f));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (p->title, (int) r.getX() + 25, (int) r.getY() + 9, 90, 26, juce::Justification::centredLeft);
    }
}

void TapeChorusEchoEditor::timerCallback()
{
    // przyciemnia elementy, które w danym momencie nie działają (nadal można nimi kręcić)
    auto setDim = [] (std::initializer_list<Knob*> ks, bool active)
    {
        for (auto* k : ks)
        {
            k->slider.setAlpha (active ? 1.0f : 0.45f);
            k->label.setAlpha  (active ? 1.0f : 0.45f);
        }
    };

    const bool duckOn   = proc.apvts.getRawParameterValue ("duckOn")->load()   > 0.5f;
    const bool compMode = proc.apvts.getRawParameterValue ("duckComp")->load() > 0.5f;
    const bool chaosOn  = proc.apvts.getRawParameterValue ("chaosOn")->load()  > 0.5f;

    setDim ({ &duckAmt },            duckOn && ! compMode);   // ściszanie: tylko w trybie DUCK
    setDim ({ &compAmt },            duckOn && compMode);     // kompresja: tylko w trybie COMP
    setDim ({ &duckAtk, &duckRel },  duckOn);                 // wspólne dla obu trybów
    setDim ({ &chaosAmt, &chaosRate }, chaosOn);

    const float chipAlpha = chaosOn ? 1.0f : 0.45f;
    evNudge.button.setAlpha (chipAlpha);
    evHold.button.setAlpha (chipAlpha);
    evFlutter.button.setAlpha (chipAlpha);
    evPull.button.setAlpha (chipAlpha);
}
