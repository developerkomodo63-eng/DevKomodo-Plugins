#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <cmath>
#include "DevKomodoUI.h"

template <typename Processor>
class SynthPedalEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    struct Preset { const char* name; int waveform; int octave; float glide; float detune; float sub; float gate; float mix; float level; };

    SynthPedalEditor (Processor& p, juce::String titleText, std::array<Preset, 6> presetValues, juce::Colour accentColour)
        : AudioProcessorEditor (&p), processor (p), presets (presetValues), accent (accentColour)
    {
        setOpaque (true); setResizable (true, true); setResizeLimits (760, 590, 1280, 820); setSize (900, 650);
        title.setText (titleText, juce::dontSendNotification); title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold))); title.setColour (juce::Label::textColourId, juce::Colours::white); addAndMakeVisible (title);
        subtitle.setText ("MONOPHONIC AUDIO  •  PITCH-LOCK SYNTH", juce::dontSendNotification); subtitle.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold))); subtitle.setColour (juce::Label::textColourId, accent.withAlpha (0.9f)); addAndMakeVisible (subtitle);
        presetBox.addItem ("MANUAL", 1);
        waveformLabel.setText ("WAVEFORM", juce::dontSendNotification);
        waveformLabel.setJustificationType (juce::Justification::centred);
        waveformLabel.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        waveformLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.78f));
        addAndMakeVisible (waveformLabel); for (int i=0;i<(int)presets.size();++i) presetBox.addItem (presets[(size_t)i].name, i+2); presetBox.setSelectedId (1, juce::dontSendNotification);
        presetBox.onChange=[this]{ const int id=presetBox.getSelectedId(); if(id>=2 && id-2<(int)presets.size()) loadPreset(presets[(size_t)id-2]); }; addAndMakeVisible(presetBox);
        configureChoice (waveform, waveformAttachment, "WAVEFORM", { "Sine", "Saw", "Square", "Triangle", "Pulse", "Soft Saw", "Super Saw", "Organ", "Custom" });
        configureSlider(octave,octaveAttachment,"OCTAVE"," oct"); configureSlider(glide,glideAttachment,"GLIDE"," ms"); configureSlider(detune,detuneAttachment,"DETUNE"," ct");
        configureSlider(tuning,tuningAttachment,"TUNING"," Hz"); configureSlider(pitchCorrection,pitchCorrectionAttachment,"PITCHCORR",""); configureSlider(pulseWidth,pulseWidthAttachment,"PULSEWIDTH","");
        configureSlider(h2,h2Attachment,"H2",""); configureSlider(h3,h3Attachment,"H3",""); configureSlider(h4,h4Attachment,"H4",""); configureSlider(h5,h5Attachment,"H5","");
        configureSlider(sub,subAttachment,"SUBLEVEL",""); configureSlider(gate,gateAttachment,"GATE"," dB"); configureSlider(mix,mixAttachment,"MIX",""); configureSlider(level,levelAttachment,"LEVEL"," dB");
        startTimerHz(20);
    }
    ~SynthPedalEditor() override {
        stopTimer();
        for (auto* slider : { &octave, &glide, &detune, &tuning, &pitchCorrection, &pulseWidth, &h2, &h3, &h4, &h5, &sub, &gate, &mix, &level })
            slider->setLookAndFeel (nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(10,11,15)); auto panel=getLocalBounds().toFloat().reduced(14.0f); g.setColour(juce::Colour::fromRGB(27,29,35)); g.fillRoundedRectangle(panel,14.0f); g.setColour(juce::Colour::fromRGB(58,61,70)); g.drawRoundedRectangle(panel,14.0f,1.0f);
        auto header=panel.removeFromTop(66.0f); g.setColour(juce::Colour::fromRGB(19,21,26)); g.fillRoundedRectangle(header,12.0f); g.setColour(accent); g.fillRoundedRectangle(header.getX(),header.getY(),4.0f,header.getHeight(),2.0f);
        auto tracker=panel.removeFromTop(105.0f).reduced(10.0f,8.0f); g.setColour(juce::Colour::fromRGB(14,15,19)); g.fillRoundedRectangle(tracker,10.0f); g.setColour(juce::Colour::fromRGB(55,58,67)); g.drawRoundedRectangle(tracker,10.0f,1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.5f)); g.setFont(juce::Font(juce::FontOptions(9.0f,juce::Font::bold))); g.drawText("PITCH TRACKER",tracker.getX()+14,tracker.getY()+9,120,16,juce::Justification::left);
        g.setColour(accent); g.setFont(juce::Font(juce::FontOptions(28.0f,juce::Font::bold))); g.drawText(detectedNote.isEmpty()?"--":detectedNote,tracker.getX()+14,tracker.getY()+25,100,40,juce::Justification::left);
        g.setColour(juce::Colours::white.withAlpha(0.72f)); g.setFont(juce::Font(juce::FontOptions(12.0f))); g.drawText(detectedFrequency.isEmpty()?"-- Hz":detectedFrequency,tracker.getX()+118,tracker.getY()+40,120,20,juce::Justification::left);
        auto meter=tracker.removeFromRight(190).reduced(10,40); g.setColour(juce::Colour::fromRGB(45,48,56)); g.fillRoundedRectangle(meter,4); float amount=juce::jlimit(0.0f,1.0f,processor.getDetectedRms()*7.0f); g.setColour(accent.withAlpha(0.85f)); g.fillRoundedRectangle(meter.withWidth(meter.getWidth()*amount),4);
        g.setColour(juce::Colours::white.withAlpha(0.42f)); g.setFont(juce::Font(juce::FontOptions(8.0f,juce::Font::bold))); g.drawText("INPUT LEVEL",meter.getX(),meter.getY()-15,meter.getWidth(),12,juce::Justification::left);
        auto footer=getLocalBounds().removeFromBottom(20).reduced(20,0); g.setColour(juce::Colours::white.withAlpha(0.22f)); g.setFont(juce::Font(juce::FontOptions(8.0f))); g.drawText("DEVKOMODO  •  NOTE LOCK  •  CUSTOM HARMONICS",footer,juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area=getLocalBounds().reduced(28,18);
        auto header=area.removeFromTop(48);
        title.setBounds(header.removeFromLeft(230));
        subtitle.setBounds(header.removeFromLeft(250).withY(17).withHeight(20));
        presetBox.setBounds(header.removeFromRight(190).withY(6).withHeight(28));
        auto waveHeader = header.removeFromRight(150);
        waveformLabel.setBounds(waveHeader.removeFromTop(14));
        waveform.setBounds(waveHeader.withY(waveHeader.getY() + 14).withHeight(28));
        area.removeFromTop(105); area.removeFromBottom(18); area.reduce(4,4);
        auto row1=area.removeFromTop(128); auto row2=area.removeFromTop(128); auto row3=area.removeFromTop(128); auto row4=area;
        place4(row1,octave,glide,detune,tuning);
        place4(row2,pitchCorrection,pulseWidth,h2,h3);
        place4(row3,h4,h5,sub,gate);
        mix.setBounds(row4.removeFromLeft(row4.getWidth()/2).reduced(7));
        level.setBounds(row4.reduced(7));
    }

private:
    void place4(juce::Rectangle<int> r, juce::Component& a, juce::Component& b, juce::Component& c, juce::Component& d){ int w=r.getWidth()/4; a.setBounds(r.removeFromLeft(w).reduced(7)); b.setBounds(r.removeFromLeft(w).reduced(7)); c.setBounds(r.removeFromLeft(w).reduced(7)); d.setBounds(r.reduced(7)); }
    template <typename Attachment> void configureSlider(juce::Slider& slider,std::unique_ptr<Attachment>& attachment,const char* id,const char* suffix){
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,92,22);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.92f));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(14,16,20));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(55,60,70));
        slider.setTextValueSuffix(suffix);
        const juce::String upperId = juce::String (id).toUpperCase();
        const bool percent = upperId == "MIX" || upperId == "PULSEWIDTH" || upperId == "PITCHCORR"
                          || upperId == "H2" || upperId == "H3" || upperId == "H4" || upperId == "H5" || upperId == "SUBLEVEL";
        if (percent)
        {
            slider.setNumDecimalPlacesToDisplay (0);
            slider.textFromValueFunction = [] (double value)
            { return juce::String (juce::roundToInt ((float) value * 100.0f)) + "%"; };
            slider.valueFromTextFunction = [] (const juce::String& text)
            { return text.trimCharactersAtEnd ("% ").getDoubleValue() * 0.01; };
        }
        slider.setName(id);
        slider.setTooltip(id);
        slider.setScrollWheelEnabled(false);
        slider.setMouseDragSensitivity (320);
        slider.setVelocityBasedMode (false);
        slider.setLookAndFeel (&knobLookAndFeel);
        if (auto* parameter = processor.apvts.getParameter(id))
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                slider.setDoubleClickReturnValue(true, ranged->getNormalisableRange().convertFrom0to1(ranged->getDefaultValue()));
        }
        addAndMakeVisible(slider);
        attachment=std::make_unique<Attachment>(processor.apvts,id,slider);
    }
    void configureChoice(juce::ComboBox& box,std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment,const char* id,juce::StringArray choices){ box.setName(id); box.addItemList(choices,1); box.setTooltip(id); addAndMakeVisible(box); attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,id,box); }
    void setParameter(const char* id,float value){ if(auto* p=processor.apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(value)); }
    void loadPreset(const Preset& p){ setParameter("WAVEFORM", (float) p.waveform); setParameter("OCTAVE",(float)p.octave); setParameter("GLIDE",p.glide); setParameter("DETUNE",p.detune); setParameter("SUBLEVEL",p.sub); setParameter("GATE",p.gate); setParameter("MIX",p.mix); setParameter("LEVEL",p.level); }
    void timerCallback() override{ const float f=processor.getDetectedFrequency(); if(f>0.0f&&std::isfinite(f)){ detectedFrequency=juce::String(f,1)+" Hz"; const float midi=69.0f+12.0f*std::log2(f/440.0f); const int n=juce::jlimit(0,127,(int)std::lround(midi)); static constexpr const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; detectedNote=juce::String(names[n%12])+juce::String(n/12-1); } else {detectedFrequency="-- Hz"; detectedNote.clear();} repaint(); }
    DevKomodoKnobLookAndFeel knobLookAndFeel;
    Processor& processor; std::array<Preset,6> presets; juce::Colour accent;
    juce::Label title,subtitle,waveformLabel; juce::ComboBox presetBox,waveform;
    juce::Slider octave,glide,detune,tuning,pitchCorrection,pulseWidth,h2,h3,h4,h5,sub,gate,mix,level;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment,glideAttachment,detuneAttachment,tuningAttachment,pitchCorrectionAttachment,pulseWidthAttachment,h2Attachment,h3Attachment,h4Attachment,h5Attachment,subAttachment,gateAttachment,mixAttachment,levelAttachment;
    juce::String detectedNote,detectedFrequency;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthPedalEditor)
};
