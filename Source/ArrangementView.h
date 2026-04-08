#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiFileData.h"
#include "CompingModel.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;

struct ClipBlock
{
    juce::Rectangle<float> bounds;
    int                    laneIndex   = 0;
    int                    regionIndex = 0;
};

class ArrangementView : public juce::Component,
                        public juce::DragAndDropTarget,
                        public juce::FileDragAndDropTarget
{
public:
    explicit ArrangementView(PatternFlowProcessor& proc);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void addClipToLane(const MidiClip& clip, int laneIndex, double beatPos);
    void addClipToNewLane(const MidiClip& clip, double beatPos);

    std::function<void()> onAddLaneClicked;
    std::function<void()> onLoopChanged;
    std::function<void(int laneIdx, int regionIdx)> onRegionDoubleClicked;
    std::function<void(const MidiClip&, int laneIdx, int regionIdx)> onClipDoubleClicked;
    std::function<void()> onEmptyArrangementDoubleClicked;
    std::function<void(int laneIdx, int regionIdx)> onColourPickRequested;

    float beatsPerPixel = 0.1f;
    bool initialZoomDone = false;
    float scrollBeatOffset = 0.0f;

    int openPianoRollLane = -1;
    int openPianoRollRegion = -1;

    void refresh();
    void zoomToFitSession();
    void refreshComponentColours();
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int rulerH = metrics::rulerH;

    int selLane   = -1;
    int selRegion = -1;

    // Multi-selection: (laneIdx, regionIdx) for each selected clip
    std::vector<std::pair<int, int>> selectedClips;

    // Time range selection (for Cmd+L loop-from-selection)
    bool   hasTimeSelection    = false;
    double timeSelStartBeat    = 0.0;
    double timeSelEndBeat      = 0.0;

    void setLoopToSelection();
    void setLoopToSelectedClip();
    void toggleLoopFromContext();

    float verticalScrollOffset = 0.0f;

private:
    PatternFlowProcessor& processor;

    std::vector<ClipBlock> clipBlocks;
    int effectiveLaneHeight_ = metrics::laneHeight;  // Computed from lane count
    int   hoveredLane   = -1;
    int   hoveredRegion = -1;
    float dropBeatPos   = 0.0f;
    int   dropLaneIdx   = -1;
    bool  draggingOver  = false;

    // Playhead dragging
    bool draggingPlayhead = false;

    enum class LoopDragTarget { None, Start, End, Body };
    LoopDragTarget loopDragging = LoopDragTarget::None;
    double loopDragBodyOffset = 0.0;
    double loopDragBodyLength = 0.0;

    // Ruler time range selection drag
    bool  rulerDragging = false;
    double rulerDragStartBeat = 0.0;

    bool  draggingClip = false;
    bool  clipDragPending = false;
    juce::Point<float> clipDragDownPos;
    double clipDragOrigBeat = 0.0;
    double clipDragOrigEnd = 0.0;
    double clipDragOrigSelStart = -1.0;
    double clipDragOrigSelEnd = -1.0;
    std::vector<std::pair<double, double>> clipDragOrigRanges;
    double clipDragMouseOffset = 0.0;
    int    clipDragOrigLane = -1;
    float  clipDragMouseYOffset = 0.0f;

    // Region edge drag (clip resize)
    enum class EdgeDragTarget {
        None, Start, End
    };
    EdgeDragTarget edgeDragging = EdgeDragTarget::None;
    int edgeDragLane = -1;
    int edgeDragRegion = -1;
    double edgeDragOrigStart = 0.0;
    double edgeDragOrigEnd = 0.0;
    double edgeDragOrigSelStart = -1.0;
    double edgeDragOrigSelEnd = -1.0;
    std::vector<std::pair<double, double>> edgeDragOrigRanges;
    int edgeDragLinkedLane = -1;
    int edgeDragLinkedRegion = -1;
    double edgeDragLinkedOrigSelStart = -1.0;
    double edgeDragLinkedOrigSelEnd = -1.0;
    std::vector<std::pair<double, double>> edgeDragLinkedOrigRanges;

    bool isNearRegionEdge(const juce::MouseEvent& e, const ClipBlock& cb,
                         const CompRegion& region, EdgeDragTarget& which,
                         int& /*unused*/, bool /*unused*/) const;

    // Master clip external drag-to-DAW
    bool draggingMasterClip = false;
    bool masterDragInitiated = false;
    juce::Point<float> masterDragStartPos;

    // Selection box drag (marquee select)
    bool selectionBoxDragging = false;
    juce::Point<float> selectionBoxStart;
    juce::Point<float> selectionBoxCurrent;

    // Comp swipe (Alt + drag on clip when Comp + Swipe are on)
    bool   compSwipeDragging = false;
    int    compSwipeLane     = -1;
    int    compSwipeRegion   = -1;
    double compSwipeStartB   = 0.0;
    double compSwipeEndB     = 0.0;

    bool   compBoundaryDragging = false;
    /** Beat position used as `fromBeat` for the next applyCompBoundaryDrag (updated each drag frame). */
    double compBoundaryDragFromBeat = 0.0;
    std::vector<TakeCompSegment> compSwipeSegmentsUndoBefore;
    std::vector<TakeCompSegment> compBoundarySegmentsUndoBefore;

    double xToBeat(float x) const;
    float  beatToX(double beat) const;
    int    yToLane(float y) const;
    float  laneToY(int lane) const;
    float  masterLaneY() const;
    float  arrangementContentBottomY() const;

    void rebuildClipBlocks();
    void paintRuler(juce::Graphics&);
    void paintLaneHeaders(juce::Graphics&);
    void paintClipBlocks(juce::Graphics&);
    void paintPlayhead(juce::Graphics&);
    void paintMasterLane(juce::Graphics&);
    void paintDropIndicator(juce::Graphics&);
    void paintBeatGrid(juce::Graphics&);
    void paintLoopMarkers(juce::Graphics&);
    void paintTimeSelection(juce::Graphics&);

    /** Hit-test comp boundary handles on the COMP lane; returns segment edge indices in processor.takeComps[0].segments. */
    bool findCompBoundaryAtMouse(float x, float y,
                                 std::vector<std::pair<int, bool>>& outEdges,
                                 double& outBoundaryBeat) const;

    void updateLoopButton();
    void showClipContextMenu(int laneIdx, int regionIdx);
    void showLaneContextMenu(int laneIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementView)
};

} // namespace pflow
