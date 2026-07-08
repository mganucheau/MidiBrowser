#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include "EditModel.h"
#include "GrooveEngine.h"
#include <atomic>
#include <map>

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
        const auto mainOut = layouts.getMainOutputChannelSet();
        if (mainOut.isDisabled()) return false;
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

    // ── Preview transport ────────────────────────────────────────────────────
    // Synced: preview follows the host transport. Free-run: an internal clock
    // at freeBpm loops the armed clip; freerunBeat is the loop-local playhead.
    // Armed by default so a synced plugin sounds as soon as the host plays;
    // the plugin's play/stop buttons disarm it.
    std::atomic<bool> syncToHost { true };
    std::atomic<bool> previewArmed { true };
    std::atomic<double> freeBpm { 124.0 };
    std::atomic<double> freerunBeat { 0.0 };

    /** True when the preview is audibly playing right now. */
    bool isPreviewSounding() const
    {
        if (!previewArmed.load()) return false;
        return syncToHost.load() ? hostPlaying.load() : true;
    }

    // ── Per-clip non-destructive state (message thread; persisted) ──────────
    ClipEdit& editFor(const juce::String& filePath) { return clipEdits[filePath]; }
    GrooveParams& grooveFor(const juce::String& filePath) { return clipGrooves[filePath]; }
    std::map<juce::String, ClipEdit> clipEdits;
    std::map<juce::String, GrooveParams> clipGrooves;

    // Browse-lock: when set, the locked pitch edits (octave / fit / map /
    // root / mode) are stamped onto every clip selected while browsing.
    bool editLock = false;
    ClipEdit lockedEdit;

    // UI layout state (persisted)
    bool editorOpen = true;
    bool sidebarCollapsed = true;
    bool miniOpen = true;

    juce::String lastBrowserDir;
    bool trimEmptyMeasuresPreview = false;
    juce::StringArray savedBrowserDirs;

    void addSavedBrowserDir(const juce::String& path);
    void removeSavedBrowserDir(const juce::String& path);

    void setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed);

private:
    void generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                             juce::MidiBuffer& output, int numSamples,
                             int sampleOffsetBase = 0);

    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;
    bool wasSounding_ = false;

    juce::CriticalSection previewLock_;
    MidiClip previewClip_;
    bool previewHasClip_ = false;
    bool previewMuted_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserProcessor)
};

} // namespace pflow
