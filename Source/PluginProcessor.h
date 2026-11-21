#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>

// Prosta linia opóźniająca z interpolacją Hermite'a
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;

    void prepare (int size) { buf.assign ((size_t) size, 0.0f); w = 0; }

    void push (float x)
    {
        buf[(size_t) w] = x;
        if (++w >= (int) buf.size()) w = 0;
    }

    // d = opóźnienie w próbkach (>= 2)
    float read (float d) const
    {
        const int n = (int) buf.size();
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) n;
        int i1 = (int) rp;
        const float f = rp - (float) i1;
        if (i1 >= n) i1 -= n;
        const int i0 = (i1 == 0) ? n - 1 : i1 - 1;
        const int i2 = (i1 + 1) % n;
        const int i3 = (i1 + 2) % n;

        const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1],
                    y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

class TapeChorusEchoProcessor : public juce::AudioProcessor
{
public:
    TapeChorusEchoProcessor();
    ~TapeChorusEchoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tape Chorus Echo"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void triggerChaos (float amt);
    float coefFromMs (float ms) const
    {
        return 1.0f - std::exp (-1.0f / (juce::jmax (0.01f, ms) * 0.001f * (float) sr));
    }

    static constexpr int maxCh = 2;
    double sr = 44100.0;

    // linie opóźniające: echo, "wobble" w pętli (degradacja), chorus
    DelayLine echoLine[maxCh], wobbleLine[maxCh], chorusLine[maxCh];

    // stany filtrów / degradacji (osobno na kanał)
    float hpState[maxCh] {}, lpState[maxCh] {}, chLp[maxCh] {}, noiseLp[maxCh] {};
    float dropGain[maxCh] { 1.0f, 1.0f }, dropTarget[maxCh] { 1.0f, 1.0f };
    int   dropCount[maxCh] {};
    float dcState[maxCh] {};

    // LFO
    double wowPhase = 0.0, flutterPhase = 0.0, chorusPhase = 0.0,
           wobPhase1 = 0.0, wobPhase2 = 0.0, chaosFlutPhase = 0.0;

    // ducking
    float detEnv = 0.0f, duckState = 0.0f;

    // kompresor taśmowy (tryb 2 sekcji ducking)
    float compEnv = 0.0f, compGrDb = 0.0f, compMix = 0.0f;

    // chaos ("ręka na taśmie")
    juce::Random rng;
    int   chaosMask = 15;     // które zdarzenia są włączone (bity 0-3)
    float chaosOffsetTarget = 0.0f, chaosOffset1 = 0.0f, chaosOffset = 0.0f, chaosSlewCoef = 0.001f;
    float chaosMsLim = 0.0f;  // ograniczony szybkościowo wynik modulacji czasu
    int   chaosHold = 0;
    float muffleTarget = 0.0f, muffle = 0.0f;
    float flutterAmpTarget = 0.0f, flutterAmp = 0.0f, flutterFreq = 8.0f;

    // współczynniki zależne od sample rate
    float muffleDecay = 0.999f, flutterDecay = 0.999f, dropCoef = 0.01f, detRelCoef = 0.001f,
          flutterAtkCoef = 0.01f, muffleAtkCoef = 0.01f, compMixCoef = 0.01f, compDetRelCoef = 0.01f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSm;   // wolna zmiana = "pitch glide" taśmy
    juce::SmoothedValue<float> fbSm, mixSm, toneSm, satSm, wowSm, degSm,
                               cRateSm, cDepthSm, cMixSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeChorusEchoProcessor)
};
