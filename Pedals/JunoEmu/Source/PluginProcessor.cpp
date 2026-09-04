#include "PluginProcessor.h"
#include "JunoUI.h"

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

float parameter (juce::AudioProcessorValueTreeState& state, const char* id, float fallback = 0.0f)
{
    if (auto* value = state.getRawParameterValue (id))
        return value->load();
    return fallback;
}

float envCoeff (float seconds, double sr) noexcept
{
    if (seconds <= 0.0001f)
        return 0.0f;
    return std::exp (-1.0f / (seconds * (float) sr));
}
}

juce::AudioProcessorValueTreeState::ParameterLayout JunoEmuAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    };

    choice ("WAVE", "DCO Wave", { "Saw", "Pulse", "Saw + Pulse" }, 2);
    f ("PULSE", "Pulse Width", 0.05f, 0.95f, 0.50f);
    f ("PWM_RATE", "PWM Rate", 0.05f, 12.0f, 0.55f);
    f ("PWM_DEPTH", "PWM Depth", 0.0f, 1.0f, 0.0f);
    f ("SUB", "Sub Osc", 0.0f, 1.0f, 0.35f);
    choice ("SUB_OCT", "Sub Octave", { "-1 Oct", "-2 Oct" }, 0);
    f ("NOISE", "Noise", 0.0f, 1.0f, 0.04f);
    f ("HPF", "HPF", 0.0f, 1.0f, 0.18f);
    f ("CUTOFF", "VCF Cutoff", 60.0f, 18000.0f, 4200.0f);
    f ("RESONANCE", "VCF Resonance", 0.0f, 1.0f, 0.18f);
    f ("ENV_AMOUNT", "VCF Env", -1.0f, 1.0f, 0.45f);
    f ("ATTACK", "Attack", 0.001f, 2.0f, 0.008f);
    f ("DECAY", "Decay", 0.005f, 3.0f, 0.22f);
    f ("SUSTAIN", "Sustain", 0.0f, 1.0f, 0.72f);
    f ("RELEASE", "Release", 0.01f, 4.0f, 0.35f);
    f ("FILTER_ATTACK", "Filter Attack", 0.001f, 2.0f, 0.01f);
    f ("FILTER_DECAY", "Filter Decay", 0.005f, 3.0f, 0.25f);
    f ("LFO_RATE", "LFO Rate", 0.05f, 12.0f, 4.8f);
    f ("LFO_DEPTH", "Vibrato", 0.0f, 1.0f, 0.0f);
    // Modern controls: these extend the classic architecture without
    // replacing its core Juno-style DCO/VCF/chorus character.
    f ("UNISON", "Modern Unison", 0.0f, 1.0f, 0.0f);
    f ("DETUNE", "Unison Detune", 0.0f, 30.0f, 7.0f);
    f ("DRIFT", "Analog Drift", 0.0f, 1.0f, 0.08f);
    f ("FILTER_DRIVE", "Filter Drive", 0.0f, 1.0f, 0.10f);
    f ("KEYTRACK", "Filter Key Track", 0.0f, 1.0f, 0.55f);
    f ("VEL_FILTER", "Velocity Filter", 0.0f, 1.0f, 0.25f);
    choice ("CHORUS", "Chorus", { "Off", "I", "II" }, 1);
    f ("CHORUS_MIX", "Chorus Mix", 0.0f, 1.0f, 0.38f);
    f ("DELAY_TIME", "Modern Delay Time", 30.0f, 800.0f, 280.0f);
    f ("DELAY_FEEDBACK", "Modern Delay Feedback", 0.0f, 0.82f, 0.18f);
    f ("DELAY_MIX", "Modern Delay Mix", 0.0f, 1.0f, 0.0f);
    f ("REVERB_MIX", "Modern Reverb Mix", 0.0f, 1.0f, 0.0f);
    f ("WIDTH", "Stereo Width", 0.0f, 1.0f, 0.72f);
    f ("DRIVE", "Output Drive", 0.0f, 1.0f, 0.0f);
    f ("LEVEL", "Level", -24.0f, 6.0f, -3.0f);
    return { p.begin(), p.end() };
}

JunoEmuAudioProcessor::JunoEmuAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    synth.setNoteStealingEnabled (true);
    for (int i = 0; i < 6; ++i)
        synth.addVoice (new JunoEmuVoice (*this));
    synth.addSound (new JunoEmuSound());
}

JunoEmuAudioProcessor::~JunoEmuAudioProcessor() = default;

void JunoEmuAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastBlockSize = samplesPerBlock;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getMainBusNumOutputChannels());
    chorus.prepare (spec);
    chorus.setCentreDelay (8.0f);
    chorus.setDepth (0.28f);
    chorus.setFeedback (0.0f);
    chorus.setMix (parameter (apvts, "CHORUS_MIX", 0.38f));
    chorus.setRate (0.85f);
    chorus.reset();

    reverb.reset();
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.55f;
    reverbParams.damping = 0.42f;
    reverbParams.wetLevel = 0.0f;
    reverbParams.dryLevel = 1.0f;
    reverbParams.width = 1.0f;
    reverb.setParameters (reverbParams);

    // A small fixed stereo delay buffer keeps the modern FX lightweight and
    // avoids allocations on the audio thread.
    const int delayCapacity = juce::jmax (1, (int) std::ceil (0.9 * sampleRate));
    delayBuffer.setSize (2, delayCapacity);
    delayBuffer.clear();
    delayWritePosition = 0;
}

void JunoEmuAudioProcessor::releaseResources()
{
    synth.allNotesOff (0, false);
}

bool JunoEmuAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JunoEmuAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif

    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    const int chorusMode = (int) parameter (apvts, "CHORUS", 1.0f);
    if (chorusMode != 0)
    {
        chorus.setRate (chorusMode == 1 ? 0.82f : 1.25f);
        chorus.setDepth (chorusMode == 1 ? 0.24f : 0.34f);
        chorus.setCentreDelay (chorusMode == 1 ? 7.5f : 10.0f);
        chorus.setMix (parameter (apvts, "CHORUS_MIX", 0.38f));
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        chorus.process (context);
    }

    // Optional modern delay. It is deliberately post-chorus, so the classic
    // Juno chorus stays intact and the extra effect can be blended in cleanly.
    const float delayMix = parameter (apvts, "DELAY_MIX", 0.0f);
    if (delayMix > 0.0001f && delayBuffer.getNumSamples() > 1)
    {
        const float delayMs = parameter (apvts, "DELAY_TIME", 280.0f);
        const int delaySamples = juce::jlimit (1, delayBuffer.getNumSamples() - 1,
                                              (int) std::round (delayMs * (float) getSampleRate() * 0.001f));
        const float feedback = parameter (apvts, "DELAY_FEEDBACK", 0.18f);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const int write = delayWritePosition;
            const int read = (write - delaySamples + delayBuffer.getNumSamples())
                           % delayBuffer.getNumSamples();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const int dc = juce::jmin (ch, delayBuffer.getNumChannels() - 1);
                const float input = buffer.getSample (ch, i);
                const float delayed = delayBuffer.getSample (dc, read);
                buffer.setSample (ch, i, input * (1.0f - delayMix) + delayed * delayMix);
                delayBuffer.setSample (dc, write, input + delayed * feedback);
            }

            delayWritePosition = (delayWritePosition + 1) % delayBuffer.getNumSamples();
        }
    }

    const float reverbMix = parameter (apvts, "REVERB_MIX", 0.0f);
    if (reverbMix > 0.0001f)
    {
        auto rp = reverb.getParameters();
        rp.roomSize = 0.40f + reverbMix * 0.52f;
        rp.damping = 0.55f - reverbMix * 0.22f;
        rp.wetLevel = reverbMix * 0.48f;
        rp.dryLevel = 1.0f - reverbMix * 0.22f;
        rp.width = parameter (apvts, "WIDTH", 0.72f);
        reverb.setParameters (rp);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
    }

    // Modern stereo widening is intentionally conservative and mono-safe.
    const float width = parameter (apvts, "WIDTH", 0.72f);
    if (buffer.getNumChannels() > 1 && width < 0.999f)
    {
        const float side = width;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float l = buffer.getSample (0, i);
            const float r = buffer.getSample (1, i);
            const float mid = 0.5f * (l + r);
            const float s = 0.5f * (l - r) * side;
            buffer.setSample (0, i, mid + s);
            buffer.setSample (1, i, mid - s);
        }
    }

    const float drive = parameter (apvts, "DRIVE", 0.0f);
    if (drive > 0.0001f)
    {
        const float amount = 1.0f + drive * 5.0f;
        const float makeup = 1.0f / std::tanh (amount);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, std::tanh (buffer.getSample (ch, i) * amount) * makeup);
    }

    const float gain = juce::Decibels::decibelsToGain (parameter (apvts, "LEVEL", -3.0f));
    buffer.applyGain (gain);
}

void JunoEmuAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary (*xml, destData);
}

void JunoEmuAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr)
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType (apvts.state.getType()))
            apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* JunoEmuAudioProcessor::createEditor()
{
    return new junoui::JunoEmuEditor (*this, apvts);
}

bool JunoEmuVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<JunoEmuSound*> (sound) != nullptr;
}

void JunoEmuVoice::startNote (int midiNoteNumber, float noteVelocity, juce::SynthesiserSound*, int)
{
    note = midiNoteNumber;
    velocity = noteVelocity;
    targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (note);
    currentFreq = targetFreq;
    phase = 0.0f;
    unisonPhaseA = 0.0f;
    unisonPhaseB = 0.0f;
    subPhase = 0.0f;
    lfoPhase = random.nextFloat();
    driftPhase = random.nextFloat();
    driftValue = random.nextFloat() * 2.0f - 1.0f;
    env = 0.0f;
    filterEnv = 0.0f;
    std::fill (std::begin (filterL), std::end (filterL), 0.0f);
    std::fill (std::begin (filterR), std::end (filterR), 0.0f);
    hpStateL = 0.0f;
    hpStateR = 0.0f;
    releasing = false;
    sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    updateEnvelopeCoefficients();
}

void JunoEmuVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
        releasing = true;
    else
    {
        clearCurrentNote();
        env = 0.0f;
        filterEnv = 0.0f;
    }
}

float JunoEmuVoice::polyBlep (float t, float dt) const noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float JunoEmuVoice::oscSaw (float p, float dt) const noexcept
{
    return (2.0f * p - 1.0f) - polyBlep (p, dt);
}

float JunoEmuVoice::oscPulse (float p, float dt, float width) const noexcept
{
    const float second = std::fmod (p + (1.0f - width), 1.0f);
    return (p < width ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (second, dt);
}

float JunoEmuVoice::nextNoise() noexcept
{
    return random.nextFloat() * 2.0f - 1.0f;
}

void JunoEmuVoice::updateEnvelopeCoefficients()
{
    auto& s = processor.apvts;
    envAttack = envCoeff (parameter (s, "ATTACK", 0.008f), sampleRate);
    envDecay = envCoeff (parameter (s, "DECAY", 0.22f), sampleRate);
    envRelease = envCoeff (parameter (s, "RELEASE", 0.35f), sampleRate);
    filterAttack = envCoeff (parameter (s, "FILTER_ATTACK", 0.01f), sampleRate);
    filterDecay = envCoeff (parameter (s, "FILTER_DECAY", 0.25f), sampleRate);
    filterRelease = envCoeff (parameter (s, "RELEASE", 0.35f), sampleRate);
}

void JunoEmuVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    auto& s = processor.apvts;
    const int wave = (int) parameter (s, "WAVE", 2.0f);
    const float pulseBase = parameter (s, "PULSE", 0.50f);
    const float pwmRate = parameter (s, "PWM_RATE", 0.55f);
    const float pwmDepth = parameter (s, "PWM_DEPTH", 0.0f);
    const float subLevel = parameter (s, "SUB", 0.35f);
    const int subOct = (int) parameter (s, "SUB_OCT", 0.0f);
    const float noiseLevel = parameter (s, "NOISE", 0.04f);
    const float hpf = parameter (s, "HPF", 0.18f);
    const float cutoff = parameter (s, "CUTOFF", 4200.0f);
    const float resonance = parameter (s, "RESONANCE", 0.18f);
    const float envAmount = parameter (s, "ENV_AMOUNT", 0.45f);
    const float sustain = parameter (s, "SUSTAIN", 0.72f);
    const float lfoRate = parameter (s, "LFO_RATE", 4.8f);
    const float lfoDepth = parameter (s, "LFO_DEPTH", 0.0f);
    const float unison = parameter (s, "UNISON", 0.0f);
    const float detuneCents = parameter (s, "DETUNE", 7.0f);
    const float drift = parameter (s, "DRIFT", 0.08f);
    const float filterDrive = parameter (s, "FILTER_DRIVE", 0.10f);
    const float keyTrack = parameter (s, "KEYTRACK", 0.55f);
    const float velocityFilter = parameter (s, "VEL_FILTER", 0.25f);
    const float glide = 0.015f;
    const float glideCoeff = std::exp (-1.0f / (glide * (float) sampleRate));
    const float hpFreq = juce::jmap (hpf, 20.0f, 700.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        if (! releasing)
        {
            if (env < 0.999f)
                env = 1.0f - (1.0f - env) * envAttack;
            else
                env = sustain + (env - sustain) * envDecay;

            if (filterEnv < 0.999f)
                filterEnv = 1.0f - (1.0f - filterEnv) * filterAttack;
            else
                filterEnv = sustain + (filterEnv - sustain) * filterDecay;
        }
        else
        {
            env *= envRelease;
            filterEnv *= filterRelease;
            if (env < 0.00005f)
            {
                clearCurrentNote();
                return;
            }
        }

        currentFreq = targetFreq + (currentFreq - targetFreq) * glideCoeff;

        // Very slow per-voice drift adds the small pitch instability associated
        // with analogue instruments without turning the synth into a detuned
        // supersaw. The random walk is intentionally tiny and cheap.
        driftPhase += 0.17f / (float) sampleRate;
        if (driftPhase >= 1.0f)
        {
            driftPhase -= 1.0f;
            driftValue += (random.nextFloat() * 2.0f - 1.0f) * 0.16f;
            driftValue = juce::jlimit (-1.0f, 1.0f, driftValue);
        }

        const float vibrato = std::sin (2.0f * pi * lfoPhase) * lfoDepth * 0.035f;
        const float driftCents = drift * driftValue * 3.0f;
        const float freq = currentFreq * (1.0f + vibrato) * std::pow (2.0f, driftCents / 1200.0f);
        const float detuneRatio = std::pow (2.0f, detuneCents / 1200.0f);
        const float freqA = freq * (1.0f - unison * (detuneRatio - 1.0f));
        const float freqB = freq * (1.0f + unison * (detuneRatio - 1.0f));
        const float dt = juce::jlimit (0.000001f, 0.49f, freq / (float) sampleRate);
        const float dtA = juce::jlimit (0.000001f, 0.49f, freqA / (float) sampleRate);
        const float dtB = juce::jlimit (0.000001f, 0.49f, freqB / (float) sampleRate);
        const float subFreq = freq * (subOct == 0 ? 0.5f : 0.25f);
        const float subDt = juce::jlimit (0.000001f, 0.49f, subFreq / (float) sampleRate);

        const float pwm = juce::jlimit (0.05f, 0.95f,
            pulseBase + std::sin (2.0f * pi * lfoPhase) * pwmDepth * 0.45f);
        const float sawA = oscSaw (unisonPhaseA, dtA);
        const float sawB = oscSaw (unisonPhaseB, dtB);
        const float pulseA = oscPulse (unisonPhaseA, dtA, pwm);
        const float pulseB = oscPulse (unisonPhaseB, dtB, pwm);
        const float saw = oscSaw (phase, dt);
        const float pulse = oscPulse (phase, dt, pwm);
        const float mainDco = wave == 0 ? saw : (wave == 1 ? pulse : 0.5f * (saw + pulse));
        const float unisonDco = wave == 0 ? 0.5f * (sawA + sawB)
                             : (wave == 1 ? 0.5f * (pulseA + pulseB)
                                          : 0.25f * (sawA + sawB + pulseA + pulseB));
        const float dco = mainDco * (1.0f - unison * 0.55f) + unisonDco * (unison * 0.55f);
        const float sub = (subPhase < 0.5f ? 1.0f : -1.0f) * subLevel;
        const float noise = nextNoise() * noiseLevel;
        float x = (dco * 0.62f + sub * 0.30f + noise * 0.16f) * velocity;

        // Juno-style 24 dB/oct low-pass approximation: four cascaded one-pole
        // stages with resonance fed back into the input. The slight saturation
        // before the ladder gives the DCO/VCF path some analogue density.
        const float noteTracking = std::pow (2.0f, ((float) note - 60.0f) / 12.0f * keyTrack);
        const float velocityTracking = 1.0f + (velocity - 0.5f) * velocityFilter * 1.5f;
        const float modCutoff = cutoff * noteTracking * velocityTracking
                              * std::pow (2.0f, envAmount * filterEnv * 2.0f);
        const float fc = juce::jlimit (30.0f, (float) sampleRate * 0.45f, modCutoff);
        const float g = 1.0f - std::exp (-2.0f * pi * fc / (float) sampleRate);
        const float feedback = resonance * 3.55f;
        x = std::tanh (x * (1.0f + resonance * 1.5f + filterDrive * 2.5f));
        const float inputL = x - filterL[3] * feedback;
        const float inputR = x - filterR[3] * feedback;
        for (int stage = 0; stage < 4; ++stage)
        {
            filterL[stage] += g * (stage == 0 ? inputL - filterL[stage] : filterL[stage - 1] - filterL[stage]);
            filterR[stage] += g * (stage == 0 ? inputR - filterR[stage] : filterR[stage - 1] - filterR[stage]);
            filterL[stage] = std::tanh (filterL[stage] * (1.0f + resonance * 0.08f));
            filterR[stage] = std::tanh (filterR[stage] * (1.0f + resonance * 0.08f));
        }

        // Simple high-pass stage before the VCF output, mirroring the Juno's
        // dedicated HPF rather than carving the bass out of the VCF itself.
        const float hpCoeff = std::exp (-2.0f * pi * hpFreq / (float) sampleRate);
        hpStateL = hpCoeff * hpStateL + (1.0f - hpCoeff) * filterL[3];
        const float outL = filterL[3] - hpStateL * hpf * 0.85f;
        hpStateR = hpCoeff * hpStateR + (1.0f - hpCoeff) * filterR[3];
        const float outR = filterR[3] - hpStateR * hpf * 0.85f;

        outputBuffer.addSample (0, startSample + i, outL * env * 0.72f);
        if (outputBuffer.getNumChannels() > 1)
            outputBuffer.addSample (1, startSample + i, outR * env * 0.72f);

        phase += dt;
        unisonPhaseA += dtA;
        unisonPhaseB += dtB;
        subPhase += subDt;
        lfoPhase += lfoRate / (float) sampleRate;
        phase -= std::floor (phase);
        unisonPhaseA -= std::floor (unisonPhaseA);
        unisonPhaseB -= std::floor (unisonPhaseB);
        subPhase -= std::floor (subPhase);
        lfoPhase -= std::floor (lfoPhase);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JunoEmuAudioProcessor();
}
