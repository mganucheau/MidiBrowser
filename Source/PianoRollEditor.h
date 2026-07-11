#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "KnobPanel.h"
#include "UiAtoms.h"

namespace pflow {

// ── Shared vertical pitch mapping for the mini roll ──────────────────────────
struct PitchRowMap
{
    int minPitch = 48, maxPitch = 72;   // inclusive
    float rowH = 8.0f;

    int numRows() const { return maxPitch - minPitch + 1; }
    float yForPitchTop(int pitch, float height) const
    {
        return height - (float) (pitch - minPitch + 1) * rowH;
    }
    int pitchForY(float y, float height) const
    {
        return minPitch + (int) std::floor((height - y) / rowH);
    }
    void fit(const std::vector<RollNote>& notes, float height, int minRange = 16);
};

/** Effective per-note velocity (file velocity as base, groove on top). */
double effectiveVelocity(const RollNote& n, const GrooveParams& k);

// ── PianoRollMini ────────────────────────────────────────────────────────────
// Read-only compact roll: resolved+grooved notes + playhead. No editing.

class PianoRollMini : public juce::Component
{
public:
    void setNotes(std::vector<RollNote> resolvedGrooved, int bars, const GrooveParams& groove);
    void setPlayheadStep(double step, bool playing);
    void setTimeStretch(double stretch);
    void paint(juce::Graphics&) override;

private:
    std::vector<RollNote> notes;
    GrooveParams knobs;
    int bars = 1;
    double playheadStep = 0.0;
    bool playing = false;
    double timeStretch = 1.0;
};

// ── PianoRollEditorD ─────────────────────────────────────────────────────────
// instrument strip · toolbar · roll · velocity lane · Groove panel.
//
// Roll geometry (Live/Logic-style): the content spans the full MIDI pitch
// range (or only note-bearing rows when folded) at a fixed, zoomable row
// height, scrolling both axes inside a viewport. The key gutter shares the
// exact same row mapping, offset by the viewport's vertical scroll.

class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor();
    ~PianoRollEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;

    /** Feed the selected clip + its edit + groove. The editor never mutates
        the clip; user actions come back through the callbacks. */
    void setClip(const StepClip& clip, const ClipEdit& edit, const GrooveParams& groove);
    void clearClip();
    void setPlayheadStep(double step, bool playing);
    void setLockActive(bool locked);
    void setTimeStretch(double stretch);   // 1/bpmMultiplier — stretches roll + export view
    void setScalePlacement(ScalePlacement placement);
    bool hasSelection() const { return !selection.empty(); }
    void deleteSelectedNotes();   // non-destructive (edit.deleted)

    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(const GrooveParams&)> onGrooveChanged;
    std::function<void(bool)> onLockToggled;   // browse-lock for pitch edits
    /** Loop region in unstretched steps [start, end); end exclusive. */
    std::function<void(double startStep, double endStep)> onLoopChanged;

private:
    // ── inner surfaces ──
    class RollContent : public juce::Component
    {
    public:
        explicit RollContent(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
        PianoRollEditor& owner;

        // interaction state
        enum class Drag { None, Note, Marquee, Pan, LoopStart, LoopEnd };
        Drag drag = Drag::None;
        int dragNoteId = -1;
        juce::Point<float> dragStart;
        juce::Point<int> panStartView;
        int dragDRows = 0, dragDStep = 0;       // live preview offsets
        juce::Rectangle<float> marquee;
        bool marqueeAdditive = false;
    };

    /** Bar/beat labels + loop brace handles above the roll grid. */
    class TimeRuler : public juce::Component
    {
    public:
        explicit TimeRuler(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
        enum class Drag { None, LoopStart, LoopEnd };
        Drag drag = Drag::None;
    };

    class KeyGutter : public juce::Component
    {
    public:
        explicit KeyGutter(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
    };

    class VelocityLane : public juce::Component
    {
    public:
        explicit VelocityLane(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
        static constexpr int headerH = 22;
        static constexpr int laneH = 64;

    private:
        const RollNote* noteAtX(float x) const;
        void applyDragVelocity(const juce::MouseEvent&);
        int dragNoteId = -1;
    };

    /** Folding Scale section (Bottom placement): octave / key / mode / fit / map. */
    class ScalePanel : public juce::Component
    {
    public:
        explicit ScalePanel(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void resized() override;
        bool hitTest(int x, int y) override;
        int idealHeight() const;
        bool isOpen() const { return open; }
        void setOpen(bool shouldOpen);
        PianoRollEditor& owner;
        static constexpr int headerH = 32;
        static constexpr int bodyH = 44;
        bool open = true;
    };

    struct NotifyingViewport : juce::Viewport
    {
        std::function<void()> onScrolled;
        void visibleAreaChanged(const juce::Rectangle<int>&) override
        {
            if (onScrolled) onScrolled();
        }
    };

    friend class RollContent;
    friend class TimeRuler;
    friend class KeyGutter;
    friend class VelocityLane;
    friend class ScalePanel;

    // ── model → view ──
    void rebuildResolved();
    void applyEdit(std::function<void(ClipEdit&)> mutate);
    void refreshControls();
    void updateRollSize();
    void scrollToContent();
    void layoutScaleControls(juce::Rectangle<int> area);
    void nudgeSelection(int dPitch, int dStep);

    // ── row geometry ──
    float pxPerStep() const { return pxPerStepBase * zoomX; }
    float effRowH() const { return folded ? rowH * 2.0f : rowH; }   // folded rows are 2x tall
    int totalSteps() const
    {
        return juce::jmax(1, (int) std::lround((double) resolved.bars * kStepsPerBar * timeStretch));
    }
    int numRows() const { return folded ? juce::jmax(1, (int) foldPitches.size()) : 128; }
    int rowForPitch(int pitch) const;
    int pitchForRow(int row) const;
    juce::Rectangle<float> noteRect(const RollNote&, int rowOffset = 0, double stepOffset = 0.0) const;
    const RollNote* noteAt(juce::Point<float> contentPos) const;
    void computePxPerStepBase();
    void zoomXAround(float factor, float contentX);
    void zoomRowsAround(float factor, float contentY);
    void commitNoteDrag();
    void selectPitch(int pitch, bool additive);
    void toggleTrim();
    void removeBadge(const juce::String& key);
    void resetLoopToClip();
    void setLoopSteps(double start, double end, bool notify);
    void notifyLoopChanged();
    double clipSteps() const { return (double) resolved.bars * (double) kStepsPerBar; }
    double minLoopSteps() const { return (double) juce::jmax(1, divisionSteps); }
    double stepFromContentX(float x) const;
    float contentXFromStep(double step) const;
    int hitLoopHandle(float contentX, float hitPx = 6.0f) const; // -1 none, 0 start, 1 end
    void paintLoopOverlay(juce::Graphics&, juce::Rectangle<float> clipB, float top, float bottom) const;
    void paintTimeRulerLabels(juce::Graphics&, float viewX, float width, float height) const;

    // ── state ──
    bool hasClip = false;
    StepClip clip;
    ClipEdit edit;
    GrooveParams groove;
    ResolvedClip resolved;                  // resolveClip output
    std::vector<RollNote> grooved;          // applyGroove(resolved)
    std::vector<int> emptyBarIndices;       // all empty bars pre-trim
    std::set<int> selection;
    std::vector<int> foldPitches;           // ascending note-bearing pitches
    bool folded = false;
    float pxPerStepBase = 6.0f;             // fit-to-width at zoomX 1
    float zoomX = 1.0f;                     // horizontal zoom, 1..6
    float rowH = 13.0f;                     // vertical zoom (px per semitone)
    int divisionSteps = 4;                  // grid + snap (1/4 default)
    double playheadStep = 0.0;
    bool playing = false;
    bool lockActive = false;
    double timeStretch = 1.0;               // 1 / bpmMultiplier
    ScalePlacement scalePlacement = ScalePlacement::Top;
    double loopStartStep = 0.0;             // unstretched steps
    double loopEndStep = 16.0;              // exclusive

    static constexpr float kMinRowH = 5.0f;    // max notes on screen
    static constexpr float kMaxRowH = 26.0f;   // min notes on screen
    static constexpr float kMaxZoomX = 6.0f;
    static constexpr int rulerH = 20;

    // ── children ──
    Stepper octaveStepper;
    juce::ComboBox rootPicker, modePicker, divisionPicker;
    MiniSwitch fitSwitch { "Fit to scale" };
    MiniSwitch mapSwitch { "Map to root" };
    IconBtn btnLock { icons::lockOpen, "Lock pitch edits while browsing" };
    IconBtn btnRevert { icons::undo, "Revert all edits" };
    ChipBtn btnTrim { "Trim", icons::scissors };
    ChipBtn btnFold { "Fold", icons::foldRows };
    IconBtn btnZoomOut { icons::zoomOut, "Zoom out" };
    IconBtn btnZoomIn { icons::zoomIn, "Zoom in" };
    ChipBtn selBadge { "0 sel" };
    std::vector<std::unique_ptr<ChipBtn>> badgeChips;

    KeyGutter gutter { *this };
    TimeRuler timeRuler { *this };
    NotifyingViewport rollViewport;
    RollContent rollContent { *this };
    VelocityLane velocityLane { *this };
    bool velocityOpen = false;
    ScalePanel scalePanel { *this };
    KnobsPanel knobsPanel;

    static constexpr int stripH = 46;
    static constexpr int toolbarH = 36;
    static constexpr int gutterW = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
