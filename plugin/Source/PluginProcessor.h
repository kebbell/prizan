#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"

class PrizanProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::Timer
{
public:
    PrizanProcessor();
    ~PrizanProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Prizan"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    // Editor API (message thread only)
    bool loadSampleFile (const juce::File&, juce::String& error);
    void audition() { pendingAudition = true; }
    float playProgress() const { return progress.load(); }
    prizan::Params currentParams() const;
    void getPoints (std::vector<prizan::EnvPoint>& pts, float& endBend) const;
    void setPoints (const std::vector<prizan::EnvPoint>& pts, float endBend);
    bool renderToFile (const juce::File& dest, juce::String& error);
    juce::String suggestedRenderName() const;

    struct SampleInfo
    {
        juce::String name;
        double duration = 0, sampleRate = 44100;
        int channels = 2;
        std::vector<float> mins, maxs;   // 2048 display bins of the original, before reverse/stretch
    };
    const SampleInfo& getSampleInfo() const { return info; }
    std::atomic<int> sampleVersion { 0 }, pointsVersion { 0 };

private:
    void parameterChanged (const juce::String&, float) override { sourceDirty = true; }
    void timerCallback() override { if (sourceDirty.exchange (false)) rebuildSource(); }
    void rebuildSource();
    void setOriginal (juce::AudioBuffer<float>&& stereo, double sr, const juce::String& name, int channels, const juce::File& file);
    void loadDemo();

    prizan::Engine engine;
    juce::SpinLock engineLock;           // held by processBlock; the message thread takes it to swap the source
    prizan::Source::Ptr current;

    juce::AudioBuffer<float> original;
    double originalRate = 44100.0;
    juce::File sampleFile;
    SampleInfo info;

    mutable juce::SpinLock pointsLock;
    std::vector<prizan::EnvPoint> points;
    float endBend = 0.0f;
    prizan::Params audioParams;          // audio-thread copy, refreshed when the points lock is free

    std::atomic<bool> pendingAudition { false }, sourceDirty { false };
    std::atomic<float> progress { -1.0f };
    juce::AudioFormatManager formats;

    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }
    std::atomic<float> *pAttack, *pDecay, *pRelease, *pFlanger, *pPitch, *pTone, *pWidth,
                       *pMix, *pRate, *pFeedback, *pVolume, *pReverse, *pStretch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizanProcessor)
};
