#include "PluginProcessor.h"
#include "ProcessEditor.h"

InflationPluginAudioProcessor::InflationPluginAudioProcessor()
: AudioProcessor (getBusesProperties()),
                state (*this, nullptr, "state",
                       { std::make_unique<AudioParameterFloat> (ParameterID { "preGain",  1 }, "Input",  NormalisableRange<float> (-100.0f, 12.0f, 0.1f, 4.f), 0.0f),  // start, end, interval, skew
                         std::make_unique<AudioParameterFloat> (ParameterID { "postGain", 1 }, "Output", NormalisableRange<float>(-100.0f, 0.0f, 0.1f, 4.f), 0.0f),
                         std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Wet/Dry", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f),
                         std::make_unique<AudioParameterFloat> (ParameterID { "curve", 1 }, "Curve", NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f),
                         std::make_unique<AudioParameterBool>  (ParameterID( "zeroClip", 1), "Zero Clip", true),
                        std::make_unique<AudioParameterBool>  (ParameterID( "bandSplit", 1), "Band Split", false),
                    
                })
{
    // Add a sub-tree to store the state of our UI
    state.state.addChild ({ "uiState", { { "width",  600 }, { "height", 450 } }, {} }, -1, nullptr);

    resetMeterValues();
}

bool InflationPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Only mono/stereo and input/output must have same layout
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    // input and output layout must either be the same or the input must be disabled altogether
    if (! mainInput.isDisabled() && mainInput != mainOutput)
        return false;

    // only allow stereo and mono
    if (mainOutput.size() > 2)
        return false;

    return true;
}

void InflationPluginAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    reset();
    
    // reinitilise meter
    for (auto i = 0; i < inputRMS.size(); ++i)
        inputRMS[i].reset(newSampleRate, 0.5f);
                    
    for (auto i = 0; i < outputRMS.size(); ++i)
        outputRMS[i].reset(newSampleRate, 0.5f);
    
    lpf.reset();
    hpf.reset();
    
    lpf.setType(dsp::StateVariableTPTFilterType::lowpass);
    hpf.setType(dsp::StateVariableTPTFilterType::highpass);
    
    juce::dsp::ProcessSpec spec {
        newSampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (getTotalNumInputChannels())
    };
    lpf.prepare(spec);
    hpf.prepare(spec);
    
    // 12 db per octave
    lpf.setResonance(1.0f / std::sqrt(2));
    hpf.setResonance(1.0f / std::sqrt(2));
    
    lpf.setCutoffFrequency(240.0f);
    hpf.setCutoffFrequency(2400.0f);
    
    if (isUsingDoublePrecision())
    {
        dryBuffer_double.setSize (getTotalNumInputChannels(), samplesPerBlock);
        lowBuffer_double.setSize (getTotalNumInputChannels(), samplesPerBlock);
        highBuffer_double.setSize (getTotalNumInputChannels(), samplesPerBlock);
        dryBuffer_float .setSize (1, 1);
        lowBuffer_float .setSize (1, 1);
        highBuffer_float .setSize (1, 1);
    }
    else
    {
        dryBuffer_float.setSize (getTotalNumInputChannels(), samplesPerBlock);
        lowBuffer_float.setSize (getTotalNumInputChannels(), samplesPerBlock);
        highBuffer_float.setSize (getTotalNumInputChannels(), samplesPerBlock);
        dryBuffer_double.setSize (1, 1);
        lowBuffer_double.setSize (1, 1);
        highBuffer_double.setSize (1, 1);
    }
}

void InflationPluginAudioProcessor::releaseResources()
{
}

void InflationPluginAudioProcessor::reset()
{
    // reset temp buffers
    dryBuffer_float.clear();
    dryBuffer_double.clear();
    
    lowBuffer_float.clear();
    lowBuffer_double.clear();
    
    highBuffer_float.clear();
    highBuffer_double.clear();
    
    // reset meter values
    resetMeterValues();
}

void InflationPluginAudioProcessor::resetMeterValues()
{
    inputRMS.clear();
    outputRMS.clear();
    
    for (auto i = 0; i < getTotalNumInputChannels(); ++i)
    {
        juce::LinearSmoothedValue<float> val;
        val.setTargetValue(-100.0f);
        inputRMS.push_back(val);
    }
                    
    for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
    {
        juce::LinearSmoothedValue<float> val;
        val.setTargetValue(-100.0f);
        outputRMS.push_back(val);
    }
}

//==============================================================================
void InflationPluginAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    jassert (! isUsingDoublePrecision());
    process (buffer, dryBuffer_float, lowBuffer_float, highBuffer_float);
}

void InflationPluginAudioProcessor::processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages)
{
    jassert (isUsingDoublePrecision());
    process (buffer, dryBuffer_double, lowBuffer_double, highBuffer_double);
}

//==============================================================================
bool InflationPluginAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* InflationPluginAudioProcessor::createEditor()
{
    return new InflationPluginAudioProcessorEditor (*this);
}

//==============================================================================
void InflationPluginAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // Store an xml representation of our state.
    if (auto xmlState = state.copyState().createXml())
        copyXmlToBinary (*xmlState, destData);
}

void InflationPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore our plug-in's state from the xml representation stored in the above
    // method.
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        state.replaceState (ValueTree::fromXml (*xmlState));
}

template <typename FloatType>
void InflationPluginAudioProcessor::process (AudioBuffer<FloatType>& buffer,
                                             AudioBuffer<FloatType>& dry_buffer,
                                             AudioBuffer<FloatType>& low_buffer,
                                             AudioBuffer<FloatType>& high_buffer)
{
    // deal with it
    if (buffer.getNumSamples() == 0){
        return; // there is nothing to do
    }
    
    //Returns 0 to 1 values
    auto preGainRawValue  = state.getParameter ("preGain")->getValue();
    auto postGainRawValue = state.getParameter ("postGain")->getValue();
    auto mixRawValue = state.getParameter("mix")->getValue();
    auto curveRawValue = state.getParameter("curve")->getValue();
    auto toClipValue = state.getParameter("zeroClip")->getValue();
    auto toBandSplit = state.getParameter("bandSplit")->getValue();
    
    // convert back to representation
    auto preGainParamValue  = state.getParameter ("preGain")->convertFrom0to1(preGainRawValue);
    auto postGainParamValue  = state.getParameter ("postGain")->convertFrom0to1(postGainRawValue);
    auto mixParamValue  = mixRawValue;
    auto curveParamValue  = state.getParameter ("curve")->convertFrom0to1(curveRawValue);
    bool toClipParamValue = state.getParameter("zeroClip")->convertFrom0to1(toClipValue);
    bool toBandSplitParamValue = state.getParameter("bandSplit")->convertFrom0to1(toBandSplit);
    
    auto numSamples = buffer.getNumSamples();

    // In case we have more outputs than inputs, we'll clear any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    // Apply our gain change to the outgoing data..
    applyGain (buffer, Decibels::decibelsToGain(preGainParamValue));
    
    // calculate input rms for metering
    for (auto i = 0; i < getTotalNumInputChannels(); ++i)
    {
        const auto channelRMS =  static_cast<float>(Decibels::gainToDecibels(buffer.getRMSLevel(i, 0, numSamples)));
        
        inputRMS[i].skip(numSamples);
        
        if (channelRMS < inputRMS[i].getCurrentValue())
        {
            inputRMS[i].setTargetValue(channelRMS); // smooth down
        }
        else
        {
            inputRMS[i].setCurrentAndTargetValue(channelRMS); // immediate set to target
        }
    }
    
    // add dry signal
    for (auto i = 0; i < buffer.getNumChannels(); ++i)
    {
        dry_buffer.copyFrom(i, 0, buffer.getWritePointer(i), buffer.getNumSamples());
    }
    
    // process wet
    if (toBandSplitParamValue)
    {
        // bandspliting
        for (auto i = 0; i < buffer.getNumChannels(); ++i)
        {
            low_buffer.copyFrom(i, 0, buffer.getWritePointer(i), buffer.getNumSamples());
        }
        
        juce::dsp::AudioBlock<FloatType> low_block (low_buffer);
        juce::dsp::ProcessContextReplacing<FloatType> low_context (low_block);

        for (auto i = 0; i < buffer.getNumChannels(); ++i)
        {
            high_buffer.copyFrom(i, 0, buffer.getWritePointer(i), buffer.getNumSamples());
        }
        
        juce::dsp::AudioBlock<FloatType> high_block (high_buffer);
        juce::dsp::ProcessContextReplacing<FloatType> high_context (high_block);

        lpf.process(low_context);
        hpf.process(high_context);
        
        // get mid stuff by cancelling lows and highs
        buffer.applyGain(-1.0f); // invert phase
        for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
        {
            buffer.addFrom(i, 0, low_buffer, i, 0, low_buffer.getNumSamples());
            buffer.addFrom(i, 0, high_buffer, i, 0, high_buffer.getNumSamples());
        }
        buffer.applyGain(-1.0f); // revert phase
        
        // by now, buffer contains mid info
        
        // zero cliping
        if (toClipParamValue)
            for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
            {
                FloatVectorOperations::clip (low_buffer.getWritePointer (i), low_buffer.getWritePointer (i),
                                             -1.0f, 1.0f, numSamples);
                FloatVectorOperations::clip (buffer.getWritePointer (i), buffer.getWritePointer (i),
                                             -1.0f, 1.0f, numSamples);
                FloatVectorOperations::clip (high_buffer.getWritePointer (i), high_buffer.getWritePointer (i),
                                             -1.0f, 1.0f, numSamples);
            }
        
        // apply wave shaping independently
        applyWaveShaping(low_buffer, curveParamValue);
        applyWaveShaping(buffer, curveParamValue);
        applyWaveShaping(high_buffer, curveParamValue);
        
        // sum back
        for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
        {
            buffer.addFrom(i, 0, low_buffer, i, 0, low_buffer.getNumSamples());
            buffer.addFrom(i, 0, high_buffer, i, 0, high_buffer.getNumSamples());
        }
    }
    else
    {
        // no bandsplit
        
        // zero cliping
        if (toClipParamValue)
            for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
               FloatVectorOperations::clip (buffer.getWritePointer (i), buffer.getWritePointer (i),
                                            -1.0f, 1.0f, numSamples);
        
        applyWaveShaping(buffer, curveParamValue);
    }
    
    // add wet
    applyMixing(buffer, dry_buffer, mixParamValue);
        
    // apply output gain
    applyGain (buffer, Decibels::decibelsToGain(postGainParamValue));
    
    // calculate output rms for metering
    for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
    {
        const auto channelRMS =  static_cast<float>(Decibels::gainToDecibels(buffer.getRMSLevel(i, 0, numSamples)));
        
        outputRMS[i].skip(numSamples);
        
        if (channelRMS < outputRMS[i].getCurrentValue())
        {
            outputRMS[i].setTargetValue(channelRMS); // smooth down
        }
        else
        {
            outputRMS[i].setCurrentAndTargetValue(channelRMS); // immediate targt
        }
    }
}

template <typename FloatType>
void InflationPluginAudioProcessor::applyMixing(AudioBuffer<FloatType>& buffer, AudioBuffer<FloatType>& dryBuffer, float mix)
{
    // use linear
    auto dryValue = (1.0f) - mix;
    auto wetValue = mix;

    buffer.applyGain(wetValue);
    dryBuffer.applyGain(dryValue);
    
    for (auto i = 0; i < getTotalNumOutputChannels(); ++i)
        buffer.addFrom(i, 0, dryBuffer, i, 0, buffer.getNumSamples());
}

template <typename FloatType>
void InflationPluginAudioProcessor::applyGain (AudioBuffer<FloatType>& buffer, float gainLevel)
{
    for (auto channel = 0; channel < getTotalNumOutputChannels(); ++channel)
        buffer.applyGain (channel, 0, buffer.getNumSamples(), gainLevel);
}

template <typename FloatType>
void InflationPluginAudioProcessor::applyWaveShaping (AudioBuffer<FloatType>& buffer, float curve)
{
    //    where the coefficients A, B, C and D are given by the Curve parameter:
    //
    //    A(curve) = 1 + (curve + 50)/100
    //    B(curve) = - curve/50
    //    C(curve) = (curve - 50)/100
    //    D(curve) = ¹⁄₁₆ - curve/400 + curve²/(4⋅10⁴)
    auto a = 1.0f + (curve + 50.0f) / 100.0f;
    auto b = -curve / 50.0f;
    auto c = (curve - 50.0f) / 100.0f;
    auto d = 1.0f / 16.0f - curve / 400.0f + curve * curve / (40000.0f);
    
    // f(x) = A⋅x + B⋅x² + C⋅x³ - D⋅(x² - 2⋅x³ + x⁴)
    
    auto numSamples = buffer.getNumSamples();

    for (auto channel = 0; channel < getTotalNumInputChannels(); ++channel)
    {
        auto channelData = buffer.getWritePointer (channel);
      
        for (auto i = 0; i < numSamples; ++i)
        {
            auto x = channelData[i];
            channelData[i] = a * x +
                             b * std::pow(x, 2) +
                             c * std::pow(x, 3) -
                             d * (std::pow(x, 2) - 2 * std::pow(x, 3) + std::pow(x, 4));
        }
    }
}

std::vector<float> InflationPluginAudioProcessor::getInputRMSValue()
{
    std::vector<float> rmsValues;
    for (auto i : inputRMS)
    {
        rmsValues.push_back(i.getCurrentValue());
    }
    return rmsValues;
}

std::vector<float> InflationPluginAudioProcessor::getOutputRMSValue()
{
    std::vector<float> rmsValues;
    for (auto i : outputRMS)
    {
        rmsValues.push_back(i.getCurrentValue());
    }
    return rmsValues;
}
