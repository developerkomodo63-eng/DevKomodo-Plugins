#include "PluginProcessor.h"
#include "DevKomodoUI.h"

#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout ToneSculptorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "STYLE", 1 }, "Style",
        juce::StringArray { "Tube", "Tape", "Console", "Edge" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TONE", 1 }, "Tone", 0.0f, 10.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BODY", 1 }, "Body", -6.0f, 6.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "AIR", 1 }, "Air", -6.0f, 6.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
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
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float drive = apvts.getRawParameterValue ("DRIVE")->load();
    const int style = (int) apvts.getRawParameterValue ("STYLE")->load();
    const float tone = apvts.getRawParameterValue ("TONE")->load();
    const float bodyDb = apvts.getRawParameterValue ("BODY")->load();
    const float airDb = apvts.getRawParameterValue ("AIR")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float levelGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    const float preGain = 1.0f + drive * 0.30f;
    const float driveComp = 1.0f / (1.0f + drive * 0.055f);
    const float toneTilt = (tone - 5.0f) / 5.0f;
    const float lowCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 700.0f / (float) fs);
    const float bodyGain = juce::Decibels::decibelsToGain (bodyDb);
    const float airGain = juce::Decibels::decibelsToGain (airDb);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float lowStateCh = lowState[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            const float pushed = dry * preGain;
            float driven = 0.0f;
            switch (style)
            {
                case 0: // Tube: asymmetric, soft and slightly compressed.
                {
                    const float biased = pushed + 0.08f * pushed * pushed;
                    driven = fast_tanh (biased) * driveComp;
                    break;
                }
                case 1: // Tape: rounded knee with stronger level-dependent compression.
                    driven = (pushed / (1.0f + 0.45f * std::abs (pushed))) * driveComp;
                    break;
                case 2: // Console: deliberately subtle and mostly transparent.
                    driven = (pushed / (1.0f + 0.18f * std::abs (pushed))) * driveComp;
                    break;
                default: // Edge: sharper knee for presence without becoming a distortion plugin.
                    driven = fast_tanh (pushed * 1.35f) * 0.92f * driveComp;
                    break;
            }

            // DC blocker: only Tube (case 0) actually introduces an offset,
            // but running it unconditionally is cheap and harmless for the
            // other styles.
            {
                const float filtered = driven - dcX1[(size_t) ch] + dcR * dcY1[(size_t) ch];
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

            const float out = (dry * (1.0f - mix) + sculpted * mix) * levelGain;
            data[i] = juce::jlimit (-1.2f, 1.2f, out);
        }

        lowState[(size_t) ch] = lowStateCh;
    }
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
