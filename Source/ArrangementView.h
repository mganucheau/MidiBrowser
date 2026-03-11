#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;

// ── Single clip visual block on a lane ───────────────────────────────────────
struct ClipBlock
{
    juce::Rectangle<float> bounds;
    int                    laneIndex   = 0;
    int                    regionIndex = 0;
};

// ── Arrangement / comping view (right main area) ─────────────────────────────
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

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

    // Add a clip to a specific lane or create a new lane
    void addClipToLane(const MidiClip& clip, int laneIndex, double beatPos);
    void addClipToNewLane(const MidiClip& clip, double beatPos);

    // Open comp view for a region
    std::function<void(int laneIdx, int regionIdx)> onRegionDoubleClicked;
    // Open piano roll for a clip
    std::function<void(const MidiClip&)>            onClipDoubleClicked;
    // Colour picker
    std::function<void(int laneIdx, int regionIdx)> onColourPickRequested;

    // Zoom / scroll
    float beatsPerPixel = 0.1f;
    float scrollBeatOffset = 0.0f;

    void refresh();

private:
    PatternFlowProcessor& processor;

    // Visual layout
    std::vector<ClipBlock> clipBlocks;
    int   hoveredLane   = -1;
    int   hoveredRegion = -1;
    float dropBeatPos   = 0.0f;
    int   dropLaneIdx   = -1;
    bool  draggingOver  = false;

    // Selected region
    int selLane   = -1;
    int selRegion = -1;

    // Convert pixel x to beat position
    double xToBeat(float x) const;
    float  beatToX(double beat) const;
    int    yToLane(float y) const;
    float  laneToY(int lane) const;

    void rebuildClipBlocks();
    void paintLaneHeaders(juce::Graphics&);
    void paintClipBlocks(juce::Graphics&);
    void paintPlayhead(juce::Graphics&);
    void paintDropIndicator(juce::Graphics&);
    void paintBeatGrid(juce::Graphics&);

    // Context menu for clip colour etc.
    void showClipContextMenu(int laneIdx, int regionIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementView)
};

} // namespace pflow
