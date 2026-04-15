#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include "MidiFileData.h"
#include "CompingModel.h"
#include <vector>
#include <atomic>

namespace pflow {

class PatternFlowProcessor : public juce::AudioProcessor
{
public:
    /**
     * Automatable DAW parameters + MIDI CC: each index is one **sorted** internal comp edge
     * (combined “slice” boundary, left→right). Value 0–1 = position along the session length.
     * MIDI: map hardware to CC numbers `kCompSliceMidiCcStart` … (+31) on this track.
     */
    static constexpr int kNumCompBoundaryAutomationParams = 32;
    /** First MIDI CC (0–127) that maps to “Comp slice 1”; CC+1 = slice 2, etc. */
    static constexpr int kCompSliceMidiCcStart = 20;

    PatternFlowProcessor();
    ~PatternFlowProcessor() override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

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

    // Lanes (the arrangement)
    std::vector<CompLane> lanes;
    juce::CriticalSection laneLock;

    // Arrangement length in bars (default 4)
    std::atomic<int> arrangementBars { 4 };

    // Grid snap
    enum class GridSize { Off, Bar, Beat, HalfBeat, QuarterBeat, Eighth, Sixteenth, ThirtySecond, EighthTriplet, SixteenthTriplet };
    std::atomic<int> gridSnap { (int)GridSize::Beat };
    double snapBeat(double beat) const;

    // Grid division for drawing (beats per grid line)
    static double getGridDivision(GridSize gs);

    // Lane/region validation (caller must hold laneLock when accessing lanes)
    bool isValidLane(int laneIdx) const;
    bool isValidRegion(int laneIdx, int regionIdx) const;
    bool isValidClipIndex(int laneIdx, int clipIdx) const;

    // Take comping (single comp, mutually exclusive segments in time)
    std::vector<TakeComp> takeComps;
    int                    activeTakeCompIndex = 0; // always 0 (single-comp mode)
    std::atomic<bool>    compsEnabled { false };
    /** Number of regions for Random comp (2–16). */
    std::atomic<int>     compRandomRegionCount { 8 };

    void ensureDefaultTakeComp();

    /** Snapshot of takeComps[0].segments (empty if no take comp). Thread-safe. */
    std::vector<TakeCompSegment> getActiveTakeCompSegmentsSnapshot() const;
    void applyCompSwipe(int laneIndex, double startBeat, double endBeat);
    /** Fill the active comp with random contiguous regions across the session timeline. */
    void randomizeActiveComp();
    /** Move every comp region to the next lane (down); wraps from the last lane to the first. */
    void cycleCompSegmentsToNextLane();
    /**
     * Move all comp segment edges at `fromBeat` to `toBeat`. Re-finds touching edges each call so
     * indices stay valid after sort/normalize. Returns a beat anchor for the next drag frame
     * (after normalize / min-width, so shrinking continues to track the handle).
     */
    double applyCompBoundaryDrag(double fromBeat, double toBeat);

    /** Remove the comp segment on `laneIndex` whose [start,end) contains `beat`, if any. */
    void deleteCompSegmentCoveringBeat(int laneIndex, double beat);
    void remapLanesAfterDelete(int removedLaneIndex);
    void remapLanesAfterInsert(int insertLaneIndex);
    /** Same as remapLanesAfterDelete but caller must already hold `laneLock` (undo). */
    void remapLanesAfterDeleteLocked(int removedLaneIndex);
    /** Same as remapLanesAfterInsert but caller must already hold `laneLock` (undo). */
    void remapLanesAfterInsertLocked(int insertLaneIndex);

    // App theme (Material Design 3-inspired presets, 0..19)
    std::atomic<int> appThemeId { 11 }; // default: Dark - Sky

    // Loop mode
    std::atomic<bool>   loopEnabled  { false };
    std::atomic<double> loopStartBeat{ 0.0 };
    std::atomic<double> loopEndBeat  { 16.0 }; // default session = 4 bars × 4 beats
    /** When true, moving loop start/end shifts the other by the same delta. */
    std::atomic<bool>   loopSyncMoveTogether { false };

    // Scale / transpose
    std::atomic<int>       scaleRoot    { 0 };
    std::atomic<int>       scaleType    { 0 };
    /** Live transpose toggle (UI label: Transpose). */
    std::atomic<bool>      scaleEnabled { false };
    std::atomic<int>       rootNoteRemap{ 24 };
    /** Global octave shift in semitones applied to output + preview (multiples of 12). */
    std::atomic<int>       octaveShiftSemitones { 0 };

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

    juce::AudioProcessorValueTreeState apvts;

    // Transport state from host
    std::atomic<double>    hostBpm      { 120.0 };
    std::atomic<double>    hostBeatPos  { 0.0 };
    std::atomic<bool>      hostPlaying  { false };
    /** Playback tempo multiplier applied to our internal playhead (0.5, 1.0, 2.0). */
    std::atomic<double>    playheadTempoMul { 1.0 };

    // Loop-wrapped beat position for visual playhead display
    std::atomic<double>    mappedBeatPos { 0.0 };

    // User-positionable edit playhead (used when host is stopped)
    std::atomic<double>    editPlayheadBeat { 0.0 };

    // Combined lane: merged MIDI clip from all lanes (visual summary + output source)
    MidiClip combinedClip;
    void rebuildCombinedClip();
    void removeEmptyLanesExceptFirst();

    // Session length = arrangementBars × 4 beats; trim/remove regions & take-comp segments, loop, playhead
    void clampArrangementToSessionLength();
    void trimEmptyMeasuresInSelectedClips(const std::vector<std::pair<int, int>>& selectedRegions);
    /** Full factory defaults: empty arrangement, default transport/scale/theme, etc. */
    void resetToDefaultSession();
    /** Transpose all clip notes toward the selected scale root/type (estimates key from note content). */
    void transposeAllClipsToSelectedScale();
    /** For live Transpose: snapshot clip notes then apply current root/type. */
    void enableLiveScaleSnapshotsAndApply();
    /** Restore notes from snapshots (Transpose toggle off). */
    void disableLiveScaleRevert();
    /** Re-apply mapping from snapshots (root/type changed while Transpose on). */
    void applyLiveScaleMappingFromBaselines();

    /**
     * Push current comp boundary beats into APVTS (message thread). Coalesced via AsyncUpdater.
     * Only updates parameters that map to an existing boundary; does not overwrite unused slice
     * slots or compare via getValue() so host LFO/modulation is not reset.
     */
    void syncCompBoundaryAutomationParamsFromModel();

    /** Schedule APVTS sync from model (call after UI/session edits to keep DAW params aligned). */
    void scheduleCompBoundaryParamSync();

    // ── Recording ─────────────────────────────────────────────────────────────
    std::atomic<bool>   recording       { false };
    std::atomic<bool>   recordingArmed  { false };  // armed, waiting for play

    // Recorded note data (protected by laneLock)
    struct RecordingNote { int noteNumber; int velocity; int channel; double startBeat; };
    std::vector<RecordingNote> recordingActiveNotes;  // notes currently held
    int  recordingLaneIndex = -1;     // lane being recorded into
    double recordingStartBeat = 0.0;  // beat at which current clip started
    int  recordingClipCount = 0;      // clips created so far in this recording

    void startRecording();
    void stopRecording();
    void finaliseRecordingClip();      // commit current clip to lane

    // Last file browser directory (persisted across sessions)
    juce::String lastBrowserDir;

    // Preview panel state (set by editor, read in processBlock)
    void setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed);

    struct TransportInfo { double bpm; double beatPos; bool playing; };
    TransportInfo getTransport() const;
    double getPlayheadBpm() const { return hostBpm.load() * playheadTempoMul.load(); }

    /** Clear all comp segments and disable comping. */
    void clearAllComps();

private:
    struct CompBoundaryParamSync final : juce::AsyncUpdater
    {
        explicit CompBoundaryParamSync(PatternFlowProcessor& p) : proc(p) {}
        void handleAsyncUpdate() override { proc.syncCompBoundaryAutomationParamsFromModel(); }
        PatternFlowProcessor& proc;
    } compBoundaryParamSync { *this };

    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;

    juce::CriticalSection previewLock_;
    MidiClip previewClip_;
    bool previewHasClip_ = false;
    bool previewMuted_ = true;
    bool previewSoloed_ = false;

    // Live Transpose baseline snapshots (message thread only; protected by laneLock at use sites)
    bool liveScaleBaselinesValid_ = false;
    std::vector<std::vector<MidiClip>> liveScaleBaselines_;

    void generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                             juce::MidiBuffer& output, int numSamples, int sampleOffsetBase = 0);

    struct ActiveNote { int pitch; int channel; };
    std::vector<ActiveNote> activeNotes_;

    void generateMidiForBeatRange(double startBeat, double endBeat,
                                  juce::MidiBuffer& output, int numSamples,
                                  int sampleOffsetBase = 0);

    /** Internal: assumes `laneLock` is already held. */
    void rebuildCombinedClipImpl();
    /** Clip take-comp segments to [0, sessionLen] and merge; caller holds `laneLock`. */
    void clampTakeCompsToSessionLengthLocked(double sessionLen);

    /** Read host parameters + MIDI CCs and update comp slice positions (audio thread). */
    void applyCompMovesFromMidiAndHostParameters(juce::MidiBuffer& midi);

    /**
     * Move boundary at fromBeat toward toBeat; if snapToGrid, quantize to edit grid.
     * Caller must hold `laneLock`. Returns anchor beat after normalize (for UI drag).
     */
    double applyCompBoundaryDragLocked(double fromBeat, double toBeat, bool snapToGrid, bool rebuildAfter = true);

    void applyCompBoundaryMovesFromNorms(const float* norms, int numNorms);

    int applyHumanVelocity(int vel);
    double applyHumanTiming(double beatPos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternFlowProcessor)
};

} // namespace pflow
