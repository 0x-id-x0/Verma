#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Wavetables.h"

namespace verma
{
constexpr int kMaxUnison = 16;
constexpr int kChunk     = 32;

inline juce::StringArray getLfoTargetNames()
{
    return { "None", "Osc A Pos", "Osc B Pos", "Filter Cutoff", "Pitch", "Osc A Level", "Osc B Level" };
}
inline juce::StringArray getLfoShapeNames()   { return { "Sine", "Triangle", "Saw", "Square", "S&H" }; }
inline juce::StringArray getFilterTypeNames() { return { "LP 12", "LP 24", "HP 12", "BP 12" }; }
inline juce::StringArray getSubShapeNames()   { return { "Sine", "Triangle", "Square" }; }

inline float midiToHz (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

struct OscParams
{
    std::atomic<float>* on = nullptr;     std::atomic<float>* table = nullptr;  std::atomic<float>* pos = nullptr;
    std::atomic<float>* level = nullptr;  std::atomic<float>* oct = nullptr;    std::atomic<float>* semi = nullptr;
    std::atomic<float>* fine = nullptr;   std::atomic<float>* unison = nullptr; std::atomic<float>* detune = nullptr;
    std::atomic<float>* blend = nullptr;  std::atomic<float>* pan = nullptr;
};

struct SynthParams
{
    OscParams osc[2];
    std::atomic<float>* subOn = nullptr;   std::atomic<float>* subLevel = nullptr; std::atomic<float>* subOct = nullptr;
    std::atomic<float>* subShape = nullptr; std::atomic<float>* noiseLevel = nullptr;
    std::atomic<float>* fOn = nullptr;     std::atomic<float>* fType = nullptr;   std::atomic<float>* fCutoff = nullptr;
    std::atomic<float>* fRes = nullptr;    std::atomic<float>* fDrive = nullptr;  std::atomic<float>* fEnv = nullptr;
    std::atomic<float>* fKey = nullptr;
    std::atomic<float>* env[2][4] {};      // A D S R
    std::atomic<float>* lfoRate[2] {};     std::atomic<float>* lfoShape[2] {};
    std::atomic<float>* lfoDepth[2] {};    std::atomic<float>* lfoTarget[2] {};
};

class VermaSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class VermaVoice : public juce::SynthesiserVoice
{
public:
    VermaVoice (const SynthParams& p, const WavetableBank& b)
        : params (p), bank (b), rng (juce::Random::getSystemRandom().nextInt64()) {}

    void prepare (double sr)
    {
        sampleRate = sr;
        ampEnv.setSampleRate (sr);
        modEnv.setSampleRate (sr);
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) kChunk, 2 };
        for (auto& f : filters) { f.prepare (spec); f.reset(); }
    }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<VermaSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNote;
        velGain = 0.4f + 0.6f * velocity;
        pitchBend = (float) (pitchWheel - 8192) / 8192.0f * 2.0f;

        for (int o = 0; o < 2; ++o)
            for (int v = 0; v < kMaxUnison; ++v)
                phase[o][v] = rng.nextDouble();   // free-running random phase like a big supersaw
        subPhase = 0.0;
        for (int i = 0; i < 2; ++i) { lfoPhase[i] = 0.0; shValue[i] = rng.nextFloat() * 2.0f - 1.0f; }

        for (auto& f : filters) f.reset();
        lastFilterType = -1;
        updateEnvelopes();
        ampEnv.noteOn();
        modEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { ampEnv.noteOff(); modEnv.noteOff(); }
        else { ampEnv.reset(); modEnv.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int v) override  { pitchBend = (float) (v - 8192) / 8192.0f * 2.0f; }
    void controllerMoved (int, int) override {}

    using juce::SynthesiserVoice::renderNextBlock;
    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        updateEnvelopes();
        while (num > 0)
        {
            const int n = juce::jmin (num, kChunk);
            renderChunk (out, start, n);
            start += n;
            num -= n;
            if (! ampEnv.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    const SynthParams& params;
    const WavetableBank& bank;
    juce::Random rng;
    double sampleRate = 44100.0;
    int note = 60;
    float velGain = 1.0f, pitchBend = 0.0f;
    double phase[2][kMaxUnison] {};
    double subPhase = 0.0;
    double lfoPhase[2] {};
    float shValue[2] {};
    juce::ADSR ampEnv, modEnv;
    juce::dsp::StateVariableTPTFilter<float> filters[2];
    int lastFilterType = -1;

    static float ld (std::atomic<float>* a) noexcept { return a != nullptr ? a->load() : 0.0f; }

    void updateEnvelopes()
    {
        juce::ADSR::Parameters a { ld (params.env[0][0]), ld (params.env[0][1]), ld (params.env[0][2]), ld (params.env[0][3]) };
        juce::ADSR::Parameters m { ld (params.env[1][0]), ld (params.env[1][1]), ld (params.env[1][2]), ld (params.env[1][3]) };
        ampEnv.setParameters (a);
        modEnv.setParameters (m);
    }

    float lfoValue (int i) const noexcept
    {
        const double ph = lfoPhase[i];
        switch ((int) ld (params.lfoShape[i]))
        {
            case 0:  return (float) std::sin (juce::MathConstants<double>::twoPi * ph);
            case 1:  return (float) (1.0 - 4.0 * std::abs (ph - 0.5)) * -1.0f;
            case 2:  return (float) (1.0 - 2.0 * ph);
            case 3:  return ph < 0.5 ? 1.0f : -1.0f;
            default: return shValue[i];
        }
    }

    void renderChunk (juce::AudioBuffer<float>& out, int start, int n)
    {
        float L[kChunk] {}, R[kChunk] {};
        const double sr = sampleRate;

        // ---- modulation (control rate, every 32 samples)
        float posMod[2] { 0.0f, 0.0f }, lvlMod[2] { 1.0f, 1.0f };
        float cutOct = 0.0f, pitchMod = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            const float v = lfoValue (i) * ld (params.lfoDepth[i]);
            switch ((int) ld (params.lfoTarget[i]))
            {
                case 1: posMod[0] += v * 0.5f; break;
                case 2: posMod[1] += v * 0.5f; break;
                case 3: cutOct += v * 4.0f; break;
                case 4: pitchMod += v * 12.0f; break;
                case 5: lvlMod[0] *= (1.0f + v); break;
                case 6: lvlMod[1] *= (1.0f + v); break;
                default: break;
            }
            lfoPhase[i] += (double) ld (params.lfoRate[i]) * (double) n / sr;
            if (lfoPhase[i] >= 1.0)
            {
                lfoPhase[i] -= std::floor (lfoPhase[i]);
                shValue[i] = rng.nextFloat() * 2.0f - 1.0f;
            }
        }

        float menv = 0.0f;
        for (int s = 0; s < n; ++s) menv = modEnv.getNextSample();

        const float baseNote = (float) note + pitchBend + pitchMod;

        // ---- wavetable oscillators with unison
        for (int o = 0; o < 2; ++o)
        {
            const auto& op = params.osc[o];
            if (ld (op.on) < 0.5f) continue;

            const int table   = juce::jlimit (0, kNumTables - 1, (int) ld (op.table));
            const float pos   = juce::jlimit (0.0f, 1.0f, ld (op.pos) + posMod[o]);
            const float level = ld (op.level) * juce::jlimit (0.0f, 2.0f, lvlMod[o]);
            const float oNote = baseNote + ld (op.oct) * 12.0f + ld (op.semi) + ld (op.fine) * 0.01f;
            const int U       = juce::jlimit (1, kMaxUnison, (int) ld (op.unison));
            const float det   = ld (op.detune);
            const float blend = ld (op.blend);
            const float pan   = ld (op.pan);

            const float fpos = pos * (float) (kFrames - 1);
            const int f0 = juce::jlimit (0, kFrames - 1, (int) fpos);
            const int f1 = juce::jmin (f0 + 1, kFrames - 1);
            const float ff = fpos - (float) f0;

            float gains[kMaxUnison], gl[kMaxUnison], gr[kMaxUnison];
            double inc[kMaxUnison];
            const float* t0[kMaxUnison];
            const float* t1[kMaxUnison];
            float sumSq = 0.0f;

            for (int v = 0; v < U; ++v)
            {
                const float off = U > 1 ? (2.0f * (float) v / (float) (U - 1) - 1.0f) : 0.0f;
                const float g = U > 1 ? juce::jmap (blend, 1.0f - std::abs (off) * 0.9f, 1.0f) : 1.0f;
                gains[v] = g;
                sumSq += g * g;
                const float freq = midiToHz (oNote + off * det * 0.7f);
                inc[v] = (double) freq / sr;
                const int mip = WavetableBank::mipForFrequency ((double) freq, sr);
                t0[v] = bank.getFrame (table, mip, f0);
                t1[v] = bank.getFrame (table, mip, f1);
                const float pp = juce::jlimit (-1.0f, 1.0f, pan + off * 0.85f);
                const float ang = (pp + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                gl[v] = std::cos (ang);
                gr[v] = std::sin (ang);
            }

            const float norm = level / std::sqrt (juce::jmax (sumSq, 1.0e-6f));

            for (int v = 0; v < U; ++v)
            {
                double ph = phase[o][v];
                const double dph = inc[v];
                const float* a0 = t0[v];
                const float* a1 = t1[v];
                const float lg = gains[v] * norm * gl[v];
                const float rg = gains[v] * norm * gr[v];

                for (int s = 0; s < n; ++s)
                {
                    const double idx = ph * (double) kTableSize;
                    const int i0 = (int) idx;
                    const float fr = (float) (idx - (double) i0);
                    const float sA = a0[i0] + (a0[i0 + 1] - a0[i0]) * fr;
                    const float sB = a1[i0] + (a1[i0 + 1] - a1[i0]) * fr;
                    const float smp = sA + (sB - sA) * ff;
                    L[s] += smp * lg;
                    R[s] += smp * rg;
                    ph += dph;
                    if (ph >= 1.0) ph -= 1.0;
                }
                phase[o][v] = ph;
            }
        }

        // ---- sub oscillator
        if (ld (params.subOn) > 0.5f)
        {
            const float lvl = ld (params.subLevel) * 0.707f;
            const int shape = (int) ld (params.subShape);
            const double dph = (double) midiToHz (baseNote + ld (params.subOct) * 12.0f) / sr;
            for (int s = 0; s < n; ++s)
            {
                float v;
                if (shape == 0)      v = (float) std::sin (juce::MathConstants<double>::twoPi * subPhase);
                else if (shape == 1) v = (float) (4.0 * std::abs (subPhase - 0.5) - 1.0);
                else                 v = subPhase < 0.5 ? 0.8f : -0.8f;
                L[s] += v * lvl;
                R[s] += v * lvl;
                subPhase += dph;
                if (subPhase >= 1.0) subPhase -= 1.0;
            }
        }

        // ---- noise
        const float noise = ld (params.noiseLevel);
        if (noise > 0.0001f)
            for (int s = 0; s < n; ++s)
            {
                L[s] += (rng.nextFloat() * 2.0f - 1.0f) * noise * 0.4f;
                R[s] += (rng.nextFloat() * 2.0f - 1.0f) * noise * 0.4f;
            }

        // ---- filter
        if (ld (params.fOn) > 0.5f)
        {
            const int type = (int) ld (params.fType);
            if (type != lastFilterType)
            {
                const auto t = type == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
                             : type == 3 ? juce::dsp::StateVariableTPTFilterType::bandpass
                                         : juce::dsp::StateVariableTPTFilterType::lowpass;
                for (auto& f : filters) f.setType (t);
                lastFilterType = type;
            }

            const float octs = ld (params.fEnv) * menv * 7.0f + cutOct + ld (params.fKey) * ((float) note - 60.0f) / 12.0f;
            const float cutoff = juce::jlimit (20.0f, (float) (sr * 0.45), ld (params.fCutoff) * std::pow (2.0f, octs));
            const float res = ld (params.fRes);
            const float q = 0.6f + res * res * 11.0f;
            for (auto& f : filters) { f.setCutoffFrequency (cutoff); f.setResonance (q); }

            const float drive = ld (params.fDrive);
            const float dg = 1.0f + drive * 12.0f;
            const float dcomp = 1.0f / (1.0f + drive * 2.0f);
            const bool two = (type == 1);

            for (int s = 0; s < n; ++s)
            {
                float l = L[s], r = R[s];
                if (drive > 0.001f) { l = std::tanh (l * dg) * dcomp * 1.5f; r = std::tanh (r * dg) * dcomp * 1.5f; }
                l = filters[0].processSample (0, l);
                r = filters[0].processSample (1, r);
                if (two) { l = filters[1].processSample (0, l); r = filters[1].processSample (1, r); }
                L[s] = l; R[s] = r;
            }
        }

        // ---- amp envelope + output
        const int nch = out.getNumChannels();
        for (int s = 0; s < n; ++s)
        {
            const float g = ampEnv.getNextSample() * velGain;
            if (nch > 1)
            {
                out.addSample (0, start + s, L[s] * g);
                out.addSample (1, start + s, R[s] * g);
            }
            else if (nch == 1)
            {
                out.addSample (0, start + s, 0.5f * (L[s] + R[s]) * g);
            }
        }
    }
};
} // namespace verma
