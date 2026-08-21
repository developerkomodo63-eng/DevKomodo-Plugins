#include "PluginEditor.h"

AmpSimAudioProcessorEditor::AmpSimAudioProcessorEditor (AmpSimAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    editor = std::make_unique<DevKomodoUniversalEditor> (p, p.apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 138, 61));

    addAndMakeVisible (loadCabButton);
    addAndMakeVisible (clearCabButton);
    addAndMakeVisible (cabFileLabel);
    addAndMakeVisible (*editor);

    loadCabButton.setLookAndFeel (&controlsLookAndFeel);
    clearCabButton.setLookAndFeel (&controlsLookAndFeel);
    loadCabButton.setTooltip ("Load a cabinet impulse response from a WAV, AIFF, or FLAC file");
    clearCabButton.setTooltip ("Remove the external cabinet IR and return to the built-in cabinet model");

    cabFileLabel.setJustificationType (juce::Justification::centredLeft);
    cabFileLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    cabFileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
    updateCabLabel();

    loadCabButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select a cabinet impulse response",
            juce::File(),
            "*.wav;*.aif;*.aiff;*.flac");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.loadCabIRFile (file);
                updateCabLabel();
            }
        });
    };

    clearCabButton.onClick = [this]
    {
        audioProcessor.clearCabIR();
        updateCabLabel();
    };

    setSize (800, 570);

    lastBassMode = audioProcessor.apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;
    updateVoiceLabels (lastBassMode);
    startTimerHz (10);
}

AmpSimAudioProcessorEditor::~AmpSimAudioProcessorEditor()
{
    stopTimer();
    loadCabButton.setLookAndFeel (nullptr);
    clearCabButton.setLookAndFeel (nullptr);
}

void AmpSimAudioProcessorEditor::timerCallback()
{
    const bool bassMode = audioProcessor.apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;
    if (bassMode != lastBassMode)
    {
        lastBassMode = bassMode;
        updateVoiceLabels (bassMode);
    }
}

void AmpSimAudioProcessorEditor::updateVoiceLabels (bool bassMode)
{
    // The DSP already has 3 dedicated bass amp voices (Bass Clean / Bass SVT
    // / Bass Modern) alongside the 5 guitar voices, but the "Amp Voice"
    // dropdown always showed the guitar names even in Bass mode -- this is
    // what looked like "missing bass amps". Swap the visible labels to match
    // the selected instrument; the underlying VOICE parameter/index doesn't
    // change, so presets and automation stay intact.
    static const juce::StringArray guitarVoices { "Clean", "Crunch", "British Lead", "American Lead", "High Gain",
                                                    "Vintage Tweed", "Modern Metal" };
    static const juce::StringArray bassVoices    { "Bass Clean", "Bass Vintage", "Bass SVT", "Bass Modern", "Bass Hi-Gain" };
    if (editor != nullptr)
        editor->relabelChoiceItems ("VOICE", bassMode ? bassVoices : guitarVoices);
}

void AmpSimAudioProcessorEditor::updateCabLabel()
{
    const auto name = audioProcessor.getLoadedCabName();
    cabFileLabel.setText (name.isEmpty() ? "CAB: built-in preset" : "CAB IR: " + name,
                          juce::dontSendNotification);
}

void AmpSimAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (34).reduced (2, 2);
    loadCabButton.setBounds (top.removeFromLeft (112));
    top.removeFromLeft (6);
    clearCabButton.setBounds (top.removeFromLeft (86));
    top.removeFromLeft (10);
    cabFileLabel.setBounds (top);
    area.removeFromTop (4);
    editor->setBounds (area);
}

void AmpSimAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Without this, the strip above the embedded editor (where the cab IR
    // buttons live) never got painted, so it showed the host's flat default
    // grey instead of matching the rest of the plugin.
    g.fillAll (juce::Colour::fromRGB (9, 10, 13).interpolatedWith (juce::Colour::fromRGB (255, 138, 61), 0.10f));
}
