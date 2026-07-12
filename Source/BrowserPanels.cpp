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

inline int sidebarHeaderH() { return metrics::scaled(kSidebarHeaderH); }
inline int sidebarSectionH() { return metrics::scaled(kSidebarSectionH); }
inline int sidebarRowH() { return metrics::scaled(kSidebarRowH); }
inline int sidebarRowHCollapsed() { return metrics::scaled(kSidebarRowHCollapsed); }

} // namespace

// ── SearchInlinePanel ────────────────────────────────────────────────────────

FavoritesSidebar::SearchInlinePanel::SearchInlinePanel()
{
    styleEditors();
    for (auto* ed : { &queryField, &bpmField, &barsField })
        addAndMakeVisible(*ed);

    bpmField.setInputRestrictions(6, "0123456789.");
    barsField.setInputRestrictions(4, "0123456789");

    juce::StringArray keys;
    keys.add("Any");
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyPicker.setItems(keys, 0);
    addAndMakeVisible(keyPicker);

    subdirsSwitch.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(subdirsSwitch);

    btnSearch.onClick = [this]
    {
        if (onSearch) onSearch(getCriteria());
    };
    btnSearch.active = true;
    addAndMakeVisible(btnSearch);
}

void FavoritesSidebar::SearchInlinePanel::styleEditors()
{
    const auto& t = inspectorTokens();
    queryField.setTextToShowWhenEmpty("Name contains…", t.valueText);
    bpmField.setTextToShowWhenEmpty("Any", t.valueText);
    barsField.setTextToShowWhenEmpty("Any", t.valueText);

    for (auto* ed : { &queryField, &bpmField, &barsField })
    {
        ed->setFont(fx::inspectorFont());
        // Transparent outside the control surface — only the field itself paints.
        ed->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        ed->setColour(juce::TextEditor::focusedOutlineColourId, t.accent);
        ed->setColour(juce::TextEditor::outlineColourId, t.controlHairline);
        ed->setColour(juce::TextEditor::textColourId, t.rowLabel);
        ed->setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        ed->setColour(juce::CaretComponent::caretColourId, t.accent);
        ed->setIndents(8, 2);
    }
}

void FavoritesSidebar::SearchInlinePanel::lookAndFeelChanged()
{
    styleEditors();
    repaint();
}

BrowserSearch FavoritesSidebar::SearchInlinePanel::getCriteria() const
{
    BrowserSearch s;
    s.query = queryField.getText().trim();
    s.bpm = bpmField.getText().getDoubleValue();
    s.bars = barsField.getText().getIntValue();
    const int idx = keyPicker.getIndex();
    s.keyRoot = idx <= 0 ? -1 : idx - 1;
    s.subdirs = subdirsSwitch.getToggleState();
    return s;
}

void FavoritesSidebar::SearchInlinePanel::setCriteria(const BrowserSearch& s)
{
    queryField.setText(s.query, juce::dontSendNotification);
    bpmField.setText(s.bpm > 0.0 ? juce::String(s.bpm, 1) : juce::String(), juce::dontSendNotification);
    barsField.setText(s.bars > 0 ? juce::String(s.bars) : juce::String(), juce::dontSendNotification);
    keyPicker.setIndex(s.keyRoot >= 0 ? s.keyRoot + 1 : 0, juce::dontSendNotification);
    subdirsSwitch.setToggleState(s.subdirs, juce::dontSendNotification);
}

void FavoritesSidebar::SearchInlinePanel::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    // Match effects-column control surfaces for text fields.
    auto paintField = [&](juce::Rectangle<int> r)
    {
        drawInspectorControlSurface(g, r.toFloat(), false, false);
    };
    paintField(queryField.getBounds());
    paintField(bpmField.getBounds());
    paintField(barsField.getBounds());

    g.setFont(fx::inspectorFont());
    g.setColour(t.rowLabel);
    g.drawText("BPM", bpmRow, juce::Justification::centredLeft);
    g.drawText("Key", keyRow, juce::Justification::centredLeft);
    g.drawText("Maximum Bars", barsRow, juce::Justification::centredLeft);
    g.drawText("Include Subdirectories", subdirsRow, juce::Justification::centredLeft);
}

void FavoritesSidebar::SearchInlinePanel::resized()
{
    // Match effects column padding / row metrics.
    auto r = getLocalBounds().reduced(fx::kSectionPadH, 0);
    const int rowH = fx::kRowMinH;
    const int gap = fx::kRowGap;
    const int ctrlH = fx::kControlH;

    queryField.setBounds(r.removeFromTop(rowH).withSizeKeepingCentre(
        getLocalBounds().reduced(fx::kSectionPadH, 0).getWidth(), ctrlH));
    r.removeFromTop(gap);

    bpmRow = r.removeFromTop(rowH);
    {
        auto ctrl = bpmRow.removeFromRight(metrics::scaled(72)).withSizeKeepingCentre(metrics::scaled(72), ctrlH);
        bpmField.setBounds(ctrl);
    }
    r.removeFromTop(gap);

    keyRow = r.removeFromTop(rowH);
    {
        const int kw = juce::jmax(keyPicker.idealWidth(), metrics::scaled(72));
        keyPicker.setBounds(keyRow.removeFromRight(kw).withSizeKeepingCentre(kw, ctrlH));
    }
    r.removeFromTop(gap);

    barsRow = r.removeFromTop(rowH);
    {
        auto ctrl = barsRow.removeFromRight(metrics::scaled(72)).withSizeKeepingCentre(metrics::scaled(72), ctrlH);
        barsField.setBounds(ctrl);
    }
    r.removeFromTop(gap);

    subdirsRow = r.removeFromTop(rowH);
    subdirsSwitch.setBounds(subdirsRow.removeFromRight(subdirsSwitch.idealWidth())
                                .withSizeKeepingCentre(subdirsSwitch.idealWidth(), ctrlH));
    r.removeFromTop(gap);

    btnSearch.setBounds(r.removeFromTop(ctrlH));
}

// ── FavoritesSidebar ─────────────────────────────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.ghost = true;
    btnToggle.iconScale = 1.0f;
    btnToggle.onClick = [this] { setCollapsed(!collapsed); };
    addAndMakeVisible(btnToggle);

    searchForm.setVisible(false);
    searchForm.onSearch = [this](const BrowserSearch& s)
    {
        if (onRunSearch) onRunSearch(s);
    };
    addChildComponent(searchForm);

    rebuildRows();
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

void FavoritesSidebar::setStarredFilter(bool on)
{
    if (starredFilter == on) return;
    starredFilter = on;
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
    if (browseMode == 2 || searchFormOpen)
        rows.push_back({ RowKind::SaveSearch });
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int rowIdx) const
{
    if (!juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return {};

    const int rowH = collapsed ? sidebarRowHCollapsed() : sidebarRowH();
    int y = sidebarHeaderH() + metrics::scaled(4);

    // Mirror paint(): Open, then FAVORITES section before SavedDir/AddSaved,
    // then STARRED section before Starred, then SEARCH before Search.
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto kind = rows[(size_t) i].kind;
        if (!collapsed)
        {
            if (kind == RowKind::SavedDir || kind == RowKind::AddSaved)
            {
                // Section once before the favorites block.
                bool firstFav = true;
                for (int j = 0; j < i; ++j)
                {
                    const auto k = rows[(size_t) j].kind;
                    if (k == RowKind::SavedDir || k == RowKind::AddSaved)
                    {
                        firstFav = false;
                        break;
                    }
                }
                if (firstFav)
                    y += sidebarSectionH();
            }
            else if (kind == RowKind::Starred)
                y += sidebarSectionH();
            else if (kind == RowKind::Search)
                y += sidebarSectionH();
        }

        const auto r = collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                                 : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);
        if (i == rowIdx)
            return r;
        y += rowH;
        // Push rows below Search down when the inline form is open.
        if (kind == RowKind::Search)
            y += searchFormOccupiedHeight();
    }
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

int FavoritesSidebar::searchFormOccupiedHeight() const
{
    if (!searchFormOpen || collapsed)
        return 0;
    return metrics::scaled(SearchInlinePanel::kHeight) + metrics::scaled(6);
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
    const int railW = metrics::sidebarRailWidth();
    const int headerH = sidebarHeaderH();
    btnToggle.setBounds(juce::Rectangle<int>(0, 0, railW, headerH)
                            .withSizeKeepingCentre(metrics::scaled(26), metrics::scaled(26)));

    if (searchFormOpen && !collapsed)
    {
        const int idx = searchRowIndex();
        if (idx >= 0)
        {
            const auto r = rowBounds(idx);
            searchForm.setBounds(fx::kSectionPadH, r.getBottom() + metrics::scaled(4),
                                 getWidth() - fx::kSectionPadH * 2,
                                 metrics::scaled(SearchInlinePanel::kHeight));
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
    const bool activeStar = row.kind == RowKind::Starred && starredFilter;
    const bool activeSearch = (row.kind == RowKind::Search && searchFormOpen)
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
        case RowKind::Open:       icon = icons::folderOpen; label = "Open Folder"; break;
        case RowKind::SavedDir:
        {
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            break;
        }
        case RowKind::AddSaved:   icon = icons::plus; label = "Save Folder"; break;
        case RowKind::Starred:    icon = icons::star; label = "Starred"; break;
        case RowKind::Search:     icon = icons::search; label = "Search"; break;
        case RowKind::SavedSearch:
            icon = icons::search;
            label = savedSearches[(size_t) row.index].name;
            break;
        case RowKind::SaveSearch: icon = icons::plus; label = "Save Search"; break;
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
        g.drawText("LIBRARY", metrics::sidebarRailWidth() - metrics::scaled(4), 0, metrics::scaled(100),
                   sidebarHeaderH(), juce::Justification::centredLeft);
    }

    const int rowH = collapsed ? sidebarRowHCollapsed() : sidebarRowH();
    int y = sidebarHeaderH() + metrics::scaled(4);
    bool drewSaved = false, drewFav = false, drewSearch = false;

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto kind = rows[(size_t) i].kind;
        if (!collapsed)
        {
            auto sectionFont = uiFont(10.0f, true);
            sectionFont.setExtraKerningFactor(0.08f);
            if (!drewSaved && (kind == RowKind::SavedDir || kind == RowKind::AddSaved))
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("FAVORITES", metrics::scaled(10), y, getWidth() - metrics::scaled(16),
                           sidebarSectionH(), juce::Justification::centredLeft);
                y += sidebarSectionH();
                drewSaved = true;
            }
            if (!drewFav && kind == RowKind::Starred)
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("STARRED", metrics::scaled(10), y, getWidth() - metrics::scaled(16),
                           sidebarSectionH(), juce::Justification::centredLeft);
                y += sidebarSectionH();
                drewFav = true;
            }
            if (!drewSearch && kind == RowKind::Search)
            {
                g.setColour(colours::text3());
                g.setFont(sectionFont);
                g.drawText("SEARCH", metrics::scaled(10), y, getWidth() - metrics::scaled(16),
                           sidebarSectionH(), juce::Justification::centredLeft);
                y += sidebarSectionH();
                drewSearch = true;
            }
        }

        const auto r = collapsed ? juce::Rectangle<int>(4, y, getWidth() - 8, rowH - 6)
                                   : juce::Rectangle<int>(6, y, getWidth() - 12, rowH - 4);
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove);
        y += rowH;
        if (kind == RowKind::Search)
            y += searchFormOccupiedHeight();
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
    // Callbacks use entry indices (parallel to MidiBrowserEditor::displayRows), not sort-order rows.
    if (notify != juce::dontSendNotification && onSelect && selected >= 0)
        onSelect(selected);
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
    const bool browseNav = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::returnKey;

    // Browse navigation always reclaims focus from effects/combos/text fields.
    if (browseNav)
    {
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
                if (onEnterFolder)
                    onEnterFolder(selected);
            }
            return true;
        }

        const int pageRows = juce::jmax(1, viewport.getMaximumVisibleHeight() / metrics::listRowH());
        if (key == juce::KeyPress::pageUpKey)   { selectAdjacent(-pageRows); return true; }
        if (key == juce::KeyPress::pageDownKey) { selectAdjacent(pageRows); return true; }
        return true;
    }

    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr
        || dynamic_cast<juce::ComboBox*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    if (!hasKeyboardFocus(true))
        grabKeyboardFocus();

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
    auto h = getLocalBounds().removeFromTop(metrics::listHeaderH()).reduced(metrics::scaled(8), 0);
    FileListPanel::ColumnRects cols;
    auto row = h;
    // Fixed-width columns pack left so the table can stay slim.
    cols.name = row.removeFromLeft(metrics::scaled(kNameW));
    cols.key = row.removeFromLeft(metrics::scaled(kKeyW));
    cols.tempo = row.removeFromLeft(metrics::scaled(kTempoW));
    cols.bars = row.removeFromLeft(metrics::scaled(kBarsW));
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
    row = row.reduced(metrics::scaled(8), 0);
    cols.name = row.removeFromLeft(metrics::scaled(kNameW));
    cols.key = row.removeFromLeft(metrics::scaled(kKeyW));
    cols.tempo = row.removeFromLeft(metrics::scaled(kTempoW));
    cols.bars = row.removeFromLeft(metrics::scaled(kBarsW));
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

}

void FileListPanel::mouseDown(const juce::MouseEvent& e)
{
    grabBrowseFocus();

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
        const int arrowW = metrics::scaled(kSortArrowW);
        auto labelR = r.withTrimmedRight(arrowW);
        auto arrowR = r.removeFromRight(arrowW);
        g.setColour(active ? colours::text() : colours::text3());
        g.setFont(uiFont(10.5f, true));
        g.drawText(label, labelR, just, true);
        if (active)
            g.drawText(sortAscending ? juce::String::fromUTF8("▲")
                                     : juce::String::fromUTF8("▼"),
                       arrowR, juce::Justification::centred, false);
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
    g.drawText(e.name, row, juce::Justification::centredLeft, true); // truncate
}

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    if (owner.entries.empty())
    {
        g.setColour(colours::accent());
        g.setFont(uiFont(13.0f, true));
        g.drawText("Click Here to open a folder", getLocalBounds().reduced(24, 32),
                   juce::Justification::centred);
        return;
    }

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
    // Screen→local avoids AffineTransform/content-scale drift in nested Viewport coords.
    const int y = getLocalPoint(nullptr, e.getScreenPosition()).y;
    const int disp = rowH > 0 ? y / rowH : -1;
    const bool valid = juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size());
    const int entryIdx = valid ? owner.displayToEntry(disp) : -1;
    const bool isDir = entryIdx >= 0 && owner.entries[(size_t) entryIdx].isDirectory;
    // Star sits in the leading icon slot of the name column.
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();
    const int x = getLocalPoint(nullptr, e.getScreenPosition()).x;
    const bool newHoverStar = valid && !isDir && x >= starLeft && x < starLeft + 20;

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
    owner.grabBrowseFocus();

    if (owner.entries.empty())
    {
        if (owner.onEmptyOpenFolder)
            owner.onEmptyOpenFolder();
        return;
    }

    // Column header clicks are handled by the parent panel.
    const auto local = getLocalPoint(nullptr, e.getScreenPosition());
    if (local.y < 0) return;

    const int rowH = metrics::listRowH();
    const int disp = rowH > 0 ? local.y / rowH : -1;
    if (!juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size()))
        return;

    const int entryIdx = owner.displayToEntry(disp);
    if (entryIdx < 0) return;
    const auto& entry = owner.entries[(size_t) entryIdx];
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();

    // Pass entry indices so the editor can index displayRows / clips correctly
    // even when the list is re-ordered by column sort.
    if (!entry.isDirectory && local.x >= starLeft && local.x < starLeft + 20)
    {
        if (owner.onToggleStar) owner.onToggleStar(entryIdx);
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
        owner.onEnterFolder(entryIdx);
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
