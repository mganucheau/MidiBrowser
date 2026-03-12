#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;
enum class EditorViewMode { Notes, Expression, Automation };

class PianoRollEditor : public juce::Component
{
public:
    explicit PianoRollEditor(PatternFlowProcessor& proc);

    void setClip(const MidiClip& clip, int laneIdx = -1, int regionIdx = -1);
    void clearClip();
    bool hasClip() const { return clipLoaded; }
    int getEditLaneIndex() const { return editLaneIdx; }
    int getEditRegionIndex() const { return editRegionIdx; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;

    std::function<void(const MidiClip&, int laneIdx, int regionIdx)> onClipEdited;
    std::function<void()> onCloseRequested;

private:
    PatternFlowProcessor& processor;
    MidiClip     currentClip;
    bool         clipLoaded = false;
    int          editLaneIdx = -1;
    int          editRegionIdx = -1;
    EditorViewMode viewMode = EditorViewMode::Notes;

    juce::TextButton btnNotes      { "Notes" };
    juce::TextButton btnExpression { "Expression" };
    juce::TextButton btnAutomation { "Automation" };
    juce::TextButton btnClose      { "X" };

    float noteHeight     = 8.0f;
    float pixelsPerBeat  = 60.0f;
    int   scrollNoteY    = 48;
    float scrollBeatX    = 0.0f;
    int   pianoKeyWidth  = 40;

    int   selectedNote   = -1;
    bool  draggingNote   = false;
    float dragStartX     = 0.0f;
    float dragStartY     = 0.0f;
    double dragNoteOrigBeat = 0.0;
    int    dragNoteOrigPitch = 0;

    bool  resizingNote   = false;
    double resizeOrigLen = 0.0;

    float noteToY(int noteNum) const;
    int   yToNote(float y) const;
    float beatToX(double beat) const;
    double xToBeat(float x) const;

    void autoZoomToNotes();

    void paintPianoKeys(juce::Graphics&);
    void paintNoteGrid(juce::Graphics&);
    void paintNotes(juce::Graphics&);
    void paintExpressionView(juce::Graphics&);
    void paintAutomationView(juce::Graphics&);

    static bool isBlackKey(int noteNum);
    bool isNearRightEdge(const juce::MouseEvent& e, int noteIdx) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
