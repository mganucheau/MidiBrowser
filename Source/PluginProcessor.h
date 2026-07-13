#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include "EditModel.h"
#include "GrooveEngine.h"
#include "BrowserPanels.h"
#include "LibraryStore.h"
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
    std::atomic<double> bpmMultiplier { 1.0 };   // synced ÷2 / ×2 playback speed
    /** Freerun loop region in preview beats [start, end). end<=start → full clip. */
    std::atomic<double> previewLoopStartBeat { 0.0 };
    std::atomic<double> previewLoopEndBeat { 0.0 };

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
    // lockAutoTrim also applies empty-edge-bar trim to each browsed clip.
    bool editLock = false;
    bool lockAutoTrim = false;
    ClipEdit lockedEdit;

    // Effects browse-lock: when set, groove params persist across file selection.
    bool effectsLock = false;
    GrooveParams lockedGroove;

    // UI layout state (persisted)
    bool editorOpen = false;
    bool effectsOpen = false;
    bool previewOpen = true;
    bool sidebarCollapsed = true;
    BrowserColumnVisibility columnVisibility;

    juce::String lastBrowserDir;
    bool trimEmptyMeasuresPreview = false;
    juce::StringArray savedBrowserDirs;
    juce::StringArray starredFiles;   // favourites — mirrored from LibraryStore
    std::vector<SavedSearchEntry> savedSearches;

    void addSavedBrowserDir(const juce::String& path);
    void removeSavedBrowserDir(const juce::String& path);
    void addSavedSearch(const SavedSearchEntry& entry);
    void removeSavedSearch(int index);
    bool isStarred(const juce::String& path) const { return starredFiles.contains(path); }
    void toggleStarred(const juce::String& path);

    /** App-owned library (stars, saved searches, search result cache). */
    LibraryStore& library() { return library_; }
    const LibraryStore& library() const { return library_; }

    /** Persist library to disk now (e.g. after search cache update). */
    void saveLibrary();

    void setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed);

    /** Ask the audio thread to release every held note on the next block. */
    void requestNoteFlush()
    {
        juce::ScopedLock sl(previewLock_);
        previewFlushPending_ = true;
    }

private:
    void generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                             juce::MidiBuffer& output, int numSamples,
                             int sampleOffsetBase = 0);

    /** Explicit note-offs for everything currently sounding (audio thread). */
    void flushActiveNotes(juce::MidiBuffer& output, int samplePosition);

    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;
    bool wasSounding_ = false;
    bool wasHostPlaying_ = false;
    bool activeNotes_[16][128] = {};   // audio-thread ledger of held note-ons

    juce::CriticalSection previewLock_;
    MidiClip previewClip_;
    bool previewHasClip_ = false;
    bool previewMuted_ = false;
    bool previewFlushPending_ = false;   // release held notes before the next block
    juce::uint64 previewFingerprint_ = 0;

    LibraryStore library_;
    void syncLibraryFromMemory();
    void applyLibraryToMemory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserProcessor)
};

} // namespace pflow
