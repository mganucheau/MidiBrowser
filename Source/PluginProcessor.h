#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
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

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        auto mainOut = layouts.getMainOutputChannelSet();
        if (mainOut.isDisabled()) return true;
        return mainOut == juce::AudioChannelSet::stereo()
            || mainOut == juce::AudioChannelSet::mono();
    }

    double getTailLengthSeconds() const override { return 0.0; }

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── PatternFlow-specific state ───────────────────────────────────────────

    // Comp lanes (the arrangement)
    std::vector<CompLane> lanes;
    juce::CriticalSection laneLock;

    // Arrangement length in bars (default 8)
    std::atomic<int> arrangementBars { 8 };

    // Grid snap
    enum class GridSize { Off, Bar, Beat, HalfBeat, QuarterBeat, Eighth, Sixteenth };
    std::atomic<int> gridSnap { (int)GridSize::Beat };
    double snapBeat(double beat) const;

    // Loop mode
    std::atomic<bool>   loopEnabled  { false };
    std::atomic<double> loopStartBeat{ 0.0 };
    std::atomic<double> loopEndBeat  { 32.0 };

    // Scale / transpose
    std::atomic<int>       scaleRoot    { 0 };
    std::atomic<int>       scaleType    { 0 };
    std::atomic<bool>      scaleEnabled { false };
    std::atomic<int>       rootNoteRemap{ 24 };

    // Humanization parameters (0..1)
    std::atomic<float>     humanTiming  { 0.0f };
    std::atomic<float>     humanVelocity{ 0.0f };
    std::atomic<float>     humanFeel    { 0.0f };
    std::atomic<float>     intonation   { 0.0f };

    // MIDI split rules
    std::vector<MidiSplitRule> splitRules;
    juce::CriticalSection      splitLock;
    std::atomic<bool>          splitEnabled { false };

    // Undo/redo manager
    juce::UndoManager undoManager { 30000, 100 };

    // Transport state from host
    std::atomic<double>    hostBpm      { 120.0 };
    std::atomic<double>    hostBeatPos  { 0.0 };
    std::atomic<bool>      hostPlaying  { false };

    // Loop-wrapped beat position for visual playhead display
    std::atomic<double>    mappedBeatPos { 0.0 };

    // Last file browser directory (persisted across sessions)
    juce::String lastBrowserDir;

    struct TransportInfo { double bpm; double beatPos; bool playing; };
    TransportInfo getTransport() const;

private:
    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;

    struct ActiveNote { int pitch; int channel; };
    std::vector<ActiveNote> activeNotes_;

    void generateMidiForBeatRange(double startBeat, double endBeat,
                                  juce::MidiBuffer& output, int numSamples,
                                  int sampleOffsetBase = 0);

    int applyHumanVelocity(int vel);
    double applyHumanTiming(double beatPos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternFlowProcessor)
};

} // namespace pflow
