#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace verma;

//==============================================================================
namespace
{
using Fmt = std::function<juce::String (float, int)>;

juce::String fmtPct (float v, int)   { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String fmtHz (float v, int)    { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 100.0f ? 1 : 0) + " Hz"; }
juce::String fmtTime (float v, int)  { return v < 1.0f ? juce::String (v * 1000.0f, v < 0.1f ? 1 : 0) + " ms" : juce::String (v, 2) + " s"; }
juce::String fmtMs (float v, int)    { return juce::String (juce::roundToInt (v)) + " ms"; }
juce::String fmtDb (float v, int)    { return juce::String (v, 1) + " dB"; }
juce::String fmtCents (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; }
juce::String fmtPan (float v, int)
{
    if (std::abs (v) < 0.01f) return "C";
    return juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + (v < 0.0f ? " L" : " R");
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      float mn, float mx, float def, Fmt fmt, float centre = -1.0f)
{
    juce::NormalisableRange<float> r (mn, mx);
    if (centre > mn && centre < mx) r.setSkewForCentre (centre);
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt)));
}
std::unique_ptr<juce::AudioParameterInt> makeInt (const juce::String& id, const juce::String& name, int mn, int mx, int def)
{
    return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, mn, mx, def);
}
std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name, const juce::StringArray& c, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def);
}
std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

//==============================================================================
struct PresetValue { const char* id; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets =
    {
        { "Init", {} },
        { "LD Supersaw Anthem", { { "aPos", 0.667f }, { "aUnison", 7 }, { "aDetune", 0.35f }, { "aBlend", 0.8f },
                                  { "bOn", 1 }, { "bPos", 0.667f }, { "bOct", 1 }, { "bUnison", 5 }, { "bDetune", 0.25f }, { "bLevel", 0.35f },
                                  { "fOn", 1 }, { "fType", 1 }, { "fCutoff", 7000 }, { "fRes", 0.15f },
                                  { "e1A", 0.005f }, { "e1D", 0.5f }, { "e1S", 0.9f }, { "e1R", 0.35f },
                                  { "chOn", 1 }, { "chMix", 0.3f }, { "dlOn", 1 }, { "dlTime", 375 }, { "dlFb", 0.35f }, { "dlMix", 0.2f },
                                  { "rvOn", 1 }, { "rvMix", 0.2f } } },
        { "BA Reese Classic", { { "aPos", 0.667f }, { "aOct", -1 }, { "aUnison", 2 }, { "aDetune", 0.15f },
                                { "bOn", 1 }, { "bPos", 0.667f }, { "bOct", -1 }, { "bFine", 14 }, { "bUnison", 2 }, { "bDetune", 0.2f },
                                { "subOn", 1 }, { "subLevel", 0.6f }, { "subOct", -1 },
                                { "fOn", 1 }, { "fType", 1 }, { "fCutoff", 900 }, { "fRes", 0.25f }, { "fDrive", 0.4f },
                                { "l1Target", 3 }, { "l1Rate", 0.3f }, { "l1Depth", 0.25f },
                                { "distOn", 1 }, { "distDrive", 0.3f }, { "distMix", 0.5f } } },
        { "BA Wobble Monster", { { "aTable", 1 }, { "aOct", -1 }, { "aUnison", 3 }, { "aDetune", 0.2f },
                                 { "subOn", 1 }, { "subLevel", 0.55f },
                                 { "fOn", 1 }, { "fType", 1 }, { "fCutoff", 400 }, { "fRes", 0.5f }, { "fDrive", 0.5f },
                                 { "l1Target", 3 }, { "l1Rate", 3.0f }, { "l1Depth", 0.7f },
                                 { "l2Target", 1 }, { "l2Rate", 3.0f }, { "l2Depth", 0.5f },
                                 { "distOn", 1 }, { "distDrive", 0.5f }, { "distMix", 0.6f } } },
        { "PL Glass Pluck", { { "aTable", 4 }, { "aPos", 0.4f }, { "aUnison", 3 }, { "aDetune", 0.1f },
                              { "bOn", 1 }, { "bPos", 0.0f }, { "bOct", 1 }, { "bLevel", 0.3f },
                              { "fOn", 1 }, { "fType", 1 }, { "fCutoff", 300 }, { "fRes", 0.3f }, { "fEnv", 0.7f },
                              { "e1A", 0.001f }, { "e1D", 0.45f }, { "e1S", 0.0f }, { "e1R", 0.4f },
                              { "e2A", 0.001f }, { "e2D", 0.3f }, { "e2S", 0.0f }, { "e2R", 0.3f },
                              { "dlOn", 1 }, { "dlTime", 300 }, { "dlFb", 0.4f }, { "dlMix", 0.3f },
                              { "rvOn", 1 }, { "rvSize", 0.7f }, { "rvMix", 0.3f } } },
        { "PD Dream Choir", { { "aTable", 3 }, { "aPos", 0.3f }, { "aUnison", 6 }, { "aDetune", 0.3f },
                              { "bOn", 1 }, { "bTable", 1 }, { "bPos", 0.2f }, { "bUnison", 4 }, { "bDetune", 0.2f }, { "bOct", 1 }, { "bLevel", 0.4f },
                              { "l1Target", 1 }, { "l1Rate", 0.15f }, { "l1Depth", 0.6f },
                              { "l2Target", 2 }, { "l2Rate", 0.1f }, { "l2Depth", 0.5f },
                              { "fOn", 1 }, { "fType", 0 }, { "fCutoff", 3500 }, { "fRes", 0.1f },
                              { "e1A", 1.2f }, { "e1D", 1.0f }, { "e1S", 0.8f }, { "e1R", 2.5f },
                              { "chOn", 1 }, { "chMix", 0.4f }, { "rvOn", 1 }, { "rvSize", 0.9f }, { "rvMix", 0.45f } } },
        { "BA Digital Growl", { { "aTable", 5 }, { "aPos", 0.5f }, { "aOct", -1 }, { "aUnison", 4 }, { "aDetune", 0.2f },
                                { "subOn", 1 }, { "subLevel", 0.5f },
                                { "l1Target", 1 }, { "l1Rate", 4.0f }, { "l1Depth", 0.8f }, { "l1Shape", 1 },
                                { "l2Target", 3 }, { "l2Rate", 2.0f }, { "l2Depth", 0.4f },
                                { "fOn", 1 }, { "fType", 3 }, { "fCutoff", 1200 }, { "fRes", 0.4f }, { "fEnv", 0.3f },
                                { "distOn", 1 }, { "distDrive", 0.6f }, { "distMix", 0.7f } } },
        { "KY Hollow Keys", { { "aTable", 4 }, { "aPos", 0.7f }, { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.3f }, { "bLevel", 0.5f },
                              { "e1A", 0.002f }, { "e1D", 1.2f }, { "e1S", 0.3f }, { "e1R", 0.6f },
                              { "fOn", 1 }, { "fType", 0 }, { "fCutoff", 2500 }, { "fEnv", 0.4f }, { "e2D", 0.6f }, { "e2S", 0.2f },
                              { "chOn", 1 }, { "chMix", 0.25f }, { "rvOn", 1 }, { "rvMix", 0.25f } } },
        { "PD PWM Strings", { { "aTable", 2 }, { "aUnison", 5 }, { "aDetune", 0.25f },
                              { "bOn", 1 }, { "bTable", 2 }, { "bFine", -8 }, { "bUnison", 3 },
                              { "l1Target", 1 }, { "l1Rate", 0.8f }, { "l1Depth", 0.4f },
                              { "l2Target", 2 }, { "l2Rate", 0.6f }, { "l2Depth", 0.4f },
                              { "e1A", 0.4f }, { "e1S", 0.9f }, { "e1R", 1.0f },
                              { "fOn", 1 }, { "fType", 0 }, { "fCutoff", 5000 },
                              { "rvOn", 1 }, { "rvSize", 0.8f }, { "rvMix", 0.35f } } },
        { "SY Vowel Talker", { { "aTable", 3 }, { "aUnison", 2 }, { "aDetune", 0.1f }, { "aOct", -1 },
                               { "l1Target", 1 }, { "l1Rate", 1.5f }, { "l1Depth", 1.0f }, { "l1Shape", 1 },
                               { "subOn", 1 }, { "subLevel", 0.35f },
                               { "distOn", 1 }, { "distDrive", 0.25f }, { "distMix", 0.4f },
                               { "dlOn", 1 }, { "dlTime", 250 }, { "dlFb", 0.3f }, { "dlMix", 0.15f } } },
    };
    return presets;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VermaAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = o == 0 ? "a" : "b";
        const juce::String n = o == 0 ? "Osc A " : "Osc B ";
        layout.add (makeBool (p + "On", n + "On", o == 0));
        layout.add (makeChoice (p + "Table", n + "Wavetable", getWavetableNames(), 0));
        layout.add (makeFloat (p + "Pos", n + "WT Pos", 0.0f, 1.0f, o == 0 ? 0.667f : 0.0f, fmtPct));
        layout.add (makeFloat (p + "Level", n + "Level", 0.0f, 1.0f, 0.75f, fmtPct));
        layout.add (makeInt (p + "Oct", n + "Octave", -4, 4, 0));
        layout.add (makeInt (p + "Semi", n + "Semi", -12, 12, 0));
        layout.add (makeFloat (p + "Fine", n + "Fine", -100.0f, 100.0f, 0.0f, fmtCents));
        layout.add (makeInt (p + "Unison", n + "Unison", 1, kMaxUnison, 1));
        layout.add (makeFloat (p + "Detune", n + "Detune", 0.0f, 1.0f, 0.25f, fmtPct));
        layout.add (makeFloat (p + "Blend", n + "Blend", 0.0f, 1.0f, 0.75f, fmtPct));
        layout.add (makeFloat (p + "Pan", n + "Pan", -1.0f, 1.0f, 0.0f, fmtPan));
    }

    layout.add (makeBool ("subOn", "Sub On", false));
    layout.add (makeFloat ("subLevel", "Sub Level", 0.0f, 1.0f, 0.5f, fmtPct));
    layout.add (makeInt ("subOct", "Sub Octave", -2, 0, -1));
    layout.add (makeChoice ("subShape", "Sub Shape", getSubShapeNames(), 0));
    layout.add (makeFloat ("noiseLevel", "Noise Level", 0.0f, 1.0f, 0.0f, fmtPct));

    layout.add (makeBool ("fOn", "Filter On", false));
    layout.add (makeChoice ("fType", "Filter Type", getFilterTypeNames(), 0));
    layout.add (makeFloat ("fCutoff", "Cutoff", 20.0f, 20000.0f, 20000.0f, fmtHz, 1000.0f));
    layout.add (makeFloat ("fRes", "Resonance", 0.0f, 1.0f, 0.1f, fmtPct));
    layout.add (makeFloat ("fDrive", "Filter Drive", 0.0f, 1.0f, 0.0f, fmtPct));
    layout.add (makeFloat ("fEnv", "Filter Env Amt", -1.0f, 1.0f, 0.0f, fmtPct));
    layout.add (makeFloat ("fKey", "Key Track", 0.0f, 1.0f, 0.0f, fmtPct));

    const float envDef[2][4] = { { 0.005f, 0.6f, 1.0f, 0.2f }, { 0.005f, 0.4f, 0.0f, 0.3f } };
    for (int e = 0; e < 2; ++e)
    {
        const juce::String p = "e" + juce::String (e + 1);
        const juce::String n = "Env " + juce::String (e + 1) + " ";
        layout.add (makeFloat (p + "A", n + "Attack", 0.001f, 10.0f, envDef[e][0], fmtTime, 0.5f));
        layout.add (makeFloat (p + "D", n + "Decay", 0.001f, 10.0f, envDef[e][1], fmtTime, 0.8f));
        layout.add (makeFloat (p + "S", n + "Sustain", 0.0f, 1.0f, envDef[e][2], fmtPct));
        layout.add (makeFloat (p + "R", n + "Release", 0.001f, 10.0f, envDef[e][3], fmtTime, 0.8f));
    }

    for (int l = 0; l < 2; ++l)
    {
        const juce::String p = "l" + juce::String (l + 1);
        const juce::String n = "LFO " + juce::String (l + 1) + " ";
        layout.add (makeFloat (p + "Rate", n + "Rate", 0.01f, 30.0f, 2.0f, fmtHz, 2.0f));
        layout.add (makeChoice (p + "Shape", n + "Shape", getLfoShapeNames(), 0));
        layout.add (makeFloat (p + "Depth", n + "Depth", 0.0f, 1.0f, 0.5f, fmtPct));
        layout.add (makeChoice (p + "Target", n + "Target", getLfoTargetNames(), 0));
    }

    layout.add (makeBool ("distOn", "Distortion On", false));
    layout.add (makeFloat ("distDrive", "Dist Drive", 0.0f, 1.0f, 0.3f, fmtPct));
    layout.add (makeFloat ("distMix", "Dist Mix", 0.0f, 1.0f, 1.0f, fmtPct));

    layout.add (makeBool ("chOn", "Chorus On", false));
    layout.add (makeFloat ("chRate", "Chorus Rate", 0.05f, 5.0f, 0.8f, fmtHz, 1.0f));
    layout.add (makeFloat ("chDepth", "Chorus Depth", 0.0f, 1.0f, 0.4f, fmtPct));
    layout.add (makeFloat ("chMix", "Chorus Mix", 0.0f, 1.0f, 0.5f, fmtPct));

    layout.add (makeBool ("dlOn", "Delay On", false));
    layout.add (makeFloat ("dlTime", "Delay Time", 10.0f, 2000.0f, 375.0f, fmtMs, 400.0f));
    layout.add (makeFloat ("dlFb", "Delay Feedback", 0.0f, 0.95f, 0.35f, fmtPct));
    layout.add (makeFloat ("dlMix", "Delay Mix", 0.0f, 1.0f, 0.25f, fmtPct));

    layout.add (makeBool ("rvOn", "Reverb On", false));
    layout.add (makeFloat ("rvSize", "Reverb Size", 0.0f, 1.0f, 0.6f, fmtPct));
    layout.add (makeFloat ("rvDamp", "Reverb Damping", 0.0f, 1.0f, 0.5f, fmtPct));
    layout.add (makeFloat ("rvWidth", "Reverb Width", 0.0f, 1.0f, 1.0f, fmtPct));
    layout.add (makeFloat ("rvMix", "Reverb Mix", 0.0f, 1.0f, 0.25f, fmtPct));

    layout.add (makeFloat ("master", "Master", -36.0f, 6.0f, -6.0f, fmtDb));
    return layout;
}

//==============================================================================
VermaAudioProcessor::VermaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VermaState", createLayout())
{
    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = o == 0 ? "a" : "b";
        auto& op = synthParams.osc[o];
        op.on = raw (p + "On");         op.table = raw (p + "Table");   op.pos = raw (p + "Pos");
        op.level = raw (p + "Level");   op.oct = raw (p + "Oct");       op.semi = raw (p + "Semi");
        op.fine = raw (p + "Fine");     op.unison = raw (p + "Unison"); op.detune = raw (p + "Detune");
        op.blend = raw (p + "Blend");   op.pan = raw (p + "Pan");
    }
    synthParams.subOn = raw ("subOn");   synthParams.subLevel = raw ("subLevel");
    synthParams.subOct = raw ("subOct"); synthParams.subShape = raw ("subShape");
    synthParams.noiseLevel = raw ("noiseLevel");
    synthParams.fOn = raw ("fOn");       synthParams.fType = raw ("fType");   synthParams.fCutoff = raw ("fCutoff");
    synthParams.fRes = raw ("fRes");     synthParams.fDrive = raw ("fDrive"); synthParams.fEnv = raw ("fEnv");
    synthParams.fKey = raw ("fKey");

    const char* adsr[4] = { "A", "D", "S", "R" };
    for (int e = 0; e < 2; ++e)
        for (int k = 0; k < 4; ++k)
            synthParams.env[e][k] = raw ("e" + juce::String (e + 1) + adsr[k]);

    for (int l = 0; l < 2; ++l)
    {
        const juce::String p = "l" + juce::String (l + 1);
        synthParams.lfoRate[l] = raw (p + "Rate");
        synthParams.lfoShape[l] = raw (p + "Shape");
        synthParams.lfoDepth[l] = raw (p + "Depth");
        synthParams.lfoTarget[l] = raw (p + "Target");
    }

    synth.addSound (new VermaSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new VermaVoice (synthParams, *bank));
}

bool VermaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void VermaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<VermaVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    chorus.prepare (spec);  chorus.reset();
    reverb.prepare (spec);  reverb.reset();
    delay.prepare (spec);   delay.reset();
    delaySamples.reset (sampleRate, 0.08);
    delaySamples.setCurrentAndTargetValue (raw ("dlTime")->load() * (float) sampleRate * 0.001f);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw ("master")->load()));
    keyboardState.reset();
}

void VermaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    synth.renderNextBlock (buffer, midi, 0, n);

    if (nch == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    // ---- Distortion
    if (raw ("distOn")->load() > 0.5f)
    {
        const float k = 1.0f + raw ("distDrive")->load() * 24.0f;
        const float mix = raw ("distMix")->load();
        const float comp = 1.0f / std::tanh (k);
        for (int c = 0; c < nch; ++c)
        {
            float* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = d[i] * (1.0f - mix) + std::tanh (d[i] * k) * comp * 0.8f * mix;
        }
    }

    juce::dsp::AudioBlock<float> block (buffer);

    // ---- Chorus
    if (raw ("chOn")->load() > 0.5f)
    {
        chorus.setRate (raw ("chRate")->load());
        chorus.setDepth (raw ("chDepth")->load());
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.15f);
        chorus.setMix (raw ("chMix")->load());
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // ---- Ping-pong delay
    const bool dlOn = raw ("dlOn")->load() > 0.5f;
    if (dlOn)
    {
        if (! delayWasOn) delay.reset();
        delaySamples.setTargetValue (juce::jmax (1.0f, raw ("dlTime")->load() * (float) currentSampleRate * 0.001f));
        const float fb = raw ("dlFb")->load();
        const float mix = raw ("dlMix")->load();
        for (int i = 0; i < n; ++i)
        {
            const float ds = delaySamples.getNextValue();
            const float inL = L[i];
            const float inR = R != nullptr ? R[i] : inL;
            if (R != nullptr)
            {
                const float dl = delay.popSample (0, ds);
                const float dr = delay.popSample (1, ds);
                delay.pushSample (0, 0.5f * (inL + inR) + dr * fb);
                delay.pushSample (1, dl * fb);
                L[i] = inL + dl * mix;
                R[i] = inR + dr * mix;
            }
            else
            {
                const float d = delay.popSample (0, ds);
                delay.pushSample (0, inL + d * fb);
                L[i] = inL + d * mix;
            }
        }
    }
    delayWasOn = dlOn;

    // ---- Reverb
    if (raw ("rvOn")->load() > 0.5f)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = raw ("rvSize")->load();
        rp.damping = raw ("rvDamp")->load();
        rp.width = raw ("rvWidth")->load();
        const float mix = raw ("rvMix")->load();
        rp.wetLevel = mix;
        rp.dryLevel = 1.0f - mix * 0.5f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // ---- Master + soft clip + scope
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (raw ("master")->load()));
    int wp = scopeWritePos.load();
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        L[i] = std::tanh (L[i] * g);
        if (R != nullptr) R[i] = std::tanh (R[i] * g);
        scopeBuffer[wp] = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
        wp = (wp + 1) % scopeSize;
    }
    scopeWritePos.store (wp);
}

//==============================================================================
int VermaAudioProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String VermaAudioProcessor::getProgramName (int index)
{
    const auto& ps = getPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? juce::String (ps[(size_t) index].name) : juce::String();
}

void VermaAudioProcessor::setCurrentProgram (int index)
{
    const auto& ps = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& v : ps[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
}

void VermaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VermaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* VermaAudioProcessor::createEditor() { return new VermaAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VermaAudioProcessor(); }
