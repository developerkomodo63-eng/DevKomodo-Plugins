#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout CassetteEmulationAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Discrete tape-formulation switch. Each option carries its own fixed
    // bandwidth and noise floor -- real physical characteristics of that
    // tape type, not a continuous EQ knob standing in for them.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "TYPE", 1 }, "Tape Type",
        juce::StringArray { "Ferric (Type I)", "Chrome (Type II)", "Metal (Type IV)" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WOW", 1 }, "Wow", 0.0f, 10.0f, 4.0f));

    // Noise-reduction "breathing": models the audible pumping real Dolby
    // B/C-style companders produce when the encode/decode envelopes don't
    // quite track each other.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DOLBY", 1 }, "Dolby", 0.0f, 10.0f, 3.0f));

    // Level-dependent hiss: emerges in quiet passages, masked by louder
    // signal -- not a constant noise floor.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HISS", 1 }, "Hiss", 0.0f, 10.0f, 3.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

CassetteEmulationAudioProcessor::CassetteEmulationAudioProcessor()
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

CassetteEmulationAudioProcessor::~CassetteEmulationAudioProcessor()
{
}

void CassetteEmulationAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    wowPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    // ~20ms of buffer comfortably covers the base delay plus the widest
    // wow excursion at full depth on the wobbliest (Ferric) setting.
    const int wowLineLength = (int) (0.020 * sampleRate) + 4;

    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.typeFilter.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 1 });
        c.wowLine.assign ((size_t) wowLineLength, 0.0f);
        c.noiseRandom.setSeedRandomly();
        c.reset();
    }

    dolbyFastCoeff = std::exp (-1.0f / (0.004f * (float) sampleRate));
    dolbySlowCoeff = std::exp (-1.0f / (0.150f * (float) sampleRate));
    hissEnvCoeff   = std::exp (-1.0f / (0.300f * (float) sampleRate));
}

void CassetteEmulationAudioProcessor::releaseResources()
{
}

bool CassetteEmulationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CassetteEmulationAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const int type = (int) apvts.getRawParameterValue ("TYPE")->load();
    const float wowNorm   = apvts.getRawParameterValue ("WOW")->load()   / 10.0f;
    const float dolbyNorm = apvts.getRawParameterValue ("DOLBY")->load() / 10.0f;
    const float hissNorm  = apvts.getRawParameterValue ("HISS")->load()  / 10.0f;
    const float mix       = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    // Per-type physical character: cutoff frequency, how much the
    // mechanism wobbles, and the base noise floor. Ferric is the
    // classic budget cassette formulation (dull, wobbly, noisy); Metal
    // is the premium formulation (extended, stable, quiet).
    struct TypeProfile { float cutoffHz; float wowMultiplier; float noiseBase; };
    static constexpr TypeProfile profiles[3] = {
        { 8000.0f,  1.4f, 0.045f },  // Ferric (Type I)
        { 12000.0f, 1.0f, 0.028f },  // Chrome (Type II)
        { 15000.0f, 0.7f, 0.018f },  // Metal (Type IV)
    };
    const auto& profile = profiles[(size_t) juce::jlimit (0, 2, type)];

    for (auto& c : channels)
        c.typeFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, profile.cutoffHz, 0.707f);

    static constexpr float wowRateHz = 0.6f;
    wowPhase += wowRateHz * (float) numSamples / (float) sampleRate;
    if (wowPhase >= 1.0f) wowPhase -= std::floor (wowPhase);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        const int lineLen = (int) c.wowLine.size();

        // Give the two channels a slightly offset wow phase so a stereo
        // source doesn't wobble in perfect lockstep (real cassette
        // mechanisms don't perfectly correlate channel-to-channel either).
        float localPhase = wowPhase + (channel == 0 ? 0.0f : 0.13f);
        if (localPhase >= 1.0f) localPhase -= 1.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            // Wow: write into the line, read back with a fractional,
            // slowly-modulated offset for the pitch-wobble.
            c.wowLine[(size_t) c.wowWritePos] = dry;
            const float lfo = std::sin (localPhase * juce::MathConstants<float>::twoPi
                                         + (float) sample * wowRateHz * juce::MathConstants<float>::twoPi / (float) sampleRate);
            const float wowDepthSamples = wowNorm * profile.wowMultiplier * 0.004f * (float) sampleRate;
            const float baseDelaySamples = 0.006f * (float) sampleRate;
            const float delaySamples = baseDelaySamples + lfo * wowDepthSamples;

            float readPosF = (float) c.wowWritePos - delaySamples;
            while (readPosF < 0.0f) readPosF += (float) lineLen;
            const int readIdx0 = (int) readPosF % lineLen;
            const int readIdx1 = (readIdx0 + 1) % lineLen;
            const float frac = readPosF - std::floor (readPosF);
            const float wowed = c.wowLine[(size_t) readIdx0] * (1.0f - frac)
                               + c.wowLine[(size_t) readIdx1] * frac;
            c.wowWritePos = (c.wowWritePos + 1) % lineLen;

            const float filtered = c.typeFilter.processSample (wowed);

            // Dolby breathing: a fast envelope and a slower one that
            // don't quite track each other. The mismatch rides the gain
            // up or down a little -- audible as the classic pump on
            // transient-heavy material.
            const float absDry = std::abs (dry);
            c.dolbyFastEnv = absDry + dolbyFastCoeff * (c.dolbyFastEnv - absDry);
            c.dolbySlowEnv = absDry + dolbySlowCoeff * (c.dolbySlowEnv - absDry);
            const float mismatch = c.dolbyFastEnv - c.dolbySlowEnv;
            const float gainRide = juce::jlimit (0.6f, 1.5f, 1.0f + dolbyNorm * mismatch * 3.5f);
            const float companded = filtered * gainRide;

            // Hiss emerges as the signal gets quiet, masked when it's loud.
            c.hissLevelEnv = absDry + hissEnvCoeff * (c.hissLevelEnv - absDry);
            const float maskingFactor = 1.0f - juce::jlimit (0.0f, 1.0f, c.hissLevelEnv * 8.0f);
            const float noiseLevel = hissNorm * profile.noiseBase * maskingFactor;
            const float noise = (c.noiseRandom.nextFloat() * 2.0f - 1.0f) * noiseLevel;

            const float enhanced = companded + noise;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* CassetteEmulationAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (196, 168, 110));
}

void CassetteEmulationAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CassetteEmulationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CassetteEmulationAudioProcessor();
}
