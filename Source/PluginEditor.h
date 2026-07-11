#pragma once
#include <memory>
#include <map>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "TransportBar.h"
#include "BrowserPanels.h"
#include "PianoRollEditor.h"
#include "EffectsInspector.h"
#include "Theme.h"

namespace pflow {

// Cupertino shell: toolbar / [sidebar | file table + preview | editor? | effects?]

class MidiBrowserEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::DragAndDropContainer
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
    void rebuildEntries();
    int displayForClip(int clipIdx) const;
    void selectIndex(int index);
    void enterFolderAtDisplay(int displayIdx);
    void enterParentFolder();
    void applyTimeStretchFromMultiplier();
    void refreshEntryMeta(int index);
    void updateMiniPreview();
    MidiClip buildRenderedClip() const;
    void pushPreviewToProcessor();
    void startDragExport();
    void startDragOriginalFile(const juce::File& file);
    void applyLayoutState();
    void layoutContent();
    void toggleEditorFold();
    void toggleEffectsFold();
    void showTweaksMenu();
    void refreshSidebar();
    void syncEffectsInspector();

    const StepClip* selectedClip() const;
    ClipEdit selectedEdit() const;
    GrooveParams selectedGroove() const;

    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    struct ContentHolder : juce::Component
    {
        std::function<void()> onLayout;
        void resized() override { if (onLayout) onLayout(); }
    };
    ContentHolder content;

    TransportBar transport;
    FavoritesSidebar sidebar;
    FileListPanel fileList;
    PianoRollMini miniRoll;
    PianoRollEditor rollEditor;
    EffectsInspector effectsInspector;

    class PreviewHeader : public juce::Component
    {
    public:
        explicit PreviewHeader(MidiBrowserEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        MidiBrowserEditor& owner;
    };
    PreviewHeader previewHeader { *this };

    juce::File rootDir;
    std::vector<StepClip> clips;
    struct DisplayRow
    {
        bool isDirectory = false;
        int clipIndex = -1;
        juce::File file;
    };
    std::vector<DisplayRow> displayRows;
    bool starFilterOn = false;
    int selectedIdx = -1;
    int lastWindowH = 560;
    int browserColW = metrics::fileTableW;
    int layoutTargetW = 0;
    int layoutAnimFromW = 0;
    double layoutAnimStartMs = 0.0;
    static constexpr double kLayoutAnimMs = 220.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
