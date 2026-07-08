#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "KnobPanel.h"
#include "UiAtoms.h"

namespace pflow {

// ── Shared vertical pitch mapping for roll surfaces ─────────────────────────
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
    void paint(juce::Graphics&) override;

private:
    std::vector<RollNote> notes;
    GrooveParams knobs;
    int bars = 1;
    double playheadStep = 0.0;
    bool playing = false;
};

// ── PianoRollEditorD ─────────────────────────────────────────────────────────
// instrument strip · toolbar · roll · velocity lane · Groove panel.

class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor();
    ~PianoRollEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    /** Feed the selected clip + its edit + groove. The editor never mutates
        the clip; user actions come back through the callbacks. */
    void setClip(const StepClip& clip, const ClipEdit& edit, const GrooveParams& groove);
    void clearClip();
    void setPlayheadStep(double step, bool playing);

    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(const GrooveParams&)> onGrooveChanged;

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
        PianoRollEditor& owner;

        // interaction state
        enum class Drag { None, Note, Marquee };
        Drag drag = Drag::None;
        int dragNoteId = -1;
        juce::Point<float> dragStart;
        int dragDPitch = 0, dragDStep = 0;      // live preview offsets
        juce::Rectangle<float> marquee;
        bool marqueeAdditive = false;
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
        PianoRollEditor& owner;
        static constexpr int headerH = 22;
        static constexpr int laneH = 64;
    };

    friend class RollContent;
    friend class KeyGutter;
    friend class VelocityLane;

    // ── model → view ──
    void rebuildResolved();
    void applyEdit(std::function<void(ClipEdit&)> mutate);
    void refreshControls();
    void updateRollSize();
    float pxPerStep() const;
    int totalSteps() const { return resolved.bars * kStepsPerBar; }
    const RollNote* noteAt(juce::Point<float> contentPos) const;
    juce::Rectangle<float> noteRect(const RollNote&) const;
    void zoomAround(float factor, float contentX);
    void commitNoteDrag();
    void selectPitch(int pitch, bool additive);
    void toggleTrim();
    void removeBadge(const juce::String& key);
    juce::String divisionName(int steps) const;

    // ── state ──
    bool hasClip = false;
    StepClip clip;
    ClipEdit edit;
    GrooveParams groove;
    ResolvedClip resolved;                  // resolveClip output
    std::vector<RollNote> grooved;          // applyGroove(resolved)
    EdgeBars edges;                         // of pre-trim resolve
    std::set<int> selection;
    float zoom = 1.0f;                      // ~1..3
    int divisionSteps = 4;                  // grid + snap (1/4 default)
    double playheadStep = 0.0;
    bool playing = false;

    // ── children ──
    Stepper octaveStepper;
    juce::ComboBox rootPicker, modePicker, divisionPicker;
    MiniSwitch fitSwitch { "Fit to scale" };
    MiniSwitch mapSwitch { "Map to root" };
    IconBtn btnRevert { icons::undo, "Revert all edits" };
    ChipBtn btnTrim { "Trim", icons::scissors };
    IconBtn btnZoomOut { icons::zoomOut, "Zoom out" };
    IconBtn btnZoomIn { icons::zoomIn, "Zoom in" };
    ChipBtn selBadge { "0 sel" };
    std::vector<std::unique_ptr<ChipBtn>> badgeChips;

    struct NotifyingViewport : juce::Viewport
    {
        std::function<void()> onScrolled;
        void visibleAreaChanged(const juce::Rectangle<int>&) override
        {
            if (onScrolled) onScrolled();
        }
    };

    KeyGutter gutter { *this };
    NotifyingViewport rollViewport;
    RollContent rollContent { *this };
    VelocityLane velocityLane { *this };
    bool velocityOpen = true;
    KnobsPanel knobsPanel;

    PitchRowMap rowMap;

    static constexpr int stripH = 46;
    static constexpr int toolbarH = 36;
    static constexpr int gutterW = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
