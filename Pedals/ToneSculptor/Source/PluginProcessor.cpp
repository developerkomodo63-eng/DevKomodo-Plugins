#include "PluginProcessor.h"
#include "DevKomodoUI.h"

#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout ToneSculptorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "STYLE", 1 }, "Style",
        juce::StringArray { "Tube", "Tape", "Console", "Edge", "Diode", "Hard" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIAS", 1 }, "Bias", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANSIENT", 1 }, "Transient", -10.0f, 10.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TONE", 1 }, "Tone", 0.0f, 10.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BODY", 1 }, "Body", -6.0f, 6.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "AIR", 1 }, "Air", -6.0f, 6.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "HQ", 1 }, "HQ", false));
    return { params.begin(), params.end() };
}

ToneSculptorAudioProcessor::ToneSculptorAudioProcessor()
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

ToneSculptorAudioProcessor::~ToneSculptorAudioProcessor() {}

void ToneSculptorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    lowState.assign ((size_t) numChannels, 0.0f);
    dcX1.assign ((size_t) numChannels, 0.0f);
    dcY1.assign ((size_t) numChannels, 0.0f);
    dcR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 20.0f / (float) fs);
    driveSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    driveBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    driveSmoothed.setCurrentAndTargetValue (0.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
}

void ToneSculptorAudioProcessor::releaseResources() {}

bool ToneSculptorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ToneSculptorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    const float drive = apvts.getRawParameterValue ("DRIVE")->load();
    const int style = (int) apvts.getRawParameterValue ("STYLE")->load();
    const float bias = apvts.getRawParameterValue ("BIAS")->load();
    const float transient = apvts.getRawParameterValue ("TRANSIENT")->load();
    const float tone = apvts.getRawParameterValue ("TONE")->load();
    const float bodyDb = apvts.getRawParameterValue ("BODY")->load();
    const float airDb = apvts.getRawParameterValue ("AIR")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const bool hq = apvts.getRawParameterValue ("HQ")->load() > 0.5f;
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float targetLevelGain = juce::Decibels::decibelsToGain (levelDb);
    driveSmoothed.setTargetValue (drive);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (targetLevelGain);
    const int numSamples = buffer.getNumSamples();
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
        if (numSamples > (int) outputGainBuffer.size()) outputGainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveBuffer[(size_t) sample] = driveSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }
    const float processingSampleRate = hq ? (float) fs * 4.0f : (float) fs;
    const float processingDcR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 20.0f / processingSampleRate);

    const float toneTilt = (tone - 5.0f) / 5.0f;
    const float lowCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 700.0f / processingSampleRate);
    const float bodyGain = juce::Decibels::decibelsToGain (bodyDb);
    const float airGain = juce::Decibels::decibelsToGain (airDb);

    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto processingBlock = audioBlock;
    if (hq)
        processingBlock = oversampling.processSamplesUp (processingBlock);
    const int processingSamples = (int) processingBlock.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = processingBlock.getChannelPointer ((size_t) ch);
        float lowStateCh = lowState[(size_t) ch];
        float previousSample = 0.0f;

        for (int i = 0; i < processingSamples; ++i)
        {
            const int sourceSample = juce::jmin (numSamples - 1, i / (hq ? 4 : 1));
            const float smoothedDrive = driveBuffer[(size_t) sourceSample];
            const float preGain = 1.0f + smoothedDrive * 0.30f;
            const float driveComp = 1.0f / (1.0f + smoothedDrive * 0.055f);
            const float smoothedMix = mixBuffer[(size_t) sourceSample];
            const float smoothedLevel = outputGainBuffer[(size_t) sourceSample];
            const float dry = data[i];
            const float pushed = dry * preGain;
            const float biasAmount = juce::jlimit (-1.0f, 1.0f, bias);
            const float asymmetricInput = pushed + biasAmount * 0.18f * (pushed * std::abs (pushed));
            float driven = 0.0f;
            switch (style)
            {
                case 0: // Tube: asymmetric, soft and slightly compressed.
                {
                    const float biased = asymmetricInput + 0.08f * asymmetricInput * asymmetricInput;
                    driven = fast_tanh (biased) * driveComp;
                    break;
                }
                case 1: // Tape: rounded knee with stronger level-dependent compression.
                    driven = (asymmetricInput / (1.0f + 0.45f * std::abs (asymmetricInput))) * driveComp;
                    break;
                case 2: // Console: deliberately subtle and mostly transparent.
                    driven = (asymmetricInput / (1.0f + 0.18f * std::abs (asymmetricInput))) * driveComp;
                    break;
                case 3: // Edge: sharper knee for presence without becoming a distortion plugin.
                    driven = fast_tanh (asymmetricInput * 1.35f) * 0.92f * driveComp;
                    break;
                // Diode and Hard absorbed from the standalone Saturator
                // plugin (now merged into this one) so this single pedal
                // covers the full range from subtle console warmth up to
                // hard clipping.
                case 4: // Diode: exponential rectifier-style asymmetric clip.
                {
                    const float k = 1.6f + smoothedDrive * 0.18f;
                    driven = std::copysign (1.0f - std::exp (-k * std::abs (asymmetricInput)), asymmetricInput) * driveComp;
                    break;
                }
                default: // Hard: threshold clip -- the most aggressive style.
                {
                    const float threshold = juce::jmax (0.30f, 0.95f - smoothedDrive * 0.05f);
                    driven = (juce::jlimit (-threshold, threshold, asymmetricInput) / threshold) * driveComp;
                    break;
                }
            }

            // DC blocker: only Tube (case 0) actually introduces an offset,
            // but running it unconditionally is cheap and harmless for the
            // other styles.
            {
                const float filtered = driven - dcX1[(size_t) ch] + processingDcR * dcY1[(size_t) ch];
                dcX1[(size_t) ch] = driven;
                dcY1[(size_t) ch] = filtered;
                driven = filtered;
            }

            lowStateCh += (driven - lowStateCh) * lowCoeff;
            const float low = lowStateCh;
            const float high = driven - low;

            // Tone is a true spectral tilt; Body/Air provide broad musical
            // shelves without the CPU cost of a full EQ graph.
            float sculpted = driven;
            sculpted += low * (bodyGain - 1.0f);
            sculpted += high * (airGain - 1.0f);
            sculpted += toneTilt * (high * 0.55f - low * 0.20f);

            const float transientEdge = (dry - previousSample) * transientAmount * 0.75f;
            const float processed = sculpted + transientEdge;
            const float out = (dry * (1.0f - smoothedMix) + processed * smoothedMix) * smoothedLevel;
            previousSample = dry;
            data[i] = juce::jlimit (-1.2f, 1.2f, out);
        }

        lowState[(size_t) ch] = lowStateCh;
    }

    if (hq)
        oversampling.processSamplesDown (audioBlock);
}

void ToneSculptorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void ToneSculptorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ToneSculptorAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (247, 168, 79));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ToneSculptorAudioProcessor(); }
