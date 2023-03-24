#include <JuceHeader.h>
#include "PluginProcessor.h"


//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InflationPluginAudioProcessor();
}
