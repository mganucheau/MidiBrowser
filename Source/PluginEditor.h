#pragma once
#include <memory>
#include <map>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "TransportBar.h"
#include "BrowserPanels.h"
#include "PianoRollEditor.h"
#include "Theme.h"

namespace pflow {

// ── MidiBrowser root ─────────────────────────────────────────────────────────
// Editor open (~980px): transport / [sidebar | file list 244px | editor].
// Folded (~300px): transport / [sidebar | file list] / full-width mini preview.

class MidiBrowserEditor : public juce::AudioProcessorEditor,
                          public juce::Timer
{
public:
    explicit MidiBrowserEditor(MidiBrowserProcessor&);
    ~MidiBrowserEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void setRootDirectory(const juce::File& dir, bool keepSelection = false);
    void rescanFolder(bool keepSelection);
    void chooseFolder();
    void selectIndex(int index);
    void refreshEntryMeta(int index);
    void pushPreviewToProcessor();
    void applyLayoutState();
    void toggleEditorFold();
    void showTweaksMenu();
    void refreshSidebar();

    const StepClip* selectedClip() const;
    ClipEdit selectedEdit() const;
    GrooveParams selectedGroove() const;

    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    TransportBar transport;
    FavoritesSidebar sidebar;
    FileListPanel fileList;
    PianoRollEditor rollEditor;
    PianoRollMini miniRoll;

    // Folded-layout mini preview header (fold toggle + clip name + bars)
    class MiniHeader : public juce::Component
    {
    public:
        explicit MiniHeader(MidiBrowserEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        MidiBrowserEditor& owner;
    };
    MiniHeader miniHeader { *this };

    juce::File rootDir;
    std::vector<StepClip> clips;             // parallel to fileList entries
    int selectedIdx = -1;
    int lastWindowH = 560;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
