#include "PluginProcessor.h"
#include "PluginEditor.h"

NihilityTShaperProcessor::NihilityTShaperProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

NihilityTShaperProcessor::~NihilityTShaperProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout NihilityTShaperProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("in_gain", "Input Gain", -36.0f, 36.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("out_gain", "Output Gain", -36.0f, 36.0f, 0.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", -48.0f, 48.0f, 0.0f));
    
    juce::NormalisableRange<float> atkTimeRange(0.1f, 50.0f, 0.01f, 0.3f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("atk_time", "Attack Time", atkTimeRange, 1.0f)); 
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", -48.0f, 48.0f, 0.0f));
    
    juce::NormalisableRange<float> susTimeRange(5.0f, 500.0f, 0.1f, 0.3f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sus_time", "Sustain Time", susTimeRange, 25.0f)); 
    
    juce::NormalisableRange<float> fishTailRange(0.0f, 100.0f, 0.1f, 0.35f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fish_tail", "Fish Tail Gap", fishTailRange, 15.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fish_tail_vol", "Fish Tail Vol", -48.0f, 48.0f, 0.0f));
    
    juce::NormalisableRange<float> scHpfRange(20.0f, 2000.0f, 1.0f, 0.3f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sc_hpf", "Sidechain HPF", scHpfRange, 100.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterInt>("dyn_mode", "Mode", 0, 2, 2)); 
    
    // Novo parâmetro: Janela RMS mapeada na APVTS janela de 0.5s
    juce::NormalisableRange<float> rmsRange(1.0f, 500.0f, 0.1f, 0.3f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("rms_window", "RMS Window", rmsRange, 5.0f));
    
    return { params.begin(), params.end() };
}

void NihilityTShaperProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    currentState = IDLE;
    stateTimer = 0;
    currentGainDb = 0.0f;
    rmsFilter = 0.0f;
    scLpfL = 0.0f; 
    scLpfR = 0.0f;
    limitEnvL = 1.0f; limitEnvR = 1.0f;
}

void NihilityTShaperProcessor::releaseResources() {}

void NihilityTShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    int numSamples = buffer.getNumSamples();
    double sr = getSampleRate();
    
    float inDb = apvts.getRawParameterValue("in_gain")->load();
    float outDb = apvts.getRawParameterValue("out_gain")->load();
    float atkDb = apvts.getRawParameterValue("attack")->load();
    float atkTimeMs = apvts.getRawParameterValue("atk_time")->load();
    float susDb = apvts.getRawParameterValue("sustain")->load();
    float susTimeMs = apvts.getRawParameterValue("sus_time")->load(); 
    float ftVolDb = apvts.getRawParameterValue("fish_tail_vol")->load();
    float fishTailMs = apvts.getRawParameterValue("fish_tail")->load();
    float scHpfFreq = apvts.getRawParameterValue("sc_hpf")->load();
    int mode = static_cast<int>(apvts.getRawParameterValue("dyn_mode")->load());
    
    // Leitura do novo parâmetro da Janela RMS
    float rmsWindowMs = apvts.getRawParameterValue("rms_window")->load();

    float inGainMultiplier = juce::Decibels::decibelsToGain(inDb);
    float outGainMultiplier = juce::Decibels::decibelsToGain(outDb);
    
    // Integração da variável rmsWindowMs no lugar da constante de 1ms (0.001)
    float rmsAtk = static_cast<float>(std::exp(-1.0 / ((rmsWindowMs * 0.001f) * sr))); 
    float rmsRel = static_cast<float>(std::exp(-1.0 / (0.050 * sr))); 
    float limitReleaseCoef = static_cast<float>(std::exp(-1.0 / (0.080 * sr))); 
    float scAlpha = static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * scHpfFreq / sr));

    for (int i = 0; i < numSamples; ++i) 
    {
        float inL = totalNumInputChannels > 0 ? buffer.getReadPointer(0)[i] * inGainMultiplier : 0.0f;
        float inR = totalNumInputChannels > 1 ? buffer.getReadPointer(1)[i] * inGainMultiplier : 0.0f;
        
        scLpfL += scAlpha * (inL - scLpfL);
        float detL = inL - scLpfL; 
        
        scLpfR += scAlpha * (inR - scLpfR);
        float detR = inR - scLpfR;
        
        float rect = std::max(std::abs(detL), std::abs(detR));

        bool triggered = false;
        if (rect > rmsFilter * 2.5f && rect > 0.005f) { 
            if (currentState == IDLE || currentState == HOLD_SUS || currentState == RELEASE) {
                triggered = true;
            }
        }
        
        if (rect > rmsFilter) {
            rmsFilter = rect + rmsAtk * (rmsFilter - rect);
        } else {
            rmsFilter = rect + rmsRel * (rmsFilter - rect);
        }

        if (triggered) {
            currentState = ATTACK;
            stateTimer = 0;
            currentStateLength = std::max(1, static_cast<int>((atkTimeMs * 0.001f) * sr));
            currentGainDb = atkDb; 
            startDb = atkDb;
            targetDb = ftVolDb; 
        }
        // ENV curve abaixo

        if (currentState != IDLE) {
            stateTimer++;
            float progress = static_cast<float>(stateTimer) / static_cast<float>(currentStateLength);
            if (progress >= 1.0f) progress = 1.0f;

            float curve = progress;
            if (currentState == ATTACK) {
                curve = std::pow(progress, 0.35f);
            } else if (currentState == SUSTAIN) {
                curve = std::pow(progress, 0.5f);
            }

            currentGainDb = startDb + (targetDb - startDb) * curve;

            if (stateTimer >= currentStateLength) {
                if (currentState == ATTACK) {
                    currentState = GAP;
                    stateTimer = 0;
                    currentStateLength = std::max(1, static_cast<int>((fishTailMs * 0.001f) * sr));
                    startDb = currentGainDb;
                    targetDb = ftVolDb; 
                } else if (currentState == GAP) {
                    currentState = SUSTAIN;
                    stateTimer = 0;
                    currentStateLength = std::max(1, static_cast<int>((susTimeMs * 0.001f) * sr));
                    startDb = currentGainDb;
                    targetDb = susDb; 
                } else if (currentState == SUSTAIN) {
                    currentState = HOLD_SUS;
                    stateTimer = 0;
                    currentStateLength = std::max(1, static_cast<int>(1.0f * sr)); 
                    startDb = currentGainDb;
                    targetDb = susDb; 
                } else if (currentState == HOLD_SUS) {
                    currentState = RELEASE;
                    stateTimer = 0;
                    currentStateLength = std::max(1, static_cast<int>(0.100f * sr)); 
                    startDb = currentGainDb;
                    targetDb = 0.0f;
                } else if (currentState == RELEASE) {
                    currentState = IDLE;
                    currentGainDb = 0.0f;
                }
            }
        } else {
            currentGainDb = 0.0f;
        }

        float shapeGain = juce::Decibels::decibelsToGain(currentGainDb);

        for (int ch = 0; ch < totalNumInputChannels; ++ch) 
        {
            float* data = buffer.getWritePointer(ch);
            float in = data[i] * inGainMultiplier;
            float out = in * shapeGain;
            
            float& limitEnv = (ch == 0) ? limitEnvL : limitEnvR; 

            if (mode == 0) { 
                float absOut = std::abs(out);
                float targetLimit = 1.0f; 
                if (absOut > 1.0f) targetLimit = 1.0f / absOut;
                
                if (targetLimit < limitEnv) limitEnv = targetLimit;
                else limitEnv = targetLimit + limitReleaseCoef * (limitEnv - targetLimit);
                
                out = out * limitEnv;
            } else if (mode == 1) { 
                out = std::tanh(out);
            }
            
            out = out * outGainMultiplier;
            data[i] = out;
        }
    }
}

void NihilityTShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NihilityTShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* NihilityTShaperProcessor::createEditor()
{
    return new NihilityTShaperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NihilityTShaperProcessor(); }
