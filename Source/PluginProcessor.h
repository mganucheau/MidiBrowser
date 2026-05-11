#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include <atomic>

namespace pflow {

/** Lightweight MIDI-file browser: session-synced preview only (no arrangement). */
class MidiBrowserProcessor : public juce::AudioProcessor
{
public:
    MidiBrowserProcessor();
    ~MidiBrowserProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isMidiEffect() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }

    const juce::String getName() const override { return "Midi Browser"; }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

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

    // ── Host transport (audio thread writes; UI may read) ────────────────────
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostBeatPos { 0.0 };
    std::atomic<bool> hostPlaying { false };

    /** Host loop cycle for preview phase (when supported). */
    std::atomic<bool> hostLoopActive { false };
    std::atomic<double> hostLoopPpqStart { 0.0 };
    std::atomic<double> hostLoopPpqEnd { 16.0 };

    /** When the host does not report a loop, wrap the playhead over this many bars. */
    std::atomic<int> syncSessionBars { 4 };

    std::atomic<int> appThemeId { 11 };

    juce::String lastBrowserDir;

    void setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed);

private:
    void generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                             juce::MidiBuffer& output, int numSamples,
                             int sampleOffsetBase = 0);

    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;

    juce::CriticalSection previewLock_;
    MidiClip previewClip_;
    bool previewHasClip_ = false;
    bool previewMuted_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserProcessor)
};

} // namespace pflow
