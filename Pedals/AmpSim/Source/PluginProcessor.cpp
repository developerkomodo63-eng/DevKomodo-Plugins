#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout AmpSimAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GAIN", 1 }, "Gain",
        0.0f, 10.0f, 2.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "VOICE", 1 }, "Amp Voice",
        juce::StringArray { "Clean", "Crunch", "British Lead", "American Lead", "High Gain",
                             "Vintage Tweed", "Modern Metal" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BASS", 1 }, "Bass", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MID", 1 }, "Mid", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TREBLE", 1 }, "Treble", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PRESENCE", 1 }, "Presence", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "CAB", 1 }, "Cabinet",
        juce::StringArray { "Guitar 4x12 (US)", "Guitar 2x12 (UK)", "Guitar 1x12 Open",
                             "Bass 1x15", "Bass 4x10", "Direct / No Cab" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", 0.0f, 10.0f, 6.0f));

    return { params.begin(), params.end() };
}

AmpSimAudioProcessor::AmpSimAudioProcessor()
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

AmpSimAudioProcessor::~AmpSimAudioProcessor()
{
}

void AmpSimAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (25.0f);

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
        c.reset();

    dcBlockerX1.assign ((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign ((size_t) spec.numChannels, 0.0f);

    cabConvolution.prepare (spec);

    if (pendingCabLoadOnRestore != juce::File())
    {
        loadCabIRFile (pendingCabLoadOnRestore);
        pendingCabLoadOnRestore = juce::File();
    }
}

void AmpSimAudioProcessor::releaseResources()
{
}

bool AmpSimAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AmpSimAudioProcessor::loadCabIRFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // los IRs de gabinete son cortos; igual ponemos un techo (2s) de
    // seguridad por si alguien carga por error un IR de sala entero
    const size_t maxSamples = (size_t) (2.0 * currentSampleRate);

    cabConvolution.loadImpulseResponse (file,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        maxSamples,
        juce::dsp::Convolution::Normalise::yes);

    cabIRLoaded = true;
    loadedCabFileName = file.getFileName();
    lastLoadedCabFullPath = file.getFullPathName();
}

void AmpSimAudioProcessor::clearCabIR()
{
    cabIRLoaded = false;
    loadedCabFileName.clear();
    lastLoadedCabFullPath.clear();
}

float AmpSimAudioProcessor::preampStage (float x, float bias) noexcept
{
    const float biased = x + bias;
    return std::tanh (biased) - std::tanh (bias);
}

float AmpSimAudioProcessor::powerAmpStage (float x, float hardness) noexcept
{
    // curva racional (no tanh) con "dureza" ajustable: a mas hardness, se
    // acerca mas rapido a +-1, dandole mas compresion/sag a las voces de
    // mas gain sin cambiar de familia de curva entre voces
    const float k = 1.0f + hardness;
    return (x * k) / (1.0f + hardness * std::abs (x));
}

void AmpSimAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float gainKnob = apvts.getRawParameterValue ("GAIN")->load();
    const float gain      = 1.0f + gainKnob * 4.9f;
    const int voiceChoice  = (int) apvts.getRawParameterValue ("VOICE")->load();
    const float bassKnob  = apvts.getRawParameterValue ("BASS")->load();
    const float midKnob   = apvts.getRawParameterValue ("MID")->load();
    const float trebleKnob= apvts.getRawParameterValue ("TREBLE")->load();
    const float presenceKnob = apvts.getRawParameterValue ("PRESENCE")->load();
    const float bassDb    = juce::jmap (bassKnob, 0.0f, 10.0f, -12.0f, 12.0f);
    const float midDb     = juce::jmap (midKnob, 0.0f, 10.0f, -12.0f, 12.0f);
    const float trebleDb  = juce::jmap (trebleKnob, 0.0f, 10.0f, -12.0f, 12.0f);
    const float presenceDb= juce::jmap (presenceKnob, 0.0f, 10.0f, -6.0f, 6.0f);
    const int cabChoice   = (int) apvts.getRawParameterValue ("CAB")->load();
    const float levelKnob = apvts.getRawParameterValue ("LEVEL")->load();
    const float levelDb   = juce::jmap (levelKnob, 0.0f, 10.0f, -24.0f, 6.0f);
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    // cada voz cambia el bias y la "dureza" de las dos etapas en cascada,
    // y cuanto empuja el pre-gain interno antes de llegar a ellas -- eso
    // alcanza para que las voces suenen bien distintas sin un motor
    // separado por voz. 7 voces de guitarra + 5 de bajo (antes eran 5+3;
    // el modo bajo solo exponia 3 de 5 espacios del combo, dejando 2
    // posiciones "huerfanas" que repetian la ultima voz).
    struct VoiceParams { float bias, hardness, preGain; };
    constexpr VoiceParams voices[12] = {
        { 0.04f, 0.6f, 0.7f },   // Clean
        { 0.12f, 1.2f, 1.3f },   // Crunch
        { 0.20f, 1.8f, 1.8f },   // British Lead
        { 0.10f, 2.2f, 2.0f },   // American Lead
        { 0.26f, 3.0f, 2.6f },   // High Gain
        { 0.08f, 0.85f, 1.0f },  // Vintage Tweed
        { 0.30f, 3.6f, 3.1f },   // Modern Metal
        { 0.025f, 0.55f, 0.55f },// Bass Clean
        { 0.035f, 0.75f, 0.72f },// Bass Vintage
        { 0.045f, 0.95f, 0.90f },// Bass SVT
        { 0.075f, 1.35f, 1.15f },// Bass Modern
        { 0.10f, 1.75f, 1.45f }  // Bass Hi-Gain
    };
    const int mappedVoice = bassMode ? juce::jlimit (7, 11, voiceChoice + 7)
                                      : juce::jlimit (0, 6, voiceChoice);
    const auto& voice = voices[(size_t) mappedVoice];

    struct CabParams { float lowCut, highCut, presenceFreq; };
    constexpr CabParams cabs[6] = {
        { 90.0f,  5000.0f, 3000.0f },  // Guitar 4x12 US
        { 100.0f, 4500.0f, 2500.0f },  // Guitar 2x12 UK
        { 110.0f, 6000.0f, 3500.0f },  // Guitar 1x12 Open
        { 45.0f,  3000.0f, 1200.0f },  // Bass 1x15
        { 55.0f,  3500.0f, 1800.0f },  // Bass 4x10
        { 20.0f,  18000.0f, 8000.0f }  // Direct / No Cab (practicamente sin colorear)
    };
    const int effectiveCabChoice =
        (cabChoice == 5) ? 5
        : bassMode ? (cabChoice < 3 ? cabChoice + 3 : cabChoice)
                   : (cabChoice >= 3 ? cabChoice - 3 : cabChoice);
    const auto& cab = cabs[(size_t) juce::jlimit (0, 5, effectiveCabChoice)];
    hpFilter.setCutoffFrequency (bassMode ? 22.0f : 55.0f);

    // El tone stack usaba las mismas frecuencias centrales para guitarra y
    // bajo. Un bajo de 4/5 cuerdas tiene fundamentales bastante mas graves
    // (hasta ~31Hz en un B bajo) y su contenido de "brillo" util vive mas
    // abajo que el de una guitarra, asi que el shelf de graves, la banda
    // de medios y el shelf de agudos ahora se centran distinto segun el
    // instrumento en vez de compartir un unico ajuste pensado para guitarra.
    const float bassShelfFreq  = bassMode ? 85.0f  : 120.0f;
    const float midFreq        = bassMode ? 500.0f : 700.0f;
    const float trebleShelfFreq= bassMode ? 1800.0f: 3000.0f;

    auto bassCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
        currentSampleRate, bassShelfFreq, 0.7f, juce::Decibels::decibelsToGain (bassDb));
    auto midCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
        currentSampleRate, midFreq, 0.8f, juce::Decibels::decibelsToGain (midDb));
    auto trebleCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        currentSampleRate, trebleShelfFreq, 0.7f, juce::Decibels::decibelsToGain (trebleDb));
    auto cabLowCutCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (
        currentSampleRate, cab.lowCut);
    auto cabHighCutCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (
        currentSampleRate, cab.highCut);
    auto cabPresenceCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
        currentSampleRate, cab.presenceFreq, 0.9f, juce::Decibels::decibelsToGain (presenceDb));

    for (auto& c : channels)
    {
        *c.bass.coefficients       = bassCoeffs;
        *c.mid.coefficients        = midCoeffs;
        *c.treble.coefficients     = trebleCoeffs;
        *c.cabLowCut.coefficients  = cabLowCutCoeffs;
        *c.cabHighCut.coefficients = cabHighCutCoeffs;
        *c.cabPresence.coefficients= cabPresenceCoeffs;
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = hpFilter.processSample (channel, channelData[sample]);
    }

    // Native-rate amp stages keep CPU and latency low. The cab/tone stack
    // after the nonlinear stages also limits excessive high-frequency energy.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float driven = data[sample] * gain * voice.preGain;
            const float stage1 = preampStage (driven, voice.bias);
            data[sample] = powerAmpStage (stage1 * 2.0f, voice.hardness);
        }
    }

    // gabinete: si hay un IR cargado, usamos convolucion real; si no, el
    // filtro algoritmico de siempre
    if (cabIRLoaded)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> cabContext (block);
        cabConvolution.process (cabContext);
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float y = c.bass.processSample (channelData[sample]);
            y = c.mid.processSample (y);
            y = c.treble.processSample (y);

            if (! cabIRLoaded)
            {
                y = c.cabLowCut.processSample (y);
                y = c.cabHighCut.processSample (y);
                y = c.cabPresence.processSample (y);
            }

            const float x0 = y;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            channelData[sample] = y0 * outputGain;
        }
    }
}

juce::AudioProcessorEditor* AmpSimAudioProcessor::createEditor()
{
    return new AmpSimAudioProcessorEditor (*this);
}

void AmpSimAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("cabIRPath", lastLoadedCabFullPath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AmpSimAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xmlState);
            apvts.replaceState (newState);

            const auto path = newState.getProperty ("cabIRPath", juce::String()).toString();
            if (path.isNotEmpty())
                pendingCabLoadOnRestore = juce::File (path);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpSimAudioProcessor();
}
