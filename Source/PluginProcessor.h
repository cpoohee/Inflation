/* ******************************************************************************/

#pragma once

#include <JuceHeader.h>

class InflationPluginAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    InflationPluginAudioProcessor();
    ~InflationPluginAudioProcessor() override = default;

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void prepareToPlay (double newSampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    void reset() override;

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override;

    //==============================================================================
    bool hasEditor() const override;

    AudioProcessorEditor* createEditor() override;

    //==============================================================================
    const String getName() const override                             { return "Inflation"; }
    bool acceptsMidi() const override                                 { return false; }
    bool producesMidi() const override                                { return false; }
    double getTailLengthSeconds() const override                      { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                     { return 0; }
    int getCurrentProgram() override                                  { return 0; }
    void setCurrentProgram (int) override                             {}
    const String getProgramName (int) override                        { return "None"; }
    void changeProgramName (int, const String&) override              {}

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;

    void setStateInformation (const void* data, int sizeInBytes) override;

    // Our plug-in's current state
    AudioProcessorValueTreeState state;
    
    std::vector<float> getInputRMSValue();
    std::vector<float> getOutputRMSValue();

private:
    //==============================================================================
    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer,
                  AudioBuffer<FloatType>& dry_buffer,
                  AudioBuffer<FloatType>& low_buffer,
                  AudioBuffer<FloatType>& high_buffer);
    
    template <typename FloatType>
    void applyGain (AudioBuffer<FloatType>& buffer, float gainLevel);
    
    template <typename FloatType>
    void applyWaveShaping(AudioBuffer<FloatType>& buffer, float curve);
    
    template <typename FloatType>
    void applyMixing(AudioBuffer<FloatType>& buffer, AudioBuffer<FloatType>& dryBuffer, float mix);
    
    static BusesProperties getBusesProperties(){
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                .withOutput ("Output", AudioChannelSet::stereo(), true);
    }
    
    std::vector<juce::LinearSmoothedValue<float>> inputRMS, outputRMS;
    void resetMeterValues();
    
    dsp::StateVariableTPTFilter<float> lpf, hpf;
    
    AudioBuffer<float> dryBuffer_float, lowBuffer_float, highBuffer_float;
    AudioBuffer<double> dryBuffer_double, lowBuffer_double, highBuffer_double;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InflationPluginAudioProcessor)
};
