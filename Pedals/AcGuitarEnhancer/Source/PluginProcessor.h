#pragma once
#include "../../Common/InstrumentEnhancerProcessor.h"

class AcGuitarEnhancerAudioProcessor final : public InstrumentEnhancerAudioProcessor
{
public:
    AcGuitarEnhancerAudioProcessor() : InstrumentEnhancerAudioProcessor (InstrumentEnhancerType::AcousticGuitar) {}
};
