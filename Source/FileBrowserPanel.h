#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

// ── File browser panel (left sidebar, Ableton-style) ─────────────────────────
class FileBrowserPanel : public juce::Component,
                         public juce::FileBrowserListener,
                         public juce::DragAndDropContainer
{
public:
    FileBrowserPanel();
    ~FileBrowserPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    // FileBrowserListener
    void selectionChanged() override {}
    void fileClicked(const juce::File&, const juce::MouseEvent&) override;
    void fileDoubleClicked(const juce::File&) override;
    void browserRootChanged(const juce::File&) override {}

    // Callbacks
    std::function<void(const MidiClip&)>           onClipDoubleClicked;
    std::function<void(const juce::File&)>         onFileDragStarted;

    // Set root directory
    void setRootDirectory(const juce::File& dir);

private:
    std::unique_ptr<juce::WildcardFileFilter>   fileFilter;
    std::unique_ptr<juce::FileTreeComponent>     fileTree;
    std::unique_ptr<juce::TimeSliceThread>       dirThread;
    std::unique_ptr<juce::DirectoryContentsList> dirContents;

    juce::TextButton  btnSetRoot { "Set Folder..." };
    juce::Label       lblHeader;

    // Custom tree item for drag support
    class DraggableMidiFileItem;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowserPanel)
};

} // namespace pflow
