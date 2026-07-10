#include "BrowserPanels.h"

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

} // namespace

// ── FavoritesSidebar ─────────────────────────────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.onClick = [this] { setCollapsed(!collapsed); };
    addAndMakeVisible(btnToggle);

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

void FavoritesSidebar::setCollapsed(bool shouldCollapse)
{
    if (collapsed == shouldCollapse) return;
    collapsed = shouldCollapse;
    btnToggle.setTooltip(collapsed ? "Show saved folders" : "Hide saved folders");
    if (onCollapsedChanged)
        onCollapsedChanged();
    resized();
    repaint();
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int index) const
{
    const int rowH = collapsed ? 40 : 30;
    const int y = kSidebarHeaderH + 4 + index * rowH;
    if (collapsed)
        return { 4, y, getWidth() - 8, rowH - 6 };
    return { 6, y, getWidth() - 12, rowH - 4 };
}

FavoritesSidebar::RowHit FavoritesSidebar::hitTest(juce::Point<int> pos) const
{
    for (int i = 0; i < dirs.size(); ++i)
    {
        const auto r = rowBounds(i);
        if (r.contains(pos))
            return { i, !collapsed && pos.x > r.getRight() - 22 };
    }
    return {};
}

void FavoritesSidebar::resized()
{
    auto top = juce::Rectangle<int>(0, 0, getWidth(), kSidebarHeaderH);
    btnToggle.setBounds(top.removeFromLeft(metrics::sidebarRailW).withSizeKeepingCentre(26, 26));
    btnAdd.setVisible(!collapsed);
    if (!collapsed)
        btnAdd.setBounds(top.removeFromRight(30).withSizeKeepingCentre(22, 22));

    btnTweaks.setBounds(juce::Rectangle<int>(0, getHeight() - 34, metrics::sidebarRailW, 30)
                            .withSizeKeepingCentre(24, 24));
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = hitTest(e.getPosition());
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
    const auto hit = hitTest(e.getPosition());
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
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));

    if (!collapsed)
    {
        g.setColour(colours::text3());
        g.setFont(uiFont(12.0f, true));
        g.drawText("SAVED", metrics::sidebarRailW - 4, 0, 60, kSidebarHeaderH,
                   juce::Justification::centredLeft);
    }

    for (int i = 0; i < dirs.size(); ++i)
    {
        const juce::File dir(dirs[i]);
        const bool isActive = dirs[i] == active;
        const bool hovered = i == hoverRow;
        auto r = rowBounds(i);

        if (isActive || hovered)
        {
            g.setColour(isActive ? colours::accentSoft()
                                 : colours::elev().withAlpha(0.7f));
            g.fillRoundedRectangle(r.toFloat(), metrics::chipRadius);
            if (isActive)
            {
                g.setColour(colours::accentLine());
                g.drawRoundedRectangle(r.toFloat().reduced(0.5f), metrics::chipRadius, 1.0f);
            }
        }

        const auto iconCol = isActive ? colours::accent() : colours::text2();
        if (collapsed)
        {
            drawIcon(g, icons::folder, r.toFloat().reduced(10.0f), iconCol, 1.5f);
        }
        else
        {
            auto row = r.reduced(7, 0);
            drawIcon(g, icons::folder, row.removeFromLeft(16).toFloat()
                        .withSizeKeepingCentre(14.0f, 14.0f), iconCol, 1.4f);
            row.removeFromLeft(6);

            if (hovered)
            {
                auto xArea = row.removeFromRight(18).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
                drawIcon(g, icons::x, xArea,
                         hoverRemove ? colours::text() : colours::text3(), 1.4f);
            }

            g.setColour(isActive ? colours::text() : colours::text2());
            g.setFont(uiFont(13.0f, isActive));
            const auto label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[i];
            g.drawText(label, row, juce::Justification::centredLeft, true);
        }
    }
}

// ── FileListPanel ────────────────────────────────────────────────────────────

FileListPanel::FileListPanel()
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false, false, false);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);

    btnOpen.onClick = [this] { if (onOpenFolder) onOpenFolder(); };
    addAndMakeVisible(btnOpen);

    btnStarFilter.onClick = [this] { if (onToggleStarFilter) onToggleStarFilter(); };
    addChildComponent(btnStarFilter);
    setWantsKeyboardFocus(true);
}

void FileListPanel::setStarFilter(bool filterOn, bool anyStarred)
{
    btnStarFilter.setVisible(anyStarred || filterOn);
    btnStarFilter.active = filterOn;
    btnStarFilter.repaint();
    resized();
}

void FileListPanel::setFolderName(const juce::String& name)
{
    folderName = name;
    repaint();
}

void FileListPanel::setEntries(std::vector<FileListEntry> newEntries)
{
    entries = std::move(newEntries);
    selected = juce::jlimit(-1, (int) entries.size() - 1, selected);
    updateContentSize();
    repaint();
}

void FileListPanel::updateEntry(int index, const FileListEntry& entry)
{
    if (juce::isPositiveAndBelow(index, (int) entries.size()))
    {
        entries[(size_t) index] = entry;
        content.repaint();
    }
}

void FileListPanel::setSelectedIndex(int index, juce::NotificationType notify)
{
    index = juce::jlimit(-1, (int) entries.size() - 1, index);
    if (index == selected) return;
    selected = index;
    ensureRowVisible(selected);
    content.repaint();
    repaint();
    if (notify != juce::dontSendNotification && onSelect && selected >= 0)
        onSelect(selected);
}

void FileListPanel::setPlaying(bool isPlaying)
{
    if (playing == isPlaying) return;
    playing = isPlaying;
    if (playing) startTimerHz(20);   // equalizer animation
    else stopTimer();
    content.repaint();
}

void FileListPanel::timerCallback()
{
    // Only the active row animates; repaint just its bounds.
    if (selected >= 0)
        content.repaint(0, selected * metrics::listRowH(), content.getWidth(), metrics::listRowH());
}

void FileListPanel::selectAdjacent(int direction)
{
    if (entries.empty()) return;
    setSelectedIndex(juce::jlimit(0, (int) entries.size() - 1,
                                  (selected < 0 ? 0 : selected + direction)));
}

bool FileListPanel::keyPressed(const juce::KeyPress& key)
{
    // Mirrors the prototype: ignore arrows while an editable control has focus.
    if (auto* focus = getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*>(focus) != nullptr
            || dynamic_cast<juce::ComboBox*>(focus) != nullptr)
            return false;

    if (key == juce::KeyPress::upKey)   { selectAdjacent(-1); return true; }
    if (key == juce::KeyPress::downKey) { selectAdjacent(1); return true; }

    // Page up/down jump by one visible page.
    const int pageRows = juce::jmax(1, viewport.getMaximumVisibleHeight() / metrics::listRowH());
    if (key == juce::KeyPress::pageUpKey)   { selectAdjacent(-pageRows); return true; }
    if (key == juce::KeyPress::pageDownKey) { selectAdjacent(pageRows); return true; }
    return false;
}

void FileListPanel::ensureRowVisible(int index)
{
    if (index < 0) return;
    // Manual scroll math against the viewport (spec: no scrollIntoView):
    // rowTop/rowBottom measured in content coordinates vs the current scrollTop.
    const int rowH = metrics::listRowH();
    const int rowTop = index * rowH;
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
    content.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
                    juce::jmax(1, (int) entries.size() * metrics::listRowH()));
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

    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(metrics::listHeaderH());
    g.setColour(colours::line());
    g.fillRect(header.removeFromBottom(1));

    auto h = header.reduced(10, 0).withTrimmedRight(btnStarFilter.isVisible() ? 52 : 28);
    g.setColour(colours::text());
    g.setFont(uiFont(14.0f, true));
    const int countW = 40;
    g.drawText(folderName, h.withTrimmedRight(countW), juce::Justification::centredLeft, true);
    g.setColour(colours::text3());
    g.setFont(monoFont(13.0f, false));
    g.drawText(juce::String((int) entries.size()), h, juce::Justification::centredRight);

    // Composed empty state when no MIDI files are listed.
    if (entries.empty())
    {
        auto area = bounds.reduced(24, 32);
        auto icon = area.removeFromTop(36).toFloat().withSizeKeepingCentre(28.0f, 28.0f);
        drawIcon(g, icons::folderOpen, icon, colours::text3(), 1.6f);
        area.removeFromTop(12);
        g.setColour(colours::text());
        g.setFont(uiFont(14.0f, true));
        g.drawText("Open a Folder", area.removeFromTop(22),
                   juce::Justification::centred);
    }
}

void FileListPanel::paintRow(juce::Graphics& g, int index, juce::Rectangle<int> r,
                             bool hovered, bool hoverPlay, bool hoverStar)
{
    const auto& e = entries[(size_t) index];
    const bool isSelected = index == selected;
    const bool showEq = isSelected && playing;

    if (isSelected)
    {
        g.setColour(colours::accentSoft());
        g.fillRect(r);
        g.setColour(colours::accent());
        g.fillRect(r.withWidth(2));
    }
    else if (hovered)
    {
        g.setColour(colours::elev().withAlpha(0.55f));
        g.fillRect(r);
    }

    auto row = r.reduced(8, 0);

    // Kind icon
    auto iconArea = row.removeFromLeft(17).toFloat().withSizeKeepingCentre(15.0f, 15.0f);
    drawIcon(g, kindIcon(e.kind), iconArea,
             isSelected ? colours::accent() : colours::text3(), 1.4f);
    row.removeFromLeft(6);

    // Fixed right zones: [ … name | star | meta | play/eq ]
    auto playZone = row.removeFromRight(kPlayZoneW);
    auto metaZone = row.removeFromRight(kMetaZoneW);
    auto starZone = row.removeFromRight(kStarZoneW);

    if (showEq)
    {
        auto eq = playZone.toFloat().withSizeKeepingCentre(13.0f, 13.0f);
        const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        g.setColour(colours::accent());
        for (int i = 0; i < 3; ++i)
        {
            const float phase = (float) std::sin(t * 7.0 + i * 2.1) * 0.5f + 0.5f;
            const float bh = 4.0f + phase * (eq.getHeight() - 4.0f);
            const float x = eq.getX() + (float) i * 5.0f;
            g.fillRoundedRectangle(x, eq.getBottom() - bh, 3.2f, bh, 1.2f);
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
            drawIcon(g, icons::play, pb.reduced(2.0f), colours::text2(), 1.4f);
        }
    }

    // Meta: root · bpm
    juce::String meta;
    if (e.rootName.isNotEmpty()) meta << e.rootName;
    if (e.bpm > 0.0) meta << (meta.isEmpty() ? "" : " ") << juce::String((int) std::lround(e.bpm));
    if (meta.isNotEmpty())
    {
        g.setFont(monoFont(13.0f, false));
        g.setColour(colours::text3());
        g.drawText(meta, metaZone, juce::Justification::centredRight);
    }

    // Star (favourite): always visible so the affordance is discoverable.
    {
        auto st = starZone.toFloat().withSizeKeepingCentre(14.0f, 14.0f);
        const auto col = e.starred ? colours::accent()
                       : hoverStar ? colours::text()
                                   : colours::text3();
        drawIcon(g, icons::star, st, col, e.starred ? 1.9f : 1.25f);
    }

    if (e.edited)
    {
        auto dot = row.removeFromRight(8).toFloat().withSizeKeepingCentre(5.0f, 5.0f);
        g.setColour(colours::accent());
        g.fillEllipse(dot);
        row.removeFromRight(1);
    }

    // Same font for every row — selection changes colour + left accent bar only.
    g.setColour(isSelected ? colours::text() : colours::text2());
    g.setFont(uiFont(14.0f, false));
    g.drawText(e.name, row, juce::Justification::centredLeft, true);
}

// ── FileListPanel::ListContent ───────────────────────────────────────────────

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    const int rowH = metrics::listRowH();
    const auto clip = g.getClipBounds();
    const int first = juce::jmax(0, clip.getY() / rowH);
    const int last = juce::jmin((int) owner.entries.size() - 1, clip.getBottom() / rowH);
    for (int i = first; i <= last; ++i)
        owner.paintRow(g, i, { 0, i * rowH, getWidth(), rowH }, i == hoverRow,
                       i == hoverRow && hoverPlay, i == hoverRow && hoverStar);
}

void FileListPanel::ListContent::mouseMove(const juce::MouseEvent& e)
{
    const int rowH = metrics::listRowH();
    const int row = e.y / rowH;
    const bool valid = juce::isPositiveAndBelow(row, (int) owner.entries.size());
    const int newHover = valid ? row : -1;
    const int fromRight = getWidth() - 8 - e.x;   // row is reduced(8) each side
    const bool newHoverPlay = valid && fromRight >= 0 && fromRight < kPlayZoneW;
    const bool newHoverStar = valid && fromRight >= kPlayZoneW + kMetaZoneW
                              && fromRight < kPlayZoneW + kMetaZoneW + kStarZoneW;
    if (newHover != hoverRow || newHoverPlay != hoverPlay || newHoverStar != hoverStar)
    {
        hoverRow = newHover;
        hoverPlay = newHoverPlay;
        hoverStar = newHoverStar;
        repaint();
    }
}

void FileListPanel::ListContent::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverPlay = false;
    hoverStar = false;
    repaint();
}

void FileListPanel::ListContent::mouseDown(const juce::MouseEvent& e)
{
    const int row = e.y / metrics::listRowH();
    if (!juce::isPositiveAndBelow(row, (int) owner.entries.size()))
        return;
    owner.grabKeyboardFocus();
    if (hoverStar && owner.onToggleStar)
    {
        owner.onToggleStar(row);
        return;
    }
    if (hoverPlay && owner.onPlayRow)
    {
        owner.setSelectedIndex(row);
        owner.onPlayRow(row);
        return;
    }
    owner.setSelectedIndex(row);
}

} // namespace pflow
