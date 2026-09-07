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

    choice ("WAVE", "DCO Wave", { "Saw", "Pulse", "Saw + Pulse", "Triangle", "Sine", "Wavetable" }, 2);
    f ("PULSE", "Pulse Width", 0.05f, 0.95f, 0.50f);
    f ("PWM_RATE", "PWM Rate", 0.05f, 12.0f, 0.55f);
    f ("PWM_DEPTH", "PWM Depth", 0.0f, 1.0f, 0.0f);
    f ("SUB", "Sub Osc", 0.0f, 1.0f, 0.35f);
    choice ("SUB_OCT", "Sub Octave", { "-1 Oct", "-2 Oct" }, 0);
    f ("NOISE", "Noise", 0.0f, 1.0f, 0.04f);
    f ("WT_POS", "Wavetable Position", 0.0f, 1.0f, 0.0f);
    f ("WT_WARP", "Wavetable Warp", -1.0f, 1.0f, 0.0f);
    f ("WT_LEVEL", "Wavetable Level", 0.0f, 1.0f, 0.0f);
    f ("FM_AMOUNT", "FM Amount", 0.0f, 1.0f, 0.0f);
    f ("HYBRID", "Analog / Modern", 0.0f, 1.0f, 0.0f);
    f ("HPF", "HPF", 0.0f, 1.0f, 0.18f);
    f ("CUTOFF", "VCF Cutoff", 60.0f, 18000.0f, 4200.0f);
    f ("RESONANCE", "VCF Resonance", 0.0f, 1.0f, 0.18f);
    // Serum-style multimode selector: the VCF is a zero-delay-feedback
    // state-variable filter, so LP/HP/BP/Notch all come from the same
    // topology -- "type" just picks which tap(s) to use, and the 24 dB/oct
    // option cascades two lowpass stages for extra slope (Serum's LP2/LP4).
    choice ("FILTER_TYPE", "Filter Type", { "LP 12dB", "LP 24dB", "HP 12dB", "BP 12dB", "Notch 12dB" }, 1);
    f ("ENV_AMOUNT", "VCF Env", -1.0f, 1.0f, 0.45f);
    f ("ATTACK", "Attack", 0.001f, 2.0f, 0.008f);
    f ("DECAY", "Decay", 0.005f, 3.0f, 0.22f);
    f ("SUSTAIN", "Sustain", 0.0f, 1.0f, 0.72f);
    f ("RELEASE", "Release", 0.01f, 4.0f, 0.35f);
    f ("FILTER_ATTACK", "Filter Attack", 0.001f, 2.0f, 0.01f);
    f ("FILTER_DECAY", "Filter Decay", 0.005f, 3.0f, 0.25f);
    // Independent filter-envelope sustain: previously the VCF envelope decayed
    // toward the shared amp SUSTAIN level, so a held note could never let the
    // filter close all the way down without also killing the amplitude --
    // exactly the thing that kills a "pluck" (attack + decay to near-silent
    // cutoff while the note itself keeps sounding). Decoupling it lets a
    // patch have a percussive filter snap with a sustained amp level, or
    // vice versa, the way Serum's separate filter-envelope sustain does.
    f ("FILTER_SUSTAIN", "Filter Sustain", 0.0f, 1.0f, 0.72f);
    f ("LFO_RATE", "LFO Rate", 0.05f, 12.0f, 4.8f);
    f ("LFO_DEPTH", "Vibrato", 0.0f, 1.0f, 0.0f);
    // LFO routed to cutoff (in octaves) -- Serum's mod matrix lets any LFO
    // hit the filter for wobble/talk effects; this gives the same result
    // without a full mod-matrix, as a dedicated depth knob.
    f ("LFO_FILTER", "LFO to Cutoff", 0.0f, 1.0f, 0.0f);
    f ("LFO2_RATE", "LFO 2 Rate", 0.05f, 20.0f, 1.2f);
    f ("LFO2_DEPTH", "LFO 2 Depth", 0.0f, 1.0f, 0.0f);
    f ("LFO2_PITCH", "LFO 2 Pitch", 0.0f, 1.0f, 0.0f);
    f ("MODENV_ATTACK", "Mod Env Attack", 0.001f, 2.0f, 0.02f);
    f ("MODENV_DECAY", "Mod Env Decay", 0.005f, 4.0f, 0.35f);
    f ("MODENV_AMOUNT", "Mod Env Amount", -1.0f, 1.0f, 0.0f);
    // Second oscillator: an independently tuned voice layered on top of the
    // classic DCO. Defaults to silent (LEVEL 0) so existing patches and the
    // classic single-DCO character are unaffected until it's dialed in.
    choice ("OSC2_WAVE", "Osc 2 Wave", { "Saw", "Pulse", "Triangle", "Sine", "Wavetable" }, 0);
    f ("OSC2_SEMI", "Osc 2 Semitone", -24.0f, 24.0f, 0.0f);
    f ("OSC2_FINE", "Osc 2 Fine", -50.0f, 50.0f, 0.0f);
    f ("OSC2_LEVEL", "Osc 2 Level", 0.0f, 1.0f, 0.0f);
    f ("OSC2_WT_POS", "Osc 2 WT Position", 0.0f, 1.0f, 0.0f);
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
    f ("GLIDE", "Glide", 0.0f, 0.5f, 0.015f);
    f ("VEL_VCA", "Velocity VCA", 0.0f, 1.0f, 0.0f);
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
    phase2 = random.nextFloat();
    pwmPhase = 0.0f;
    unisonPhaseA = 0.0f;
    unisonPhaseB = 0.0f;
    subPhase = 0.0f;
    lfoPhase = random.nextFloat();
    lfo2Phase = random.nextFloat();
    modEnv = 0.0f;
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

float JunoEmuVoice::oscTriangle (float p) const noexcept
{
    // Naive (non band-limited) triangle: cheap, and its low harmonic content
    // means aliasing is a non-issue in practice at musical pitches.
    return 4.0f * std::abs (p - 0.5f) - 1.0f;
}

float JunoEmuVoice::oscSine (float p) const noexcept
{
    return std::sin (2.0f * pi * p);
}

float JunoEmuVoice::oscWavetable (float p, float position, float warp) const noexcept
{
    // Small built-in wavetable: no external assets, no allocations, and only
    // a handful of cheap analytic shapes. Position morphs continuously while
    // warp bends the phase before the shape lookup, giving a modern synth
    // character without turning the instrument into a CPU-heavy sampler.
    float phaseWarp = p;
    const float amount = juce::jlimit (-0.95f, 0.95f, warp);
    phaseWarp = phaseWarp + amount * std::sin (2.0f * pi * phaseWarp) * 0.25f;
    phaseWarp -= std::floor (phaseWarp);

    const float pos = juce::jlimit (0.0f, 1.0f, position) * 7.0f;
    const int a = juce::jlimit (0, 7, (int) std::floor (pos));
    const int b = juce::jmin (7, a + 1);
    const float mix = pos - (float) a;
    auto shape = [phaseWarp] (int index) noexcept
    {
        switch (index)
        {
            case 0: return std::sin (2.0f * pi * phaseWarp);
            case 1: return 4.0f * std::abs (phaseWarp - 0.5f) - 1.0f;
            case 2: return 2.0f * phaseWarp - 1.0f;
            case 3: return phaseWarp < 0.5f ? 1.0f : -1.0f;
            case 4: return 0.72f * std::sin (2.0f * pi * phaseWarp)
                         + 0.28f * std::sin (4.0f * pi * phaseWarp);
            case 5: return 0.58f * std::sin (2.0f * pi * phaseWarp)
                         + 0.30f * std::sin (6.0f * pi * phaseWarp)
                         + 0.12f * std::sin (10.0f * pi * phaseWarp);
            case 6: return std::sin (2.0f * pi * phaseWarp)
                         * (0.65f + 0.35f * std::sin (2.0f * pi * phaseWarp));
            default: return 0.65f * (2.0f * phaseWarp - 1.0f)
                         + 0.35f * std::sin (6.0f * pi * phaseWarp);
        }
    };
    return shape (a) + (shape (b) - shape (a)) * mix;
}

float JunoEmuVoice::oscForWave (int waveType, float p, float dt, float width) const noexcept
{
    switch (waveType)
    {
        case 0:  return oscSaw (p, dt);
        case 1:  return oscPulse (p, dt, width);
        case 2:  return oscTriangle (p);
        case 3:  return oscSine (p);
        default: return oscSaw (p, dt);
    }
}

float JunoEmuVoice::nextNoise() noexcept
{
    return random.nextFloat() * 2.0f - 1.0f;
}

JunoEmuVoice::SvfOutputs JunoEmuVoice::processSvf (float* state, float input, float g, float k) const noexcept
{
    // Cytomic/Andrew Simper "topology-preserving-transform" state-variable
    // filter: two integrator state variables (state[0] = ic1eq, state[1] =
    // ic2eq) yield LP/BP/HP/Notch simultaneously with no unit delay in the
    // feedback path, so it stays stable even as resonance nears
    // self-oscillation.
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = input - state[1];
    const float v1 = a1 * state[0] + a2 * v3;
    const float v2 = state[1] + a2 * state[0] + a3 * v3;
    state[0] = 2.0f * v1 - state[0];
    state[1] = 2.0f * v2 - state[1];

    // Soft-saturate the integrator states themselves -- the way a real VCF's
    // op-amps would start clipping internally as resonance drives their
    // swing higher. tanh(x*0.8)*1.25 sits at ~unity gain for small signals
    // (so low-resonance patches are unaffected) and rounds off hard once the
    // state grows near self-oscillation, so the resonant peak "sings"
    // instead of ringing with harsh, perfectly-linear digital precision --
    // this in-loop character, not just a clean pre-filter drive, is a big
    // part of why Serum's filters feel more alive than a textbook SVF.
    state[0] = std::tanh (state[0] * 0.8f) * 1.25f;
    state[1] = std::tanh (state[1] * 0.8f) * 1.25f;

    SvfOutputs out;
    out.lp = v2;
    out.bp = v1;
    out.hp = input - k * v1 - v2;
    out.notch = input - k * v1;
    return out;
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
    modEnvAttack = envCoeff (parameter (s, "MODENV_ATTACK", 0.02f), sampleRate);
    modEnvDecay = envCoeff (parameter (s, "MODENV_DECAY", 0.35f), sampleRate);
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
    const float wtPos = parameter (s, "WT_POS", 0.0f);
    const float wtWarp = parameter (s, "WT_WARP", 0.0f);
    const float wtLevel = parameter (s, "WT_LEVEL", 0.0f);
    const float fmAmount = parameter (s, "FM_AMOUNT", 0.0f);
    const float hybrid = parameter (s, "HYBRID", 0.0f);
    const float hpf = parameter (s, "HPF", 0.18f);
    const float cutoff = parameter (s, "CUTOFF", 4200.0f);
    const float resonance = parameter (s, "RESONANCE", 0.18f);
    const float envAmount = parameter (s, "ENV_AMOUNT", 0.45f);
    const float sustain = parameter (s, "SUSTAIN", 0.72f);
    const float filterSustain = parameter (s, "FILTER_SUSTAIN", 0.72f);
    const float lfoRate = parameter (s, "LFO_RATE", 4.8f);
    const float lfoDepth = parameter (s, "LFO_DEPTH", 0.0f);
    const float lfoFilterDepth = parameter (s, "LFO_FILTER", 0.0f);
    const float lfo2Rate = parameter (s, "LFO2_RATE", 1.2f);
    const float lfo2Depth = parameter (s, "LFO2_DEPTH", 0.0f);
    const float lfo2Pitch = parameter (s, "LFO2_PITCH", 0.0f);
    const float modEnvAmount = parameter (s, "MODENV_AMOUNT", 0.0f);
    const float unison = parameter (s, "UNISON", 0.0f);
    const float detuneCents = parameter (s, "DETUNE", 7.0f);
    const float drift = parameter (s, "DRIFT", 0.08f);
    const float filterDrive = parameter (s, "FILTER_DRIVE", 0.10f);
    const float keyTrack = parameter (s, "KEYTRACK", 0.55f);
    const float velocityFilter = parameter (s, "VEL_FILTER", 0.25f);
    const int osc2Wave = (int) parameter (s, "OSC2_WAVE", 0.0f);
    const float osc2Semi = parameter (s, "OSC2_SEMI", 0.0f);
    const float osc2Fine = parameter (s, "OSC2_FINE", 0.0f);
    const float osc2Level = parameter (s, "OSC2_LEVEL", 0.0f);
    const float osc2WtPos = parameter (s, "OSC2_WT_POS", 0.0f);
    const float glide = parameter (s, "GLIDE", 0.015f);
    const float velocityVca = parameter (s, "VEL_VCA", 0.0f);
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
                filterEnv = filterSustain + (filterEnv - filterSustain) * filterDecay;

            if (modEnv < 0.999f)
                modEnv = 1.0f - (1.0f - modEnv) * modEnvAttack;
            else
                modEnv *= modEnvDecay;
        }
        else
        {
            env *= envRelease;
            filterEnv *= filterRelease;
            modEnv *= filterRelease;
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
        const float modernLfoPitch = std::sin (2.0f * pi * lfo2Phase) * lfo2Depth * lfo2Pitch * 0.08f;
        const float modPitch = std::pow (2.0f, modEnv * modEnvAmount * 0.08f);
        const float driftCents = drift * driftValue * 3.0f;
        const float freq = currentFreq * (1.0f + vibrato + modernLfoPitch)
                          * modPitch * std::pow (2.0f, driftCents / 1200.0f);
        const float detuneRatio = std::pow (2.0f, detuneCents / 1200.0f);
        const float freqA = freq * (1.0f - unison * (detuneRatio - 1.0f));
        const float freqB = freq * (1.0f + unison * (detuneRatio - 1.0f));
        const float dt = juce::jlimit (0.000001f, 0.49f, freq / (float) sampleRate);
        const float dtA = juce::jlimit (0.000001f, 0.49f, freqA / (float) sampleRate);
        const float dtB = juce::jlimit (0.000001f, 0.49f, freqB / (float) sampleRate);
        const float subFreq = freq * (subOct == 0 ? 0.5f : 0.25f);
        const float subDt = juce::jlimit (0.000001f, 0.49f, subFreq / (float) sampleRate);
        const float osc2Ratio = std::pow (2.0f, (osc2Semi + osc2Fine * 0.01f) / 12.0f);
        const float freq2 = freq * osc2Ratio;
        const float dt2 = juce::jlimit (0.000001f, 0.49f, freq2 / (float) sampleRate);

        const float pwm = juce::jlimit (0.05f, 0.95f,
            pulseBase + std::sin (2.0f * pi * pwmPhase) * pwmDepth * 0.45f);
        const float sawA = oscSaw (unisonPhaseA, dtA);
        const float sawB = oscSaw (unisonPhaseB, dtB);
        const float pulseA = oscPulse (unisonPhaseA, dtA, pwm);
        const float pulseB = oscPulse (unisonPhaseB, dtB, pwm);
        const float saw = oscSaw (phase, dt);
        const float pulse = oscPulse (phase, dt, pwm);
        float mainDco, unisonDco;
        if (wave == 2)
        {
            // "Saw + Pulse" keeps its own blended character rather than
            // being folded into the generic per-wave path below.
            mainDco = 0.5f * (saw + pulse);
            unisonDco = 0.25f * (sawA + sawB + pulseA + pulseB);
        }
        else
        {
            const int shapedWave = wave < 2 ? wave : wave - 1; // remap 3,4 -> 2,3 (Triangle/Sine)
            mainDco = oscForWave (shapedWave, phase, dt, pwm);
            unisonDco = 0.5f * (oscForWave (shapedWave, unisonPhaseA, dtA, pwm)
                               + oscForWave (shapedWave, unisonPhaseB, dtB, pwm));
        }
        const float dco = mainDco * (1.0f - unison * 0.55f) + unisonDco * (unison * 0.55f);
        const float sub = (subPhase < 0.5f ? 1.0f : -1.0f) * subLevel;
        const float noise = nextNoise() * noiseLevel;
        const float wavetable = oscWavetable (phase, wtPos, wtWarp);
        const float fmCarrier = std::sin (2.0f * pi * phase + oscWavetable (phase2, wtPos, wtWarp) * fmAmount * 1.6f);
        const float modernBlend = juce::jlimit (0.0f, 1.0f, hybrid);
        const float modernOsc = wavetable * wtLevel + fmCarrier * fmAmount * 0.22f;
        const float mainOsc = (wave == 5)
                            ? dco * (1.0f - modernBlend) + modernOsc * modernBlend
                            : dco * (1.0f - modernBlend * 0.35f) + modernOsc * 0.55f;
        const float osc2 = (osc2Wave == 4)
                         ? oscWavetable (phase2, osc2WtPos, wtWarp)
                         : oscForWave (osc2Wave, phase2, dt2, pwm);
        float x = (mainOsc
                 + sub * 0.30f + noise * 0.16f + osc2 * osc2Level * 0.55f) * velocity;

        // Serum-style VCF: cutoff modulation is computed in semitone/octave
        // space (note tracking, velocity and the filter envelope all stack
        // as exponential multipliers on the base cutoff) exactly as before,
        // but it now drives a zero-delay-feedback state-variable filter
        // instead of the old saturating ladder -- so sweeps stay precise
        // and resonance can push all the way to a clean self-oscillation
        // rather than softening into ladder-style compression.
        const int filterType = (int) parameter (s, "FILTER_TYPE", 1.0f);
        const float noteTracking = std::pow (2.0f, ((float) note - 60.0f) / 12.0f * keyTrack);
        const float velocityTracking = 1.0f + (velocity - 0.5f) * velocityFilter * 1.5f;
        // LFO -> cutoff (Serum-style filter wobble): up to +/-2 octaves at
        // full LFO_FILTER depth, on the same LFO phase that drives vibrato,
        // so one LFO section modulates both pitch and filter as in Serum's
        // "drag an LFO onto the cutoff knob" workflow.
        const float lfoToCutoffOct = std::sin (2.0f * pi * lfoPhase) * lfoFilterDepth * 2.0f
                                  + std::sin (2.0f * pi * lfo2Phase) * lfo2Depth * 1.5f;
        const float modCutoff = cutoff * noteTracking * velocityTracking
                              * std::pow (2.0f, envAmount * filterEnv * 2.0f + lfoToCutoffOct);
        const float fc = juce::jlimit (30.0f, (float) sampleRate * 0.45f, modCutoff);
        // k = 1/Q: near 2 is barely resonant, near 0 rings and self-oscillates,
        // matching the aggressive top-end resonance behaviour Serum's filters
        // are known for.
        const float k = juce::jmap (juce::jlimit (0.0f, 1.0f, resonance), 0.0f, 1.0f, 1.85f, 0.04f);

        // Single drive stage ahead of the filter (rather than saturating every
        // ladder stage) keeps the input clean/modern and puts all the grit
        // under one clearly-labelled DRIVE control.
        x = std::tanh (x * (1.0f + filterDrive * 3.0f));

        // 2x-oversample the resonant filter itself. Serum's filters stay
        // smooth right up through self-oscillation because their nonlinear
        // stages effectively run above audio rate; ticking the ZDF-SVF twice
        // per output sample (at half the cutoff coefficient) approximates
        // that and cleans up the aliasing that the new in-loop saturation
        // above would otherwise add, especially at high resonance.
        const float gOS = std::tan (pi * fc / (2.0f * (float) sampleRate));

        SvfOutputs stage1L = processSvf (&filterL[0], x, gOS, k);
        SvfOutputs stage1R = processSvf (&filterR[0], x, gOS, k);

        float filteredL, filteredR;
        switch (filterType)
        {
            case 0: // LP 12 dB
                filteredL = stage1L.lp; filteredR = stage1R.lp;
                break;
            case 1: // LP 24 dB: cascade a second lowpass stage (Serum's LP4)
            {
                SvfOutputs stage2L = processSvf (&filterL[2], stage1L.lp, gOS, k);
                SvfOutputs stage2R = processSvf (&filterR[2], stage1R.lp, gOS, k);
                filteredL = stage2L.lp; filteredR = stage2R.lp;
                break;
            }
            case 2: // HP 12 dB
                filteredL = stage1L.hp; filteredR = stage1R.hp;
                break;
            case 3: // BP 12 dB
                filteredL = stage1L.bp; filteredR = stage1R.bp;
                break;
            default: // Notch 12 dB
                filteredL = stage1L.notch; filteredR = stage1R.notch;
                break;
        }

        // Simple high-pass stage after the VCF output, mirroring the Juno's
        // dedicated HPF rather than carving the bass out of the VCF itself.
        const float hpCoeff = std::exp (-2.0f * pi * hpFreq / (float) sampleRate);
        hpStateL = hpCoeff * hpStateL + (1.0f - hpCoeff) * filteredL;
        const float outL = filteredL - hpStateL * hpf * 0.85f;
        hpStateR = hpCoeff * hpStateR + (1.0f - hpCoeff) * filteredR;
        const float outR = filteredR - hpStateR * hpf * 0.85f;

        const float vcaVelocity = (1.0f - velocityVca) + velocityVca * velocity;
        outputBuffer.addSample (0, startSample + i, outL * env * vcaVelocity * 0.72f);
        if (outputBuffer.getNumChannels() > 1)
            outputBuffer.addSample (1, startSample + i, outR * env * vcaVelocity * 0.72f);

        phase += dt;
        phase2 += dt2;
        pwmPhase += pwmRate / (float) sampleRate;
        unisonPhaseA += dtA;
        unisonPhaseB += dtB;
        subPhase += subDt;
        lfoPhase += lfoRate / (float) sampleRate;
        lfo2Phase += lfo2Rate / (float) sampleRate;
        phase -= std::floor (phase);
        phase2 -= std::floor (phase2);
        pwmPhase -= std::floor (pwmPhase);
        unisonPhaseA -= std::floor (unisonPhaseA);
        unisonPhaseB -= std::floor (unisonPhaseB);
        subPhase -= std::floor (subPhase);
        lfoPhase -= std::floor (lfoPhase);
        lfo2Phase -= std::floor (lfo2Phase);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JunoEmuAudioProcessor();
}
