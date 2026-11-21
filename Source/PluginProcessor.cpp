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
    const StrFn msS  = [] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v * 0.001f, 2) + " s"
                            : juce::String (juce::roundToInt (v)) + " ms";
    };
    const ValFn msV  = [] (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        const float v = s.getFloatValue();
        if (s.endsWith ("ms")) return v;
        if (s.endsWith ("s"))  return v * 1000.0f;
        return v;
    };
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
    add ("time",    "Echo Time",       R (20.0f, 2000.0f, 0.1f, 0.5f),   350.0f, msS,  msV);
    add ("repeats", "Repeats",         R (0.0f, 1.1f, 0.001f),           0.40f,  pctS, pctV);
    add ("mix",     "Delay Dry/Wet",   R (0.0f, 1.0f, 0.001f),           0.35f,  pctS, pctV);
    add ("tone",    "Tape Tone",       R (800.0f, 12000.0f, 1.0f, 0.4f), 5000.0f, hzS, hzV);
    add ("sat",     "Tape Saturation", R (0.0f, 1.0f, 0.001f),           0.30f,  pctS, pctV);
    add ("wow",     "Wow & Flutter",   R (0.0f, 1.0f, 0.001f),           0.30f,  pctS, pctV);

    // --- Tape age ---
    add ("degrade", "Degrade",         R (0.0f, 1.0f, 0.001f),           0.35f,  pctS, pctV);

    // --- Ducking / kompresja ---
    addBool ("duckOn",   "Ducking On", false);
    addBool ("duckComp", "Ducking Mode Compress", false);   // false = Duck, true = Compress
    add ("duckAmt", "Duck Amount",     R (0.0f, 40.0f, 0.1f),            12.0f,  dbS,  numV);
    add ("duckAtk", "Duck Attack",     R (1.0f, 250.0f, 0.1f, 0.4f),     15.0f,  msS,  msV);
    add ("duckRel", "Duck Release",    R (20.0f, 2000.0f, 1.0f, 0.4f),   350.0f, msS,  msV);
    add ("compAmt", "Compression",     R (0.0f, 1.0f, 0.001f),           0.50f,  pctS, pctV);

    // --- Chaos ---
    addBool ("chaosOn", "Chaos On", false);
    add ("chaosAmt",  "Chaos Amount",  R (0.0f, 1.0f, 0.001f),           0.50f,  pctS, pctV);
    add ("chaosRate", "Chaos Rate",    R (0.05f, 4.0f, 0.01f, 0.5f),     0.50f,  perS, numV);
    addBool ("chaosNudge",   "Chaos Event: Nudge",   true);
    addBool ("chaosHold",    "Chaos Event: Hold",    true);
    addBool ("chaosFlutter", "Chaos Event: Flutter", true);
    addBool ("chaosPull",    "Chaos Event: Pull",    true);

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
    const int echoSize   = (int) (sr * 2.4) + 16;     // 2000 ms + zapas na modulację
    const int wobbleSize = (int) (sr * 0.02) + 16;
    const int chorusSize = (int) (sr * 0.06) + 16;

    for (int c = 0; c < maxCh; ++c)
    {
        echoLine[c].prepare (echoSize);
        wobbleLine[c].prepare (wobbleSize);
        chorusLine[c].prepare (chorusSize);
        hpState[c] = lpState[c] = chLp[c] = noiseLp[c] = dcState[c] = 0.0f;
        dropGain[c] = dropTarget[c] = 1.0f;
        dropCount[c] = 0;
    }

    wowPhase = flutterPhase = chorusPhase = wobPhase1 = wobPhase2 = chaosFlutPhase = 0.0;
    detEnv = duckState = 0.0f;
    compEnv = compGrDb = compMix = 0.0f;
    chaosOffsetTarget = chaosOffset1 = chaosOffset = chaosMsLim = 0.0f;
    chaosHold = 0;
    muffleTarget = muffle = 0.0f;
    flutterAmpTarget = flutterAmp = 0.0f;
    chaosSlewCoef  = coefFromMs (80.0f);
    muffleDecay    = std::exp (-1.0f / (0.35f * (float) sr));
    flutterDecay   = std::exp (-1.0f / (0.25f * (float) sr));
    dropCoef       = coefFromMs (2.0f);
    detRelCoef     = coefFromMs (20.0f);
    flutterAtkCoef = coefFromMs (30.0f);
    muffleAtkCoef  = coefFromMs (25.0f);
    compMixCoef    = coefFromMs (20.0f);
    compDetRelCoef = coefFromMs (30.0f);
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

// Losowe "zdarzenie" ręki na taśmie (tylko spośród włączonych)
void TapeChorusEchoProcessor::triggerChaos (float amt)
{
    int types[4];
    int n = 0;
    for (int k = 0; k < 4; ++k)
        if (chaosMask & (1 << k))
            types[n++] = k;

    if (n == 0)
        return;

    auto r = [this] { return rng.nextFloat(); };
    const float dir = rng.nextBool() ? 1.0f : -1.0f;

    switch (types[rng.nextInt (n)])
    {
        case 0: // NUDGE: szarpnięcie taśmy w jedną lub drugą stronę (krótkie pitch-bendy)
            chaosOffsetTarget = dir * amt * (6.0f + 24.0f * r());
            chaosSlewCoef     = coefFromMs (35.0f + 50.0f * r());
            chaosHold         = (int) (sr * (0.06 + 0.20 * r()));
            break;

        case 1: // HOLD: palec przytrzymujący szpulę, czyli zwolnienie + przyciemnienie
            chaosOffsetTarget = amt * (25.0f + 60.0f * r());
            chaosSlewCoef     = coefFromMs (180.0f + 150.0f * r());
            chaosHold         = (int) (sr * (0.25 + 0.50 * r()));
            muffleTarget      = juce::jmax (muffleTarget, amt * (0.5f + 0.5f * r()));
            break;

        case 2: // FLUTTER: drżenie / "scrubbing" (wchodzi łagodną rampą)
            flutterAmpTarget = amt * (1.5f + 3.5f * r());
            flutterFreq      = 4.0f + 10.0f * r();
            chaosHold        = (int) (sr * 0.10);
            break;

        default: // PULL: pociągnięcie taśmy do przodu (pitch w górę) + lekkie przytłumienie
            chaosOffsetTarget = -amt * (10.0f + 30.0f * r());
            chaosSlewCoef     = coefFromMs (120.0f + 120.0f * r());
            chaosHold         = (int) (sr * (0.15 + 0.30 * r()));
            muffleTarget      = juce::jmax (muffleTarget, amt * 0.3f);
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
    const bool  compMode  = get ("duckComp") > 0.5f;
    const float duckAmtDb = get ("duckAmt");
    const float atkCoef   = coefFromMs (get ("duckAtk"));
    const float relCoef   = coefFromMs (get ("duckRel"));
    const float compAmt   = get ("compAmt");

    const bool  chaosOn   = get ("chaosOn") > 0.5f;
    const float chaosAmt  = get ("chaosAmt");
    const float chaosRate = get ("chaosRate");
    chaosMask = (get ("chaosNudge")   > 0.5f ? 1 : 0)
              | (get ("chaosHold")    > 0.5f ? 2 : 0)
              | (get ("chaosFlutter") > 0.5f ? 4 : 0)
              | (get ("chaosPull")    > 0.5f ? 8 : 0);

    float* data[maxCh] {};
    for (int c = 0; c < numCh; ++c)
        data[c] = buffer.getWritePointer (c);

    const float msToSamp = (float) (sr * 0.001);
    const float maxChaosStepMs = 0.45f / msToSamp;   // taśma nie zmieni prędkości o więcej niż ~45% w jednej próbce

    // parametry kompresora taśmowego (stałe w obrębie bloku)
    const float cThr   = -6.0f - 30.0f * compAmt;                  // próg
    const float cRatio = 1.5f + 8.5f * compAmt;                    // stosunek
    const float cMake  = 0.30f * (-cThr) * (1.0f - 1.0f / cRatio); // makeup (dB)
    const float cKnee  = 8.0f;
    const float cBias  = 0.12f * compAmt;                          // asymetria = parzyste harmoniczne

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

        // ---------- DUCKING (tryb 1): detektor poziomu wejścia ----------
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (data[ch][i]));

        detEnv = (peak > detEnv) ? peak : detEnv + (peak - detEnv) * detRelCoef;

        float duckTarget = 0.0f;
        if (duckOn && ! compMode)
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

        // offset: dwa kaskadowe wygładzenia, więc ruch startuje z zerową prędkością (bez "szarpnięcia")
        const float sc = juce::jmin (1.0f, chaosSlewCoef * 1.7f);
        chaosOffset1 += (chaosOffsetTarget - chaosOffset1) * sc;
        chaosOffset  += (chaosOffset1 - chaosOffset) * sc;

        // flutter: amplituda wchodzi rampą (brak skoku punktu odczytu)
        flutterAmpTarget *= flutterDecay;
        flutterAmp       += (flutterAmpTarget - flutterAmp) * flutterAtkCoef;
        chaosFlutPhase   += (double) flutterFreq / sr;  if (chaosFlutPhase >= 1.0) chaosFlutPhase -= 1.0;

        float chaosMs = chaosOffset;
        if (flutterAmp > 1.0e-4f)
            chaosMs += flutterAmp * std::sin (twoPi * (float) chaosFlutPhase);

        // ogranicznik szybkości zmian czasu opóźnienia (fizycznie niemożliwe "przeskoki" głowicy)
        chaosMsLim += juce::jlimit (-maxChaosStepMs, maxChaosStepMs, chaosMs - chaosMsLim);

        // przytłumienie "palcem": zawsze działający filtr + płynny miks (bez przełączania)
        muffleTarget *= muffleDecay;
        muffle       += (muffleTarget - muffle) * muffleAtkCoef;
        const float mufMix = juce::jmin (1.0f, muffle * 10.0f);
        const float fcM    = juce::jmin (0.45f * (float) sr,
                                         18000.0f * std::exp (-3.0f * juce::jlimit (0.0f, 1.0f, muffle)));
        const float chCoef = 1.0f - std::exp (-twoPi * fcM / (float) sr);

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

        // ================= PRZEBIEG 1: echo + pętla =================
        float wetRaw[maxCh] {};

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in    = data[ch][i];
            const float chOff = (ch == 0) ? 0.0f : 1.0f;

            // ---------- TAPE ECHO ----------
            const float wowMod = std::sin (twoPi * (float) (wowPhase + 0.08 * chOff))
                               + 0.25f * std::sin (twoPi * (float) (flutterPhase + 0.13 * chOff));
            const float modMs  = juce::jmax (-0.7f * timeMs,
                                             wow * 2.5f * wowMod + chaosMsLim * (1.0f - 0.12f * chOff));
            const float delaySamp = juce::jmax (2.0f, (timeMs + modMs - wobBaseMs) * msToSamp);

            float echo = echoLine[ch].read (delaySamp);

            // chaos: palec na taśmie przytłumia sygnał (płynny miks oryginał -> filtr)
            chLp[ch] += chCoef * (echo - chLp[ch]);
            echo     += (chLp[ch] - echo) * mufMix;

            // ---------- ANALOGOWA DEGRADACJA ----------
            float f = echo;

            // 1) utrata basu i wysokich tonów
            hpState[ch] += hpCoef * (f - hpState[ch]);  f -= hpState[ch];
            lpState[ch] += lpCoef * (f - lpState[ch]);  f  = lpState[ch];

            // 2) "rozmycie" wysokości dźwięku: wobble w pętli
            wobbleLine[ch].push (f);
            const float lfo = 0.6f * std::sin (twoPi * (float) (wobPhase1 + 0.17 * chOff))
                            + 0.4f * std::sin (twoPi * (float) (wobPhase2 + 0.31 * chOff));
            f = wobbleLine[ch].read (juce::jmax (2.0f, (wobBaseMs + deg * 1.6f * lfo) * msToSamp));

            if (deg > 0.001f)
            {
                // 3) szum modulacyjny taśmy
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                noiseLp[ch] += 0.2f * (n - noiseLp[ch]);
                f *= 1.0f + deg * 1.2f * noiseLp[ch];

                // 4) losowe dropouty
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

            wetRaw[ch] = f;
        }

        // ================= KOMPRESOR TAŚMOWY (tryb 2) =================
        // Działa na samym echu (nie w pętli), detektor zlinkowany L/R.
        compMix += (((compMode && duckOn) ? 1.0f : 0.0f) - compMix) * compMixCoef;

        float compOut[maxCh] {};
        {
            float lvl = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                lvl = juce::jmax (lvl, std::abs (wetRaw[ch]));

            compEnv = (lvl > compEnv) ? lvl : compEnv + (lvl - compEnv) * compDetRelCoef;

            const float lvlDb = juce::Decibels::gainToDecibels (compEnv, -100.0f);
            const float over  = lvlDb - cThr;
            const float slope = 1.0f - 1.0f / cRatio;

            float grTarget = 0.0f;
            if (2.0f * over > cKnee)
                grTarget = slope * over;
            else if (2.0f * over > -cKnee)
            {
                const float x = over + cKnee * 0.5f;
                grTarget = slope * x * x / (2.0f * cKnee);
            }

            // attack jak w pokrętle, release zależny od programu: im mocniejsze ściśnięcie,
            // tym szybszy powrót (to daje to "mlaskanie" / oddychanie)
            const float relProg = juce::jmin (1.0f, relCoef * (1.0f + compGrDb / 10.0f));
            compGrDb += (grTarget - compGrDb) * (grTarget > compGrDb ? atkCoef : relProg);

            const float gainLin = juce::Decibels::decibelsToGain (cMake - compGrDb);

            // saturacja taśmowa: im mocniejsza kompresja, tym więcej koloru
            const float k = 1.0f + 0.9f * compAmt * (1.0f + compGrDb / 20.0f);
            const float biasOut = std::tanh (k * cBias) / k;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float z = wetRaw[ch] * gainLin;
                float y = std::tanh (k * (z + cBias)) / k - biasOut;
                dcState[ch] += 0.0005f * (y - dcState[ch]);   // blokada składowej stałej
                compOut[ch] = y - dcState[ch];
            }
        }

        // ================= PRZEBIEG 2: miks + chorus =================
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in    = data[ch][i];
            const float chOff = (ch == 0) ? 0.0f : 1.0f;

            const float wet = wetRaw[ch] * duckGain * (1.0f - compMix) + compOut[ch] * compMix;
            const float afterEcho = in * dryG + wet * wetG;

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
