#pragma once
#include <JuceHeader.h>

class NihilityTShaperProcessor : public juce::AudioProcessor
{
public:
    NihilityTShaperProcessor();
    ~NihilityTShaperProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    const juce::String getName() const override { return "Nihility T Shaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int /*index*/) override {}
    const juce::String getProgramName (int /*index*/) override { return {}; }
    void changeProgramName (int /*index*/, const juce::String& /*newName*/) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    enum ShapeState { IDLE, ATTACK, GAP, SUSTAIN, HOLD_SUS, RELEASE };
    ShapeState currentState = IDLE;
    int stateTimer = 0;
    int currentStateLength = 1;
    float startDb = 0.0f;
    float targetDb = 0.0f;
    float currentGainDb = 0.0f;
    float rmsFilter = 0.0f;
    
    float scLpfL = 0.0f;
    float scLpfR = 0.0f;
    
    float limitEnvL = 1.0f;
    float limitEnvR = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NihilityTShaperProcessor)
};
