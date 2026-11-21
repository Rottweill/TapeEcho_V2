#pragma once
#include "PluginProcessor.h"

namespace TapeColours
{
    const juce::Colour bgTop   { 0xff27231f };
    const juce::Colour bgBot   { 0xff181513 };
    const juce::Colour panel   { 0xff2e2a26 };
    const juce::Colour panelLn { 0xff433d36 };
    const juce::Colour cream   { 0xffeadfc8 };
    const juce::Colour muted   { 0xff9d9484 };

    const juce::Colour echo    { 0xffe8803a };   // pomarańcz
    const juce::Colour age     { 0xffe2b23c };   // bursztyn
    const juce::Colour duck    { 0xff5aa6d6 };   // błękit
    const juce::Colour chaos   { 0xffd9544f };   // czerwień
    const juce::Colour chorus  { 0xff4fb59b };   // morski
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

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAtt> att;
    };

    struct Panel
    {
        juce::String title;
        juce::Colour colour;
        juce::Rectangle<int> bounds;
    };

    void initKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour accent);
    void initToggle (Toggle&, const juce::String& paramId, juce::Colour accent);
    void layoutPanel (Panel&, juce::Rectangle<int>, std::initializer_list<Knob*>);
    void placeToggle (Toggle&, const Panel&);
    void timerCallback() override;

    TapeChorusEchoProcessor& proc;
    TapeLookAndFeel laf;

    Panel echoP, ageP, duckP, chaosP, chorusP;

    Knob time, repeats, mix, tone, sat, wow;
    Knob degrade;
    Knob duckAmt, duckAtk, duckRel;
    Knob chaosAmt, chaosRate;
    Knob cRate, cDepth, cMix;

    Toggle duckT, chaosT;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeChorusEchoEditor)
};
