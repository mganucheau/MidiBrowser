#include "BrowserPanels.h"
#include <algorithm>

namespace pflow {

namespace {

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

namespace {

constexpr int kBarsChoiceValues[] = { 0, 2, 4, 8, 16, 32, 64 }; // 64 = 64+

juce::StringArray barsChoiceLabels()
{
    return { "Any", "2", "4", "8", "16", "32", "64+" };
}

int barsValueToIndex(int value)
{
    for (int i = 0; i < (int) std::size(kBarsChoiceValues); ++i)
        if (kBarsChoiceValues[i] == value)
            return i;
    return 0;
}

int barsIndexToValue(int index)
{
    if (index < 0 || index >= (int) std::size(kBarsChoiceValues))
        return 0;
    return kBarsChoiceValues[index];
}

} // namespace

FavoritesSidebar::SearchInlinePanel::SearchInlinePanel()
{
    styleEditors();
    for (auto* ed : { &queryField, &bpmMinField, &bpmMaxField })
        addAndMakeVisible(*ed);

    bpmMinField.setInputRestrictions(6, "0123456789.");
    bpmMaxField.setInputRestrictions(6, "0123456789.");

    juce::StringArray keys;
    keys.add("Any");
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyPicker.setItems(keys, 0);
    addAndMakeVisible(keyPicker);

    const auto barsLabels = barsChoiceLabels();
    barsMinPopup.setItems(barsLabels, 0);
    barsMaxPopup.setItems(barsLabels, 0);
    addAndMakeVisible(barsMinPopup);
    addAndMakeVisible(barsMaxPopup);

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
    // Leave placeholders blank — unicode ellipses were rendering incorrectly.
    queryField.setTextToShowWhenEmpty({}, t.valueText);
    bpmMinField.setTextToShowWhenEmpty({}, t.valueText);
    bpmMaxField.setTextToShowWhenEmpty({}, t.valueText);

    for (auto* ed : { &queryField, &bpmMinField, &bpmMaxField })
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
    s.bpmMin = bpmMinField.getText().getDoubleValue();
    s.bpmMax = bpmMaxField.getText().getDoubleValue();
    s.barsMin = barsIndexToValue(barsMinPopup.getIndex());
    s.barsMax = barsIndexToValue(barsMaxPopup.getIndex());
    const int idx = keyPicker.getIndex();
    s.keyRoot = idx <= 0 ? -1 : idx - 1;
    s.subdirs = subdirsSwitch.getToggleState();
    return s;
}

void FavoritesSidebar::SearchInlinePanel::setCriteria(const BrowserSearch& s)
{
    queryField.setText(s.query, juce::dontSendNotification);
    bpmMinField.setText(s.bpmMin > 0.0 ? juce::String(s.bpmMin, 1) : juce::String(), juce::dontSendNotification);
    bpmMaxField.setText(s.bpmMax > 0.0 ? juce::String(s.bpmMax, 1) : juce::String(), juce::dontSendNotification);
    barsMinPopup.setIndex(barsValueToIndex(s.barsMin), juce::dontSendNotification);
    barsMaxPopup.setIndex(barsValueToIndex(s.barsMax), juce::dontSendNotification);
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
    paintField(bpmMinField.getBounds());
    paintField(bpmMaxField.getBounds());

    g.setFont(fx::inspectorFont());
    g.setColour(t.rowLabel);
    g.drawText("BPM", bpmRow, juce::Justification::centredLeft);
    g.drawText("Key", keyRow, juce::Justification::centredLeft);
    g.drawText("Bars", barsRow, juce::Justification::centredLeft);
    g.drawText("Include Subdirectories", subdirsRow, juce::Justification::centredLeft);

    // Tiny Min/Max captions above the dual controls.
    g.setFont(uiFont(fx::kAnnotPt, false));
    g.setColour(t.valueText);
    g.drawFittedText("Min", bpmMinField.getBounds().translated(0, -14).withHeight(12),
                     juce::Justification::centred, 1);
    g.drawFittedText("Max", bpmMaxField.getBounds().translated(0, -14).withHeight(12),
                     juce::Justification::centred, 1);
    g.drawFittedText("Min", barsMinPopup.getBounds().translated(0, -14).withHeight(12),
                     juce::Justification::centred, 1);
    g.drawFittedText("Max", barsMaxPopup.getBounds().translated(0, -14).withHeight(12),
                     juce::Justification::centred, 1);
}

void FavoritesSidebar::SearchInlinePanel::resized()
{
    // Parent already insets to match the sidebar icon column; no extra side pad.
    auto r = getLocalBounds();
    const int fullW = r.getWidth();
    const int rowH = fx::kRowMinH;
    const int gap = fx::kRowGap;
    const int ctrlH = fx::kControlH;
    constexpr int kTopPad = 10;

    r.removeFromTop(kTopPad);

    queryField.setBounds(r.removeFromTop(rowH).withSizeKeepingCentre(fullW, ctrlH));
    r.removeFromTop(gap);

    bpmRow = r.removeFromTop(rowH + 10); // room for Min/Max captions
    {
        auto ctrlArea = bpmRow;
        ctrlArea.removeFromTop(10);
        const int fieldW = metrics::scaled(56);
        const int pairGap = metrics::scaled(6);
        auto maxR = ctrlArea.removeFromRight(fieldW).withSizeKeepingCentre(fieldW, ctrlH);
        ctrlArea.removeFromRight(pairGap);
        auto minR = ctrlArea.removeFromRight(fieldW).withSizeKeepingCentre(fieldW, ctrlH);
        bpmMinField.setBounds(minR);
        bpmMaxField.setBounds(maxR);
    }
    r.removeFromTop(gap);

    keyRow = r.removeFromTop(rowH);
    {
        const int kw = juce::jmax(keyPicker.idealWidth(), metrics::scaled(72));
        keyPicker.setBounds(keyRow.removeFromRight(kw).withSizeKeepingCentre(kw, ctrlH));
    }
    r.removeFromTop(gap);

    barsRow = r.removeFromTop(rowH + 10);
    {
        auto ctrlArea = barsRow;
        ctrlArea.removeFromTop(10);
        const int pw = juce::jmax(barsMinPopup.idealWidth(), metrics::scaled(52));
        const int pairGap = metrics::scaled(6);
        auto maxR = ctrlArea.removeFromRight(pw).withSizeKeepingCentre(pw, ctrlH);
        ctrlArea.removeFromRight(pairGap);
        auto minR = ctrlArea.removeFromRight(pw).withSizeKeepingCentre(pw, ctrlH);
        barsMinPopup.setBounds(minR);
        barsMaxPopup.setBounds(maxR);
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

    btnSettings.ghost = true;
    btnSettings.iconScale = 1.0f;
    btnSettings.setTooltip("Settings");
    btnSettings.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
    addAndMakeVisible(btnSettings);

    expandedWidth = metrics::sidebarExpandedWidth();

    searchForm.setVisible(false);
    searchForm.onSearch = [this](const BrowserSearch& s)
    {
        if (onRunSearch) onRunSearch(s);
    };
    addChildComponent(searchForm);

    rebuildRows();
}

int FavoritesSidebar::idealWidth() const
{
    if (collapsed)
        return metrics::sidebarRailWidth();
    const int maxW = metrics::sidebarExpandedWidth();
    const int minW = metrics::sidebarRailWidth();
    return juce::jlimit(minW, maxW, expandedWidth > 0 ? expandedWidth : maxW);
}

void FavoritesSidebar::setExpandedWidth(int w)
{
    const int maxW = metrics::sidebarExpandedWidth();
    const int minW = metrics::sidebarRailWidth();
    expandedWidth = juce::jlimit(minW, maxW, w);
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
    if (searchFormOpen == open && !(open && collapsed)) return;
    searchFormOpen = open;
    // Opening search from the icon rail should expand so the form is visible.
    if (searchFormOpen && collapsed)
        setCollapsed(false);
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
    const int iconBtn = metrics::chromeIconButton();
    btnToggle.setBounds(juce::Rectangle<int>(0, 0, railW, headerH)
                            .withSizeKeepingCentre(iconBtn, iconBtn));
    // Gear matches header chrome icon size, pinned to the bottom of the rail.
    btnSettings.setBounds(juce::Rectangle<int>(0, getHeight() - metrics::scaled(34),
                                               railW, metrics::scaled(30))
                              .withSizeKeepingCentre(iconBtn, iconBtn));

    if (searchFormOpen && !collapsed)
    {
        const int idx = searchRowIndex();
        if (idx >= 0)
        {
            const auto r = rowBounds(idx);
            // Match expanded-row icon column: row inset 6 + icon pad 7 = 13.
            const int side = metrics::scaled(13);
            searchForm.setBounds(side, r.getBottom() + metrics::scaled(4),
                                 getWidth() - side * 2,
                                 metrics::scaled(SearchInlinePanel::kHeight));
            searchForm.setVisible(true);
            return;
        }
    }
    searchForm.setVisible(false);
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    if (!collapsed && e.x >= getWidth() - 5)
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);

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
    if (!resizing)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    hoverRow = -1;
    hoverRemove = false;
    repaint();
}

void FavoritesSidebar::mouseDown(const juce::MouseEvent& e)
{
    if (searchForm.isVisible() && searchForm.getBounds().contains(e.getPosition()))
        return;

    if (!collapsed && e.x >= getWidth() - 5)
    {
        resizing = true;
        resizeStartWidth = idealWidth();
        resizeStartX = e.getScreenX();
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    const auto hit = rowHitAt(e.getPosition());
    if (!hit.valid) return;

    if (e.mods.isPopupMenu())
    {
        if (hit.kind == RowKind::Starred && onCopyStarredToFolder)
        {
            juce::PopupMenu m;
            m.addItem(1, "Copy All to Folder...");
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                 .withMousePosition(),
                [this](int result)
                {
                    if (result == 1 && onCopyStarredToFolder)
                        onCopyStarredToFolder();
                });
        }
        return;
    }

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

void FavoritesSidebar::mouseDrag(const juce::MouseEvent& e)
{
    if (!resizing) return;

    const int maxW = metrics::sidebarExpandedWidth();
    const int railW = metrics::sidebarRailWidth();
    const int dx = e.getScreenX() - resizeStartX;
    const int next = juce::jlimit(railW, maxW, resizeStartWidth + dx);

    // Dragging to the folded rail width auto-collapses.
    if (next <= railW + 2)
    {
        resizing = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        expandedWidth = maxW; // next expand opens at the default width
        setCollapsed(true);
        return;
    }

    if (next != expandedWidth)
    {
        expandedWidth = next;
        if (onWidthChanged) onWidthChanged();
    }
}

void FavoritesSidebar::mouseUp(const juce::MouseEvent&)
{
    if (!resizing) return;
    resizing = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    if (onWidthChanged) onWidthChanged();
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone)
{
    const auto& row = rows[(size_t) rowIdx];
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeStar = row.kind == RowKind::Starred
        && (starredFilter || browseMode == 1);
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
        const float s = (float) metrics::chromeIconGlyphSize();
        auto iconArea = r.toFloat().withSizeKeepingCentre(s, s);
        drawIcon(g, icon, iconArea, iconCol, 1.6f);
        return;
    }

    auto textArea = r.reduced(7, 0);
    const float s = (float) metrics::chromeIconGlyphSize();
    drawIcon(g, icon, textArea.removeFromLeft((int) s + 2).toFloat().withSizeKeepingCentre(s, s),
             iconCol, 1.6f);
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

    scrollTopBtn.ghost = true;
    scrollTopBtn.iconScale = 1.1f;
    scrollTopBtn.setTooltip("Scroll to top");
    scrollTopBtn.onClick = [this]
    {
        viewport.setViewPosition(0, 0);
        updateScrollTopButton();
    };
    addChildComponent(scrollTopBtn);
    scrollTopBtn.setVisible(false);

    startTimerHz(20);
}

void FileListPanel::updateScrollTopButton()
{
    const bool show = viewport.getViewPositionY() > 40;
    if (scrollTopBtn.isVisible() != show)
        scrollTopBtn.setVisible(show);
    if (show)
    {
        constexpr int kBtn = 28;
        constexpr int kPad = 10;
        const auto listArea = getLocalBounds().withTrimmedTop(metrics::listHeaderH());
        scrollTopBtn.setBounds(listArea.getRight() - kBtn - kPad,
                               listArea.getBottom() - kBtn - kPad,
                               kBtn, kBtn);
        scrollTopBtn.toFront(false);
    }
}

void FileListPanel::grabBrowseFocus()
{
    // Prefer the panel itself over combos/sliders/viewports so arrow keys
    // reach browse navigation after the user clicks a file row.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (focused != this && !isParentOf(focused))
            focused->giveAwayKeyboardFocus();
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
    // Timer also drives the scroll-to-top FAB visibility.
    startTimerHz(searching ? 12 : 20);
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
            case SortColumn::Kind:
                cmp = juce::String(clipKindName(ea.kind))
                          .compareNatural(juce::String(clipKindName(eb.kind)));
                break;
            case SortColumn::Complexity:
                cmp = ea.complexity - eb.complexity;
                break;
            case SortColumn::DifNotes:
                cmp = ea.difNotes - eb.difNotes;
                break;
            case SortColumn::TimeSig:
                cmp = (ea.timeSigNum * 100 + ea.timeSigDen)
                    - (eb.timeSigNum * 100 + eb.timeSigDen);
                break;
            case SortColumn::Notes:
                cmp = ea.noteCount - eb.noteCount;
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
        viewport.setViewPosition(viewport.getViewPositionX(), rowTop);
    else if (rowBottom > viewTop + viewH)
        viewport.setViewPosition(viewport.getViewPositionX(), rowBottom - viewH);
}

void FileListPanel::updateContentSize()
{
    const int viewW = juce::jmax(1, viewport.getMaximumVisibleWidth());
    const int contentW = juce::jmax(viewW, totalContentWidth());
    const bool needHScroll = contentW > viewW + 1;
    viewport.setScrollBarsShown(true, needHScroll);

    if (entries.empty())
        content.setSize(contentW, juce::jmax(1, viewport.getMaximumVisibleHeight()));
    else
        content.setSize(contentW, juce::jmax(1, (int) entries.size() * metrics::listRowH()));
}

bool FileListPanel::isColumnVisible(SortColumn col) const
{
    switch (col)
    {
        case SortColumn::Name:       return true;
        case SortColumn::Key:        return columnsVisible.key;
        case SortColumn::Tempo:      return columnsVisible.tempo;
        case SortColumn::Bars:       return columnsVisible.bars;
        case SortColumn::Kind:       return columnsVisible.kind;
        case SortColumn::Complexity: return columnsVisible.complexity;
        case SortColumn::DifNotes:   return columnsVisible.difNotes;
        case SortColumn::TimeSig:    return columnsVisible.timeSig;
        case SortColumn::Notes:      return columnsVisible.notes;
    }
    return false;
}

int FileListPanel::totalContentWidth() const
{
    int w = metrics::scaled(8) * 2 + metrics::scaled(kNameW);
    if (columnsVisible.key)        w += metrics::scaled(kKeyW);
    if (columnsVisible.tempo)      w += metrics::scaled(kTempoW);
    if (columnsVisible.bars)       w += metrics::scaled(kBarsW);
    if (columnsVisible.kind)       w += metrics::scaled(kKindW);
    if (columnsVisible.complexity) w += metrics::scaled(kComplexityW);
    if (columnsVisible.difNotes)   w += metrics::scaled(kDifNotesW);
    if (columnsVisible.timeSig)    w += metrics::scaled(kTimeSigW);
    if (columnsVisible.notes)      w += metrics::scaled(kNotesW);
    return w;
}

void FileListPanel::setColumnVisibility(const BrowserColumnVisibility& v)
{
    columnsVisible = v;
    if (!isColumnVisible(sortColumn))
    {
        sortColumn = SortColumn::Name;
        sortAscending = true;
        rebuildSortOrder();
    }
    updateContentSize();
    content.repaint();
    repaint();
}

void FileListPanel::showColumnVisibilityMenu()
{
    juce::PopupMenu m;
    auto addToggle = [&](int id, const juce::String& label, bool on)
    {
        m.addItem(id, label, true, on);
    };
    addToggle(1, "Key", columnsVisible.key);
    addToggle(2, "Tempo", columnsVisible.tempo);
    addToggle(3, "Bars", columnsVisible.bars);
    addToggle(4, "Kind", columnsVisible.kind);
    addToggle(5, "Complexity", columnsVisible.complexity);
    addToggle(6, "DifNotes", columnsVisible.difNotes);
    addToggle(7, "TimeSig", columnsVisible.timeSig);
    addToggle(8, "Notes", columnsVisible.notes);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                         .withMousePosition(),
        [this](int result)
        {
            if (result == 0) return;
            auto v = columnsVisible;
            switch (result)
            {
                case 1: v.key = !v.key; break;
                case 2: v.tempo = !v.tempo; break;
                case 3: v.bars = !v.bars; break;
                case 4: v.kind = !v.kind; break;
                case 5: v.complexity = !v.complexity; break;
                case 6: v.difNotes = !v.difNotes; break;
                case 7: v.timeSig = !v.timeSig; break;
                case 8: v.notes = !v.notes; break;
                default: return;
            }
            setColumnVisibility(v);
            if (onColumnVisibilityChanged)
                onColumnVisibilityChanged(v);
        });
}

juce::Rectangle<int> FileListPanel::headerColumnBounds(SortColumn col) const
{
    auto row = getLocalBounds().removeFromTop(metrics::listHeaderH());
    row.setX(row.getX() - viewport.getViewPositionX());
    row.setWidth(juce::jmax(getWidth(), totalContentWidth()));
    const auto c = splitRowColumns(row);
    switch (col)
    {
        case SortColumn::Name:       return c.name;
        case SortColumn::Key:        return c.key;
        case SortColumn::Tempo:      return c.tempo;
        case SortColumn::Bars:       return c.bars;
        case SortColumn::Kind:       return c.kind;
        case SortColumn::Complexity: return c.complexity;
        case SortColumn::DifNotes:   return c.difNotes;
        case SortColumn::TimeSig:    return c.timeSig;
        case SortColumn::Notes:      return c.notes;
    }
    return {};
}

FileListPanel::ColumnRects FileListPanel::splitRowColumns(juce::Rectangle<int> row) const
{
    ColumnRects cols;
    row = row.reduced(metrics::scaled(8), 0);
    cols.name = row.removeFromLeft(metrics::scaled(kNameW));
    if (columnsVisible.key)
        cols.key = row.removeFromLeft(metrics::scaled(kKeyW));
    if (columnsVisible.tempo)
        cols.tempo = row.removeFromLeft(metrics::scaled(kTempoW));
    if (columnsVisible.bars)
        cols.bars = row.removeFromLeft(metrics::scaled(kBarsW));
    if (columnsVisible.kind)
        cols.kind = row.removeFromLeft(metrics::scaled(kKindW));
    if (columnsVisible.complexity)
        cols.complexity = row.removeFromLeft(metrics::scaled(kComplexityW));
    if (columnsVisible.difNotes)
        cols.difNotes = row.removeFromLeft(metrics::scaled(kDifNotesW));
    if (columnsVisible.timeSig)
        cols.timeSig = row.removeFromLeft(metrics::scaled(kTimeSigW));
    if (columnsVisible.notes)
        cols.notes = row.removeFromLeft(metrics::scaled(kNotesW));
    return cols;
}

void FileListPanel::timerCallback()
{
    updateScrollTopButton();

    // Keep column headers aligned with horizontally scrolled row content.
    const int viewX = viewport.getViewPositionX();
    if (viewX != lastViewX)
    {
        lastViewX = viewX;
        repaint(getLocalBounds().removeFromTop(metrics::listHeaderH()));
    }

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

void FileListPanel::resized()
{
    viewport.setBounds(getLocalBounds().withTrimmedTop(metrics::listHeaderH()));
    updateContentSize();
    updateScrollTopButton();
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
    if (onActivated)
        onActivated();

    if (e.y >= metrics::listHeaderH())
        return;

    if (e.mods.isPopupMenu())
    {
        showColumnVisibilityMenu();
        return;
    }

    const SortColumn allCols[] = {
        SortColumn::Name, SortColumn::Key, SortColumn::Tempo, SortColumn::Bars,
        SortColumn::Kind, SortColumn::Complexity, SortColumn::DifNotes,
        SortColumn::TimeSig, SortColumn::Notes
    };
    for (auto col : allCols)
    {
        if (!isColumnVisible(col))
            continue;
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
    auto headerClip = getLocalBounds().removeFromTop(metrics::listHeaderH());
    g.setColour(colours::line());
    g.fillRect(headerClip.removeFromBottom(1));

    // Paint headers in content coordinates, shifted by the viewport's X scroll
    // so titles stay locked to their columns while rows scroll sideways.
    const int viewX = viewport.getViewPositionX();
    g.saveState();
    g.reduceClipRegion(getLocalBounds().removeFromTop(metrics::listHeaderH()));
    g.addTransform(juce::AffineTransform::translation((float) -viewX, 0.0f));

    auto drawCol = [&](SortColumn col, const juce::String& label)
    {
        if (!isColumnVisible(col))
            return;
        // Bounds in panel space already account for viewX; undo that for content paint.
        auto r = headerColumnBounds(col).translated(viewX, 0);
        const bool active = sortColumn == col;
        g.setColour(active ? colours::text() : colours::text3());
        g.setFont(uiFont(10.5f, true));
        const auto font = g.getCurrentFont();
        const int labelW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(font, label));
        auto labelR = r.removeFromLeft(labelW);
        g.drawText(label, labelR, juce::Justification::centredLeft, false);
        if (active)
        {
            auto arrowR = r.removeFromLeft(metrics::scaled(kSortArrowW)).toFloat()
                              .withSizeKeepingCentre(8.0f, 8.0f);
            juce::Path p;
            const float cx = arrowR.getCentreX();
            const float cy = arrowR.getCentreY();
            if (sortAscending)
                p.addTriangle(cx - 3.5f, cy + 2.0f, cx + 3.5f, cy + 2.0f, cx, cy - 2.5f);
            else
                p.addTriangle(cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
            g.fillPath(p);
        }
    };

    drawCol(SortColumn::Name, "Name");
    drawCol(SortColumn::Key, "Key");
    drawCol(SortColumn::Tempo, "Tempo");
    drawCol(SortColumn::Bars, "Bars");
    drawCol(SortColumn::Kind, "Kind");
    drawCol(SortColumn::Complexity, "Cx");
    drawCol(SortColumn::DifNotes, "DifNotes");
    drawCol(SortColumn::TimeSig, "TimeSig");
    drawCol(SortColumn::Notes, "Notes");
    g.restoreState();
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

    // Leading star (files) or folder icon — match header chrome glyph size.
    const float iconS = (float) metrics::chromeIconGlyphSize();
    auto iconArea = row.removeFromLeft((int) iconS + 3).toFloat().withSizeKeepingCentre(iconS, iconS);
    if (e.isDirectory)
    {
        drawIcon(g, icons::folder, iconArea,
                 isSelected ? juce::Colours::white : colours::accent(), 1.6f);
    }
    else
    {
        const auto starCol = e.starred
            ? (isSelected ? juce::Colours::white : colours::accent())
            : (hoverStar ? textCol : metaCol.withAlpha(isSelected ? 0.55f : 0.45f));
        drawIcon(g, icons::star, iconArea, starCol, 1.6f);
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
    if (columnsVisible.key && e.rootName.isNotEmpty())
        g.drawText(e.rootName, cols.key, juce::Justification::centredLeft);
    if (columnsVisible.tempo && e.bpm > 0.0)
        g.drawText(juce::String((int) std::lround(e.bpm)), cols.tempo, juce::Justification::centredLeft);
    if (columnsVisible.bars && e.bars > 0)
        g.drawText(juce::String(e.bars), cols.bars, juce::Justification::centredLeft);
    if (columnsVisible.kind)
        g.drawText(clipKindName(e.kind), cols.kind, juce::Justification::centredLeft);
    if (columnsVisible.complexity && e.complexity > 0)
        g.drawText(juce::String(e.complexity), cols.complexity, juce::Justification::centredLeft);
    if (columnsVisible.difNotes && e.difNotes > 0)
        g.drawText(juce::String(e.difNotes), cols.difNotes, juce::Justification::centredLeft);
    if (columnsVisible.timeSig)
        g.drawText(juce::String(e.timeSigNum) + "/" + juce::String(e.timeSigDen),
                   cols.timeSig, juce::Justification::centredLeft);
    if (columnsVisible.notes && e.noteCount > 0)
        g.drawText(juce::String(e.noteCount), cols.notes, juce::Justification::centredLeft);

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
    const int starHit = metrics::chromeIconGlyphSize() + 6;
    const bool newHoverStar = valid && !isDir && x >= starLeft && x < starLeft + starHit;

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
    if (owner.onActivated)
        owner.onActivated();

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

    if (e.mods.isPopupMenu() && !entry.isDirectory && entry.file.existsAsFile())
    {
        juce::PopupMenu m;
        m.addItem(1, "Show in Finder");
        m.addItem(2, "Copy to Folder...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                             .withMousePosition(),
            [this, file = entry.file](int result)
            {
                if (result == 1 && owner.onRevealFile)
                    owner.onRevealFile(file);
                else if (result == 2 && owner.onCopyFileToFolder)
                    owner.onCopyFileToFolder(file);
            });
        return;
    }

    // Pass entry indices so the editor can index displayRows / clips correctly
    // even when the list is re-ordered by column sort.
    const int starHit = metrics::chromeIconGlyphSize() + 6;
    if (!entry.isDirectory && local.x >= starLeft && local.x < starLeft + starHit)
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
