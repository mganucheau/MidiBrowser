#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;
enum class EditorViewMode { Notes, Expression, Automation };

class PianoRollEditor : public juce::Component,
                        public juce::KeyListener
{
public:
    explicit PianoRollEditor(PatternFlowProcessor& proc);
    ~PianoRollEditor() override;

    void setClip(const MidiClip& clip, int laneIdx = -1, int regionIdx = -1);
    void clearClip();
    bool hasClip() const { return clipLoaded; }
    int getEditLaneIndex() const { return editLaneIdx; }
    int getEditRegionIndex() const { return editRegionIdx; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshComponentColours();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;
    using juce::Component::keyPressed;
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;

    std::function<void(const MidiClip&, int laneIdx, int regionIdx)> onClipEdited;
    std::function<void()> onCloseRequested;

    void quantizeSelectedNotes();
    void transposeSelectedNotes(int semitones);

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
    juce::ComboBox   ccCombo;
    juce::TextButton btnClose      { "X" };

    float noteHeight     = 8.0f;
    float pixelsPerBeat  = 60.0f;
    int   scrollNoteY    = 48;
    float scrollBeatX    = 0.0f;
    int   pianoKeyWidth  = 40;

    int   selectedNote   = -1;
    std::set<int> selectedNotes;   // multi-selection
    bool  draggingNote   = false;
    float dragStartX     = 0.0f;
    float dragStartY     = 0.0f;
    double dragNoteOrigBeat = 0.0;
    int    dragNoteOrigPitch = 0;

    bool  resizingNote   = false;
    double resizeOrigLen = 0.0;
    NoteEvent dragOrigNote;  // snapshot before drag/resize

    float noteToY(int noteNum) const;
    int   yToNote(float y) const;
    float beatToX(double beat) const;
    double xToBeat(float x) const;

    /** Height of the note grid area (below tabs). In Expression mode = 2/3 of content; else full. */
    float getNoteAreaHeight() const;
    /** In Expression mode, the strip is the bottom 1/3 for velocity/expression. */
    juce::Rectangle<int> getExpressionStripBounds() const;
    /** In Automation mode, full content area below tabs. */
    juce::Rectangle<int> getAutomationContentBounds() const;

    void autoZoomToNotes();
    void syncPianoRollTabPills();

    void paintPianoKeys(juce::Graphics&);
    void paintNoteGrid(juce::Graphics&);
    void paintNotes(juce::Graphics&);
    void paintExpressionView(juce::Graphics&);
    void paintAutomationView(juce::Graphics&);

    // Clip start offset line dragging
    bool  draggingStartLine = false;
    float startLineDragStartX = 0.0f;
    double startLineDragOrigOffset = 0.0;

    void paintStartLine(juce::Graphics&);
    bool isNearStartLine(float x) const;

    // Velocity editing in Expression view
    int  velocityDragNote = -1;
    int  velocityDragOrigVel = 0;
    float velocityDragStartY = 0.0f;

    // Automation editing
    int  selectedAutomationPoint = -1;
    int  automationDragPoint = -1;
    std::vector<AutomationPoint> automationDragOldPoints;

    std::vector<AutomationPoint>& getAutomationPointsForCurrentCC();
    int getCurrentAutomationCC() const;
    void ensureAutomationBounds(int cc);
    int hitTestAutomationPoint(float x, float y) const;

    static bool isBlackKey(int noteNum);
    bool isNearRightEdge(const juce::MouseEvent& e, int noteIdx) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
