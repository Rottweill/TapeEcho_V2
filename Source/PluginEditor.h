#pragma once
#include "PluginProcessor.h"

namespace TapeColours
{
    // główny odcień: RGB(7, 2, 46), czyli ciemny, nocny granat
    const juce::Colour bgTop   { 0xff0a0440 };
    const juce::Colour bgBot   { 0xff04011c };
    const juce::Colour panel   { 0xff110a48 };
    const juce::Colour panelLn { 0xff2b2478 };
    const juce::Colour inset   { 0xff0a0536 };   // tła przełączników
    const juce::Colour edge    { 0xff4d46a0 };   // obramowania przełączników
    const juce::Colour edgeHi  { 0xff7d76d0 };
    const juce::Colour cream   { 0xffeadfc8 };
    const juce::Colour muted   { 0xff8e8ad0 };   // lawendowy zamiast szarego

    const juce::Colour echo    { 0xffffa04a };   // ciepły pomarańcz
    const juce::Colour age     { 0xfff0c25a };   // bursztyn
    const juce::Colour duck    { 0xff62b4ff };   // błękit
    const juce::Colour chaos   { 0xffff5f7a };   // róż-czerwień
    const juce::Colour chorus  { 0xff5ee0c0 };   // morski
}

// Wygląd pokręteł i przełączników
class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Pokrętło: scroll myszy zmienia wartość, Shift/Ctrl + scroll = precyzyjnie
class KnobSlider : public juce::Slider
{
public:
    KnobSlider()
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                             juce::MathConstants<float>::pi * 2.75f, true);
        setScrollWheelEnabled (true);
        setMouseDragSensitivity (220);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto d = w;
        if (e.mods.isShiftDown() || e.mods.isCtrlDown())
        {
            d.deltaX *= 0.2f;
            d.deltaY *= 0.2f;
        }
        juce::Slider::mouseWheelMove (e, d);
    }
};

// Segmentowy przełącznik trybu: DUCK | COMP
class ModeSwitch : public juce::ToggleButton
{
protected:
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// Mały przełącznik zdarzenia (dioda + nazwa)
class ChipToggle : public juce::ToggleButton
{
protected:
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class TapeChorusEchoEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit TapeChorusEchoEditor (TapeChorusEchoProcessor&);
    ~TapeChorusEchoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        KnobSlider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;     // musi być ostatni (niszczony pierwszy)
    };

    template <typename ButtonType>
    struct BoundButton
    {
        ButtonType button;
        std::unique_ptr<ButtonAtt> att;
    };

    struct Panel
    {
        juce::String title;
        juce::Colour colour;
        juce::Rectangle<int> bounds;
    };

    struct Star { float x, y, r, a; };

    void initKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour accent);
    void initButton (juce::ToggleButton&, std::unique_ptr<ButtonAtt>&, const juce::String& paramId,
                     juce::Colour accent);
    void layoutPanel (Panel&, juce::Rectangle<int>, std::initializer_list<Knob*>, int extraBottom = 0);
    void timerCallback() override;

    TapeChorusEchoProcessor& proc;
    TapeLookAndFeel laf;

    Panel echoP, ageP, duckP, chaosP, chorusP;
    std::vector<Star> stars;

    Knob time, repeats, mix, tone, sat, wow;
    Knob degrade;
    Knob duckAmt, duckAtk, duckRel, compAmt;
    Knob chaosAmt, chaosRate;
    Knob cRate, cDepth, cMix;

    BoundButton<juce::ToggleButton> duckT, chaosT;
    BoundButton<ModeSwitch> modeT;
    BoundButton<ChipToggle> evNudge, evHold, evFlutter, evPull;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeChorusEchoEditor)
};
