#include "BrowserPanels.h"
#include <algorithm>

namespace pflow {

namespace {

const char* kindIcon(ClipKind k)
{
    switch (k)
    {
        case ClipKind::Bass:  return icons::noteBass;
        case ClipKind::Drums: return icons::noteDrums;
        default:              return icons::noteKeys;
    }
}

constexpr int kSidebarHeaderH = 34;
constexpr int kSidebarSectionH = 18;
constexpr int kSidebarRowH = 30;
constexpr int kSidebarRowHCollapsed = 40;

} // namespace

// ── SearchInlinePanel ────────────────────────────────────────────────────────

FavoritesSidebar::SearchInlinePanel::SearchInlinePanel()
{
    queryField.setTextToShowWhenEmpty("Name contains…", colours::text3());
    bpmField.setTextToShowWhenEmpty("Any", colours::text3());
    barsField.setTextToShowWhenEmpty("Any", colours::text3());
    bpmField.setInputRestrictions(6, "0123456789.");
    barsField.setInputRestrictions(4, "0123456789");
    for (auto* ed : { &queryField, &bpmField, &barsField })
    {
        ed->setFont(uiFont(12.0f, false));
        ed->setColour(juce::TextEditor::backgroundColourId, colours::elev());
        ed->setColour(juce::TextEditor::outlineColourId, colours::line());
        ed->setColour(juce::TextEditor::textColourId, colours::text());
        addAndMakeVisible(*ed);
    }

    keyPicker.addItem("Any key", 1);
    for (int i = 0; i < 12; ++i)
        keyPicker.addItem(kNoteNames[(size_t) i], i + 2);
    keyPicker.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(keyPicker);

    btnSearch.onClick = [this]
    {
        if (onSearch) onSearch(getCriteria());
    };
    addAndMakeVisible(btnSearch);
}

BrowserSearch FavoritesSidebar::SearchInlinePanel::getCriteria() const
{
    BrowserSearch s;
    s.query = queryField.getText().trim();
    s.bpm = bpmField.getText().getDoubleValue();
    s.bars = barsField.getText().getIntValue();
    const int keyId = keyPicker.getSelectedId();
    s.keyRoot = keyId <= 1 ? -1 : keyId - 2;
    s.subdirs = true;
    return s;
}

void FavoritesSidebar::SearchInlinePanel::setCriteria(const BrowserSearch& s)
{
    queryField.setText(s.query, juce::dontSendNotification);
    bpmField.setText(s.bpm > 0.0 ? juce::String(s.bpm, 1) : juce::String(), juce::dontSendNotification);
    barsField.setText(s.bars > 0 ? juce::String(s.bars) : juce::String(), juce::dontSendNotification);
    keyPicker.setSelectedId(s.keyRoot >= 0 ? s.keyRoot + 2 : 1, juce::dontSendNotification);
}

void FavoritesSidebar::SearchInlinePanel::paint(juce::Graphics& g)
{
    g.setFont(uiFont(12.0f, false));
    g.setColour(colours::text2());
    g.drawText("BPM", bpmRow.withTrimmedRight(bpmRow.getWidth() - 40), juce::Justification::centredLeft);
    g.drawText("Key", keyRow.withTrimmedRight(keyRow.getWidth() - 40), juce::Justification::centredLeft);
    g.drawText("Bars", barsRow.withTrimmedRight(barsRow.getWidth() - 40), juce::Justification::centredLeft);
}

void FavoritesSidebar::SearchInlinePanel::resized()
{
    auto r = getLocalBounds();
    queryField.setBounds(r.removeFromTop(26));
    r.removeFromTop(6);

    bpmRow = r.removeFromTop(24);
    bpmField.setBounds(bpmRow.withTrimmedLeft(44).removeFromRight(72));
    r.removeFromTop(6);

    keyRow = r.removeFromTop(24);
    keyPicker.setBounds(keyRow.withTrimmedLeft(44).removeFromRight(110));
    r.removeFromTop(6);

    barsRow = r.removeFromTop(24);
    barsField.setBounds(barsRow.withTrimmedLeft(44).removeFromRight(72));
    r.removeFromTop(8);

    btnSearch.setBounds(r.removeFromTop(26).withSizeKeepingCentre(btnSearch.idealWidth(), 24));
}

// ── FavoritesSidebar ─────────────────────────────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.ghost = true;
    btnToggle.iconScale = 1.0f;
    btnToggle.onClick = [this] { setCollapsed(!collapsed); };
    addAndMakeVisible(btnToggle);

    btnAppearance.ghost = true;
    btnAppearance.iconScale = 1.0f;
    btnAppearance.onClick = [this] { if (onToggleAppearance) onToggleAppearance(); };
    addAndMakeVisible(btnAppearance);
    refreshAppearanceIcon();

    btnTweaks.ghost = true;
    btnTweaks.iconScale = 1.0f;
    btnTweaks.onClick = [this] { if (onOpenTweaks) onOpenTweaks(); };
    addAndMakeVisible(btnTweaks);

    searchForm.setVisible(false);
    searchForm.onSearch = [this](const BrowserSearch& s)
    {
        if (onRunSearch) onRunSearch(s);
    };
    addChildComponent(searchForm);

    rebuildRows();
}

void FavoritesSidebar::refreshAppearanceIcon()
{
    btnAppearance.icon = usesDarkAppearance() ? icons::sun : icons::moon;
    btnAppearance.setTooltip(usesDarkAppearance() ? "Switch to light appearance"
                                                  : "Switch to dark appearance");
    btnAppearance.repaint();
}

void FavoritesSidebar::setSavedDirs(const juce::StringArray& paths, const juce::String& activePath)
{
    dirs = paths;
    active = activePath;
    rebuildRows();
    repaint();
}

void FavoritesSidebar::setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeIdx)
{
    savedSearches = searches;
    activeSearchIdx = activeIdx;
    rebuildRows();
    repaint();
}

void FavoritesSidebar::setBrowseMode(int mode)
{
    browseMode = mode;
    rebuildRows();
    repaint();
}

void FavoritesSidebar::setCollapsed(bool shouldCollapse)
{
    if (collapsed == shouldCollapse) return;
    collapsed = shouldCollapse;
    if (collapsed)
        searchForm.setVisible(false);
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse to icons");
    if (onCollapsedChanged) onCollapsedChanged();
    resized();
    repaint();
}

void FavoritesSidebar::rebuildRows()
{
    rows.clear();
    rows.push_back({ RowKind::Open });
    for (int i = 0; i < dirs.size(); ++i)
        rows.push_back({ RowKind::SavedDir, i });
    rows.push_back({ RowKind::AddSaved });
    rows.push_back({ RowKind::Starred });
    rows.push_back({ RowKind::Search });
    for (int i = 0; i < (int) savedSearches.size(); ++i)
        rows.push_back({ RowKind::SavedSearch, i });
    if (browseMode == 2)
        rows.push_back({ RowKind::SaveSearch });
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int rowIdx) const
{
    if (!juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return {};

    const int rowH = collapsed ? kSidebarRowHCollapsed : kSidebarRowH;
    int y = kSidebarHeaderH + 4;

    auto sectionFor = [&](RowKind k) -> bool
    {
        return rowIdx < (int) rows.size() && rows[(size_t) rowIdx].kind == k;
    };

    // Open row
    if (sectionFor(RowKind::Open))
        return collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                         : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);
    y += rowH;

    if (!collapsed)
        y += kSidebarSectionH; // SAVED

    for (int i = 0; i < dirs.size(); ++i)
    {
        if (rows[(size_t) rowIdx].kind == RowKind::SavedDir && rows[(size_t) rowIdx].index == i)
            return { 6, y, getWidth() - 12, rowH - 4 };
        y += rowH;
    }
    if (rows[(size_t) rowIdx].kind == RowKind::AddSaved)
        return { 6, y, getWidth() - 12, rowH - 4 };
    y += rowH;

    if (!collapsed)
        y += kSidebarSectionH; // FAVORITES

    if (rows[(size_t) rowIdx].kind == RowKind::Starred)
        return collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                         : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);
    y += rowH;

    if (!collapsed)
        y += kSidebarSectionH; // SEARCH

    if (rows[(size_t) rowIdx].kind == RowKind::Search)
        return collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                         : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);

    for (int i = 0; i < (int) savedSearches.size(); ++i)
    {
        if (rows[(size_t) rowIdx].kind == RowKind::SavedSearch && rows[(size_t) rowIdx].index == i)
            return { 6, y, getWidth() - 12, rowH - 4 };
        y += rowH;
    }
    if (rows[(size_t) rowIdx].kind == RowKind::SaveSearch)
        return { 6, y, getWidth() - 12, rowH - 4 };

    return {};
}

FavoritesSidebar::RowHit FavoritesSidebar::rowHitAt(juce::Point<int> pos) const
{
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto r = rowBounds(i);
        if (!r.contains(pos)) continue;
        RowHit hit;
        hit.valid = true;
        hit.kind = rows[(size_t) i].kind;
        hit.index = rows[(size_t) i].index;
        hit.removeZone = !collapsed
            && (hit.kind == RowKind::SavedDir || hit.kind == RowKind::SavedSearch)
            && pos.x > r.getRight() - 22;
        return hit;
    }
    return {};
}

int FavoritesSidebar::searchRowIndex() const
{
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].kind == RowKind::Search)
            return i;
    return -1;
}

void FavoritesSidebar::setSearchFormOpen(bool open)
{
    if (searchFormOpen == open) return;
    searchFormOpen = open;
    if (searchFormOpen)
        searchForm.setCriteria({});
    searchForm.setVisible(searchFormOpen && !collapsed);
    resized();
    repaint();
}

void FavoritesSidebar::resized()
{
    btnToggle.setBounds(juce::Rectangle<int>(0, 0, metrics::sidebarRailW, kSidebarHeaderH)
                            .withSizeKeepingCentre(26, 26));

    // Appearance sits directly above Settings, same control size.
    constexpr int railBtn = 26;
    constexpr int railPad = 4;
    auto rail = juce::Rectangle<int>(0, getHeight() - (railBtn * 2 + railPad + 8),
                                     metrics::sidebarRailW, railBtn * 2 + railPad);
    btnAppearance.setBounds(rail.removeFromTop(railBtn).withSizeKeepingCentre(railBtn, railBtn));
    rail.removeFromTop(railPad);
    btnTweaks.setBounds(rail.removeFromTop(railBtn).withSizeKeepingCentre(railBtn, railBtn));

    if (searchFormOpen && !collapsed)
    {
        const int idx = searchRowIndex();
        if (idx >= 0)
        {
            const auto r = rowBounds(idx);
            searchForm.setBounds(6, r.getBottom() + 4, getWidth() - 12, SearchInlinePanel::kHeight);
            searchForm.setVisible(true);
            return;
        }
    }
    searchForm.setVisible(false);
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = rowHitAt(e.getPosition());
    const int rowIdx = hit.valid ? [&]
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
                return i;
        return -1;
    }() : -1;
    if (rowIdx != hoverRow || hit.removeZone != hoverRemove)
    {
        hoverRow = rowIdx;
        hoverRemove = hit.removeZone;
        repaint();
    }
}

void FavoritesSidebar::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverRemove = false;
    repaint();
}

void FavoritesSidebar::mouseDown(const juce::MouseEvent& e)
{
    if (searchForm.isVisible() && searchForm.getBounds().contains(e.getPosition()))
        return;

    const auto hit = rowHitAt(e.getPosition());
    if (!hit.valid) return;
    if (hit.removeZone)
    {
        if (hit.kind == RowKind::SavedDir && onRemoveDir)
            onRemoveDir(dirs[hit.index]);
        else if (hit.kind == RowKind::SavedSearch && onRemoveSavedSearch)
            onRemoveSavedSearch(hit.index);
        return;
    }

    switch (hit.kind)
    {
        case RowKind::Open:     if (onOpenFolder) onOpenFolder(); break;
        case RowKind::SavedDir: if (onPickDir) onPickDir(dirs[hit.index]); break;
        case RowKind::AddSaved: if (onAddCurrent) onAddCurrent(); break;
        case RowKind::Starred:  if (onShowStarred) onShowStarred(); break;
        case RowKind::Search:   if (onShowSearch) onShowSearch(); break;
        case RowKind::SavedSearch: if (onPickSavedSearch) onPickSavedSearch(hit.index); break;
        case RowKind::SaveSearch: if (onSaveCurrentSearch) onSaveCurrentSearch(); break;
    }
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone)
{
    const auto& row = rows[(size_t) rowIdx];
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeStar = row.kind == RowKind::Starred && browseMode == 1;
    const bool activeSearch = (row.kind == RowKind::Search && browseMode == 2)
        || (row.kind == RowKind::SavedSearch && row.index == activeSearchIdx);
    const bool isActive = activeDir || activeStar || activeSearch;

    if (isActive || hovered)
    {
        if (isActive)
        {
            g.setColour(colours::accent());
            g.fillRoundedRectangle(r.toFloat(), 7.0f);
        }
        else
        {
            g.setColour(colours::elev().withAlpha(0.85f));
            g.fillRoundedRectangle(r.toFloat(), 7.0f);
        }
    }

    const auto iconCol = isActive ? juce::Colours::white : colours::accent();
    const char* icon = icons::folder;
    juce::String label;
    switch (row.kind)
    {
        case RowKind::Open:       icon = icons::folderOpen; label = "Open a folder"; break;
        case RowKind::SavedDir:
        {
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            break;
        }
        case RowKind::AddSaved:   icon = icons::plus; label = "Save current folder"; break;
        case RowKind::Starred:    icon = icons::star; label = "Starred"; break;
        case RowKind::Search:     icon = icons::search; label = "Search"; break;
        case RowKind::SavedSearch:
            icon = icons::search;
            label = savedSearches[(size_t) row.index].name;
            break;
        case RowKind::SaveSearch: icon = icons::plus; label = "Save this search"; break;
    }

    if (collapsed)
    {
        drawIcon(g, icon, r.toFloat().reduced(10.0f), iconCol, 1.5f);
        return;
    }

    auto textArea = r.reduced(7, 0);
    drawIcon(g, icon, textArea.removeFromLeft(16).toFloat().withSizeKeepingCentre(14.0f, 14.0f),
             iconCol, 1.4f);
    textArea.removeFromLeft(6);
    if (hovered && (row.kind == RowKind::SavedDir || row.kind == RowKind::SavedSearch))
    {
        auto xArea = textArea.removeFromRight(18).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, icons::x, xArea,
                 removeZone ? (isActive ? juce::Colours::white : colours::text())
                            : (isActive ? juce::Colours::white.withAlpha(0.7f) : colours::text3()),
                 1.4f);
    }
    g.setColour(isActive ? juce::Colours::white : colours::text());
    g.setFont(uiFont(12.5f, isActive));
    g.drawText(label, textArea, juce::Justification::centredLeft, true);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::bg());
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));

    if (!collapsed)
    {
        auto headerFont = uiFont(10.0f, true);
        headerFont.setExtraKerningFactor(0.08f);
        g.setColour(colours::text3());
        g.setFont(headerFont);
        g.drawText("LIBRARY", metrics::sidebarRailW - 4, 0, 100, kSidebarHeaderH,
                   juce::Justification::centredLeft);
    }

    const int rowH = collapsed ? kSidebarRowHCollapsed : kSidebarRowH;
    int y = kSidebarHeaderH + 4;
    bool drewSaved = false, drewFav = false, drewSearch = false;

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto kind = rows[(size_t) i].kind;
        if (!collapsed)
        {
            auto sectionFont = uiFont(10.0f, true);
            sectionFont.setExtraKerningFactor(0.08f);
            if (!drewSaved && kind == RowKind::SavedDir)
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("FAVORITES", 10, y, getWidth() - 16, kSidebarSectionH,
                           juce::Justification::centredLeft);
                y += kSidebarSectionH;
                drewSaved = true;
            }
            if (!drewFav && kind == RowKind::Starred)
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("STARRED", 10, y, getWidth() - 16, kSidebarSectionH,
                           juce::Justification::centredLeft);
                y += kSidebarSectionH;
                drewFav = true;
            }
            if (!drewSearch && kind == RowKind::Search)
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("SEARCH", 10, y, getWidth() - 16, kSidebarSectionH,
                           juce::Justification::centredLeft);
                y += kSidebarSectionH;
                drewSearch = true;
            }
        }

        const auto r = collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                                   : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove);
        y += rowH;
    }
}

// ── FileListPanel ────────────────────────────────────────────────────────────

FileListPanel::FileListPanel()
{
    setWantsKeyboardFocus(true);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);
    startTimerHz(20);
}

void FileListPanel::grabBrowseFocus()
{
    grabKeyboardFocus();
}

void FileListPanel::setFolderName(const juce::String& name)
{
    folderName = name;
    repaint();
}

void FileListPanel::setEntries(std::vector<FileListEntry> e)
{
    entries = std::move(e);
    selected = juce::jlimit(-1, (int) entries.size() - 1, selected);
    content.clearDragState();
    rebuildSortOrder();
    updateContentSize();
    content.repaint();
    repaint();
}

void FileListPanel::updateEntry(int index, const FileListEntry& entry)
{
    if (!juce::isPositiveAndBelow(index, (int) entries.size()))
        return;
    entries[(size_t) index] = entry;
    rebuildSortOrder();
    content.repaint();
}

void FileListPanel::setSelectedIndex(int index, juce::NotificationType notify)
{
    selected = juce::jlimit(-1, (int) entries.size() - 1, index);
    ensureRowVisible(entryToDisplay(selected));
    content.repaint();
    if (notify != juce::dontSendNotification && onSelect && selected >= 0)
        onSelect(entryToDisplay(selected));
}

void FileListPanel::setPlaying(bool isPlaying)
{
    playing = isPlaying;
    content.repaint();
}

void FileListPanel::setSearching(bool isSearch)
{
    searching = isSearch;
    if (searching)
        startTimerHz(12);
    else if (!playing)
        stopTimer();
    repaint();
}

void FileListPanel::setSort(SortColumn column, bool ascending)
{
    sortColumn = column;
    sortAscending = ascending;
    rebuildSortOrder();
    content.repaint();
    repaint();
}

void FileListPanel::rebuildSortOrder()
{
    sortOrder.resize(entries.size());
    for (int i = 0; i < (int) entries.size(); ++i)
        sortOrder[(size_t) i] = i;

    std::stable_sort(sortOrder.begin(), sortOrder.end(), [this](int a, int b)
    {
        const auto& ea = entries[(size_t) a];
        const auto& eb = entries[(size_t) b];
        // Folders always first.
        if (ea.isDirectory != eb.isDirectory)
            return ea.isDirectory && !eb.isDirectory;

        int cmp = 0;
        switch (sortColumn)
        {
            case SortColumn::Name:
                cmp = ea.name.compareNatural(eb.name);
                break;
            case SortColumn::Key:
                cmp = ea.rootName.compareNatural(eb.rootName);
                break;
            case SortColumn::Tempo:
                cmp = (ea.bpm < eb.bpm) ? -1 : (ea.bpm > eb.bpm ? 1 : 0);
                break;
            case SortColumn::Bars:
                cmp = ea.bars - eb.bars;
                break;
        }
        if (cmp == 0)
            cmp = ea.name.compareNatural(eb.name);
        return sortAscending ? cmp < 0 : cmp > 0;
    });
}

int FileListPanel::displayToEntry(int displayIdx) const
{
    if (!juce::isPositiveAndBelow(displayIdx, (int) sortOrder.size()))
        return -1;
    return sortOrder[(size_t) displayIdx];
}

int FileListPanel::entryToDisplay(int entryIdx) const
{
    for (int i = 0; i < (int) sortOrder.size(); ++i)
        if (sortOrder[(size_t) i] == entryIdx)
            return i;
    return -1;
}

void FileListPanel::selectAdjacent(int direction)
{
    if (sortOrder.empty()) return;
    int disp = entryToDisplay(selected);
    if (disp < 0) disp = direction > 0 ? -1 : (int) sortOrder.size();
    disp = juce::jlimit(0, (int) sortOrder.size() - 1, disp + direction);
    setSelectedIndex(displayToEntry(disp));
}

bool FileListPanel::keyPressed(const juce::KeyPress& key)
{
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr
        || dynamic_cast<juce::ComboBox*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    if (!hasKeyboardFocus(true))
        grabKeyboardFocus();

    if (key == juce::KeyPress::upKey)    { selectAdjacent(-1); return true; }
    if (key == juce::KeyPress::downKey)  { selectAdjacent(1); return true; }
    if (key == juce::KeyPress::leftKey)
    {
        if (onEnterParent) onEnterParent();
        return true;
    }
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::returnKey)
    {
        if (selected >= 0 && entries[(size_t) selected].isDirectory)
        {
            const int disp = entryToDisplay(selected);
            if (disp >= 0 && onEnterFolder)
                onEnterFolder(disp);
        }
        return true;
    }

    const int pageRows = juce::jmax(1, viewport.getMaximumVisibleHeight() / metrics::listRowH());
    if (key == juce::KeyPress::pageUpKey)   { selectAdjacent(-pageRows); return true; }
    if (key == juce::KeyPress::pageDownKey) { selectAdjacent(pageRows); return true; }
    return false;
}

void FileListPanel::ensureRowVisible(int displayIdx)
{
    if (displayIdx < 0) return;
    const int rowH = metrics::listRowH();
    const int rowTop = displayIdx * rowH;
    const int rowBottom = rowTop + rowH;
    const int viewTop = viewport.getViewPositionY();
    const int viewH = viewport.getMaximumVisibleHeight();
    if (rowTop < viewTop)
        viewport.setViewPosition(0, rowTop);
    else if (rowBottom > viewTop + viewH)
        viewport.setViewPosition(0, rowBottom - viewH);
}

void FileListPanel::updateContentSize()
{
    const int w = juce::jmax(1, viewport.getMaximumVisibleWidth());
    if (entries.empty())
        content.setSize(w, juce::jmax(1, viewport.getMaximumVisibleHeight()));
    else
        content.setSize(w, juce::jmax(1, (int) entries.size() * metrics::listRowH()));
}

void FileListPanel::timerCallback()
{
    if (searching)
    {
        searchAnimT = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        repaint();
        return;
    }
    if (playing && selected >= 0)
        content.repaint(0, entryToDisplay(selected) * metrics::listRowH(),
                        content.getWidth(), metrics::listRowH());
}

juce::Rectangle<int> FileListPanel::headerColumnBounds(SortColumn col) const
{
    auto h = getLocalBounds().removeFromTop(metrics::listHeaderH()).reduced(8, 0);
    FileListPanel::ColumnRects cols;
    auto row = h;
    cols.bars = row.removeFromRight(kBarsW);
    cols.tempo = row.removeFromRight(kTempoW);
    cols.key = row.removeFromRight(kKeyW);
    cols.name = row;
    switch (col)
    {
        case SortColumn::Name:  return cols.name;
        case SortColumn::Key:   return cols.key;
        case SortColumn::Tempo: return cols.tempo;
        case SortColumn::Bars:  return cols.bars;
    }
    return {};
}

FileListPanel::ColumnRects FileListPanel::splitRowColumns(juce::Rectangle<int> row) const
{
    ColumnRects cols;
    row = row.reduced(8, 0);
    cols.bars = row.removeFromRight(kBarsW);
    cols.tempo = row.removeFromRight(kTempoW);
    cols.key = row.removeFromRight(kKeyW);
    cols.name = row;
    return cols;
}

void FileListPanel::resized()
{
    viewport.setBounds(getLocalBounds().withTrimmedTop(metrics::listHeaderH()));
    updateContentSize();
}

void FileListPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    paintColumnHeader(g);
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));

    if (searching)
    {
        auto area = getLocalBounds().withTrimmedTop(metrics::listHeaderH());
        g.setColour(colours::text2());
        g.setFont(uiFont(13.0f, false));
        const int dots = 1 + (int) std::fmod(searchAnimT * 2.0, 3.0);
        g.drawText("Searching" + juce::String::repeatedString(".", dots),
                   area, juce::Justification::centred);
        return;
    }

    if (entries.empty())
    {
        auto area = getLocalBounds().withTrimmedTop(metrics::listHeaderH()).reduced(24, 32);
        g.setColour(colours::text3());
        g.setFont(uiFont(13.0f, false));
        g.drawText("No files to show", area, juce::Justification::centred);
    }
}

void FileListPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= metrics::listHeaderH())
        return;

    for (auto col : { SortColumn::Name, SortColumn::Key, SortColumn::Tempo, SortColumn::Bars })
    {
        if (headerColumnBounds(col).contains(e.getPosition()))
        {
            if (sortColumn == col)
                sortAscending = !sortAscending;
            else
            {
                sortColumn = col;
                sortAscending = true;
            }
            rebuildSortOrder();
            content.repaint();
            repaint();
            return;
        }
    }
}

void FileListPanel::paintColumnHeader(juce::Graphics& g)
{
    auto header = getLocalBounds().removeFromTop(metrics::listHeaderH());
    g.setColour(colours::line());
    g.fillRect(header.removeFromBottom(1));

    auto drawCol = [&](SortColumn col, const juce::String& label, juce::Justification just)
    {
        auto r = headerColumnBounds(col);
        const bool active = sortColumn == col;
        g.setColour(active ? colours::text() : colours::text3());
        g.setFont(uiFont(10.5f, true));
        juce::String text = label;
        if (active)
            text += sortAscending ? " ▲" : " ▼";
        g.drawText(text, r, just, true);
    };

    drawCol(SortColumn::Name, "Name", juce::Justification::centredLeft);
    drawCol(SortColumn::Key, "Key", juce::Justification::centredRight);
    drawCol(SortColumn::Tempo, "Tempo", juce::Justification::centredRight);
    drawCol(SortColumn::Bars, "Bars", juce::Justification::centredRight);
}

void FileListPanel::paintRow(juce::Graphics& g, int displayIdx, juce::Rectangle<int> r,
                             bool hovered, bool hoverStar)
{
    const int entryIdx = displayToEntry(displayIdx);
    if (entryIdx < 0) return;
    const auto& e = entries[(size_t) entryIdx];
    const bool isSelected = entryIdx == selected;

    if (isSelected)
    {
        g.setColour(colours::accent());
        g.fillRect(r);
    }
    else if (hovered)
    {
        g.setColour(colours::elev().withAlpha(0.55f));
        g.fillRect(r);
    }
    else if ((displayIdx % 2) == 1)
    {
        g.setColour(colours::tableAlt());
        g.fillRect(r);
    }

    const auto textCol = isSelected ? juce::Colours::white : colours::text2();
    const auto metaCol = isSelected ? juce::Colours::white.withAlpha(0.8f) : colours::text3();

    auto cols = splitRowColumns(r);
    auto row = cols.name;
    auto barsZone = cols.bars;
    auto tempoZone = cols.tempo;
    auto keyZone = cols.key;

    // Leading star (files) or folder icon — solid star for both states.
    auto iconArea = row.removeFromLeft(17).toFloat().withSizeKeepingCentre(14.0f, 14.0f);
    if (e.isDirectory)
    {
        drawIcon(g, icons::folder, iconArea,
                 isSelected ? juce::Colours::white : colours::accent(), 1.4f);
    }
    else
    {
        const auto starCol = e.starred
            ? (isSelected ? juce::Colours::white : colours::accent())
            : (hoverStar ? textCol : metaCol.withAlpha(isSelected ? 0.55f : 0.45f));
        drawIcon(g, icons::star, iconArea, starCol, 1.55f);
    }
    row.removeFromLeft(6);

    if (e.isDirectory)
    {
        g.setColour(isSelected ? juce::Colours::white : colours::text());
        g.setFont(uiFont(12.0f, true));
        g.drawText(e.name, row, juce::Justification::centredLeft, true);
        return;
    }

    g.setFont(monoFont(11.0f, false));
    g.setColour(metaCol);
    if (e.rootName.isNotEmpty())
        g.drawText(e.rootName, keyZone, juce::Justification::centredRight);
    if (e.bpm > 0.0)
        g.drawText(juce::String((int) std::lround(e.bpm)), tempoZone, juce::Justification::centredRight);
    if (e.bars > 0)
        g.drawText(juce::String(e.bars), barsZone, juce::Justification::centredRight);

    if (e.edited)
    {
        auto dot = row.removeFromRight(8).toFloat().withSizeKeepingCentre(5.0f, 5.0f);
        g.setColour(isSelected ? juce::Colours::white : colours::accent());
        g.fillEllipse(dot);
        row.removeFromRight(1);
    }

    g.setColour(isSelected ? juce::Colours::white : colours::text());
    g.setFont(uiFont(12.0f, false));
    g.drawText(e.name, row, juce::Justification::centredLeft, true);
}

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    const int rowH = metrics::listRowH();
    const auto clip = g.getClipBounds();
    const int first = juce::jmax(0, clip.getY() / rowH);
    const int last = juce::jmin((int) owner.sortOrder.size() - 1, clip.getBottom() / rowH);
    for (int i = first; i <= last; ++i)
        owner.paintRow(g, i, { 0, i * rowH, getWidth(), rowH }, i == hoverRow,
                       i == hoverRow && hoverStar);
}

void FileListPanel::ListContent::mouseMove(const juce::MouseEvent& e)
{
    const int rowH = metrics::listRowH();
    const int disp = e.y / rowH;
    const bool valid = juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size());
    const int entryIdx = valid ? owner.displayToEntry(disp) : -1;
    const bool isDir = entryIdx >= 0 && owner.entries[(size_t) entryIdx].isDirectory;
    // Star sits in the leading icon slot of the name column.
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();
    const bool newHoverStar = valid && !isDir && e.x >= starLeft && e.x < starLeft + 20;

    if (disp != hoverRow || newHoverStar != hoverStar)
    {
        hoverRow = valid ? disp : -1;
        hoverStar = newHoverStar;
        repaint();
    }
}

void FileListPanel::ListContent::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverStar = false;
    repaint();
}

void FileListPanel::ListContent::mouseDown(const juce::MouseEvent& e)
{
    // Column header clicks are handled by the parent panel.
    if (e.y < 0) return;

    // Sort header lives on the parent — check if click was forwarded from header.
    const int rowH = metrics::listRowH();
    const int disp = e.y / rowH;
    if (!juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size()))
        return;

    const int entryIdx = owner.displayToEntry(disp);
    if (entryIdx < 0) return;
    const auto& entry = owner.entries[(size_t) entryIdx];
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), metrics::listRowH() });
    const int starLeft = cols.name.getX();

    if (!entry.isDirectory && e.x >= starLeft && e.x < starLeft + 20)
    {
        if (owner.onToggleStar) owner.onToggleStar(owner.entryToDisplay(entryIdx));
        return;
    }

    owner.setSelectedIndex(entryIdx);
    if (!entry.isDirectory)
    {
        dragSourcePath = entry.file.getFullPathName();
        if (dragSourcePath.isEmpty())
            clearDragState();
    }
    else
        clearDragState();

    if (entry.isDirectory && e.getNumberOfClicks() > 1 && owner.onEnterFolder)
        owner.onEnterFolder(disp);
}

void FileListPanel::ListContent::clearDragState()
{
    dragSourcePath.clear();
}

void FileListPanel::ListContent::mouseUp(const juce::MouseEvent&)
{
    clearDragState();
}

void FileListPanel::ListContent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragSourcePath.isEmpty() || e.getDistanceFromDragStart() < 8)
        return;

    const juce::File file(dragSourcePath);
    clearDragState();

    if (!file.getFullPathName().isNotEmpty() || !file.existsAsFile())
        return;

    if (owner.onDragFile)
        owner.onDragFile(file);
}

} // namespace pflow
