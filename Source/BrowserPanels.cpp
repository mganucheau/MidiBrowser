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

constexpr int kSidebarPad = 10;
constexpr int kSectionH = 22;
constexpr int kRowH = 26;

} // namespace

// ── FavoritesSidebar ─────────────────────────────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnAdd.onClick = [this] { if (onAddCurrent) onAddCurrent(); };
    addAndMakeVisible(btnAdd);

    btnTweaks.onClick = [this] { if (onOpenTweaks) onOpenTweaks(); };
    addAndMakeVisible(btnTweaks);
}

void FavoritesSidebar::setSavedDirs(const juce::StringArray& paths, const juce::String& activePath)
{
    dirs = paths;
    active = activePath;
    repaint();
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int index) const
{
    const int y = kSectionH + 4 + index * kRowH;
    return { kSidebarPad, y, getWidth() - kSidebarPad * 2, kRowH - 2 };
}

FavoritesSidebar::RowHit FavoritesSidebar::rowHitAt(juce::Point<int> pos) const
{
    for (int i = 0; i < dirs.size(); ++i)
    {
        const auto r = rowBounds(i);
        if (r.contains(pos))
            return { i, pos.x > r.getRight() - 22 };
    }
    return {};
}

void FavoritesSidebar::resized()
{
    btnAdd.setBounds(getWidth() - 34, 2, 24, 22);
    btnTweaks.setBounds(kSidebarPad, getHeight() - 34, 24, 24);
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = rowHitAt(e.getPosition());
    if (hit.index != hoverRow || hit.removeZone != hoverRemove)
    {
        hoverRow = hit.index;
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
    const auto hit = rowHitAt(e.getPosition());
    if (hit.index < 0 || hit.index >= dirs.size())
        return;
    if (hit.removeZone)
    {
        if (onRemoveDir) onRemoveDir(dirs[hit.index]);
        return;
    }
    if (onPickDir) onPickDir(dirs[hit.index]);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::sidebarTop(), 0, 0,
                                           colours::sidebarBot(), 0, b.getHeight(), false));
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));

    g.setColour(colours::text3());
    g.setFont(uiFont(10.0f, true));
    g.drawText("FAVORITES", kSidebarPad, 4, getWidth() - 50, 16,
               juce::Justification::centredLeft);

    for (int i = 0; i < dirs.size(); ++i)
    {
        const auto r = rowBounds(i).toFloat();
        const bool isActive = dirs[i] == active;
        const bool hovered = i == hoverRow;

        if (isActive || hovered)
        {
            g.setColour(isActive ? colours::accentSoft()
                                 : colours::elev().withAlpha(0.7f));
            g.fillRoundedRectangle(r, 6.0f);
        }

        auto icon = juce::Rectangle<float>(r.getX() + 6.0f, r.getY() + 5.0f, 14.0f, 14.0f);
        drawIcon(g, icons::folder, icon, isActive ? colours::accent() : colours::text2(), 1.4f);

        g.setColour(isActive ? colours::text() : colours::text2());
        g.setFont(uiFont(12.5f, isActive));
        const auto name = juce::File(dirs[i]).getFileName();
        g.drawText(name, r.withTrimmedLeft(26.0f).withTrimmedRight(22.0f).toNearestInt(),
                   juce::Justification::centredLeft, true);

        if (hovered)
            drawIcon(g, icons::x,
                     juce::Rectangle<float>(r.getRight() - 18.0f, r.getY() + 5.0f, 12.0f, 12.0f),
                     hoverRemove ? colours::text() : colours::text3(), 1.4f);
    }

    // LOCATIONS header under favorites
    const int locY = kSectionH + 4 + dirs.size() * kRowH + 10;
    g.setColour(colours::text3());
    g.setFont(uiFont(10.0f, true));
    g.drawText("LOCATIONS", kSidebarPad, locY, getWidth() - 20, 16,
               juce::Justification::centredLeft);
}

// ── FileListPanel ────────────────────────────────────────────────────────────

FileListPanel::FileListPanel()
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);

    btnOpen.onClick = [this] { if (onOpenFolder) onOpenFolder(); };
    addAndMakeVisible(btnOpen);

    btnStarFilter.onClick = [this] { if (onToggleStarFilter) onToggleStarFilter(); };
    addChildComponent(btnStarFilter);

    startTimerHz(20);
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
        onSelect(selected);
}

void FileListPanel::setPlaying(bool isPlaying)
{
    playing = isPlaying;
    content.repaint();
}

void FileListPanel::setStarFilter(bool filterOn, bool anyStarred)
{
    btnStarFilter.setVisible(anyStarred);
    btnStarFilter.active = filterOn;
    btnStarFilter.repaint();
    resized();
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

    if (key == juce::KeyPress::upKey)    { selectAdjacent(-1); return true; }
    if (key == juce::KeyPress::downKey)  { selectAdjacent(1); return true; }
    if (key == juce::KeyPress::leftKey)
    {
        if (onEnterParent) onEnterParent();
        return true;
    }
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::returnKey)
    {
        if (selected >= 0 && entries[(size_t) selected].isDirectory && onEnterFolder)
            onEnterFolder(selected);
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
    if (playing && selected >= 0)
        content.repaint(0, entryToDisplay(selected) * metrics::listRowH(),
                        content.getWidth(), metrics::listRowH());
}

juce::Rectangle<int> FileListPanel::headerColumnBounds(SortColumn col) const
{
    auto h = getLocalBounds().removeFromTop(metrics::listHeaderH()).reduced(8, 0);
    h.removeFromRight(btnStarFilter.isVisible() ? 52 : 28);
    auto bars = h.removeFromRight(kBarsW);
    auto tempo = h.removeFromRight(kTempoW);
    auto key = h.removeFromRight(kKeyW);
    switch (col)
    {
        case SortColumn::Name:  return h;
        case SortColumn::Key:   return key;
        case SortColumn::Tempo: return tempo;
        case SortColumn::Bars:  return bars;
    }
    return {};
}

void FileListPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop(metrics::listHeaderH());
    btnOpen.setBounds(header.removeFromRight(28).withSizeKeepingCentre(22, 22));
    if (btnStarFilter.isVisible())
        btnStarFilter.setBounds(header.removeFromRight(24).withSizeKeepingCentre(20, 20));
    viewport.setBounds(r);
    updateContentSize();
}

void FileListPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    paintColumnHeader(g);

    if (entries.empty())
    {
        auto area = getLocalBounds().withTrimmedTop(metrics::listHeaderH()).reduced(24, 32);
        auto icon = area.removeFromTop(36).toFloat().withSizeKeepingCentre(28.0f, 28.0f);
        drawIcon(g, icons::folderOpen, icon, colours::text3(), 1.6f);
        area.removeFromTop(12);
        g.setColour(colours::text());
        g.setFont(uiFont(14.0f, true));
        g.drawText("Open a Folder", area.removeFromTop(22), juce::Justification::centred);
    }
}

void FileListPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= metrics::listHeaderH())
    {
        // Empty-state open folder.
        if (entries.empty() && onOpenFolder)
            onOpenFolder();
        return;
    }

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
                             bool hovered, bool hoverPlay, bool hoverStar)
{
    const int entryIdx = displayToEntry(displayIdx);
    if (entryIdx < 0) return;
    const auto& e = entries[(size_t) entryIdx];
    const bool isSelected = entryIdx == selected;
    const bool showEq = isSelected && playing && !e.isDirectory;

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
    const auto iconCol = isSelected ? juce::Colours::white : colours::text3();

    auto row = r.reduced(8, 0);
    auto playZone = row.removeFromRight(kPlayZoneW);
    auto barsZone = row.removeFromRight(kBarsW);
    auto tempoZone = row.removeFromRight(kTempoW);
    auto keyZone = row.removeFromRight(kKeyW);
    auto starZone = row.removeFromRight(kStarZoneW);

    auto iconArea = row.removeFromLeft(17).toFloat().withSizeKeepingCentre(15.0f, 15.0f);
    drawIcon(g, e.isDirectory ? icons::folder : kindIcon(e.kind), iconArea, iconCol, 1.4f);
    row.removeFromLeft(6);

    if (e.isDirectory)
    {
        g.setColour(isSelected ? juce::Colours::white : colours::text());
        g.setFont(uiFont(12.0f, true));
        g.drawText(e.name, row, juce::Justification::centredLeft, true);
        return;
    }

    if (showEq)
    {
        auto eq = playZone.toFloat().withSizeKeepingCentre(13.0f, 13.0f);
        const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        for (int i = 0; i < 3; ++i)
        {
            const float phase = (float) std::sin(t * 7.0 + i * 2.1) * 0.5f + 0.5f;
            const float bh = 4.0f + phase * (eq.getHeight() - 4.0f);
            g.fillRoundedRectangle(eq.getX() + (float) i * 5.0f, eq.getBottom() - bh, 3.2f, bh, 1.2f);
        }
    }
    else if (hovered)
    {
        auto pb = playZone.toFloat().withSizeKeepingCentre(16.0f, 16.0f);
        if (hoverPlay)
        {
            g.setColour(colours::accent());
            g.fillEllipse(pb.expanded(3.0f));
            drawIcon(g, icons::play, pb.reduced(2.0f), colours::accentInk(), 1.4f);
        }
        else
        {
            drawIcon(g, icons::play, pb.reduced(2.0f), textCol, 1.4f);
        }
    }

    g.setFont(monoFont(11.0f, false));
    g.setColour(metaCol);
    if (e.rootName.isNotEmpty())
        g.drawText(e.rootName, keyZone, juce::Justification::centredRight);
    if (e.bpm > 0.0)
        g.drawText(juce::String((int) std::lround(e.bpm)), tempoZone, juce::Justification::centredRight);
    if (e.bars > 0)
        g.drawText(juce::String(e.bars), barsZone, juce::Justification::centredRight);

    {
        auto st = starZone.toFloat().withSizeKeepingCentre(14.0f, 14.0f);
        const auto col = e.starred ? (isSelected ? juce::Colours::white : colours::accent())
                       : hoverStar ? textCol : metaCol;
        drawIcon(g, icons::star, st, col, e.starred ? 1.9f : 1.25f);
    }

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
                       i == hoverRow && hoverPlay, i == hoverRow && hoverStar);
}

void FileListPanel::ListContent::mouseMove(const juce::MouseEvent& e)
{
    const int rowH = metrics::listRowH();
    const int disp = e.y / rowH;
    const bool valid = juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size());
    const int entryIdx = valid ? owner.displayToEntry(disp) : -1;
    const bool isDir = entryIdx >= 0 && owner.entries[(size_t) entryIdx].isDirectory;
    const int fromRight = getWidth() - 8 - e.x;
    const bool newHoverPlay = valid && !isDir && fromRight >= 0 && fromRight < kPlayZoneW;
    const bool newHoverStar = valid && !isDir
        && fromRight >= kPlayZoneW + kBarsW + kTempoW + kKeyW
        && fromRight < kPlayZoneW + kBarsW + kTempoW + kKeyW + kStarZoneW;

    if (disp != hoverRow || newHoverPlay != hoverPlay || newHoverStar != hoverStar)
    {
        hoverRow = valid ? disp : -1;
        hoverPlay = newHoverPlay;
        hoverStar = newHoverStar;
        repaint();
    }
}

void FileListPanel::ListContent::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverPlay = hoverStar = false;
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
    const int fromRight = getWidth() - 8 - e.x;

    if (!entry.isDirectory && fromRight >= 0 && fromRight < kPlayZoneW)
    {
        if (owner.onPlayRow) owner.onPlayRow(entryIdx);
        return;
    }
    if (!entry.isDirectory
        && fromRight >= kPlayZoneW + kBarsW + kTempoW + kKeyW
        && fromRight < kPlayZoneW + kBarsW + kTempoW + kKeyW + kStarZoneW)
    {
        if (owner.onToggleStar) owner.onToggleStar(entryIdx);
        return;
    }

    owner.setSelectedIndex(entryIdx);
    if (entry.isDirectory && e.getNumberOfClicks() > 1 && owner.onEnterFolder)
        owner.onEnterFolder(entryIdx);
}

} // namespace pflow
