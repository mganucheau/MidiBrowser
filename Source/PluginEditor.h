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
    void rebuildEntries();                  // display list (star filter applied)
    int displayForClip(int clipIdx) const;
    void selectIndex(int index);            // index into `clips`
    void refreshEntryMeta(int index);
    MidiClip buildRenderedClip() const;   // resolved + groove + velocities
    void pushPreviewToProcessor();
    void startDragExport();
    void applyLayoutState();
    void layoutContent();
    void toggleEditorFold();
    void showTweaksMenu();
    void refreshSidebar();

    const StepClip* selectedClip() const;
    ClipEdit selectedEdit() const;
    GrooveParams selectedGroove() const;

    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    /** All UI lives inside this holder so the content-size tweak can scale
        the whole interface with one transform. */
    struct ContentHolder : juce::Component
    {
        std::function<void()> onLayout;
        void resized() override { if (onLayout) onLayout(); }
    };
    ContentHolder content;

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
    std::vector<StepClip> clips;             // every clip in the folder
    std::vector<int> shown;                  // display order → index into clips
    bool starFilterOn = false;
    int selectedIdx = -1;                    // index into clips
    int lastWindowH = 560;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
