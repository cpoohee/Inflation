#include "PluginProcessor.h"
#include "ProcessEditor.h"


InflationPluginAudioProcessorEditor:: InflationPluginAudioProcessorEditor(InflationPluginAudioProcessor &owner)
    : AudioProcessorEditor    (&owner),
    preGainAttachment       (owner.state, "preGain",  preGainSlider),
    postGainAttachment      (owner.state, "postGain", postGainSlider),
    mixAttachment           (owner.state, "mix", mixSlider),
    curveAttachment         (owner.state, "curve", curveSlider),
    zeroClipButtonAttachment(owner.state, "zeroClip", zeroClipButton),
    bandSplitButtonAttachment(owner.state, "bandSplit", bandSplitButton)
{
    
    // add some components..
    addAndMakeVisible (preGainSlider);
    addAndMakeVisible (postGainSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (curveSlider);
    addAndMakeVisible (zeroClipButton);
    addAndMakeVisible (bandSplitButton);
    addAndMakeVisible (titleLabel);
        
    resetMeters(); // adds meters and make visible
    
    // set label text
    zeroClipButton.setButtonText("0 dB Clip");
    bandSplitButton.setButtonText("Band Split");
    
    // slider init
    preGainSlider.setSliderStyle (Slider::LinearVertical);
    postGainSlider.setSliderStyle (Slider::LinearVertical);
    mixSlider.setSliderStyle (Slider::LinearVertical);
    curveSlider.setSliderStyle (Slider::LinearVertical);

    preGainSlider.setDoubleClickReturnValue(true, 0.0f);
    postGainSlider.setDoubleClickReturnValue(true, 0.0f);
    mixSlider.setDoubleClickReturnValue(true, 100.0f);
    curveSlider.setDoubleClickReturnValue(true, 0.0f);
    
    mixSlider.setNumDecimalPlacesToDisplay(2);
    curveSlider.setNumDecimalPlacesToDisplay(2);
    
    preGainLabel.attachToComponent (&preGainSlider, false);
    postGainLabel.attachToComponent (&postGainSlider, false);
    mixLabel.attachToComponent (&mixSlider, false);
    curveLabel.attachToComponent (&curveSlider, false);
    
    preGainLabel.setFont (Font (sonicLookAndFeel.getFontSize()));
    postGainLabel.setFont (Font (sonicLookAndFeel.getFontSize()));
    mixLabel.setFont (Font (sonicLookAndFeel.getFontSize()));
    curveLabel.setFont (Font (sonicLookAndFeel.getFontSize()));
    titleLabel.setFont(sonicLookAndFeel.getTitleFont());
        
    preGainLabel.setJustificationType(juce::Justification::centred);
    postGainLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setJustificationType(juce::Justification::centred);
    curveLabel.setJustificationType(juce::Justification::centred);
    
    preGainSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, preGainSlider.getTextBoxWidth(), preGainSlider.getTextBoxHeight());
    postGainSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, postGainSlider.getTextBoxWidth(), postGainSlider.getTextBoxHeight());
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, mixSlider.getTextBoxWidth(), mixSlider.getTextBoxHeight());
    curveSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, curveSlider.getTextBoxWidth(), curveSlider.getTextBoxHeight());

    // set resize limits for this plug-in
    setResizeLimits (600, 450, 600, 450);  // prevent resize for now
    setResizable (false, false);
        
//    setResizeLimits (600, 450, 1200, 900);
//    setResizable (true, owner.wrapperType != owner.wrapperType_AudioUnitv3);

    lastUIWidth.referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("width",  nullptr));
    lastUIHeight.referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));

    // set our component's initial size to be the last one that was stored in the filter's settings
    setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
    
    lastUIWidth.addListener (this);
    lastUIHeight.addListener (this);
        
    // set look and feel
    backgroundColour = juce::Colours::whitesmoke;
    setLookAndFeel(&sonicLookAndFeel);

    // start a timer to update level meter
    startTimerHz (24);
}

InflationPluginAudioProcessorEditor::~InflationPluginAudioProcessorEditor()
{
    for (auto levelMeter : inputMeters )
        removeChildComponent(levelMeter);
    
    for (auto levelMeter : outputMeters )
        removeChildComponent(levelMeter);
    
    inputMeters.clear(true); // delete components
    outputMeters.clear(true); // delete components
    
    setLookAndFeel(nullptr);
}

//==============================================================================
void InflationPluginAudioProcessorEditor::paint (Graphics& g)
{
    if(Process::isForegroundProcess())
    {
        g.setColour (backgroundColour);
        g.fillAll();
    }
}

void InflationPluginAudioProcessorEditor::resized()
{
    lastUIWidth  = getWidth();
    lastUIHeight = getHeight();
    
    auto bounds = getLocalBounds();
    
    titleLabel.setBounds(bounds.removeFromTop(sonicLookAndFeel.getTitleFontSize()));
    titleLabel.setJustificationType(Justification::centred);
    
    // add some margin between title and controls
    bounds.removeFromTop(sonicLookAndFeel.getFontSize() * 2);
    
    // left to right controls
    FlexBox controlsFlexbox;
    controlsFlexbox.flexDirection = FlexBox::Direction::row;
    controlsFlexbox.flexWrap = FlexBox::Wrap::noWrap;
    controlsFlexbox.alignContent = FlexBox::AlignContent::stretch;

    int meterMargin = 10;
    int sliderWidth = (bounds.getWidth() + meterMargin * 8) / 8;
    int sliderHeight = bounds.getHeight() * 0.8;

    Array<FlexItem> controlsItemArray;
    controlsItemArray.add(FlexItem(sliderWidth, sliderHeight, preGainSlider));
    for (int i = 0; i < inputMeters.size(); i++)
    {
        auto meterItem = FlexItem(sliderWidth / 4, sliderHeight, *inputMeters[i]).withMargin(meterMargin);
        controlsItemArray.add(meterItem);
    }

    controlsItemArray.add(FlexItem(sliderWidth, sliderHeight, mixSlider));

    // nested flex item
    FlexBox buttonFlexBox;
    buttonFlexBox.flexDirection = FlexBox::Direction::column;
    buttonFlexBox.flexWrap = FlexBox::Wrap::noWrap;
    buttonFlexBox.alignContent = FlexBox::AlignContent::stretch;
    buttonFlexBox.items.add(FlexItem(sliderWidth, sonicLookAndFeel.getFontSize() * 2, bandSplitButton));
    buttonFlexBox.items.add(FlexItem(sliderWidth, sonicLookAndFeel.getFontSize() * 2, zeroClipButton));
    
    controlsItemArray.add(FlexItem(sliderWidth, sliderHeight, buttonFlexBox)
                          .withFlex(1.0f)
                          .withMargin(
                                      FlexItem::Margin(bounds.getHeight() / 2, 0, 0, 0)
                                      )
                          );

    // continue other items
    controlsItemArray.add(FlexItem(sliderWidth, sliderHeight, curveSlider));
    for (int i = 0; i < outputMeters.size(); i++)
    {
        auto meterItem = FlexItem(sliderWidth/4, sliderHeight, *outputMeters[i]).withMargin(meterMargin);
        controlsItemArray.add(meterItem);
    }
    controlsItemArray.add(FlexItem(sliderWidth, sliderHeight, postGainSlider));

    controlsFlexbox.items = controlsItemArray;
    controlsFlexbox.performLayout(bounds);

}

void InflationPluginAudioProcessorEditor::timerCallback()
{
    // update level meter
    std::vector<float> inputRmsValues = getProcessor().getInputRMSValue();
    std::vector<float> outputRmsValues = getProcessor().getOutputRMSValue();
    
//    // TODO: need to improve this when plugin changes it's channel size after initialisation.
//    jassert(inputRmsValues.size() == inputMeters.size());
//    jassert(outputRmsValues.size() == outputMeters.size());
    
    // it means setting only 1 channel when mono, leaving right channel untouched.
    for (int i = 0; i < inputRmsValues.size(); i++)
    {
        inputMeters[i]->setLevel(inputRmsValues[i]);
        if(Process::isForegroundProcess())
            inputMeters[i]->repaint();
    }

    for (int i = 0; i < outputRmsValues.size(); i++)
    {
        outputMeters[i]->setLevel(outputRmsValues[i]);
        if ( Process::isForegroundProcess())
            outputMeters[i]->repaint();
    }
}
    
int InflationPluginAudioProcessorEditor::getControlParameterIndex (Component& control)
{
    if (&control == &preGainSlider)
        return 0;

    if (&control == &postGainSlider)
        return 1;
    
    if (&control == &mixSlider)
        return 2;
    
    if (&control == &curveSlider)
        return 3;

    return -1;
}

void InflationPluginAudioProcessorEditor::resetMeters()
{
//    auto inputChannels =  getProcessor().getTotalNumInputChannels();
//    auto outputChannels =  getProcessor().getTotalNumOutputChannels();
    
    // fix at stereo for now
    auto inputChannels =  2;
    auto outputChannels =  2;
    
    for (auto levelMeter : inputMeters )
        removeChildComponent(levelMeter);
    
    for (auto levelMeter : outputMeters )
        removeChildComponent(levelMeter);
    
    inputMeters.clear(true); // delete components
    outputMeters.clear(true); // delete components
    
    for (int i = 0; i < inputChannels; ++i)
    {
        inputMeters.add(new Gui::LevelMeter(-100.0f, 12.0f)); // allow over 12 db
        addAndMakeVisible(inputMeters[i]);
    }
    for (int i = 0; i < outputChannels; ++i)
    {
        outputMeters.add(new Gui::LevelMeter(-100.0f, 0.0f));
        addAndMakeVisible(outputMeters[i]);
    }
}
