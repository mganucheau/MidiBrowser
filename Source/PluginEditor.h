#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <map>
#include <vector>
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
                          public juce::DragAndDropContainer,
                          private juce::DarkModeSettingListener
{
public:
    explicit MidiBrowserEditor(MidiBrowserProcessor&);
    ~MidiBrowserEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&) override;
    void darkModeSettingChanged() override;

private:
    void applyNativeWindowChrome();
    void setRootDirectory(const juce::File& dir, bool keepSelection = false);
    /** Async folder scan — never parses MIDI on the message thread. */
    void rescanFolder(bool keepSelection, const juce::String& preferredPath = {});
    /** Load every MIDI file under the current folder tree (flat list). */
    void scanAllFolders();
    void chooseFolder();
    void rebuildEntries();
    /** Refilter the already-loaded clip list (no disk rescan). */
    void applyBrowserFilter();
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
    void copyFileToFolder(const juce::File& file);
    void copyStarredToFolder();
    void copyRenderedClipToFolder();
    void applyLayoutState();
    void updateWindowLimits();
    void layoutContent();
    void toggleEditorFold();
    void toggleEffectsFold();
    void showTweaksMenu();
    void refreshSidebar();
    void syncEffectsInspector();
    void setBrowseMode(int mode);
    void loadStarredClips(const juce::String& preferredPath = {});
    /** Mirror browser UI into the processor (survives editor teardown / host save). */
    void persistBrowserSession();
    /** Rebuild folder/search/filters/selection from processor session state. */
    void restoreBrowserSession();
    void selectPathOrFirst(const juce::String& path);
    /** Cancel in-flight scans and show the searching affordance. */
    int beginBackgroundClipLoad();
    /** Parse MIDI paths on a worker thread; apply callback on the message thread. */
    void loadClipsFromPathsAsync(juce::StringArray paths,
                                 std::function<void(std::vector<StepClip>&&)> onDone);
    void runSearch(const BrowserSearch& criteria);
    /** savedIdx >= 0 keeps that saved-search selection and stores/restores its result snapshot. */
    void runSearchAsync(const BrowserSearch& criteria, int savedIdx = -1,
                        juce::File searchRoot = {}, bool allowCache = true);
    void saveCurrentSearch();
    bool clipMatchesSearch(const StepClip& clip, const juce::File& file,
                           const BrowserSearch& criteria) const;
    void applySearchSnapshot(std::vector<StepClip> found,
                             std::map<juce::String, juce::StringArray> locMap,
                             const BrowserSearch& criteria, int savedIdx,
                             const juce::String& rootPath, bool showSearching);

    enum class KeyNavTarget { Browser, Editor, Effects };
    void claimKeyNav(KeyNavTarget target);

    const StepClip* selectedClip() const;
    ClipEdit selectedEdit() const;
    GrooveParams selectedGroove() const;

    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;

    /** Hides tips when Settings → Show Tooltips is off; styled via LookAndFeel. */
    struct AppTooltipWindow : juce::TooltipWindow
    {
        AppTooltipWindow(juce::Component* parent, int delayMs)
            : juce::TooltipWindow(parent, delayMs) {}

        juce::String getTipFor(juce::Component& c) override
        {
            if (tweaks().showTooltips.load() == 0)
                return {};
            return juce::TooltipWindow::getTipFor(c);
        }
    };
    AppTooltipWindow tooltips { this, 450 };

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
    /** True after "Scan all folders" — flat recursive MIDI list, no subfolder rows. */
    bool recursiveBrowse = false;
    struct DisplayRow
    {
        bool isDirectory = false;
        int clipIndex = -1;
        juce::File file;
    };
    std::vector<DisplayRow> displayRows;
    int browseMode = 0;   // 0 folder, 2 search
    bool starredFilter = false;
    BrowserSearch activeSearch;
    int activeSavedSearchIdx = -1;
    /** Bumps on each search so stale async completions are ignored. */
    std::atomic<int> searchGeneration { 0 };
    /** Bumps on each async folder Update so stale completions are ignored. */
    std::atomic<int> folderScanGeneration { 0 };
    bool sidebarScanning = false;
    /** Primary clip path -> all Finder locations (including primary) after dedupe. */
    std::map<juce::String, juce::StringArray> searchDuplicateLocations;

    /** In-session full results so re-picking a saved search is instant (no rescan). */
    struct SearchSnapshot
    {
        BrowserSearch criteria;
        juce::String rootPath;
        std::vector<StepClip> clips;
        std::map<juce::String, juce::StringArray> locations;
    };
    SearchSnapshot lastSearchSnapshot;
    std::map<int, SearchSnapshot> savedSearchSnapshots;
    void rememberSearchSnapshot(int savedIdx, SearchSnapshot snap);
    void forgetSavedSearchSnapshot(int index);
    int selectedIdx = -1;
    KeyNavTarget keyNavTarget = KeyNavTarget::Browser;
    int lastWindowH = 560;
    int browserColW = metrics::fileTableW;
    int layoutTargetW = 0;
    int layoutAnimFromW = 0;
    double layoutAnimStartMs = 0.0;
    static constexpr double kLayoutAnimMs = 220.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
