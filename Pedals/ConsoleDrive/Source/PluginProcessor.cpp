#include "PluginProcessor.h"
#include "DevKomodoUI.h"

#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout ConsoleDriveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TONE", 1 }, "Tone", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

ConsoleDriveAudioProcessor::ConsoleDriveAudioProcessor()
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

ConsoleDriveAudioProcessor::~ConsoleDriveAudioProcessor() {}

void ConsoleDriveAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = newSampleRate;
    lowState.assign ((size_t) juce::jmax (1, getTotalNumOutputChannels()), 0.0f);
    driveSmoothed.reset (newSampleRate, 0.02);
    mixSmoothed.reset (newSampleRate, 0.02);
    gainSmoothed.reset (newSampleRate, 0.02);
    driveBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    gainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void ConsoleDriveAudioProcessor::releaseResources() {}

bool ConsoleDriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
 #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
 #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
 #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
 #endif
    return true;
 #endif
}

void ConsoleDriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float driveKnob = apvts.getRawParameterValue ("DRIVE")->load();
    const float drive = driveKnob * 0.1f;
    const float tone  = apvts.getRawParameterValue ("TONE")->load();
    const float mix   = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float gain = juce::Decibels::decibelsToGain (levelDb);
    driveSmoothed.setTargetValue (drive);
    mixSmoothed.setTargetValue (mix);
    gainSmoothed.setTargetValue (gain);
    // jassert-only bounds checks are compiled out entirely in Release
    // builds, so they gave zero real protection: if a host/exporter uses
    // a block size larger than what prepareToPlay() originally sized
    // these buffers for (this happens with some DAWs' offline bounce/
    // export, which can use a different block size than realtime
    // playback), the per-sample smoothing loop below would write past
    // the end of these vectors -- a real heap buffer overflow, not just
    // a debug-mode warning. Actually growing the buffers here fixes it
    // for any block size the host throws at us.
        if (numSamples > (int) driveBuffer.size()) driveBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
        if (numSamples > (int) gainBuffer.size()) gainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveBuffer[(size_t) sample] = driveSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        gainBuffer[(size_t) sample] = gainSmoothed.getNextValue();
    }
    const float lowCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sampleRate);
    const float toneTilt = tone / 12.0f;

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float low = lowState[(size_t) ch];
        for (int i = 0; i < numSamples; ++i)
        {
            const float in = data[i];
            const float smoothedDrive = driveBuffer[(size_t) i];
            const float smoothedMix = mixBuffer[(size_t) i];
            // Console drive is intentionally subtle: low-frequency energy is
            // compressed a little more and the tone control is a true spectral
            // tilt, rather than a post-saturation volume multiplier.
            low += (in - low) * lowCoeff;
            const float high = in - low;
            const float driven = low * (1.0f + smoothedDrive * 1.8f) + high * (1.0f + smoothedDrive * 2.4f);
            float shaped = driven / (1.0f + 0.22f * std::abs (driven));
            shaped += toneTilt * (high * 0.35f - low * 0.10f);
            // Was previously a pure serial effect with no way to blend back
            // in the dry signal -- MIX now allows parallel/New-York-style
            // drive instead of only 100% wet.
            data[i] = (in * (1.0f - smoothedMix) + shaped * smoothedMix) * gainBuffer[(size_t) i];
        }
        lowState[(size_t) ch] = low;
    }
}

void ConsoleDriveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void ConsoleDriveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ConsoleDriveAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (214, 100, 40));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ConsoleDriveAudioProcessor(); }
