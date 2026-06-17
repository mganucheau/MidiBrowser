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
    std::function<juce::StringArray()> onGetSavedFolders;
    std::function<void(const juce::String&)> onSaveFolder;
    std::function<void(const juce::String&)> onRemoveSavedFolder;
    std::function<void(bool)> onTrimModeChanged;

    /** Session transport: (ppqBeat, loopStart, loopEnd, hostLoopEnabled) */
    std::function<std::tuple<double, double, double, bool>()> onGetPlayheadState;
    std::function<double()> onGetSessionLengthBeats;
    std::function<bool()> onGetHostPlaying;

    void setRootDirectory(const juce::File& dir);
    juce::File getRootDirectory() const;

    /** Same vertical size as the "Select a folder" row (shared with editor title bar). */
    static int folderBarHeightPx();

    void setTrimEmptyMeasuresMode(bool enabled);
    bool getTrimEmptyMeasuresMode() const { return truncateEmptyMeasuresMode_; }
    void toggleTrimEmptyMeasuresMode();

    const MidiClip& getPreviewClip() const { return previewClip; }
    bool getHasPreviewClip() const { return hasPreviewClip; }
    bool getPreviewMuted() const { return previewMuted; }

private:
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<PflowFileTreeComponent> fileTree;
    std::unique_ptr<juce::TimeSliceThread> dirThread;
    std::unique_ptr<juce::DirectoryContentsList> dirContents;

    juce::TextButton btnSetRoot { "Select a folder" };
    juce::TextButton btnBookmarks { juce::String::charToString(0x2605) };
    juce::TextButton btnFileUp { juce::String::charToString(0x25B2) };
    juce::TextButton btnFileDown { juce::String::charToString(0x25BC) };
    juce::TextButton btnTrim { "Trim" };
    juce::Label browserEmptyHint_;

    MidiClip previewClip;
    bool hasPreviewClip = false;
    bool previewMuted = false;

    juce::Point<float> dragStartPos;
    bool externalDragStarted = false;

    juce::File currentPreviewMidiFile_;
    bool truncateEmptyMeasuresMode_ = false;

    void updatePreviewForSelection();
    void rebuildPreviewClipFromDisk();
    void showBookmarksMenu();
    void selectAdjacentMidiFile(int direction);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBrowserPanel)
};

} // namespace pflow
