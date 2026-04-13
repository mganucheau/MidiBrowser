#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

// ── File browser panel (left sidebar, Ableton-style) ─────────────────────────
class FileBrowserPanel : public juce::Component,
                         public juce::FileBrowserListener,
                         public juce::DragAndDropContainer,
                         public juce::Timer
{
public:
    FileBrowserPanel();
    ~FileBrowserPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshComponentColours();
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    bool tryAddSelectedToNewLane();

    // FileBrowserListener
    void selectionChanged() override;
    void fileClicked(const juce::File&, const juce::MouseEvent&) override;
    void fileDoubleClicked(const juce::File&) override;
    void browserRootChanged(const juce::File&) override {}

    std::function<void(const MidiClip&)>           onClipDoubleClicked;
    std::function<void(const MidiClip&)>           onClipAddToNewLane;
    std::function<void(const MidiClip&)>           onClipAddFromBrowser;  // First file -> lane 1, else new lane
    /** Left arrow: add clip into the focused / selected arrangement lane (Ableton-style column). */
    std::function<void(const MidiClip&)>           onClipAddToFocusedLaneColumn;
    std::function<void(const juce::File&)>         onFileDragStarted;
    std::function<void(const juce::String&)>       onDirectoryChanged;

    // Preview sync: returns (currentBeat, loopStart, loopEnd, loopEnabled)
    std::function<std::tuple<double, double, double, bool>()> onGetPlayheadState;
    std::function<double()> onGetSessionLengthBeats;

    // Set root directory
    void setRootDirectory(const juce::File& dir);

    // Preview state (for processor MIDI output)
    const MidiClip& getPreviewClip() const { return previewClip; }
    bool getHasPreviewClip() const { return hasPreviewClip; }
    bool getPreviewMuted() const { return previewMuted; }
    bool getPreviewSoloed() const { return previewSoloed; }

private:
    std::unique_ptr<juce::WildcardFileFilter>   fileFilter;
    std::unique_ptr<juce::FileTreeComponent>     fileTree;
    std::unique_ptr<juce::TimeSliceThread>       dirThread;
    std::unique_ptr<juce::DirectoryContentsList> dirContents;

    juce::TextButton  btnSetRoot { "Select a folder" };
    juce::Label       lblHeader;
    juce::Label       previewFileLabel { {}, "no midi no cry" };

    MidiClip previewClip;
    bool     hasPreviewClip = false;
    bool     previewMuted   = true;   // Default muted
    bool     previewSoloed  = false;
    double   previewPlayheadBeat = 0.0;  // Advances when browsing, loops at clip end
    double   previewLastTime = 0.0;

    juce::Point<float> dragStartPos;
    bool     externalDragStarted = false;

    void updatePreviewForSelection();
    void timerCallback() override;

    // Custom tree item for drag support
    class DraggableMidiFileItem;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowserPanel)
};

} // namespace pflow
