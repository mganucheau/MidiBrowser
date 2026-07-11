#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "UiAtoms.h"

namespace pflow {

struct BrowserSearch
{
    juce::String query;
    double bpm = 0.0;   // 0 = any
    int keyRoot = -1;   // -1 = any
    int bars = 0;       // 0 = any
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

    void setSavedDirs(const juce::StringArray& paths, const juce::String& activePath);
    void setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeSearchIdx);
    void setBrowseMode(int mode); // 0 folder, 1 starred, 2 search
    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool shouldCollapse);
    int idealWidth() const { return collapsed ? metrics::sidebarRailW : metrics::sidebarExpandedW; }

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
    std::function<void()> onCollapsedChanged;
    std::function<void()> onOpenTweaks;

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

    IconBtn btnToggle { icons::sidebar, "Show or hide sidebar" };
    IconBtn btnTweaks { icons::gear, "Tweaks: spacing, content size" };
    juce::StringArray dirs;
    juce::String active;
    std::vector<SavedSearchEntry> savedSearches;
    int activeSearchIdx = -1;
    int browseMode = 0;
    bool collapsed = true;
    int hoverRow = -1;
    bool hoverRemove = false;

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
        static constexpr int kHeight = 148;
        std::function<void(const BrowserSearch&)> onSearch;
    private:
        juce::TextEditor queryField;
        juce::TextEditor bpmField;
        juce::ComboBox keyPicker;
        juce::TextEditor barsField;
        ChipBtn btnSearch { "Search" };
        juce::Rectangle<int> bpmRow, keyRow, barsRow;
    };
    SearchInlinePanel searchForm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesSidebar)
};

// Sortable 4-column file table: Name · Key · Tempo · Bars.

struct FileListEntry
{
    juce::File file;
    juce::String name;
    ClipKind kind = ClipKind::Keys;
    juce::String rootName;
    double bpm = 0.0;
    int bars = 0;
    bool edited = false;
    bool starred = false;
    bool isDirectory = false;
};

class FileListPanel : public juce::Component, private juce::Timer
{
public:
    enum class SortColumn { Name, Key, Tempo, Bars };

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

    std::function<void(int)> onSelect;
    std::function<void(int)> onPlayRow;
    std::function<void(int)> onToggleStar;
    std::function<void()> onEnterParent;
    std::function<void(int)> onEnterFolder;
    std::function<void(const juce::File&)> onDragFile;

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
        bool hoverPlay = false;
        bool hoverStar = false;
        juce::String dragSourcePath;
    };

    struct ColumnRects
    {
        juce::Rectangle<int> name, key, tempo, bars, star, play;
    };

    static constexpr int kKeyW = 40;
    static constexpr int kTempoW = 48;
    static constexpr int kBarsW = 40;
    static constexpr int kPlayZoneW = 20;
    static constexpr int kStarZoneW = 18;
    static constexpr int kIconW = 23;

    void timerCallback() override;
    void ensureRowVisible(int index);
    void paintRow(juce::Graphics&, int index, juce::Rectangle<int> r,
                  bool hovered, bool hoverPlay, bool hoverStar);
    void paintColumnHeader(juce::Graphics&);
    void updateContentSize();
    void rebuildSortOrder();
    int displayToEntry(int displayIdx) const;
    int entryToDisplay(int entryIdx) const;
    juce::Rectangle<int> headerColumnBounds(SortColumn col) const;
    ColumnRects splitRowColumns(juce::Rectangle<int> row) const;

    juce::String folderName { "Select a folder" };
    std::vector<FileListEntry> entries;
    std::vector<int> sortOrder;
    SortColumn sortColumn = SortColumn::Name;
    bool sortAscending = true;
    int selected = -1;
    bool playing = false;
    bool searching = false;
    double searchAnimT = 0.0;

    juce::Viewport viewport;
    ListContent content { *this };

    friend class ListContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileListPanel)
};

} // namespace pflow
