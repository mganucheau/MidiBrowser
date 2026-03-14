#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiFileData.h"
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
                        public juce::DragAndDropTarget
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

    void addClipToLane(const MidiClip& clip, int laneIndex, double beatPos);
    void addClipToNewLane(const MidiClip& clip, double beatPos);

    std::function<void()> onAddLaneClicked;
    std::function<void(int laneIdx, int regionIdx)> onRegionDoubleClicked;
    std::function<void(const MidiClip&, int laneIdx, int regionIdx)> onClipDoubleClicked;
    std::function<void(int laneIdx, int regionIdx)> onColourPickRequested;

    float beatsPerPixel = 0.1f;
    float scrollBeatOffset = 0.0f;

    int openPianoRollLane = -1;
    int openPianoRollRegion = -1;

    void refresh();
    void refreshComponentColours();
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int rulerH = 24;

    juce::TextButton btnAddBars { "+4 Bars" };
    juce::TextButton btnLoop    { "Loop" };

    int selLane   = -1;
    int selRegion = -1;

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
    double clipDragOrigBeat = 0.0;
    double clipDragOrigEnd = 0.0;
    double clipDragMouseOffset = 0.0;
    int    clipDragOrigLane = -1;
    float  clipDragMouseYOffset = 0.0f;

    // Region edge drag-to-loop
    enum class EdgeDragTarget { None, Start, End };
    EdgeDragTarget edgeDragging = EdgeDragTarget::None;
    int edgeDragLane = -1;
    int edgeDragRegion = -1;
    double edgeDragOrigStart = 0.0;
    double edgeDragOrigEnd = 0.0;

    bool isNearRegionEdge(const juce::MouseEvent& e, const ClipBlock& cb, EdgeDragTarget& which) const;

    double xToBeat(float x) const;
    float  beatToX(double beat) const;
    int    yToLane(float y) const;
    float  laneToY(int lane) const;

    void rebuildClipBlocks();
    void paintRuler(juce::Graphics&);
    void paintLaneHeaders(juce::Graphics&);
    void paintClipBlocks(juce::Graphics&);
    void paintPlayhead(juce::Graphics&);
    void paintDropIndicator(juce::Graphics&);
    void paintBeatGrid(juce::Graphics&);
    void paintLoopMarkers(juce::Graphics&);
    void paintTimeSelection(juce::Graphics&);

    void updateLoopButton();
    void ensureBarsForBeat(double endBeat);
    void showClipContextMenu(int laneIdx, int regionIdx);
    void showLaneContextMenu(int laneIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementView)
};

} // namespace pflow
