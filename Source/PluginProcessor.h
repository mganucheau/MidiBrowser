#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include <atomic>

namespace pflow {

class PatternFlowProcessor : public juce::AudioProcessor
{
public:
    PatternFlowProcessor();
    ~PatternFlowProcessor() override = default;

    // ── AudioProcessor interface ─────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isMidiEffect() const override { return true; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }

    const juce::String getName() const override { return "PatternFlow"; }

    bool   hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    int    getNumPrograms()    override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram(int) override {}
    const  juce::String getProgramName(int) override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}

    double getTailLengthSeconds() const override { return 0.0; }

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── PatternFlow-specific state ───────────────────────────────────────────

    // Comp lanes (the arrangement)
    std::vector<CompLane> lanes;
    juce::CriticalSection laneLock;

    // Scale / transpose
    std::atomic<int>       scaleRoot    { 0 };  // 0=C
    std::atomic<int>       scaleType    { 0 };  // ScaleType enum
    std::atomic<bool>      scaleEnabled { false };
    std::atomic<int>       rootNoteRemap{ 24 }; // default C1

    // Humanization parameters (0..1)
    std::atomic<float>     humanTiming  { 0.0f };
    std::atomic<float>     humanVelocity{ 0.0f };
    std::atomic<float>     humanFeel    { 0.0f };  // swing
    std::atomic<float>     intonation   { 0.0f };

    // MIDI split rules
    std::vector<MidiSplitRule> splitRules;
    std::atomic<bool>          splitEnabled { false };

    // Transport state from host
    std::atomic<double>    hostBpm      { 120.0 };
    std::atomic<double>    hostBeatPos  { 0.0 };
    std::atomic<bool>      hostPlaying  { false };

    // Notify UI about transport
    struct TransportInfo { double bpm; double beatPos; bool playing; };
    TransportInfo getTransport() const;

private:
    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;

    void generateMidiForBeatRange(double startBeat, double endBeat,
                                  juce::MidiBuffer& output, int numSamples);

    int applyHumanVelocity(int vel);
    double applyHumanTiming(double beatPos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternFlowProcessor)
};

} // namespace pflow
