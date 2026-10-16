#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MinimalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MinimalLookAndFeel()
    {
        // Trilho magenta e indicador amarelo
        setColour(juce::Slider::trackColourId, juce::Colour(0xff0a0a0a));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff)); 
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float /*minSliderPos*/,
    float /*maxSliderPos*/, 
    const juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        g.setColour(findColour(juce::Slider::trackColourId));
        if (slider.isHorizontal()) {
            g.fillRoundedRectangle((float)x, (float)y + height * 0.5f - 2.0f, (float)width, 4.0f, 2.0f);
            g.setColour(findColour(juce::Slider::thumbColourId));
            g.fillEllipse(sliderPos - 6.0f, (float)y + height * 0.5f - 6.0f, 12.0f, 12.0f);
        } else {
            g.fillRoundedRectangle((float)x + width * 0.5f - 2.0f, (float)y, 4.0f, (float)height, 2.0f);
            g.setColour(findColour(juce::Slider::thumbColourId));
            g.fillEllipse((float)x + width * 0.5f - 6.0f, sliderPos - 6.0f, 12.0f, 12.0f);
        }
    }
};

class ShapePreviewComponent : public juce::Component, public juce::AudioProcessorValueTreeState::Listener
{
public:
    ShapePreviewComponent(juce::AudioProcessorValueTreeState& vts) : apvts(vts)
    {
        apvts.addParameterListener("attack", this);
        apvts.addParameterListener("sustain", this);
        apvts.addParameterListener("fish_tail", this);
        apvts.addParameterListener("fish_tail_vol", this);
        apvts.addParameterListener("atk_time", this); // Adicionado
        apvts.addParameterListener("sus_time", this); // Adicionado
        
        a = apvts.getRawParameterValue("attack")->load();
        s = apvts.getRawParameterValue("sustain")->load();
        ftMs = apvts.getRawParameterValue("fish_tail")->load();
        ftVol = apvts.getRawParameterValue("fish_tail_vol")->load();
    }
    ~ShapePreviewComponent() override
    {
        apvts.removeParameterListener("attack", this);
        apvts.removeParameterListener("sustain", this);
        apvts.removeParameterListener("fish_tail", this);
        apvts.removeParameterListener("fish_tail_vol", this);
        apvts.removeParameterListener("atk_time", this); // Adicionado
        apvts.removeParameterListener("sus_time", this); // Adicionado
    }
    void parameterChanged(const juce::String& paramID, float val) override
    {
        if (paramID == "attack") a = val;
        if (paramID == "sustain") s = val;
        if (paramID == "fish_tail") ftMs = val;
        if (paramID == "fish_tail_vol") ftVol = val;
        juce::MessageManager::callAsync([this](){ repaint(); });
    }

void paint(juce::Graphics& g) override
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff252129)); 

    float zeroY = bounds.getCentreY();
    g.setColour(juce::Colour(0x50FFFFFF));
    g.drawLine(10.0f, zeroY, bounds.getWidth() - 10.0f, zeroY, 1.0f);

    float atkMs = apvts.getRawParameterValue("atk_time")->load();
    float atkDb = apvts.getRawParameterValue("attack")->load();
    float gapMs = apvts.getRawParameterValue("fish_tail")->load();
    float gapDb = apvts.getRawParameterValue("fish_tail_vol")->load();
    float susMs = apvts.getRawParameterValue("sus_time")->load();
    float susDb = apvts.getRawParameterValue("sustain")->load();

    float startX = 10.0f;
    float endX = bounds.getWidth() - 10.0f;
    float w = endX - startX;

    float atkNorm = atkMs / 50.0f;   
    float gapNorm = gapMs / 100.0f;  
    float susNorm = susMs / 500.0f;  

    float atkW = w * 0.05f + (w * 0.20f * atkNorm); 
    float gapW = w * 0.10f + (w * 0.25f * gapNorm); 
    float susW = w * 0.20f + (w * 0.35f * susNorm); 

    float peakX = startX + atkW;
    float gapX = peakX + gapW;
    float susX = gapX + susW; 
    float relX = endX;

    float maxDb = 48.0f;
    auto dbToY = [&](float db) {
        float norm = juce::jlimit(-maxDb, maxDb, db) / maxDb;
        return zeroY - (norm * (bounds.getHeight() * 0.45f));
    };

    float peakY = dbToY(atkDb);
    float gapY = dbToY(gapDb);
    float susY = dbToY(susDb);
    
    // A ponta final representa o mesmo nível de ganho do Sustain
    float relY = susY; 

    juce::Path p;
    p.startNewSubPath(startX, zeroY);

    p.cubicTo(startX + (peakX - startX) * 0.2f, zeroY, 
              peakX - (peakX - startX) * 0.8f, peakY, 
              peakX, peakY);

    p.cubicTo(peakX + (gapX - peakX) * 0.1f, gapY, 
              gapX - (gapX - peakX) * 0.5f, gapY, 
              gapX, gapY);

    p.cubicTo(gapX + (susX - gapX) * 0.3f, gapY, 
              susX - (susX - gapX) * 0.3f, susY, 
              susX, susY);

    // O release estende-se mantendo o nível do sustain até a extremidade direita
    p.lineTo(relX, relY);
    //preview color
    g.setColour(juce::Colour(0xffFFff00));
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}
    
private:
    juce::AudioProcessorValueTreeState& apvts;
    float a = 0.0f, s = 0.0f, ftMs = 0.0f, ftVol = 0.0f;
};

class NihilityTShaperEditor : public juce::AudioProcessorEditor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    NihilityTShaperEditor (NihilityTShaperProcessor&);
    ~NihilityTShaperEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parameterChanged(const juce::String& paramID, float val) override;

private:
    void updateButtonStates(int mode);
    void setupShapeButton(juce::ShapeButton& btn, const juce::Path& path);

    NihilityTShaperProcessor& audioProcessor;
    MinimalLookAndFeel customLAF;
    
    juce::Slider inGainSlider;
    juce::Slider outGainSlider;
    juce::Slider attackSlider;
    juce::Slider attackTimeSlider; 
    juce::Slider fishTailVolSlider;
    juce::Slider sustainSlider;
    juce::Slider sustainTimeSlider; 
    juce::Slider fishTailSlider;
    juce::Slider scHpfSlider;
    
    juce::ShapeButton btnLimit{"limit", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    juce::ShapeButton btnClip{"clip", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    juce::ShapeButton btnOff{"off", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    
    //rms abaixo
    juce::Slider rmsWindowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rmsWindowAtt;
    
    ShapePreviewComponent shapePreview;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt, atkAtt, atkTimeAtt, ftVolAtt, susAtt, susTimeAtt, fishAtt, scHpfAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NihilityTShaperEditor)
};
