#pragma once
#include "../../Common/InstrumentEnhancerProcessor.h"

class KeysEnhancerAudioProcessor final : public InstrumentEnhancerAudioProcessor
{
public:
    KeysEnhancerAudioProcessor() : InstrumentEnhancerAudioProcessor (InstrumentEnhancerType::Keys) {}
};
