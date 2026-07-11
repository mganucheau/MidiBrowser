#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "UiAtoms.h"

namespace pflow {

// Cupertino source sidebar: collapsible rail (icons) or expanded list.

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
    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool shouldCollapse);
    int idealWidth() const { return collapsed ? metrics::sidebarRailW : metrics::sidebarExpandedW; }

    std::function<void(const juce::String&)> onPickDir;
    std::function<void(const juce::String&)> onRemoveDir;
    std::function<void()> onAddCurrent;
    std::function<void()> onCollapsedChanged;
    std::function<void()> onOpenTweaks;

private:
    struct RowHit { int index = -1; bool removeZone = false; };
    RowHit rowHitAt(juce::Point<int> pos) const;
    juce::Rectangle<int> rowBounds(int index) const;

    IconBtn btnToggle { icons::sidebar, "Show or hide saved folders" };
    IconBtn btnAdd { icons::plus, "Save current folder" };
    IconBtn btnTweaks { icons::gear, "Tweaks: spacing, content size" };
    juce::StringArray dirs;
    juce::String active;
    bool collapsed = true;
    int hoverRow = -1;
    bool hoverRemove = false;

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

    void selectAdjacent(int direction);

    std::function<void(int)> onSelect;
    std::function<void(int)> onPlayRow;
    std::function<void(int)> onToggleStar;
    std::function<void()> onOpenFolder;
    std::function<void()> onToggleStarFilter;
    std::function<void()> onEnterParent;
    std::function<void(int)> onEnterFolder;
    /** Drag original MIDI file to the DAW (no edits applied). */
    std::function<void(const juce::File&)> onDragFile;

    void setStarFilter(bool filterOn, bool anyStarred);
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

    static constexpr int kKeyW = 40;
    static constexpr int kTempoW = 48;
    static constexpr int kBarsW = 40;
    static constexpr int kPlayZoneW = 20;
    static constexpr int kStarZoneW = 18;

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

    juce::String folderName { "Select a folder" };
    std::vector<FileListEntry> entries;
    std::vector<int> sortOrder;   // display index → entry index
    SortColumn sortColumn = SortColumn::Name;
    bool sortAscending = true;
    int selected = -1;
    bool playing = false;

    juce::Viewport viewport;
    ListContent content { *this };
    IconBtn btnOpen { icons::folderOpen, "Open a folder of MIDI files" };
    IconBtn btnStarFilter { icons::star, "Show only starred files" };

    friend class ListContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileListPanel)
};

} // namespace pflow
