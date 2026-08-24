#pragma once
#include "../../Common/InstrumentEnhancerProcessor.h"

class GuitarEnhancerAudioProcessor final : public InstrumentEnhancerAudioProcessor
{
public:
    GuitarEnhancerAudioProcessor() : InstrumentEnhancerAudioProcessor (InstrumentEnhancerType::Guitar) {}
};
