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
    int complexityMin = 0; // 0 = no lower bound
    int complexityMax = 0; // 0 = no upper bound
    bool subdirs = true;
    /** Collapse content-identical hits from multiple folders into one row. */
    bool removeDuplicates = true;
};

struct SavedSearchEntry
{
    juce::String name;
    BrowserSearch search;
};

// Photos Caps B2 library rail: Open · Saved (Favorites + folders) · Search.

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
    bool keyPressed(const juce::KeyPress&) override;

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
    /** Full column height: header + rows + search form + settings footer. */
    int idealMinHeight() const;

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
    /** Fired when search expand/collapse changes the column’s natural height. */
    std::function<void()> onContentHeightChanged;

    void setSearchFormOpen(bool open);
    bool isSearchFormOpen() const { return searchFormOpen; }
    /** Collapse filters + unfocus; keep query text unless clearCriteria. */
    void deactivateSearch(bool clearCriteria = false);

private:
    enum class RowKind { Open, SavedDir, Starred, Search, SavedSearch };

    struct RowHit
    {
        bool valid = false;
        RowKind kind = RowKind::Open;
        int index = -1;
        bool removeZone = false;
    };
    enum class ChromeHit { None, SavedPlus, SearchPlus, SearchTitle, Settings };
    void rebuildRows();
    RowHit rowHitAt(juce::Point<int> pos) const;
    ChromeHit chromeHitAt(juce::Point<int> pos) const;
    juce::Rectangle<int> rowBounds(int rowIdx) const;
    void paintRow(juce::Graphics&, int rowIdx, const juce::Rectangle<int>& r, bool hovered, bool removeZone);
    int searchFormOccupiedHeight() const;
    void layoutSearchForm();
    int contentTopY() const;
    int contentBottomY() const;
    int footerHeight() const;

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
    bool hoverSettings = false;
    bool resizing = false;
    int resizeStartWidth = 0;
    int resizeStartX = 0;
    juce::Rectangle<int> savedPlusBounds, searchPlusBounds, searchTitleBounds, settingsRowBounds;

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
        void mouseDown(const juce::MouseEvent&) override;
        BrowserSearch getCriteria() const;
        void setCriteria(const BrowserSearch&);
        void lookAndFeelChanged() override;
        void setFiltersExpanded(bool on);
        bool areFiltersExpanded() const { return filtersExpanded; }
        int idealHeight() const;
        static constexpr int kQueryH = 26;
        static constexpr int kKeySelectW = 72; // Caps B2 SELECT_W
        std::function<void(const BrowserSearch&)> onSearch;
        std::function<void()> onActivate; // click / focus query → expand filters
        std::function<void()> onClear;    // clear × → collapse + reset
        std::function<void()> onDeactivate; // ESC from the query field
        std::function<void()> onHeightChanged;
        void focusQuery();
        void blurQuery();
        bool isQueryFocused() const;
    private:
        void styleEditors();
        void syncRangeLabels();
        void updateClearVisible();

        struct QueryEditor : juce::TextEditor
        {
            std::function<void()> onFocused;
            std::function<void()> onEscape;
            std::function<void()> onFocusChanged;
            void focusGained(FocusChangeType cause) override
            {
                juce::TextEditor::focusGained(cause);
                if (onFocused) onFocused();
                if (onFocusChanged) onFocusChanged();
            }
            void focusLost(FocusChangeType cause) override
            {
                juce::TextEditor::focusLost(cause);
                if (onFocusChanged) onFocusChanged();
            }
            bool keyPressed(const juce::KeyPress& key) override
            {
                if (key == juce::KeyPress::escapeKey)
                {
                    if (onEscape) onEscape();
                    return true;
                }
                return juce::TextEditor::keyPressed(key);
            }
        };

        QueryEditor queryField;
        IconBtn clearBtn { icons::x, "Clear search" };
        fx::FlatPopup keyPicker;
        fx::FlatRangeSliderRow bpmRange { "BPM", 40, 240, 40, 240 };
        fx::FlatRangeSliderRow barsRange { "Bars", 1, 64, 1, 64 };
        fx::FlatRangeSliderRow complexityRange { "Complexity", 0, 100, 0, 100 };
        fx::FlatSwitch subdirsSwitch;
        fx::FlatSwitch dedupeSwitch;
        fx::FlatTextButton btnSearch { "Search" };
        juce::Rectangle<int> keyRow, subdirsRow, dedupeRow, queryWell;
        bool filtersExpanded = false;
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
    /** When remove-duplicates collapsed several paths: Location 1..N for Finder. */
    juce::StringArray locations;
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
    /** Pixel width needed to show every visible column without horizontal scroll. */
    int idealContentWidth() const { return totalContentWidth(); }
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
    int lastViewX = 0;

    juce::Viewport viewport;
    ListContent content { *this };
    IconBtn scrollTopBtn { icons::caretUp, "Scroll to top" };
    void updateScrollTopButton();

    friend class ListContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileListPanel)
};

} // namespace pflow
