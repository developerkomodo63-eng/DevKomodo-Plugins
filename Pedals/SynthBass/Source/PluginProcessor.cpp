#include "PluginProcessor.h"
#include "SynthEditor.h"
#include <array>
#include <memory>
#include <vector>

juce::AudioProcessorValueTreeState::ParameterLayout SynthBassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "WAVEFORM", 1 }, "Waveform",
        juce::StringArray { "Sine", "Saw", "Square", "Triangle", "Pulse", "Soft Saw", "Super Saw", "Organ", "Custom" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "OCTAVE", 1 }, "Octave", -2, 2, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GLIDE", 1 }, "Glide",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.0f, 0.5f }, 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DETUNE", 1 }, "Detune",
        juce::NormalisableRange<float> { 0.0f, 50.0f }, 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TUNING", 1 }, "Reference Tuning",
        juce::NormalisableRange<float> { 432.0f, 448.0f, 0.01f }, 440.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCHCORR", 1 }, "Pitch Correction",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PULSEWIDTH", 1 }, "Pulse Width",
        juce::NormalisableRange<float> { 0.05f, 0.95f, 0.001f }, 0.50f));

    for (const auto* id : { "H2", "H3", "H4", "H5" })
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, id,
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUBLEVEL", 1 }, "Sub Level", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GATE", 1 }, "Gate", -60.0f, -10.0f, -35.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

SynthBassAudioProcessor::SynthBassAudioProcessor()
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

SynthBassAudioProcessor::~SynthBassAudioProcessor()
{
}

void SynthBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    subLevelSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    subLevelBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);

    // rango de tracking para bajo: hasta el Mi grave de 5 cuerdas (~31 Hz),
    // ventana mas grande porque las frecuencias bajas necesitan mas
    // muestras para resolver un periodo completo con precision
    pitchTracker.prepare (sampleRate, 4096, 512, 24.0f, 500.0f);

    oscillatorMain.setSampleRate (sampleRate);
    oscillatorMain.reset();
    oscillatorUnison.setSampleRate (sampleRate);
    oscillatorUnison.reset();
    oscillatorSub.setSampleRate (sampleRate);
    oscillatorSub.reset();

    smoothedFreqValue = 220.0f;
    currentTargetFrequency = 220.0f;
    hasTrackedPitch = false;
    pendingFrequency = 0.0f;
    pendingMidiNote = -1;
    pendingPitchFrames = 0;
    invalidPitchFrames = 0;
    synthGateState = 0.0f;
    synthGateAttackCoeff = std::exp (-1.0f / (0.010f * (float) sampleRate));
    synthGateReleaseCoeff = std::exp (-1.0f / (0.030f * (float) sampleRate));

    // ataque rapido (~5ms), release mas lento (~120ms), como un pedal real
    attackCoeff  = std::exp (-1.0f / (0.005f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.120f * (float) sampleRate));
    envelopeState = 0.0f;
    detectedFrequency.store (0.0f);
    detectedRms.store (0.0f);
}

void SynthBassAudioProcessor::releaseResources()
{
}

bool SynthBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SynthBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const int waveform    = (int) apvts.getRawParameterValue ("WAVEFORM")->load();
    const int octave      = (int) apvts.getRawParameterValue ("OCTAVE")->load();
    const float glideMs   = apvts.getRawParameterValue ("GLIDE")->load();
    const float detuneCts = apvts.getRawParameterValue ("DETUNE")->load();
    const float tuningHz  = apvts.getRawParameterValue ("TUNING")->load();
    const float pitchCorrection = apvts.getRawParameterValue ("PITCHCORR")->load();
    const float pulseWidth = apvts.getRawParameterValue ("PULSEWIDTH")->load();
    const float h2 = apvts.getRawParameterValue ("H2")->load();
    const float h3 = apvts.getRawParameterValue ("H3")->load();
    const float h4 = apvts.getRawParameterValue ("H4")->load();
    const float h5 = apvts.getRawParameterValue ("H5")->load();
    const float subLevel  = apvts.getRawParameterValue ("SUBLEVEL")->load();
    const float gateDb    = apvts.getRawParameterValue ("GATE")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();

    oscillatorMain.setWaveform (waveform);
    oscillatorUnison.setWaveform (waveform);
    // el sub siempre suena mejor en cuadrada/senoidal (mas fundamental,
    // menos armonicos altos peleando con la nota principal)
    oscillatorSub.setWaveform (waveform == 8 ? 0 : 1);
    oscillatorMain.setPulseWidth (pulseWidth);
    oscillatorUnison.setPulseWidth (pulseWidth);
    oscillatorSub.setPulseWidth (pulseWidth);
    oscillatorMain.setCustomHarmonics (h2, h3, h4, h5);
    oscillatorUnison.setCustomHarmonics (h2, h3, h4, h5);
    oscillatorSub.setCustomHarmonics (h2, h3, h4, h5);

    const float detuneRatio = std::pow (2.0f, detuneCts / 1200.0f);

    // coeficiente de suavizado exponencial para el glide; se recalcula si
    // el parametro cambio, sin resetear el valor actual (sin saltos)
    const float glideSeconds = juce::jmax (glideMs, 0.1f) / 1000.0f;
    const float glideCoeff = std::exp (-1.0f / (glideSeconds * (float) getSampleRate()));

    const float gateLevel = juce::Decibels::decibelsToGain (gateDb);
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);
    const float octaveMultiplier = std::pow (2.0f, (float) octave);
    subLevelSmoothed.setTargetValue (subLevel);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (outputGain);
    jassert (numSamples <= (int) subLevelBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        subLevelBuffer[(size_t) sample] = subLevelSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    // trackeamos el pitch a partir del canal 0 (el pedal es mono por naturaleza)
    const float* trackingChannel = buffer.getReadPointer (0);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryInput = trackingChannel[sample];

        pitchTracker.pushSample (dryInput);

        float newFreq, newRms;
        if (pitchTracker.consumeNewPitch (newFreq, newRms))
        {
            detectedRms.store (newRms);
            if (newFreq > 0.0f)
            {
                invalidPitchFrames = 0;
                const int candidateMidi = juce::jlimit (0, 127,
                    (int) std::lround (69.0 + 12.0 * std::log2 (juce::jmax (newFreq, 1.0e-6f) / 440.0)));
                const float noteHz = 440.0f * std::pow (2.0f, (candidateMidi - 69) / 12.0f);
                const float cents = 1200.0f * std::log2 (juce::jmax (newFreq, 1.0e-6f) / juce::jmax (noteHz, 1.0e-6f));
                if (pendingMidiNote == candidateMidi && std::abs (cents) < 90.0f)
                { ++pendingPitchFrames; pendingFrequency = newFreq; }
                else
                { pendingMidiNote = candidateMidi; pendingPitchFrames = 1; pendingFrequency = newFreq; }
                // Same debounce fix as SynthGuitar: 2 hops (~12ms) was fast
                // enough for a pick/finger attack transient to lock in a
                // wrong note before the string was really speaking. One
                // extra hop for a brand-new lock; continuing an already-
                // tracked note stays just as fast as before.
                const int framesNeeded = hasTrackedPitch ? 2 : 3;
                if (pendingPitchFrames >= framesNeeded)
                {
                    detectedFrequency.store (pendingFrequency);

                    // The detector reports the measured string frequency.
                    // For a synth this can feel slightly flat/sharp because
                    // the fundamental of a picked string is not perfectly
                    // stationary. Pull the synth toward the nearest equal-
                    // temperament note using the user's reference pitch.
                    const float midiRelative = 12.0f * std::log2 (
                        juce::jmax (pendingFrequency, 1.0e-6f) / tuningHz);
                    const float nearestSemitone = std::round (midiRelative);
                    const float targetHz = tuningHz * std::pow (
                        2.0f, nearestSemitone / 12.0f);
                    const float correctedFrequency = pendingFrequency
                        + (targetHz - pendingFrequency) * pitchCorrection;
                    currentTargetFrequency = correctedFrequency * octaveMultiplier;
                    if (! hasTrackedPitch)
                    { smoothedFreqValue = currentTargetFrequency; hasTrackedPitch = true; }
                }
            }
            else ++invalidPitchFrames;
        }
        if (invalidPitchFrames >= 3)
            hasTrackedPitch = false;

        // seguidor de envolvente por muestra, para que el ataque/release
        // se sienta suave y no cuantizado al tamaño del hop del pitch tracker
        const float absIn = std::abs (dryInput);
        const float envCoeff = (absIn > envelopeState) ? attackCoeff : releaseCoeff;
        envelopeState = absIn + envCoeff * (envelopeState - absIn);

        const float gateAmount = (envelopeState > gateLevel) ? 1.0f : (envelopeState / juce::jmax (gateLevel, 1.0e-6f));

        smoothedFreqValue = currentTargetFrequency + glideCoeff * (smoothedFreqValue - currentTargetFrequency);

        oscillatorMain.setFrequency (smoothedFreqValue);
        oscillatorUnison.setFrequency (smoothedFreqValue * detuneRatio);
        oscillatorSub.setFrequency (smoothedFreqValue * 0.5f);

        // pesos fijos entre voces: principal siempre presente, unisono al
        // 70% (da ancho sin lavar la afinacion), sub escalado por su propio knob
        const float voicesSum = oscillatorMain.getNextSample()
                               + oscillatorUnison.getNextSample() * 0.7f
                               + oscillatorSub.getNextSample() * subLevelBuffer[(size_t) sample];

        const float targetSynthGate = hasTrackedPitch ? 1.0f : 0.0f;
        const float gateCoeff = targetSynthGate > synthGateState ? synthGateAttackCoeff : synthGateReleaseCoeff;
        synthGateState = targetSynthGate + gateCoeff * (synthGateState - targetSynthGate);
        const float synthSample = voicesSum * envelopeState * gateAmount * synthGateState * 1.6f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            const float smoothedMix = mixBuffer[(size_t) sample];
            channelData[sample] = (dry * (1.0f - smoothedMix) + synthSample * smoothedMix)
                                * outputGainBuffer[(size_t) sample];
        }
    }
}


juce::AudioProcessorEditor* SynthBassAudioProcessor::createEditor()
{
    static constexpr std::array<SynthPedalEditor<SynthBassAudioProcessor>::Preset, 6> presets = {{
        { "Deep Clean", 2, 0, 8.0f, 3.0f, 0.75f, -43.0f, 0.50f, -1.0f, 0.50f },
        { "Sub Bass", 1, -1, 18.0f, 5.0f, 0.95f, -40.0f, 0.72f, -2.0f, 0.50f },
        { "Analog Bass", 0, 0, 24.0f, 10.0f, 0.65f, -38.0f, 0.70f, -1.0f, 0.50f },
        { "Wide Bass", 0, 0, 45.0f, 24.0f, 0.55f, -36.0f, 0.68f, -2.0f, 0.50f },
        { "Octave Growl", 1, -1, 30.0f, 16.0f, 0.85f, -35.0f, 0.80f, -3.0f, 0.50f },
        { "Synth Bass", 0, 1, 12.0f, 12.0f, 0.45f, -37.0f, 0.75f, -2.0f, 0.50f }
    }};
    return new SynthPedalEditor<SynthBassAudioProcessor> (*this, "SYNTH BASS", presets, juce::Colour::fromRGB (95, 220, 170));
}

void SynthBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SynthBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthBassAudioProcessor();
}
