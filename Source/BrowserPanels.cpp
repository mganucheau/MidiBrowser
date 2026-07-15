#include "BrowserPanels.h"
#include <algorithm>

namespace pflow {

namespace {

// Caps B2 library sidebar metrics (library-sidebar-caps-b2.canvas.tsx)
    constexpr int kSidebarHeaderH = 40; // match Toolkit / browser list headers
constexpr int kSidebarSectionH = 22;
constexpr int kSidebarRowH = 26;
constexpr int kSidebarRowGap = 12;
constexpr int kSectionGap = 14;
constexpr int kHeaderToRows = 4;
constexpr int kBodyPadT = 8;
constexpr int kBodyPadB = 10;
constexpr int kPadH = 10;       // shell horizontal pad
constexpr int kRowPadX = 4;     // content inset inside shell
constexpr int kIconCol = 16;
constexpr int kRowIconGap = 8;
constexpr int kCollapsedBtn = 28; // CTRL_H + 4
constexpr int kCapPt = 11;
constexpr int kTitlePt = 11;
constexpr int kCtrlPt = 12;
constexpr int kFilterPt = 11;

// Caps B2 base metrics — scale with Settings → Text & icons.
inline int sidebarHeaderH() { return metrics::scaled(kSidebarHeaderH); }
inline int sidebarSectionH() { return metrics::scaled(kSidebarSectionH); }
inline int sidebarRowH() { return metrics::scaled(kSidebarRowH); }
inline int sidebarRowGap() { return metrics::scaled(kSidebarRowGap); }
inline int sidebarSectionGap() { return metrics::scaled(kSectionGap); }
inline int sidebarHeaderToRows() { return metrics::scaled(kHeaderToRows); }
inline int sidebarBodyPadT() { return metrics::scaled(kBodyPadT); }
inline int sidebarPadH() { return metrics::scaled(kPadH); }
inline int sidebarRowPadX() { return metrics::scaled(kRowPadX); }
inline int sidebarIconCol() { return metrics::scaled(kIconCol); }
inline int sidebarRowIconGap() { return metrics::scaled(kRowIconGap); }
inline int collapsedBtnSize() { return metrics::scaled(kCollapsedBtn); }

void updateRangeLabel(fx::FlatRangeSliderRow& row, int minV, int maxV)
{
    if (row.getLo() <= minV && row.getHi() >= maxV)
        row.valueText = "Any";
    else
        row.valueText = juce::String(row.getLo()) + "-" + juce::String(row.getHi());
    row.repaint();
}

} // namespace

// ── FilterPanel ──────────────────────────────────────────────────────────────

FavoritesSidebar::FilterPanel::FilterPanel()
{
    juce::StringArray keys;
    keys.add("Any");
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyPicker.setItems(keys, 0);
    keyPicker.setTooltip("Filter by musical key");
    keyPicker.onChange = [this](int)
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(keyPicker);

    for (auto* range : { &bpmRange, &barsRange, &complexityRange })
    {
        range->accentFill = true;
        range->onChange = [this](int, int)
        {
            syncRangeLabels();
            if (onCriteriaChanged) onCriteriaChanged();
        };
        addAndMakeVisible(*range);
    }
    syncRangeLabels();
}

void FavoritesSidebar::FilterPanel::lookAndFeelChanged()
{
    syncRangeLabels();
    repaint();
}

void FavoritesSidebar::FilterPanel::syncRangeLabels()
{
    updateRangeLabel(bpmRange, 40, 240);
    updateRangeLabel(barsRange, 1, 64);
    updateRangeLabel(complexityRange, 0, 100);
}

int FavoritesSidebar::FilterPanel::idealHeight() const
{
    const int gap = sidebarRowGap();
    // key dropdown + bpm + bars + complexity
    return 4 * kRowH + 3 * gap;
}

void FavoritesSidebar::FilterPanel::applyTo(BrowserSearch& s) const
{
    const int bpmLo = bpmRange.getLo();
    const int bpmHi = bpmRange.getHi();
    if (bpmLo > 40 || bpmHi < 240)
    {
        s.bpmMin = (double) bpmLo;
        s.bpmMax = (double) bpmHi;
    }
    const int barsLo = barsRange.getLo();
    const int barsHi = barsRange.getHi();
    if (barsLo > 1 || barsHi < 64)
    {
        s.barsMin = barsLo;
        s.barsMax = barsHi;
    }
    const int cxLo = complexityRange.getLo();
    const int cxHi = complexityRange.getHi();
    if (cxLo > 0 || cxHi < 100)
    {
        s.complexityMin = cxLo;
        s.complexityMax = cxHi;
    }
    const int idx = keyPicker.getIndex();
    s.keyRoot = idx <= 0 ? -1 : idx - 1;
}

void FavoritesSidebar::FilterPanel::loadFrom(const BrowserSearch& s)
{
    if (s.bpmMin <= 0.0 && s.bpmMax <= 0.0)
        bpmRange.setRange(40, 240, juce::dontSendNotification);
    else
        bpmRange.setRange(s.bpmMin > 0.0 ? (int) std::lround(s.bpmMin) : 40,
                          s.bpmMax > 0.0 ? (int) std::lround(s.bpmMax) : 240,
                          juce::dontSendNotification);
    if (s.barsMin <= 0 && s.barsMax <= 0)
        barsRange.setRange(1, 64, juce::dontSendNotification);
    else
        barsRange.setRange(s.barsMin > 0 ? s.barsMin : 1,
                           s.barsMax > 0 ? s.barsMax : 64,
                           juce::dontSendNotification);
    if (s.complexityMin <= 0 && s.complexityMax <= 0)
        complexityRange.setRange(0, 100, juce::dontSendNotification);
    else
        complexityRange.setRange(s.complexityMin > 0 ? s.complexityMin : 0,
                                 s.complexityMax > 0 ? s.complexityMax : 100,
                                 juce::dontSendNotification);
    keyPicker.setIndex(s.keyRoot >= 0 ? s.keyRoot + 1 : 0, juce::dontSendNotification);
    syncRangeLabels();
}

void FavoritesSidebar::FilterPanel::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    const int padX = sidebarRowPadX();
    g.setFont(uiFontFixed((float) kFilterPt));
    g.setColour(t.rowLabel);
    g.drawText("Key", keyRow.withTrimmedLeft(padX), juce::Justification::centredLeft);
}

void FavoritesSidebar::FilterPanel::resized()
{
    auto r = getLocalBounds();
    const int rowH = kRowH;
    const int gap = sidebarRowGap();
    const int padX = sidebarRowPadX();
    const int ctrlH = rowH;
    auto chrome = [&](juce::Rectangle<int> slot) { return slot.reduced(padX, 0); };

    keyRow = r.removeFromTop(rowH);
    {
        auto area = chrome(keyRow);
        keyPicker.setBounds(area.removeFromRight(kKeySelectW).withSizeKeepingCentre(kKeySelectW, ctrlH));
    }
    r.removeFromTop(gap);
    bpmRange.setBounds(chrome(r.removeFromTop(rowH)));
    r.removeFromTop(gap);
    barsRange.setBounds(chrome(r.removeFromTop(rowH)));
    r.removeFromTop(gap);
    complexityRange.setBounds(chrome(r.removeFromTop(rowH)));
}

// ── SearchQueryPanel ─────────────────────────────────────────────────────────

FavoritesSidebar::SearchQueryPanel::SearchQueryPanel()
{
    styleEditors();
    addAndMakeVisible(queryField);
    queryField.onTextChange = [this]
    {
        updateTrailingVisible();
        repaint();
    };
    queryField.onFocused = [this]
    {
        if (onActivate) onActivate();
    };
    queryField.onEscape = [this]
    {
        if (onDeactivate) onDeactivate();
    };
    queryField.onFocusChanged = [this] { repaint(); };
    queryField.onReturnKey = [this]
    {
        if (onReturn) onReturn();
    }; // juce::TextEditor::onReturnKey
    queryField.setComponentID("searchQuery");

    clearBtn.ghost = true;
    clearBtn.iconScale = 0.85f;
    clearBtn.onClick = [this]
    {
        queryField.clear();
        updateTrailingVisible();
        if (onClear) onClear();
    };
    addChildComponent(clearBtn);

    saveBtn.ghost = true;
    saveBtn.iconScale = 0.9f;
    saveBtn.setTooltip("Save current search");
    saveBtn.onClick = [this]
    {
        if (onSave) onSave();
    };
    addAndMakeVisible(saveBtn);

    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::styleEditors()
{
    const auto& t = inspectorTokens();
    queryField.setTextToShowWhenEmpty("Search...", t.valueText);
    queryField.setFont(uiFontFixed((float) kFilterPt));
    queryField.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::textColourId, t.headerText);
    queryField.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    queryField.setColour(juce::TextEditor::highlightColourId, t.accent.withAlpha(0.28f));
    queryField.setColour(juce::CaretComponent::caretColourId, t.accent);
    queryField.setIndents(0, 2);
}

void FavoritesSidebar::SearchQueryPanel::lookAndFeelChanged()
{
    styleEditors();
    repaint();
}

void FavoritesSidebar::SearchQueryPanel::updateTrailingVisible()
{
    clearBtn.setVisible(queryField.getText().isNotEmpty());
    resized();
}

juce::String FavoritesSidebar::SearchQueryPanel::getQuery() const
{
    return queryField.getText().trim();
}

void FavoritesSidebar::SearchQueryPanel::setQuery(const juce::String& q)
{
    queryField.setText(q, juce::dontSendNotification);
    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::clearQuery()
{
    queryField.clear();
    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::focusQuery()
{
    queryField.grabKeyboardFocus();
}

void FavoritesSidebar::SearchQueryPanel::blurQuery()
{
    if (queryField.hasKeyboardFocus(true))
        queryField.giveAwayKeyboardFocus();
}

bool FavoritesSidebar::SearchQueryPanel::isQueryFocused() const
{
    return queryField.hasKeyboardFocus(true);
}

void FavoritesSidebar::SearchQueryPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!queryWell.contains(e.getPosition()))
        return;
    if (onActivate) onActivate();
    focusQuery();
}

void FavoritesSidebar::SearchQueryPanel::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    const int iconCol = sidebarIconCol();
    const bool active = queryField.hasKeyboardFocus(true);

    if (active)
    {
        auto ring = queryWell.toFloat().reduced(0.5f);
        g.setColour(t.accent);
        g.drawRoundedRectangle(ring, fx::kTallRadius, 1.2f);
    }

    const float s = 15.0f;
    auto iconArea = juce::Rectangle<float>((float) queryWell.getX(),
                                           (float) queryWell.getY(),
                                           (float) iconCol,
                                           (float) queryWell.getHeight())
                        .withSizeKeepingCentre(s, s);
    drawIcon(g, icons::search, iconArea, active ? t.accent : t.valueText, 1.3f);
}

void FavoritesSidebar::SearchQueryPanel::resized()
{
    auto r = getLocalBounds();
    const int rowH = kQueryH;
    const int padX = sidebarRowPadX();
    const int iconCol = sidebarIconCol();
    const int iconGap = sidebarRowIconGap();

    auto querySlot = r.removeFromTop(rowH).reduced(padX, 0);
    queryWell = querySlot;

    // Far right: save +, then clear × when text present.
    saveBtn.setBounds(querySlot.removeFromRight(22).withSizeKeepingCentre(18, 18));
    if (clearBtn.isVisible())
        clearBtn.setBounds(querySlot.removeFromRight(22).withSizeKeepingCentre(18, 18));
    else
        clearBtn.setBounds({});

    queryField.setBounds(querySlot.withTrimmedLeft(iconCol + iconGap).reduced(0, 3));
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

    scanSwitch.setToggleState(true, juce::dontSendNotification);
    scanSwitch.setTooltip("Include MIDI files in subfolders when searching");
    scanSwitch.onClick = [this] { scheduleLiveSearch(); };
    addChildComponent(scanSwitch);

    hideDupesSwitch.setToggleState(true, juce::dontSendNotification);
    hideDupesSwitch.setTooltip("Keep one row when the same MIDI file appears in multiple folders");
    hideDupesSwitch.onClick = [this] { scheduleLiveSearch(); };
    addChildComponent(hideDupesSwitch);

    filterPanel.onCriteriaChanged = [this] { scheduleLiveSearch(); };
    addChildComponent(filterPanel);

    searchQuery.onActivate = [this]
    {
        if (!searchOpen)
            setSectionOpen(SectionId::Search, true);
        if (collapsed)
            setCollapsed(false);
    };
    searchQuery.onDeactivate = [this] { deactivateSearch(false); };
    searchQuery.onClear = [this]
    {
        searchQuery.clearQuery();
        deactivateSearch(true);
        if (onExitSearch) onExitSearch();
    };
    searchQuery.onSave = [this]
    {
        if (onSaveCurrentSearch) onSaveCurrentSearch();
    };
    searchQuery.onReturn = [this]
    {
        if (onRunSearch) onRunSearch(getCriteria());
    };
    addChildComponent(searchQuery);

    setWantsKeyboardFocus(true);
    rebuildRows();
}

void FavoritesSidebar::notifyHeightChanged()
{
    layoutChildren();
    resized();
    repaint();
    if (onContentHeightChanged) onContentHeightChanged();
}

bool FavoritesSidebar::sectionOpen(SectionId id) const
{
    switch (id)
    {
        case SectionId::Filter: return filterOpen;
        case SectionId::Search: return searchOpen;
        case SectionId::Saved:  return savedOpen;
    }
    return false;
}

void FavoritesSidebar::setSectionOpen(SectionId id, bool open)
{
    bool* flag = nullptr;
    switch (id)
    {
        case SectionId::Filter: flag = &filterOpen; break;
        case SectionId::Search: flag = &searchOpen; break;
        case SectionId::Saved:  flag = &savedOpen; break;
    }
    if (flag == nullptr || *flag == open) return;
    *flag = open;
    notifyHeightChanged();
}

BrowserSearch FavoritesSidebar::getCriteria() const
{
    BrowserSearch s;
    s.query = searchQuery.getQuery();
    filterPanel.applyTo(s);
    s.subdirs = scanSwitch.getToggleState();
    s.removeDuplicates = hideDupesSwitch.getToggleState();
    return s;
}

void FavoritesSidebar::setCriteria(const BrowserSearch& s)
{
    searchQuery.setQuery(s.query);
    filterPanel.loadFrom(s);
    scanSwitch.setToggleState(s.subdirs, juce::dontSendNotification);
    hideDupesSwitch.setToggleState(s.removeDuplicates, juce::dontSendNotification);
}

void FavoritesSidebar::scheduleLiveSearch()
{
    startTimer(280);
}

void FavoritesSidebar::timerCallback()
{
    stopTimer();
    if (onRunSearch) onRunSearch(getCriteria());
}

int FavoritesSidebar::contentTopY() const
{
    return sidebarHeaderH() + sidebarBodyPadT();
}

int FavoritesSidebar::footerPad() const
{
    return sidebarBodyPadT();
}

int FavoritesSidebar::footerHeight() const
{
    return footerPad() + sidebarRowH() + metrics::scaled(kBodyPadB);
}

std::vector<FavoritesSidebar::LayoutItem> FavoritesSidebar::collectLayout() const
{
    std::vector<LayoutItem> out;
    const int rowH = collapsed ? collapsedBtnSize() : sidebarRowH();
    const int gap = sidebarRowGap();
    const int padH = sidebarPadH();
    const int W = getWidth();
    int y = contentTopY();

    auto placeRow = [&](int rowIdx)
    {
        const auto r = collapsed
            ? juce::Rectangle<int>(0, y, W, rowH).withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize())
            : juce::Rectangle<int>(padH, y, W - padH * 2, rowH);
        out.push_back({ LayoutKind::Row, rowIdx, r });
        y += rowH + gap;
    };

    auto placeSection = [&](SectionId id)
    {
        if (collapsed) return;
        const auto header = juce::Rectangle<int>(padH, y, W - padH * 2, sidebarSectionH());
        out.push_back({ LayoutKind::Section, (int) id, header });
        y += sidebarSectionH() + sidebarHeaderToRows();
    };

    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].kind == RowKind::Open)
            placeRow(i);

    if (!collapsed)
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::CurrentFolder)
                placeRow(i);

        {
            const auto r = juce::Rectangle<int>(padH, y, W - padH * 2, rowH);
            out.push_back({ LayoutKind::Scan, -1, r });
            y += rowH + gap;
        }
        {
            const auto r = juce::Rectangle<int>(padH, y, W - padH * 2, rowH);
            out.push_back({ LayoutKind::HideDupes, -1, r });
            y += rowH + gap;
        }

        y += sidebarSectionGap() - gap;

        placeSection(SectionId::Filter);
        if (filterOpen)
        {
            const int h = filterPanel.idealHeight();
            out.push_back({ LayoutKind::FilterBody, -1, juce::Rectangle<int>(padH, y, W - padH * 2, h) });
            y += h + gap;
        }

        y += sidebarSectionGap() - gap;

        placeSection(SectionId::Search);
        if (searchOpen)
        {
            const int qh = searchQuery.idealHeight();
            out.push_back({ LayoutKind::Query, -1, juce::Rectangle<int>(padH, y, W - padH * 2, qh) });
            y += qh + gap;

            for (int i = 0; i < (int) rows.size(); ++i)
                if (rows[(size_t) i].kind == RowKind::SavedSearch)
                    placeRow(i);
        }

        y += sidebarSectionGap() - gap;

        placeSection(SectionId::Saved);
        if (savedOpen)
        {
            for (int i = 0; i < (int) rows.size(); ++i)
                if (rows[(size_t) i].kind == RowKind::Starred
                    || rows[(size_t) i].kind == RowKind::SavedDir)
                    placeRow(i);
        }
    }
    else
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Starred)
                placeRow(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedDir)
                placeRow(i);
    }

    out.push_back({ LayoutKind::End, -1, juce::Rectangle<int>(0, y, W, 0) });
    return out;
}

int FavoritesSidebar::contentBottomY() const
{
    int bottom = contentTopY();
    for (const auto& item : collectLayout())
    {
        if (item.kind == LayoutKind::End)
            bottom = item.bounds.getY();
        else if (!item.bounds.isEmpty())
            bottom = juce::jmax(bottom, item.bounds.getBottom());
    }
    return bottom;
}

int FavoritesSidebar::idealMinHeight() const
{
    return contentBottomY() + footerHeight();
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
    layoutChildren();
    repaint();
}

void FavoritesSidebar::setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeIdx)
{
    savedSearches = searches;
    activeSearchIdx = activeIdx;
    rebuildRows();
    layoutChildren();
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
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse to icons");
    if (onCollapsedChanged) onCollapsedChanged();
    notifyHeightChanged();
}

void FavoritesSidebar::rebuildRows()
{
    rows.clear();
    rows.push_back({ RowKind::Open });
    if (active.isNotEmpty() && juce::File(active).isDirectory())
        rows.push_back({ RowKind::CurrentFolder });
    rows.push_back({ RowKind::Starred });
    for (int i = 0; i < dirs.size(); ++i)
        rows.push_back({ RowKind::SavedDir, i });
    for (int i = 0; i < (int) savedSearches.size(); ++i)
        rows.push_back({ RowKind::SavedSearch, i });
}

void FavoritesSidebar::layoutChildren()
{
    filterHeaderBounds = {};
    searchHeaderBounds = {};
    savedHeaderBounds = {};
    scanRowBounds = {};
    currentFolderPlusBounds = {};

    hideDupesRowBounds = {};

    if (collapsed)
    {
        filterPanel.setVisible(false);
        searchQuery.setVisible(false);
        scanSwitch.setVisible(false);
        hideDupesSwitch.setVisible(false);
        return;
    }

    auto placeToggle = [](fx::FlatSwitch& sw, juce::Rectangle<int> row)
    {
        auto area = row.reduced(sidebarRowPadX(), 0);
        const int w = sw.idealWidth();
        sw.setBounds(area.removeFromRight(w).withSizeKeepingCentre(w, sidebarRowH()));
        sw.setVisible(true);
    };

    for (const auto& item : collectLayout())
    {
        switch (item.kind)
        {
            case LayoutKind::Section:
                switch ((SectionId) item.id)
                {
                    case SectionId::Filter: filterHeaderBounds = item.bounds; break;
                    case SectionId::Search: searchHeaderBounds = item.bounds; break;
                    case SectionId::Saved:  savedHeaderBounds = item.bounds; break;
                }
                break;
            case LayoutKind::Scan:
                scanRowBounds = item.bounds;
                placeToggle(scanSwitch, item.bounds);
                break;
            case LayoutKind::HideDupes:
                hideDupesRowBounds = item.bounds;
                placeToggle(hideDupesSwitch, item.bounds);
                break;
            case LayoutKind::FilterBody:
                filterPanel.setBounds(item.bounds);
                filterPanel.setVisible(filterOpen);
                break;
            case LayoutKind::Query:
                searchQuery.setBounds(item.bounds);
                searchQuery.setVisible(searchOpen);
                break;
            case LayoutKind::Row:
                if (juce::isPositiveAndBelow(item.id, (int) rows.size())
                    && rows[(size_t) item.id].kind == RowKind::CurrentFolder)
                    currentFolderPlusBounds = item.bounds.withTrimmedLeft(item.bounds.getWidth() - 22);
                break;
            default:
                break;
        }
    }

    if (!filterOpen) filterPanel.setVisible(false);
    if (!searchOpen) searchQuery.setVisible(false);
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int rowIdx) const
{
    for (const auto& item : collectLayout())
        if (item.kind == LayoutKind::Row && item.id == rowIdx)
            return item.bounds;
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
        hit.addZone = !collapsed
            && hit.kind == RowKind::CurrentFolder
            && pos.x > r.getRight() - 22;
        return hit;
    }
    return {};
}

FavoritesSidebar::ChromeHit FavoritesSidebar::chromeHitAt(juce::Point<int> pos) const
{
    if (settingsRowBounds.contains(pos)) return ChromeHit::Settings;
    if (collapsed) return ChromeHit::None;
    if (filterHeaderBounds.contains(pos)) return ChromeHit::FilterHeader;
    if (searchHeaderBounds.contains(pos)) return ChromeHit::SearchHeader;
    if (savedHeaderBounds.contains(pos)) return ChromeHit::SavedHeader;
    if (currentFolderPlusBounds.contains(pos)) return ChromeHit::CurrentFolderPlus;
    if (scanRowBounds.contains(pos)) return ChromeHit::ScanRow;
    if (hideDupesRowBounds.contains(pos)) return ChromeHit::HideDupesRow;
    return ChromeHit::None;
}

void FavoritesSidebar::setSearchFormOpen(bool open)
{
    if (open && collapsed)
        setCollapsed(false);
    setSectionOpen(SectionId::Search, open);
    if (open)
        searchQuery.focusQuery();
    else
        searchQuery.blurQuery();
}

void FavoritesSidebar::deactivateSearch(bool clearCriteria)
{
    searchQuery.blurQuery();
    if (clearCriteria)
        searchQuery.clearQuery();
}

void FavoritesSidebar::resized()
{
    const int railW = metrics::sidebarRailWidth();
    const int headerH = sidebarHeaderH();
    const int padH = sidebarPadH();
    const int iconBtn = metrics::chromeIconButton();
    btnToggle.iconScale = 0.9f;
    btnToggle.setBounds(juce::Rectangle<int>(collapsed ? 0 : padH - 2, 0,
                                             collapsed ? railW : iconBtn + 4, headerH)
                            .withSizeKeepingCentre(iconBtn, iconBtn));

    const int footerH = footerHeight();
    const int contentBottom = contentBottomY();
    const int pad = footerPad();
    const int settingsTop = juce::jmax(contentBottom + pad,
                                       getHeight() - footerH + pad);
    if (collapsed)
    {
        settingsRowBounds = juce::Rectangle<int>(0, settingsTop, getWidth(), sidebarRowH())
                                .withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize());
    }
    else
    {
        settingsRowBounds = juce::Rectangle<int>(padH, settingsTop,
                                                 getWidth() - padH * 2,
                                                 sidebarRowH());
    }
    btnSettings.setVisible(false);
    layoutChildren();
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    if (!collapsed && e.x >= getWidth() - 5)
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);

    const bool overSettings = settingsRowBounds.contains(e.getPosition());
    const auto hit = rowHitAt(e.getPosition());
    const int rowIdx = hit.valid ? [&]
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
                return i;
        return -1;
    }() : -1;
    if (rowIdx != hoverRow || hit.removeZone != hoverRemove || hit.addZone != hoverAdd
        || overSettings != hoverSettings)
    {
        hoverRow = rowIdx;
        hoverRemove = hit.removeZone;
        hoverAdd = hit.addZone;
        hoverSettings = overSettings;
        repaint();
    }
}

void FavoritesSidebar::mouseExit(const juce::MouseEvent&)
{
    if (!resizing)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    hoverRow = -1;
    hoverRemove = false;
    hoverAdd = false;
    hoverSettings = false;
    repaint();
}

void FavoritesSidebar::mouseDown(const juce::MouseEvent& e)
{
    if (filterPanel.isVisible() && filterPanel.getBounds().contains(e.getPosition()))
        return;
    if (searchQuery.isVisible() && searchQuery.getBounds().contains(e.getPosition()))
        return;
    if (scanSwitch.isVisible() && scanSwitch.getBounds().contains(e.getPosition()))
        return;
    if (hideDupesSwitch.isVisible() && hideDupesSwitch.getBounds().contains(e.getPosition()))
        return;

    if (!collapsed && e.x >= getWidth() - 5)
    {
        resizing = true;
        resizeStartWidth = idealWidth();
        resizeStartX = e.getScreenX();
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    switch (chromeHitAt(e.getPosition()))
    {
        case ChromeHit::Settings:
            if (onOpenSettings) onOpenSettings();
            return;
        case ChromeHit::FilterHeader:
            setSectionOpen(SectionId::Filter, !filterOpen);
            return;
        case ChromeHit::SearchHeader:
            setSectionOpen(SectionId::Search, !searchOpen);
            return;
        case ChromeHit::SavedHeader:
            setSectionOpen(SectionId::Saved, !savedOpen);
            return;
        case ChromeHit::CurrentFolderPlus:
            if (onAddCurrent) onAddCurrent();
            return;
        case ChromeHit::ScanRow:
            scanSwitch.setToggleState(!scanSwitch.getToggleState(), juce::sendNotification);
            scheduleLiveSearch();
            repaint(scanRowBounds);
            return;
        case ChromeHit::HideDupesRow:
            hideDupesSwitch.setToggleState(!hideDupesSwitch.getToggleState(), juce::sendNotification);
            scheduleLiveSearch();
            repaint(hideDupesRowBounds);
            return;
        case ChromeHit::None:
            break;
    }

    const auto hit = rowHitAt(e.getPosition());
    if (!hit.valid)
    {
        if (searchQuery.isQueryFocused())
            deactivateSearch(false);
        return;
    }

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

    if (hit.addZone)
    {
        if (onAddCurrent) onAddCurrent();
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
        case RowKind::Open:
            if (onOpenFolder) onOpenFolder();
            break;
        case RowKind::CurrentFolder:
            if (onPickDir && active.isNotEmpty()) onPickDir(active);
            break;
        case RowKind::SavedDir:
            if (onPickDir) onPickDir(dirs[hit.index]);
            break;
        case RowKind::Starred:
            if (onShowStarred) onShowStarred();
            break;
        case RowKind::SavedSearch:
            if (onPickSavedSearch) onPickSavedSearch(hit.index);
            break;
    }
}

bool FavoritesSidebar::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && searchQuery.isQueryFocused())
    {
        deactivateSearch(false);
        return true;
    }
    return false;
}

void FavoritesSidebar::mouseDrag(const juce::MouseEvent& e)
{
    if (!resizing) return;

    const int maxW = metrics::sidebarExpandedWidth();
    const int railW = metrics::sidebarRailWidth();
    const int dx = e.getScreenX() - resizeStartX;
    const int next = juce::jlimit(railW, maxW, resizeStartWidth + dx);

    if (next <= railW + 2)
    {
        resizing = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        expandedWidth = maxW;
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

void FavoritesSidebar::paintSectionHeader(juce::Graphics& g, const juce::String& title, bool open,
                                          juce::Rectangle<int> headerBounds)
{
    if (headerBounds.isEmpty()) return;
    auto area = headerBounds.reduced(sidebarRowPadX(), 0);
    const auto chevCol = open ? colours::accent() : colours::text3();
    const auto titleCol = open ? colours::accent() : colours::text3();

    auto chev = area.removeFromLeft(14).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    // Chevron: right when folded, down when open (draw as small triangle path).
    juce::Path p;
    const float cx = chev.getCentreX(), cy = chev.getCentreY();
    if (open)
        p.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    else
        p.addTriangle(cx - 2.0f, cy - 4.0f, cx + 3.0f, cy, cx - 2.0f, cy + 4.0f);
    g.setColour(chevCol);
    g.fillPath(p);

    area.removeFromLeft(6);
    g.setColour(titleCol);
    g.setFont(uiFontFixed((float) kTitlePt, true));
    g.drawText(title, area, juce::Justification::centredLeft, false);
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone, bool addZone)
{
    if (r.isEmpty() || !juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return;

    const auto& row = rows[(size_t) rowIdx];
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeCurrent = row.kind == RowKind::CurrentFolder && active.isNotEmpty();
    const bool activeStar = row.kind == RowKind::Starred
        && (starredFilter || browseMode == 1);
    const bool activeSearch = row.kind == RowKind::SavedSearch && row.index == activeSearchIdx;
    const bool isActive = activeDir || activeCurrent || activeStar || activeSearch;
    const bool lit = isActive || hovered;

    const auto iconColour = lit ? colours::accent() : colours::text3();
    const auto textCol = lit ? colours::text() : colours::text2();
    const char* icon = icons::folder;
    juce::String label;
    switch (row.kind)
    {
        case RowKind::Open:       icon = icons::folderOpen; label = "Open Folder"; break;
        case RowKind::CurrentFolder:
        {
            icon = icons::folder;
            const juce::File dir(active);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : active;
            break;
        }
        case RowKind::SavedDir:
        {
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            break;
        }
        case RowKind::Starred:    icon = icons::starOutline; label = "Favorites"; break;
        case RowKind::SavedSearch:
            icon = icons::search;
            label = savedSearches[(size_t) row.index].name;
            break;
    }

    if (collapsed)
    {
        if (hovered)
            fx::fillTallWell(g, r.toFloat(), true);
        drawIcon(g, icon, r.toFloat().withSizeKeepingCentre(16.0f, 16.0f), iconColour, 1.4f);
        return;
    }

    auto textArea = r.reduced(sidebarRowPadX(), 0);
    const int iconCol = sidebarIconCol();
    auto iconSlot = textArea.removeFromLeft(iconCol).toFloat();
    drawIcon(g, icon, iconSlot.withSizeKeepingCentre(15.0f, 15.0f), iconColour, 1.3f);
    textArea.removeFromLeft(sidebarRowIconGap());

    if (row.kind == RowKind::CurrentFolder)
    {
        auto plusArea = textArea.removeFromRight(18).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
        drawIcon(g, icons::plus, plusArea,
                 addZone || hovered ? colours::accent() : colours::text3(), 1.4f);
    }
    else if (hovered && (row.kind == RowKind::SavedDir || row.kind == RowKind::SavedSearch))
    {
        auto xArea = textArea.removeFromRight(18).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, icons::x, xArea,
                 removeZone ? colours::text() : colours::text3(), 1.4f);
    }

    g.setColour(textCol);
    g.setFont(uiFontFixed((float) kCtrlPt, isActive));
    g.drawText(label, textArea, juce::Justification::centredLeft, true);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::bg());
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));

    const int padH = sidebarPadH();
    const int headerH = sidebarHeaderH();

    g.setColour(colours::line());
    g.fillRect(0, headerH - 1, getWidth(), 1);
    if (!collapsed)
    {
        g.setColour(colours::text3());
        g.setFont(uiFontFixed((float) kCapPt, true));
        const int titleX = padH + metrics::chromeIconButton() + 4;
        g.drawText("LIBRARY", titleX, 0, 120, headerH,
                   juce::Justification::centredLeft, false);
    }

    const int footerTop = settingsRowBounds.getY() - footerPad();
    if (footerTop > headerH)
    {
        g.setColour(colours::line());
        g.fillRect(0, footerTop, getWidth(), 1);
    }

    auto paintToggleRow = [&](juce::Rectangle<int> row, fx::FlatSwitch& sw, const juce::String& label)
    {
        if (row.isEmpty()) return;
        auto area = row.reduced(sidebarRowPadX(), 0);
        area.removeFromRight(sw.idealWidth() + 4);
        auto iconSlot = area.removeFromLeft(sidebarIconCol()).toFloat();
        drawIcon(g, icons::folder, iconSlot.withSizeKeepingCentre(15.0f, 15.0f),
                 colours::text3(), 1.3f);
        area.removeFromLeft(sidebarRowIconGap());
        g.setColour(colours::text2());
        g.setFont(uiFontFixed((float) kCtrlPt, false));
        g.drawText(label, area, juce::Justification::centredLeft, true);
    };

    if (!collapsed)
    {
        paintToggleRow(scanRowBounds, scanSwitch, "Scan subdirectories");
        paintToggleRow(hideDupesRowBounds, hideDupesSwitch, "Hide Duplicates");
    }

    paintSectionHeader(g, "FILTER", filterOpen, filterHeaderBounds);
    paintSectionHeader(g, "SEARCH", searchOpen, searchHeaderBounds);
    paintSectionHeader(g, "SAVED", savedOpen, savedHeaderBounds);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto r = rowBounds(i);
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove, i == hoverRow && hoverAdd);
    }

    if (!settingsRowBounds.isEmpty())
    {
        if (collapsed)
        {
            const auto& t = inspectorTokens();
            g.setColour(hoverSettings ? t.tallWellHot : t.tallWell);
            g.fillRoundedRectangle(settingsRowBounds.toFloat(), 4.0f);
            if (hoverSettings)
            {
                g.setColour(t.controlHairline);
                g.drawRoundedRectangle(settingsRowBounds.toFloat().reduced(0.5f), 4.0f, 1.0f);
            }
            drawIcon(g, icons::gear,
                     settingsRowBounds.toFloat().withSizeKeepingCentre(16.0f, 16.0f),
                     colours::text3(), 1.3f);
        }
        else
        {
            const bool lit = hoverSettings;
            const auto iconColour = lit ? colours::accent() : colours::text3();
            const auto textCol = lit ? colours::text() : colours::text2();
            auto area = settingsRowBounds.reduced(sidebarRowPadX(), 0);
            auto iconSlot = area.removeFromLeft(sidebarIconCol()).toFloat();
            drawIcon(g, icons::gear, iconSlot.withSizeKeepingCentre(16.0f, 16.0f), iconColour, 1.3f);
            area.removeFromLeft(sidebarRowIconGap());
            g.setColour(textCol);
            g.setFont(uiFontFixed((float) kCtrlPt, false));
            g.drawText("Settings", area, juce::Justification::centredLeft, true);
        }
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
    // Hide the list under the searching overlay so "Searching..." paints on top.
    viewport.setVisible(!searching);
    scrollTopBtn.setVisible(false);
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
    int w = metrics::scaled(8) * 2 + metrics::scaled(kNameW) + metrics::scaled(14);
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
    // Extra gap so the name never crowds the first meta column.
    row.removeFromLeft(metrics::scaled(14));
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
        // Dim the browser surface; message sits on top.
        g.setColour(colours::panel().withMultipliedBrightness(0.72f));
        g.fillRect(area);
        g.setColour(colours::text().withAlpha(0.22f));
        g.fillRect(area);
        g.setColour(colours::text());
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
        g.setColour(active ? colours::text() : colours::text2());
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

    // Primary name uses full text; meta columns use secondary (tuned for dark contrast).
    const auto textCol = isSelected ? juce::Colours::white : colours::text();
    const auto metaCol = isSelected ? juce::Colours::white.withAlpha(0.88f) : colours::text2();

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
        juce::StringArray locs = entry.locations;
        if (locs.isEmpty())
            locs.add(entry.file.getFullPathName());

        if (locs.size() <= 1)
        {
            m.addItem(1, "Show in Finder");
        }
        else
        {
            for (int i = 0; i < locs.size(); ++i)
                m.addItem(100 + i, "Show in Finder Location " + juce::String(i + 1));
        }
        m.addItem(2, "Copy to Folder...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                             .withMousePosition(),
            [this, locs, file = entry.file](int result)
            {
                if (result == 1 && owner.onRevealFile)
                    owner.onRevealFile(file);
                else if (result >= 100 && result < 100 + locs.size() && owner.onRevealFile)
                    owner.onRevealFile(juce::File(locs[result - 100]));
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
