#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float twoPi  = 6.283185307179586f;
    constexpr float halfPi = 1.5707963267948966f;
    constexpr float wobBaseMs = 2.0f;   // stałe opóźnienie pętli degradacji (kompensowane w czasie echa)

    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using StrFn = std::function<juce::String (float, int)>;
    using ValFn = std::function<float (const juce::String&)>;
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeChorusEchoProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto add = [&] (const char* id, const char* name, R range, float def, StrFn toStr, ValFn fromStr)
    {
        p.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def,
                                          juce::AudioParameterFloatAttributes()
                                              .withStringFromValueFunction (toStr)
                                              .withValueFromStringFunction (fromStr)));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    const StrFn pctS = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    const ValFn pctV = [] (const juce::String& t) { return t.getFloatValue() * 0.01f; };
    const StrFn msS  = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; };
    const ValFn numV = [] (const juce::String& t) { return t.getFloatValue(); };
    const StrFn hzS  = [] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v * 0.001f, 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    };
    const ValFn hzV  = [] (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        const float v = s.getFloatValue();
        return s.contains ("k") ? v * 1000.0f : v;
    };
    const StrFn dbS   = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    const StrFn rateS = [] (float v, int) { return juce::String (v, 2) + " Hz"; };
    const StrFn perS  = [] (float v, int) { return juce::String (v, 2) + " /s"; };

    // --- Echo ---
    add ("time",    "Echo Time",       R (20.0f, 1000.0f, 0.1f, 0.5f),   350.0f, msS,  numV);
    add ("repeats", "Repeats",         R (0.0f, 1.1f, 0.001f),           0.40f,  pctS, pctV);
    add ("mix",     "Delay Dry/Wet",   R (0.0f, 1.0f, 0.001f),           0.35f,  pctS, pctV);
    add ("tone",    "Tape Tone",       R (800.0f, 12000.0f, 1.0f, 0.4f), 5000.0f, hzS, hzV);
    add ("sat",     "Tape Saturation", R (0.0f, 1.0f, 0.001f),           0.30f,  pctS, pctV);
    add ("wow",     "Wow & Flutter",   R (0.0f, 1.0f, 0.001f),           0.30f,  pctS, pctV);

    // --- Tape age (degradacja kolejnych powtórek) ---
    add ("degrade", "Degrade",         R (0.0f, 1.0f, 0.001f),           0.35f,  pctS, pctV);

    // --- Ducking ---
    addBool ("duckOn", "Ducking", false);
    add ("duckAmt", "Duck Amount",     R (0.0f, 40.0f, 0.1f),            12.0f,  dbS,  numV);
    add ("duckAtk", "Duck Attack",     R (1.0f, 250.0f, 0.1f, 0.4f),     15.0f,  msS,  numV);
    add ("duckRel", "Duck Release",    R (20.0f, 2000.0f, 1.0f, 0.4f),   350.0f, msS,  numV);

    // --- Chaos ---
    addBool ("chaosOn", "Chaos", false);
    add ("chaosAmt",  "Chaos Amount",  R (0.0f, 1.0f, 0.001f),           0.50f,  pctS, pctV);
    add ("chaosRate", "Chaos Rate",    R (0.05f, 4.0f, 0.01f, 0.5f),     0.50f,  perS, numV);

    // --- Chorus ---
    add ("cRate",   "Chorus Rate",     R (0.1f, 6.0f, 0.01f, 0.5f),      0.8f,   rateS, numV);
    add ("cDepth",  "Chorus Depth",    R (0.0f, 1.0f, 0.001f),           0.5f,   pctS, pctV);
    add ("cMix",    "Chorus Dry/Wet",  R (0.0f, 1.0f, 0.001f),           0.5f,   pctS, pctV);

    return { p.begin(), p.end() };
}

TapeChorusEchoProcessor::TapeChorusEchoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool TapeChorusEchoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void TapeChorusEchoProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int echoSize   = (int) (sr * 1.4) + 16;
    const int wobbleSize = (int) (sr * 0.02) + 16;
    const int chorusSize = (int) (sr * 0.06) + 16;

    for (int c = 0; c < maxCh; ++c)
    {
        echoLine[c].prepare (echoSize);
        wobbleLine[c].prepare (wobbleSize);
        chorusLine[c].prepare (chorusSize);
        hpState[c] = lpState[c] = chLp[c] = noiseLp[c] = 0.0f;
        dropGain[c] = dropTarget[c] = 1.0f;
        dropCount[c] = 0;
    }

    wowPhase = flutterPhase = chorusPhase = wobPhase1 = wobPhase2 = chaosFlutPhase = 0.0;
    detEnv = duckState = 0.0f;
    chaosOffset = chaosOffsetTarget = 0.0f;
    chaosHold = 0;
    muffle = flutterAmp = 0.0f;
    chaosSlewCoef = coefFromMs (80.0f);
    muffleDecay   = std::exp (-1.0f / (0.35f * (float) sr));
    flutterDecay  = std::exp (-1.0f / (0.25f * (float) sr));
    dropCoef      = coefFromMs (2.0f);
    detRelCoef    = coefFromMs (20.0f);
    rng.setSeed (juce::Time::currentTimeMillis());

    auto init = [&] (auto& sm, const char* id, double secs)
    {
        sm.reset (sr, secs);
        sm.setCurrentAndTargetValue (apvts.getRawParameterValue (id)->load());
    };
    init (timeSm,  "time",    0.20);   // wolna zmiana czasu = "tape pitch glide"
    init (fbSm,    "repeats", 0.05);
    init (mixSm,   "mix",     0.05);
    init (toneSm,  "tone",    0.05);
    init (satSm,   "sat",     0.05);
    init (wowSm,   "wow",     0.05);
    init (degSm,   "degrade", 0.05);
    init (cRateSm, "cRate",   0.05);
    init (cDepthSm,"cDepth",  0.05);
    init (cMixSm,  "cMix",    0.05);
}

// Losowe "zdarzenie" ręki na taśmie
void TapeChorusEchoProcessor::triggerChaos (float amt)
{
    auto r = [this] { return rng.nextFloat(); };
    const float dir = rng.nextBool() ? 1.0f : -1.0f;

    switch (rng.nextInt (4))
    {
        case 0: // szarpnięcie taśmy w jedną lub drugą stronę (krótkie pitch-bendy)
            chaosOffsetTarget = dir * amt * (6.0f + 24.0f * r());
            chaosSlewCoef     = coefFromMs (35.0f + 50.0f * r());
            chaosHold         = (int) (sr * (0.06 + 0.20 * r()));
            break;

        case 1: // palec przytrzymujący szpulę: zwolnienie + przyciemnienie
            chaosOffsetTarget = amt * (25.0f + 60.0f * r());
            chaosSlewCoef     = coefFromMs (180.0f + 150.0f * r());
            chaosHold         = (int) (sr * (0.25 + 0.50 * r()));
            muffle            = juce::jmax (muffle, amt * (0.5f + 0.5f * r()));
            break;

        case 2: // drżenie / "scrubbing" - szybki flutter
            flutterAmp   = amt * (1.5f + 3.5f * r());
            flutterFreq  = 4.0f + 10.0f * r();
            chaosHold    = (int) (sr * 0.10);
            break;

        default: // pociągnięcie taśmy do przodu (pitch w górę) + lekkie przytłumienie
            chaosOffsetTarget = -amt * (10.0f + 30.0f * r());
            chaosSlewCoef     = coefFromMs (120.0f + 120.0f * r());
            chaosHold         = (int) (sr * (0.15 + 0.30 * r()));
            muffle            = juce::jmax (muffle, amt * 0.3f);
            break;
    }
    chaosHold = juce::jmax (1, chaosHold);
}

void TapeChorusEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = juce::jmin (buffer.getNumChannels(), maxCh);
    const int numSamples = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    timeSm .setTargetValue (get ("time"));
    fbSm   .setTargetValue (get ("repeats"));
    mixSm  .setTargetValue (get ("mix"));
    toneSm .setTargetValue (get ("tone"));
    satSm  .setTargetValue (get ("sat"));
    wowSm  .setTargetValue (get ("wow"));
    degSm  .setTargetValue (get ("degrade"));
    cRateSm.setTargetValue (get ("cRate"));
    cDepthSm.setTargetValue(get ("cDepth"));
    cMixSm .setTargetValue (get ("cMix"));

    const bool  duckOn    = get ("duckOn") > 0.5f;
    const float duckAmtDb = get ("duckAmt");
    const float atkCoef   = coefFromMs (get ("duckAtk"));
    const float relCoef   = coefFromMs (get ("duckRel"));

    const bool  chaosOn   = get ("chaosOn") > 0.5f;
    const float chaosAmt  = get ("chaosAmt");
    const float chaosRate = get ("chaosRate");

    float* data[maxCh] {};
    for (int c = 0; c < numCh; ++c)
        data[c] = buffer.getWritePointer (c);

    const float msToSamp = (float) (sr * 0.001);

    for (int i = 0; i < numSamples; ++i)
    {
        const float timeMs = timeSm.getNextValue();
        const float fb     = fbSm.getNextValue();
        const float mix    = mixSm.getNextValue();
        const float tone   = toneSm.getNextValue();
        const float sat    = satSm.getNextValue();
        const float wow    = wowSm.getNextValue();
        const float deg    = degSm.getNextValue();
        const float cRate  = cRateSm.getNextValue();
        const float cDepth = cDepthSm.getNextValue();
        const float cMix   = cMixSm.getNextValue();

        // ---------- DUCKING: detektor poziomu wejścia ----------
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (data[ch][i]));

        detEnv = (peak > detEnv) ? peak : detEnv + (peak - detEnv) * detRelCoef;

        float duckTarget = 0.0f;
        if (duckOn)
        {
            const float db = juce::Decibels::gainToDecibels (detEnv, -100.0f);
            duckTarget = juce::jlimit (0.0f, 1.0f, (db + 50.0f) / 20.0f);   // pełne ściszanie od ok. -30 dB
        }
        duckState += (duckTarget - duckState) * (duckTarget > duckState ? atkCoef : relCoef);
        const float duckGain = juce::Decibels::decibelsToGain (-duckAmtDb * duckState);

        // ---------- CHAOS: losowe "ruchy ręką" ----------
        if (chaosOn && chaosAmt > 0.001f)
        {
            if (chaosHold > 0)
            {
                if (--chaosHold == 0)
                {
                    chaosOffsetTarget = 0.0f;                    // puszczenie taśmy - powrót
                    chaosSlewCoef = coefFromMs (160.0f);
                }
            }
            else if (rng.nextFloat() < chaosRate / (float) sr)
            {
                triggerChaos (chaosAmt);
            }
        }
        else
        {
            chaosOffsetTarget = 0.0f;
            chaosHold = 0;
        }

        chaosOffset += (chaosOffsetTarget - chaosOffset) * chaosSlewCoef;
        muffle      *= muffleDecay;
        flutterAmp  *= flutterDecay;
        chaosFlutPhase += (double) flutterFreq / sr;  if (chaosFlutPhase >= 1.0) chaosFlutPhase -= 1.0;

        float chaosMs = chaosOffset;
        if (flutterAmp > 1.0e-4f)
            chaosMs += flutterAmp * std::sin (twoPi * (float) chaosFlutPhase);

        const bool  muf    = muffle > 0.003f;
        float       chCoef = 1.0f;
        if (muf)
        {
            const float fc = 18000.0f * std::exp (-3.0f * juce::jlimit (0.0f, 1.0f, muffle));
            chCoef = 1.0f - std::exp (-twoPi * fc / (float) sr);
        }

        // ---------- stałe dla pętli ----------
        const float hpCoef = 1.0f - std::exp (-twoPi * (90.0f + 220.0f * deg) / (float) sr);
        const float lpCoef = 1.0f - std::exp (-twoPi * tone * (1.0f - 0.65f * deg) / (float) sr);
        const float drive  = 1.0f + 5.0f * sat + 1.5f * deg;
        const float wetG   = std::sin (mix * halfPi);   // dry/wet z zachowaniem głośności
        const float dryG   = std::cos (mix * halfPi);

        // LFO
        wowPhase     += 0.55 / sr;  if (wowPhase     >= 1.0) wowPhase     -= 1.0;
        flutterPhase += 6.8  / sr;  if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        wobPhase1    += 1.1  / sr;  if (wobPhase1    >= 1.0) wobPhase1    -= 1.0;
        wobPhase2    += 0.37 / sr;  if (wobPhase2    >= 1.0) wobPhase2    -= 1.0;
        chorusPhase  += (double) cRate / sr;  if (chorusPhase >= 1.0) chorusPhase -= 1.0;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in    = data[ch][i];
            const float chOff = (ch == 0) ? 0.0f : 1.0f;

            // ---------- TAPE ECHO ----------
            const float wowMod = std::sin (twoPi * (float) (wowPhase + 0.08 * chOff))
                               + 0.25f * std::sin (twoPi * (float) (flutterPhase + 0.13 * chOff));
            const float modMs  = juce::jmax (-0.7f * timeMs,
                                             wow * 2.5f * wowMod + chaosMs * (1.0f - 0.12f * chOff));
            const float delaySamp = juce::jmax (2.0f, (timeMs + modMs - wobBaseMs) * msToSamp);

            float echo = echoLine[ch].read (delaySamp);

            // chaos: palec na taśmie przytłumia sygnał
            if (muf) { chLp[ch] += chCoef * (echo - chLp[ch]); echo = chLp[ch]; }
            else     { chLp[ch] = echo; }

            // ---------- ANALOGOWA DEGRADACJA (każdy przebieg przez pętlę) ----------
            float f = echo;

            // 1) utrata basu i wysokich tonów (HP rośnie, LP spada z Degrade)
            hpState[ch] += hpCoef * (f - hpState[ch]);  f -= hpState[ch];
            lpState[ch] += lpCoef * (f - lpState[ch]);  f  = lpState[ch];

            // 2) "rozmycie" wysokości dźwięku: powolny wobble w pętli (kumuluje się z każdym powtórzeniem)
            wobbleLine[ch].push (f);
            const float lfo = 0.6f * std::sin (twoPi * (float) (wobPhase1 + 0.17 * chOff))
                            + 0.4f * std::sin (twoPi * (float) (wobPhase2 + 0.31 * chOff));
            f = wobbleLine[ch].read (juce::jmax (2.0f, (wobBaseMs + deg * 1.6f * lfo) * msToSamp));

            if (deg > 0.001f)
            {
                // 3) szum modulacyjny taśmy (ziarno), mnożone przez sygnał, więc bez sykania w ciszy
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                noiseLp[ch] += 0.2f * (n - noiseLp[ch]);
                f *= 1.0f + deg * 1.2f * noiseLp[ch];

                // 4) losowe dropouty (ubytki tlenku)
                if (dropCount[ch] > 0)
                {
                    if (--dropCount[ch] == 0) dropTarget[ch] = 1.0f;
                }
                else if (rng.nextFloat() < deg * 2.5f / (float) sr)
                {
                    dropTarget[ch] = 0.15f + 0.5f * rng.nextFloat();
                    dropCount[ch]  = (int) (sr * (0.004 + 0.030 * rng.nextFloat()));
                }
            }
            else
            {
                dropTarget[ch] = 1.0f;
                dropCount[ch]  = 0;
            }
            dropGain[ch] += (dropTarget[ch] - dropGain[ch]) * dropCoef;
            f *= dropGain[ch];

            // ---------- sprzężenie zwrotne + saturacja taśmy ----------
            float toLine = in + f * fb;
            toLine = std::tanh (toLine * drive) / drive;
            echoLine[ch].push (toLine);

            // wyjście: dry/wet (equal-power), ducking działa tylko na echo
            const float afterEcho = in * dryG + f * duckGain * wetG;

            // ---------- CHORUS (2 głosy, przeciwna faza L/R) ----------
            chorusLine[ch].push (afterEcho);

            const float ph1 = (float) chorusPhase + 0.5f * chOff;
            const float ph2 = ph1 + 0.5f;
            const float depthMs = 0.5f + 3.5f * cDepth;
            const float d1 = (7.0f  + depthMs * std::sin (twoPi * ph1)) * msToSamp;
            const float d2 = (10.5f + depthMs * std::sin (twoPi * ph2)) * msToSamp;
            const float chorusWet = 0.5f * (chorusLine[ch].read (d1) + chorusLine[ch].read (d2));

            data[ch][i] = afterEcho * (1.0f - cMix) + chorusWet * cMix;
        }
    }
}

juce::AudioProcessorEditor* TapeChorusEchoProcessor::createEditor()
{
    return new TapeChorusEchoEditor (*this);
}

void TapeChorusEchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeChorusEchoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeChorusEchoProcessor();
}
