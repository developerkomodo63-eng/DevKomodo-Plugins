#include "PluginProcessor.h"
#include "DevKomodoUI.h"


juce::AudioProcessorValueTreeState::ParameterLayout CleanUpProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GATE", 1 }, "Gate Threshold", -60.0f, 0.0f, -48.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DEESSF", 1 }, "De-esser Freq", 2000.0f, 8000.0f, 5000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DEESSA", 1 }, "De-esser Amount", 0.0f, 1.0f, 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANS", 1 }, "Transient", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OUTPUT", 1 }, "Output", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

CleanUpProAudioProcessor::CleanUpProAudioProcessor()
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

CleanUpProAudioProcessor::~CleanUpProAudioProcessor() {}

void CleanUpProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
    const auto channels = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    gateGain.assign (channels, 1.0f);
    gateDetector.assign (channels, 0.0f);
    deEssLow.assign (channels, 0.0f);
    transientLow.assign (channels, 0.0f);
}

void CleanUpProAudioProcessor::releaseResources() {}

bool CleanUpProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CleanUpProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float gateDb = apvts.getRawParameterValue ("GATE")->load();
    const float gateThreshold = juce::Decibels::decibelsToGain (gateDb);
    const float deFreq = apvts.getRawParameterValue ("DEESSF")->load();
    const float deAmt = apvts.getRawParameterValue ("DEESSA")->load();
    const float trans = apvts.getRawParameterValue ("TRANS")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("OUTPUT")->load());

    // Lightweight three-stage cleanup: smooth gate, frequency-aware de-esser,
    // and transient enhancement. No FFT and no allocations in processBlock.
    const float gateOpenCoeff  = std::exp (-1.0f / (0.003f * (float) fs));
    const float gateCloseCoeff = std::exp (-1.0f / (0.045f * (float) fs));
    const float detectorCoeff  = std::exp (-1.0f / (0.008f * (float) fs));
    const float lowCoeff       = std::exp (-juce::MathConstants<float>::twoPi * deFreq / (float) fs);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float gate = gateGain[(size_t) ch];
        float low = deEssLow[(size_t) ch];
        float transientBase = transientLow[(size_t) ch];
        float detector = gateDetector[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];
            const float absInput = std::abs (input);
            const float detectorTarget = juce::jmax (absInput, 1.0e-6f);
            detector += (detectorTarget - detector) * (1.0f - detectorCoeff);

            const bool aboveGate = detector >= gateThreshold;
            const float targetGate = aboveGate ? 1.0f : 0.08f;
            const float gateCoeff = aboveGate ? gateOpenCoeff : gateCloseCoeff;
            gate = targetGate + (gate - targetGate) * gateCoeff;
            float cleaned = input * gate;

            low += (cleaned - low) * (1.0f - lowCoeff);
            const float highBand = cleaned - low;
            const float sibilance = juce::jlimit (0.0f, 1.0f, (std::abs (highBand) - 0.008f) * 9.0f);
            const float deGain = 1.0f - deAmt * sibilance * 0.72f;
            cleaned *= deGain;

            transientBase += (cleaned - transientBase) * 0.035f;
            const float transient = cleaned - transientBase;
            cleaned += transient * trans * 0.65f;

            const float processed = juce::jlimit (-1.2f, 1.2f, cleaned);
            data[i] = (input * (1.0f - mix) + processed * mix) * outputGain;
        }

        gateGain[(size_t) ch] = gate;
        gateDetector[(size_t) ch] = detector;
        deEssLow[(size_t) ch] = low;
        transientLow[(size_t) ch] = transientBase;
    }
}

void CleanUpProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void CleanUpProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* CleanUpProAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CleanUpProAudioProcessor(); }
