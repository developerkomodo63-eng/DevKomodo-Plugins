#pragma once
#include "../../Common/InstrumentEnhancerProcessor.h"

class BassEnhancerAudioProcessor final : public InstrumentEnhancerAudioProcessor
{
public:
    BassEnhancerAudioProcessor() : InstrumentEnhancerAudioProcessor (InstrumentEnhancerType::Bass) {}
};
