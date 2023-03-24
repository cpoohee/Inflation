#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DecibelSlider.h"
#include "NumeralSlider.h"
#include "LevelMeter.h"
#include "SonicLookAndFeel.h"

class InflationPluginAudioProcessorEditor  : public AudioProcessorEditor,
                                            private Timer,
                                            private Value::Listener
{
public:
    InflationPluginAudioProcessorEditor (InflationPluginAudioProcessor& owner);
    ~InflationPluginAudioProcessorEditor() override; 

    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    int getControlParameterIndex (Component& control) override;
    void resetMeters();

private:
    Label preGainLabel  { {}, "Input" },
          postGainLabel { {}, "Output" },
          mixLabel  { {}, "Wet/Dry" },
          curveLabel { {}, "Curve" },
          titleLabel { {}, "Inflation" }  ;

    DecibelSlider preGainSlider, postGainSlider;
    NumeralSlider mixSlider, curveSlider;
    AudioProcessorValueTreeState::SliderAttachment preGainAttachment, postGainAttachment,mixAttachment, curveAttachment;
    
    juce::ToggleButton zeroClipButton, bandSplitButton;
    AudioProcessorValueTreeState::ButtonAttachment zeroClipButtonAttachment, bandSplitButtonAttachment;
    
    OwnedArray<Gui::LevelMeter> inputMeters;
    OwnedArray<Gui::LevelMeter> outputMeters;
    
    SonicLookAndFeel sonicLookAndFeel;
    Colour backgroundColour;

    // these are used to persist the UI's size - the values are stored along with the
    // filter's other parameters, and the UI component will update them when it gets
    // resized.
    Value lastUIWidth, lastUIHeight;

    //==============================================================================
    InflationPluginAudioProcessor& getProcessor() const
    {
        return static_cast<InflationPluginAudioProcessor&> (processor);
    }

    // called when the stored window size changes
    void valueChanged (Value&) override
    {
        setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
    }
};
