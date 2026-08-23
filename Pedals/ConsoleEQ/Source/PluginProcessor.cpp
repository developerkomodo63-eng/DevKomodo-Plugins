#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout ConsoleEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "XOVERLOW", 1 }, "Low/Mid Crossover",
        juce::NormalisableRange<float> { 100.0f, 1200.0f, 0.0f, 0.4f }, 400.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "XOVERHIGH", 1 }, "Mid/High Crossover",
        juce::NormalisableRange<float> { 1500.0f, 9000.0f, 0.0f, 0.4f }, 3000.0f));

    // 0 = fully clean for that band, 10 = heavily saturated. Defaults match
    // the brief this pedal was built for: highs driven, lows/mids clean.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "LOWDRIVE", 1 }, "Low Drive", 0.0f, 10.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "MIDDRIVE", 1 }, "Mid Drive", 0.0f, 10.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "HIGHDRIVE", 1 }, "High Drive", 0.0f, 10.0f, 4.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "HQ", 1 }, "HQ", false));

    return { params.begin(), params.end() };
}

ConsoleEQAudioProcessor::ConsoleEQAudioProcessor()
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

ConsoleEQAudioProcessor::~ConsoleEQAudioProcessor()
{
}

void ConsoleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    splits.resize ((size_t) numChannels);
    for (auto& s : splits)
    {
        s.lowLP.prepare (spec);
        s.lowHP.prepare (spec);
        s.midLP.prepare (spec);
        s.midHP.prepare (spec);
        s.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        s.lowHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        s.midLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        s.midHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        s.reset();
    }
    lowBand.setSize (numChannels, samplesPerBlock);
    midBand.setSize (numChannels, samplesPerBlock);
    highBand.setSize (numChannels, samplesPerBlock);
    lowDriveSmoothed.reset (sampleRate, 0.02);
    midDriveSmoothed.reset (sampleRate, 0.02);
    highDriveSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    lowDriveBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    midDriveBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    highDriveBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    lowDriveSmoothed.setCurrentAndTargetValue (0.0f);
    midDriveSmoothed.setCurrentAndTargetValue (0.0f);
    highDriveSmoothed.setCurrentAndTargetValue (0.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
}

void ConsoleEQAudioProcessor::releaseResources()
{
}

bool ConsoleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ConsoleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float xoverLow  = apvts.getRawParameterValue ("XOVERLOW")->load();
    const float xoverHigh = apvts.getRawParameterValue ("XOVERHIGH")->load();
    // Keep the two crossovers from crossing each other (a min gap keeps the
    // mid band from collapsing to nothing if both knobs are swept together).
    const float safeXoverHigh = juce::jmax (xoverHigh, xoverLow * 1.5f);

    const float lowDrive  = apvts.getRawParameterValue ("LOWDRIVE")->load()  / 10.0f;
    const float midDrive  = apvts.getRawParameterValue ("MIDDRIVE")->load()  / 10.0f;
    const float highDrive = apvts.getRawParameterValue ("HIGHDRIVE")->load() / 10.0f;
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());
    const bool hq = apvts.getRawParameterValue ("HQ")->load() > 0.5f;
    lowDriveSmoothed.setTargetValue (lowDrive);
    midDriveSmoothed.setTargetValue (midDrive);
    highDriveSmoothed.setTargetValue (highDrive);
    outputGainSmoothed.setTargetValue (outputGain);
    // jassert-only bounds checks are compiled out entirely in Release
    // builds, so they gave zero real protection: if a host/exporter uses
    // a block size larger than what prepareToPlay() originally sized
    // these buffers for (this happens with some DAWs' offline bounce/
    // export, which can use a different block size than realtime
    // playback), the per-sample smoothing loop below would write past
    // the end of these vectors -- a real heap buffer overflow, not just
    // a debug-mode warning. Actually growing the buffers here fixes it
    // for any block size the host throws at us.
        if (numSamples > (int) lowDriveBuffer.size()) lowDriveBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) midDriveBuffer.size()) midDriveBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) highDriveBuffer.size()) highDriveBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) outputGainBuffer.size()) outputGainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        lowDriveBuffer[(size_t) sample] = lowDriveSmoothed.getNextValue();
        midDriveBuffer[(size_t) sample] = midDriveSmoothed.getNextValue();
        highDriveBuffer[(size_t) sample] = highDriveSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    for (auto& s : splits)
    {
        s.lowLP.setCutoffFrequency (xoverLow);
        s.lowHP.setCutoffFrequency (xoverLow);
        s.midLP.setCutoffFrequency (safeXoverHigh);
        s.midHP.setCutoffFrequency (safeXoverHigh);
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        float* lowData = lowBand.getWritePointer (channel);
        float* midData = midBand.getWritePointer (channel);
        float* highData = highBand.getWritePointer (channel);
        auto& s = splits[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float low       = s.lowLP.processSample (0, dry);
            const float aboveLow  = s.lowHP.processSample (0, dry);
            const float mid       = s.midLP.processSample (0, aboveLow);
            const float high      = s.midHP.processSample (0, aboveLow);

            lowData[sample] = low;
            midData[sample] = mid;
            highData[sample] = high;
        }
    }

    auto processBand = [this, hq, numSamples] (juce::AudioBuffer<float>& band,
                                                 const std::vector<float>& driveValues)
    {
        juce::dsp::AudioBlock<float> audioBlock (band);
        auto processingBlock = audioBlock;
        if (hq)
        {
            oversampling.reset();
            processingBlock = oversampling.processSamplesUp (processingBlock);
        }

        for (size_t channel = 0; channel < processingBlock.getNumChannels(); ++channel)
        {
            float* data = processingBlock.getChannelPointer (channel);
            for (size_t sample = 0; sample < processingBlock.getNumSamples(); ++sample)
            {
                const int sourceSample = juce::jmin (numSamples - 1, (int) sample / (hq ? 4 : 1));
                const float driveAmount = driveValues[(size_t) sourceSample];
                const float gainStage = 1.0f + driveAmount * 6.0f;
                const float input = data[sample];
                const float saturated = std::tanh (input * gainStage) / juce::jmax (gainStage, 1.0f);
                data[sample] = input * (1.0f - driveAmount) + saturated * driveAmount;
            }
        }

        if (hq)
            oversampling.processSamplesDown (audioBlock);
    };

    processBand (lowBand, lowDriveBuffer);
    processBand (midBand, midDriveBuffer);
    processBand (highBand, highDriveBuffer);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        const float* lowData = lowBand.getReadPointer (channel);
        const float* midData = midBand.getReadPointer (channel);
        const float* highData = highBand.getReadPointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = (lowData[sample] + midData[sample] + highData[sample])
                                * outputGainBuffer[(size_t) sample];
    }
}

juce::AudioProcessorEditor* ConsoleEQAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (230, 126, 34));
}

void ConsoleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ConsoleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConsoleEQAudioProcessor();
}
