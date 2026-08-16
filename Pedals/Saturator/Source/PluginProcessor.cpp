#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout SaturatorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"DRIVE",1}, "Drive", 0.0f, 10.0f, 6.0f));
    p.push_back (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"MODE",1}, "Mode", juce::StringArray{"Tube", "Tape", "Diode", "Hard"}, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"COLOR",1}, "Color", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"MIX",1}, "Mix", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"OUTPUT",1}, "Output", -18.0f, 6.0f, -3.0f));
    return {p.begin(), p.end()};
}
SaturatorAudioProcessor::SaturatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

void SaturatorAudioProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    juce::ignoreUnused (block);
    dryBuffer.setSize(getTotalNumOutputChannels(), block, false, false, true);
}
void SaturatorAudioProcessor::releaseResources() {}
bool SaturatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}
float SaturatorAudioProcessor::saturate (float x, float mode, float character) noexcept
{
    // Four genuinely different transfer families rather than four drive
    // multipliers: tube, tape, diode and hard clip.
    const float bias = character * 0.22f;
    const float biased = x + bias;
    float y = 0.0f;
    if (mode < 0.5f)
    {
        const float asym = biased + 0.10f * biased * biased;
        y = std::tanh (asym * 1.15f);
    }
    else if (mode < 1.5f)
    {
        const float compressed = biased / (1.0f + 0.55f * std::abs (biased));
        y = std::tanh (compressed * 1.45f);
    }
    else if (mode < 2.5f)
    {
        const float k = 1.6f + character * 2.0f;
        y = std::copysign (1.0f - std::exp (-k * std::abs (biased)), biased);
    }
    else
    {
        const float threshold = juce::jmax (0.35f, 0.92f - character * 0.30f);
        y = juce::jlimit (-threshold, threshold, biased) / threshold;
    }
    const float rest = mode < 2.5f ? 0.0f : juce::jlimit (-1.0f, 1.0f, bias);
    return juce::jlimit (-1.0f, 1.0f, y - rest);
}
void SaturatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals;
    const int in = getTotalNumInputChannels(), out = getTotalNumOutputChannels(), n = buffer.getNumSamples();
    for (int c=in;c<out;++c) buffer.clear(c,0,n);
    const float drive = apvts.getRawParameterValue("DRIVE")->load();
    const float mode = apvts.getRawParameterValue("MODE")->load();
    const float color = apvts.getRawParameterValue("COLOR")->load();
    const float mix = apvts.getRawParameterValue("MIX")->load();
    const float outGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("OUTPUT")->load());
    for (int c = 0; c < in; ++c) dryBuffer.copyFrom(c, 0, buffer, c, 0, n);
    const float gain = juce::Decibels::decibelsToGain(drive);
    for (int c = 0; c < in; ++c)
    {
        auto* d = buffer.getWritePointer(c);
        for (int s = 0; s < n; ++s)
            d[s] = saturate(d[s] * gain, mode, color);
    }
    const float dryGain=1.0f-mix;
    for(int c=0;c<in;++c)
    {
        auto* d=buffer.getWritePointer(c);
        const auto* dry=dryBuffer.getReadPointer(c);
        for(int s=0;s<n;++s) d[s]=(dry[s]*dryGain + d[s]*mix)*outGain;
    }
}
juce::AudioProcessorEditor* SaturatorAudioProcessor::createEditor(){ return new DevKomodoUniversalEditor(*this,apvts,JucePlugin_Name); }
void SaturatorAudioProcessor::getStateInformation(juce::MemoryBlock& dest){ auto st=apvts.copyState(); auto xml=st.createXml(); copyXmlToBinary(*xml,dest); }
void SaturatorAudioProcessor::setStateInformation(const void* data,int size){ std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size)); if(xml && xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new SaturatorAudioProcessor(); }
