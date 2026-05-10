#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "MidiFileData.h"
#include "PflowFileTreeComponent.h"
#include "Theme.h"

namespace pflow {

// ── File browser panel (Ableton-style tree + synced preview strip) ─────────
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

    // FileBrowserListener
    void selectionChanged() override;
    void fileClicked(const juce::File&, const juce::MouseEvent&) override;
    void fileDoubleClicked(const juce::File&) override;
    void browserRootChanged(const juce::File&) override {}

    std::function<void(const juce::File&)> onFileDragStarted;
    std::function<void(const juce::String&)> onDirectoryChanged;

    /** Preview sync: (sessionBeat, loopStartPpq, loopEndPpq, loopActive) */
    std::function<std::tuple<double, double, double, bool>()> onGetPlayheadState;
    std::function<double()> onGetSessionLengthBeats;

    void setRootDirectory(const juce::File& dir);

    const MidiClip& getPreviewClip() const { return previewClip; }
    bool getHasPreviewClip() const { return hasPreviewClip; }
    bool getPreviewMuted() const { return previewMuted; }

private:
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<PflowFileTreeComponent> fileTree;
    std::unique_ptr<juce::TimeSliceThread> dirThread;
    std::unique_ptr<juce::DirectoryContentsList> dirContents;

    juce::TextButton btnSetRoot { "Select a folder" };
    juce::Label browserEmptyHint_;

    MidiClip previewClip;
    bool hasPreviewClip = false;
    bool previewMuted = true;
    double previewPlayheadBeat = 0.0;
    double previewLastTime = 0.0;

    juce::Point<float> dragStartPos;
    bool externalDragStarted = false;

    void updatePreviewForSelection();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowserPanel)
};

} // namespace pflow
