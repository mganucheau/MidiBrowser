#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "UiAtoms.h"

namespace pflow {

// ── Favorites sidebar ────────────────────────────────────────────────────────
// Collapsed: 48px icon rail (toggle + one chip per saved folder).
// Expanded: labelled list with active highlight, add-current, remove-on-hover.

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
    RowHit hitTest(juce::Point<int> pos) const;
    juce::Rectangle<int> rowBounds(int index) const;

    IconBtn btnToggle { icons::sidebar, "Show saved folders" };
    IconBtn btnAdd { icons::plus, "Save current folder" };
    IconBtn btnTweaks { icons::gear, "Tweaks: theme, accent, spacing, grid" };
    juce::StringArray dirs;
    juce::String active;
    bool collapsed = true;
    int hoverRow = -1;
    bool hoverRemove = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesSidebar)
};

// ── File list ────────────────────────────────────────────────────────────────
// Header (folder name + count, no filter box) above scrollable rows:
// kind icon · name · root/bpm meta · edited dot · hover play · playing EQ.

struct FileListEntry
{
    juce::File file;
    juce::String name;      // display name (no extension)
    ClipKind kind = ClipKind::Keys;
    juce::String rootName;  // "C".."B" or empty
    double bpm = 0.0;
    bool edited = false;    // !editIsClean for this file
    bool starred = false;   // favourited
};

class FileListPanel : public juce::Component, private juce::Timer
{
public:
    FileListPanel();

    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;

    void setFolderName(const juce::String& name);
    void setEntries(std::vector<FileListEntry> entries);
    void updateEntry(int index, const FileListEntry& entry);
    void setSelectedIndex(int index, juce::NotificationType notify = juce::sendNotification);
    int getSelectedIndex() const { return selected; }
    int getNumEntries() const { return (int) entries.size(); }
    void setPlaying(bool isPlaying);   // equalizer on the selected row

    /** Move selection by ±1 from the keyboard; scrolls the row into view. */
    void selectAdjacent(int direction);

    std::function<void(int)> onSelect;         // row chosen (click or keys)
    std::function<void(int)> onPlayRow;        // hover play button pressed
    std::function<void(int)> onToggleStar;     // star zone clicked
    std::function<void()> onOpenFolder;        // header folder button
    std::function<void()> onToggleStarFilter;  // header star-filter button

    /** Header star-filter button: shown when the folder has starred files. */
    void setStarFilter(bool filterOn, bool anyStarred);

private:
    class ListContent : public juce::Component
    {
    public:
        explicit ListContent(FileListPanel& p) : owner(p) {}
        void paint(juce::Graphics&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;
        FileListPanel& owner;
        int hoverRow = -1;
        bool hoverPlay = false;
        bool hoverStar = false;
    };

    // Fixed right-side row zones (stable layout, no hover shifting)
    static constexpr int kPlayZoneW = 22;
    static constexpr int kMetaZoneW = 48;
    static constexpr int kStarZoneW = 16;

    void timerCallback() override;
    void ensureRowVisible(int index);
    void paintRow(juce::Graphics&, int index, juce::Rectangle<int> r,
                  bool hovered, bool hoverPlay, bool hoverStar);
    void updateContentSize();

    juce::String folderName { "Select a folder" };
    std::vector<FileListEntry> entries;
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
