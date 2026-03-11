#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

// ── Editor view modes ────────────────────────────────────────────────────────
enum class EditorViewMode { Notes, Expression, Automation };

// ── Piano roll / sequencer editor (bottom panel) ─────────────────────────────
class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor();

    void setClip(const MidiClip& clip);
    void clearClip();
    bool hasClip() const { return clipLoaded; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Callback when clip is edited
    std::function<void(const MidiClip&)> onClipEdited;

private:
    MidiClip     currentClip;
    bool         clipLoaded = false;
    EditorViewMode viewMode = EditorViewMode::Notes;

    // View tabs
    juce::TextButton btnNotes      { "Notes" };
    juce::TextButton btnExpression { "Expression" };
    juce::TextButton btnAutomation { "Automation" };
    juce::TextButton btnClose      { "X" };

    // View state
    float noteHeight     = 8.0f;
    float pixelsPerBeat  = 60.0f;
    int   scrollNoteY    = 48;   // lowest visible note
    float scrollBeatX    = 0.0f;
    int   pianoKeyWidth  = 40;

    // Editing
    int   selectedNote   = -1;
    bool  draggingNote   = false;
    float dragStartX     = 0.0f;
    float dragStartY     = 0.0f;

    // Coordinate helpers
    float noteToY(int noteNum) const;
    int   yToNote(float y) const;
    float beatToX(double beat) const;
    double xToBeat(float x) const;

    void paintPianoKeys(juce::Graphics&);
    void paintNoteGrid(juce::Graphics&);
    void paintNotes(juce::Graphics&);
    void paintExpressionView(juce::Graphics&);
    void paintAutomationView(juce::Graphics&);
    void paintViewTabs(juce::Graphics&);

    static bool isBlackKey(int noteNum);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
