#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

struct BrowserSearch
{
    juce::String query;
    double bpmMin = 0.0; // 0 = no lower bound
    double bpmMax = 0.0; // 0 = no upper bound
    int keyRoot = -1;    // -1 = any
    int barsMin = 0;     // 0 = any; 64 = 64+ (≥64)
    int barsMax = 0;     // 0 = any; 64 = 64+ (unlimited upper)
    bool subdirs = false;
};

struct SavedSearchEntry
{
    juce::String name;
    BrowserSearch search;
};

// Cupertino source sidebar: Open · Saved dirs · Starred · Search.

class FavoritesSidebar : public juce::Component
{
public:
    FavoritesSidebar();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    void setSavedDirs(const juce::StringArray& paths, const juce::String& activePath);
    void setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeSearchIdx);
    void setBrowseMode(int mode); // 0 folder, 2 search
    void setStarredFilter(bool on);
    bool isStarredFilter() const { return starredFilter; }
    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool shouldCollapse);
    /** Current layout width (rail when folded, otherwise the resizable expanded width). */
    int idealWidth() const;
    /** Expanded width while open — never wider than the default sidebar. */
    int getExpandedWidth() const { return expandedWidth; }
    void setExpandedWidth(int w);

    std::function<void()> onOpenFolder;
    std::function<void(const juce::String&)> onPickDir;
    std::function<void(const juce::String&)> onRemoveDir;
    std::function<void()> onAddCurrent;
    std::function<void()> onShowStarred;
    std::function<void()> onShowSearch;
    std::function<void(const BrowserSearch&)> onRunSearch;
    std::function<void(int)> onPickSavedSearch;
    std::function<void(int)> onRemoveSavedSearch;
    std::function<void()> onSaveCurrentSearch;
    std::function<void()> onCopyStarredToFolder;
    std::function<void()> onCollapsedChanged;
    std::function<void()> onOpenSettings;
    /** Fired while the user drags the sidebar width (before fold). */
    std::function<void()> onWidthChanged;

    void setSearchFormOpen(bool open);
    bool isSearchFormOpen() const { return searchFormOpen; }

private:
    enum class RowKind { Open, SavedDir, Starred, Search, SavedSearch, AddSaved, SaveSearch };

    struct RowHit
    {
        bool valid = false;
        RowKind kind = RowKind::Open;
        int index = -1;
        bool removeZone = false;
    };
    void rebuildRows();
    RowHit rowHitAt(juce::Point<int> pos) const;
    juce::Rectangle<int> rowBounds(int rowIdx) const;
    void paintRow(juce::Graphics&, int rowIdx, const juce::Rectangle<int>& r, bool hovered, bool removeZone);
    int searchFormOccupiedHeight() const;

    IconBtn btnToggle { icons::sidebar, "Show or hide sidebar" };
    IconBtn btnSettings { icons::gear, "Settings" };
    juce::StringArray dirs;
    juce::String active;
    std::vector<SavedSearchEntry> savedSearches;
    int activeSearchIdx = -1;
    int browseMode = 0;
    bool starredFilter = false;
    bool collapsed = true;
    int expandedWidth = 0; // pixels; 0 → default expanded width
    int hoverRow = -1;
    bool hoverRemove = false;
    bool resizing = false;
    int resizeStartWidth = 0;
    int resizeStartX = 0;

    struct Row { RowKind kind; int index = -1; };
    std::vector<Row> rows;
    bool searchFormOpen = false;
    int searchRowIndex() const;

    class SearchInlinePanel : public juce::Component
    {
    public:
        SearchInlinePanel();
        void resized() override;
        void paint(juce::Graphics&) override;
        BrowserSearch getCriteria() const;
        void setCriteria(const BrowserSearch&);
        void lookAndFeelChanged() override;
        static constexpr int kHeight = 210;
        std::function<void(const BrowserSearch&)> onSearch;
    private:
        void styleEditors();
        juce::TextEditor queryField;
        juce::TextEditor bpmMinField;
        juce::TextEditor bpmMaxField;
        fx::FlatPopup keyPicker;
        fx::FlatPopup barsMinPopup;
        fx::FlatPopup barsMaxPopup;
        fx::FlatSwitch subdirsSwitch;
        fx::FlatTextButton btnSearch { "Search" };
        juce::Rectangle<int> bpmRow, keyRow, barsRow, subdirsRow;
    };
    SearchInlinePanel searchForm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesSidebar)
};

// Sortable file table with optional metadata columns.

struct BrowserColumnVisibility
{
    bool key = true;
    bool tempo = true;
    bool bars = true;
    bool kind = true;
    bool complexity = false;
    bool difNotes = false;
    bool timeSig = false;
    bool notes = false;
};

struct FileListEntry
{
    juce::File file;
    juce::String name;
    ClipKind kind = ClipKind::Lead;
    juce::String rootName;
    double bpm = 0.0;
    int bars = 0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int noteCount = 0;
    int difNotes = 0;
    int complexity = 0;
    bool edited = false;
    bool starred = false;
    bool isDirectory = false;
};

class FileListPanel : public juce::Component, private juce::Timer
{
public:
    enum class SortColumn
    {
        Name, Key, Tempo, Bars, Kind, Complexity, DifNotes, TimeSig, Notes
    };

    FileListPanel();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    void setFolderName(const juce::String& name);
    void setEntries(std::vector<FileListEntry> entries);
    void updateEntry(int index, const FileListEntry& entry);
    void setSelectedIndex(int index, juce::NotificationType notify = juce::sendNotification);
    int getSelectedIndex() const { return selected; }
    int getNumEntries() const { return (int) entries.size(); }
    void setPlaying(bool isPlaying);
    void setSearching(bool searching);

    void selectAdjacent(int direction);
    void grabBrowseFocus();

    void setColumnVisibility(const BrowserColumnVisibility& v);
    BrowserColumnVisibility getColumnVisibility() const { return columnsVisible; }
    std::function<void(const BrowserColumnVisibility&)> onColumnVisibilityChanged;

    std::function<void(int)> onSelect;       // entry index
    std::function<void(int)> onPlayRow;      // entry index
    std::function<void(int)> onToggleStar;   // entry index
    std::function<void()> onEnterParent;
    std::function<void(int)> onEnterFolder;  // entry index
    std::function<void(const juce::File&)> onDragFile;
    std::function<void()> onEmptyOpenFolder;
    /** Fired when the user clicks the file list (sticky browser key-nav). */
    std::function<void()> onActivated;
    std::function<void(const juce::File&)> onRevealFile;
    std::function<void(const juce::File&)> onCopyFileToFolder;

    void setSort(SortColumn column, bool ascending);

    const FileListEntry* entryAt(int index) const
    {
        return juce::isPositiveAndBelow(index, (int) entries.size())
                   ? &entries[(size_t) index] : nullptr;
    }

private:
    class ListContent : public juce::Component
    {
    public:
        explicit ListContent(FileListPanel& p) : owner(p) {}
        void paint(juce::Graphics&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void clearDragState();
        FileListPanel& owner;
        int hoverRow = -1;
        bool hoverStar = false;
        juce::String dragSourcePath;
    };

    struct ColumnRects
    {
        juce::Rectangle<int> name, key, tempo, bars, kind, complexity, difNotes, timeSig, notes;
    };

    static constexpr int kNameW = 148;
    static constexpr int kKeyW = 44;
    static constexpr int kTempoW = 52;
    static constexpr int kBarsW = 44;
    static constexpr int kKindW = 56;
    static constexpr int kComplexityW = 52;
    static constexpr int kDifNotesW = 56;
    static constexpr int kTimeSigW = 52;
    static constexpr int kNotesW = 48;
    static constexpr int kSortArrowW = 12;
    static constexpr int kIconW = 23;

    void timerCallback() override;
    void ensureRowVisible(int index);
    void paintRow(juce::Graphics&, int index, juce::Rectangle<int> r,
                  bool hovered, bool hoverStar);
    void paintColumnHeader(juce::Graphics&);
    void updateContentSize();
    void rebuildSortOrder();
    int displayToEntry(int displayIdx) const;
    int entryToDisplay(int entryIdx) const;
    juce::Rectangle<int> headerColumnBounds(SortColumn col) const;
    ColumnRects splitRowColumns(juce::Rectangle<int> row) const;
    int totalContentWidth() const;
    bool isColumnVisible(SortColumn col) const;
    void showColumnVisibilityMenu();

    juce::String folderName { "Select a folder" };
    std::vector<FileListEntry> entries;
    std::vector<int> sortOrder;
    SortColumn sortColumn = SortColumn::Name;
    bool sortAscending = true;
    BrowserColumnVisibility columnsVisible;
    int selected = -1;
    bool playing = false;
    bool searching = false;
    double searchAnimT = 0.0;

    juce::Viewport viewport;
    ListContent content { *this };
    IconBtn scrollTopBtn { icons::caretUp, "Scroll to top" };
    void updateScrollTopButton();

    friend class ListContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileListPanel)
};

} // namespace pflow
