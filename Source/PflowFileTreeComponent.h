#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pflow {

/**
 * File tree with per-item width based on filename length so the TreeView can show
 * a horizontal scrollbar for long paths (JUCE's FileTreeComponent uses -1 width only).
 */
class PflowFileTreeComponent : public juce::TreeView,
                               public juce::DirectoryContentsDisplayComponent
{
public:
    explicit PflowFileTreeComponent(juce::DirectoryContentsList& listToShow);
    ~PflowFileTreeComponent() override;

    int getNumSelectedFiles() const override { return TreeView::getNumSelectedItems(); }

    juce::File getSelectedFile(int index = 0) const override;

    void deselectAllFiles() override;

    void scrollToTop() override;

    void setSelectedFile(const juce::File&) override;

    void refresh();

    void setDragAndDropDescription(const juce::String& description);

    const juce::String& getDragAndDropDescription() const noexcept { return dragAndDropDescription; }

    void setItemHeight(int newHeight);

    int getItemHeight() const noexcept { return itemHeight; }

    /** Select previous/next visible MIDI file in tree order (skips folders). */
    void selectAdjacentMidiFile(int direction);

private:
    juce::String dragAndDropDescription;
    int itemHeight = 22;

    class Controller;
    std::unique_ptr<Controller> controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PflowFileTreeComponent)
};

} // namespace pflow
