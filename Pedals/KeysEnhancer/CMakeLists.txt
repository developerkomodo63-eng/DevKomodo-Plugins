juce_add_plugin(KeysEnhancer
    COMPANY_NAME "DevKomodo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Key1
    FORMATS ${DEVKOMODO_PLUGIN_FORMATS}
    PRODUCT_NAME "Keys Enhancer")

target_sources(KeysEnhancer PRIVATE Source/PluginProcessor.cpp ../Common/InstrumentEnhancerProcessor.cpp)
juce_generate_juce_header(KeysEnhancer)
target_compile_definitions(KeysEnhancer PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(KeysEnhancer PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
