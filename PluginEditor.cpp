#include "PluginProcessor.h"
#include "PluginEditor.h"

NihilityTShaperEditor::NihilityTShaperEditor (NihilityTShaperProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), shapePreview(p.apvts)
{
    setSize (600, 540);
    setLookAndFeel(&customLAF);

    auto setupSlider = [this](juce::Slider& s, const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att, bool isVertical) {
        s.setSliderStyle(isVertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(250); 
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, paramID, s);
    };

    setupSlider(inGainSlider, "in_gain", inAtt, false);
    setupSlider(outGainSlider, "out_gain", outAtt, false);
    setupSlider(attackSlider, "attack", atkAtt, true);
    setupSlider(attackTimeSlider, "atk_time", atkTimeAtt, false);
    setupSlider(fishTailVolSlider, "fish_tail_vol", ftVolAtt, true);
    setupSlider(sustainSlider, "sustain", susAtt, true);
    setupSlider(sustainTimeSlider, "sus_time", susTimeAtt, false); 
    setupSlider(fishTailSlider, "fish_tail", fishAtt, false);
    setupSlider(scHpfSlider, "sc_hpf", scHpfAtt, false); 
    setupSlider(rmsWindowSlider, "rms_window", rmsWindowAtt, false);

    juce::Path circlePath, squarePath, crossPath;
    circlePath.addEllipse(0, 0, 24, 24);
    squarePath.addRectangle(0, 0, 26, 18);
    crossPath.addLineSegment(juce::Line<float>(0.0f, 0.0f, 20.0f, 20.0f), 3.0f);
    crossPath.addLineSegment(juce::Line<float>(0.0f, 20.0f, 20.0f, 0.0f), 3.0f);

    setupShapeButton(btnLimit, circlePath);
    btnLimit.onClick = [this] { 
        updateButtonStates(0); 
        audioProcessor.apvts.getParameter("dyn_mode")->setValueNotifyingHost(0.0f); 
    };
    addAndMakeVisible(btnLimit);

    setupShapeButton(btnClip, squarePath);
    btnClip.onClick = [this] { 
        updateButtonStates(1); 
        audioProcessor.apvts.getParameter("dyn_mode")->setValueNotifyingHost(0.5f); 
    };
    addAndMakeVisible(btnClip);

    setupShapeButton(btnOff, crossPath);
    btnOff.onClick = [this] { 
        updateButtonStates(2); 
        audioProcessor.apvts.getParameter("dyn_mode")->setValueNotifyingHost(1.0f); 
    };
    addAndMakeVisible(btnOff);

    addAndMakeVisible(shapePreview);
    
    audioProcessor.apvts.addParameterListener("dyn_mode", this);
    updateButtonStates(static_cast<int>(audioProcessor.apvts.getRawParameterValue("dyn_mode")->load()));
}

NihilityTShaperEditor::~NihilityTShaperEditor()
{
    setLookAndFeel(nullptr);
    audioProcessor.apvts.removeParameterListener("dyn_mode", this);
}

void NihilityTShaperEditor::setupShapeButton(juce::ShapeButton& btn, const juce::Path& path)
{
    btn.setShape(path, true, true, false);
    btn.setClickingTogglesState(true);
    btn.setRadioGroupId(1); 
}

void NihilityTShaperEditor::parameterChanged(const juce::String& paramID, float val)
{
    if (paramID == "dyn_mode") {
        juce::MessageManager::callAsync([this, val](){ updateButtonStates(static_cast<int>(val)); });
    }
}

void NihilityTShaperEditor::updateButtonStates(int mode)
{
    juce::Colour amareloAtivo(0xffFFff00);
    juce::Colour brancoInativo(0x50FFFFFF);
    juce::Colour brancoHover(0x80FFFFFF);
    
    btnLimit.setColours(mode == 0 ? amareloAtivo : brancoInativo, mode == 0 ? amareloAtivo : brancoHover, amareloAtivo);
    btnClip.setColours(mode == 1 ? amareloAtivo : brancoInativo, mode == 1 ? amareloAtivo : brancoHover, amareloAtivo);
    btnOff.setColours(mode == 2 ? amareloAtivo : brancoInativo, mode == 2 ? amareloAtivo : brancoHover, amareloAtivo);

    btnLimit.setToggleState(mode == 0, juce::dontSendNotification);
    btnClip.setToggleState(mode == 1, juce::dontSendNotification);
    btnOff.setToggleState(mode == 2, juce::dontSendNotification);
}

void NihilityTShaperEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bgGradient(juce::Colour(0xff767687), 0.8f, 0.0f, 
                                    juce::Colour(0xff42404a), (float)getWidth(), (float)getHeight(), false);
    
    g.setGradientFill(bgGradient);
    g.fillAll(); 
    
    g.setColour (juce::Colour(0xffFFFFFF));
    g.setFont (juce::FontOptions().withHeight(13.0f));
    
    g.drawText("Input", 40, 20, 200, 20, juce::Justification::centred);
    g.drawText("Output", 350, 20, 200, 20, juce::Justification::centred);

    g.drawText("Attack", 40, 280, 50, 20, juce::Justification::centred);
    g.drawText("Hold", 115, 280, 50, 20, juce::Justification::centred);
    g.drawText("Sustain", 190, 280, 50, 20, juce::Justification::centred);
    
    g.drawText("Atk Time", 40, 345, 200, 20, juce::Justification::centred);
    g.drawText("H Time", 40, 405, 200, 20, juce::Justification::centred);
    g.drawText("Sus Time", 40, 465, 200, 20, juce::Justification::centred);

    g.drawText("Sidechain High Pass", 335, 405, 200, 20, juce::Justification::centred);
    g.drawText("RMS Window (ms)", 335, 465, 200, 20, juce::Justification::centred);

    g.drawText("Limit", 350, 115, 50, 20, juce::Justification::centred);
    g.drawText("Clip", 400, 115, 50, 20, juce::Justification::centred);
    g.drawText("Off", 450, 115, 50, 20, juce::Justification::centred);

    g.drawText("Shape Preview", 325, 145, 220, 20, juce::Justification::centred);
    
    g.setFont (juce::FontOptions().withHeight(11.0f));
    g.setColour (juce::Colour(0xa0FFFFFF)); 
    g.drawText("Transient shaper by @nihilitystuffs", 40, 510, 520, 20, juce::Justification::centred);
}

void NihilityTShaperEditor::resized()
{
    inGainSlider.setBounds(40, 40, 200, 30);
    outGainSlider.setBounds(350, 40, 200, 30);

    attackSlider.setBounds(40, 110, 50, 160);
    fishTailVolSlider.setBounds(115, 110, 50, 160);
    sustainSlider.setBounds(190, 110, 50, 160);
    
    attackTimeSlider.setBounds(40, 320, 200, 25);
    
    fishTailSlider.setBounds(40, 380, 200, 25);
    scHpfSlider.setBounds(335, 380, 200, 25); 
    
    sustainTimeSlider.setBounds(40, 440, 200, 25);
    rmsWindowSlider.setBounds(335, 440, 200, 25);
    
    btnLimit.setBounds(360, 80, 30, 30);
    btnClip.setBounds(410, 85, 40, 25);
    btnOff.setBounds(465, 85, 25, 25);
    
    shapePreview.setBounds(325, 170, 220, 180);
}
