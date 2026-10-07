#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Wavetables.h"
#include "SynthVoice.h"

class VermaAudioProcessor : public juce::AudioProcessor
{
public:
    VermaAudioProcessor();
    ~VermaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Verma"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    const verma::WavetableBank& getBank() const { return *bank; }

    static constexpr int scopeSize = 2048;
    float scopeBuffer[scopeSize] {};
    std::atomic<int> scopeWritePos { 0 };

private:
    juce::SharedResourcePointer<verma::WavetableBank> bank;
    verma::SynthParams synthParams;
    juce::Synthesiser synth;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 400000 };
    juce::SmoothedValue<float> delaySamples, masterGain;
    bool delayWasOn = false;
    double currentSampleRate = 44100.0;
    int currentProgram = 0;

    std::atomic<float>* raw (const juce::String& id) { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VermaAudioProcessor)
};
