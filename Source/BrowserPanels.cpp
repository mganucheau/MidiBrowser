#include "BrowserPanels.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace pflow {

namespace {

// 3c source-list metrics (Cupertino sidebar).
constexpr int kSidebarHeaderH = 36;
constexpr int kSidebarSectionH = 18;
constexpr int kSidebarRowH = 29;
constexpr int kOpenFolderH = 30;
constexpr int kActionRowH = 24;
constexpr int kSectionGap = 12;
constexpr int kHeaderToRows = 4;
constexpr int kBodyPadT = 10;
constexpr int kBodyPadB = 10;
constexpr int kPadH = 14;
constexpr int kPadV = 12;
constexpr int kRowPadX = 6;
constexpr int kIconCol = 18;
constexpr int kRowIconGap = 6;
constexpr int kCollapsedBtn = 28;
constexpr int kActionGap = 6;
constexpr float kCapPt = 10.0f;
constexpr float kTitlePt = 12.5f;
constexpr float kCtrlPt = 12.5f;
constexpr float kPathPt = 10.0f;
constexpr float kSidebarIconStroke = 1.5f;

inline int sidebarHeaderH() { return metrics::scaled(kSidebarHeaderH); }
inline int sidebarSectionH() { return metrics::scaled(kSidebarSectionH); }
inline int sidebarRowH() { return metrics::scaled(kSidebarRowH); }
inline int sidebarRowGap() { return metrics::scaled(2); }
inline int sidebarSectionGap() { return metrics::scaled(kSectionGap); }
inline int sidebarHeaderToRows() { return metrics::scaled(kHeaderToRows); }
inline int sidebarBodyPadT() { return metrics::scaled(kBodyPadT); }
inline int sidebarPadH() { return metrics::scaled(kPadH); }
inline int sidebarPadV() { return metrics::scaled(kPadV); }
inline int sidebarRowPadX() { return metrics::scaled(kRowPadX); }
inline int sidebarIconCol() { return metrics::scaled(kIconCol); }
inline int sidebarRowIconGap() { return metrics::scaled(kRowIconGap); }
inline int collapsedBtnSize() { return metrics::scaled(kCollapsedBtn); }
inline int openFolderH() { return metrics::scaled(kOpenFolderH); }
inline int actionRowH() { return metrics::scaled(kActionRowH); }
inline int actionGap() { return metrics::scaled(kActionGap); }

struct SourceListPalette
{
    juce::Colour tint, card, control, text, text2, text3, accent, selectWash, border, gold, shadow;
};

inline SourceListPalette sl()
{
    // All colours from ds:: tokens (§2) — no local hex.
    return {
        ds::side(),
        ds::panel(),
        ds::ctl(),
        ds::tx(),
        ds::tx2(),
        ds::tx3(),
        ds::acc(),
        ds::selsoft(),
        ds::ctlb(),
        ds::stron(),
        juce::Colours::black.withAlpha(usesDarkAppearance() ? 0.30f : 0.14f),
    };
}

inline void fillControlSurface(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    const auto p = sl();
    g.setColour(p.control);
    g.fillRoundedRectangle(r, radius);
    g.setColour(p.border);
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
    ds::shadow::control(g, r, radius);
}

} // namespace

// ── FilterPanel ──────────────────────────────────────────────────────────────

FavoritesSidebar::FilterPanel::FilterPanel()
{
    juce::StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyGrid.title = "KEY";
    keyGrid.columns = 6;
    keyGrid.multiSelect = true;
    keyGrid.sidebarLabelStyle = true;
    keyGrid.setItems(keys);
    keyGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(keyGrid);

    for (auto* range : { &bpmRange, &barsRange, &complexityRange })
    {
        range->sidebarLabelStyle = true;
        range->onChange = [this](int, int)
        {
            if (onCriteriaChanged) onCriteriaChanged();
        };
        addAndMakeVisible(*range);
    }
    bpmRange.setTooltip("Filter by tempo (BPM)");
    barsRange.setTooltip("Filter by length in bars");
    complexityRange.setTooltip("Filter by complexity score");

    hideDupesSwitch.setToggleState(true, juce::dontSendNotification);
    hideDupesSwitch.setTooltip("Keep one row when the same MIDI file appears in multiple folders");
    hideDupesSwitch.onClick = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(hideDupesSwitch);
}

void FavoritesSidebar::FilterPanel::lookAndFeelChanged()
{
    repaint();
}

int FavoritesSidebar::FilterPanel::rowH() const
{
    return sidebarRowH();
}

int FavoritesSidebar::FilterPanel::rangeH() const
{
    // Histogram + range track sized to two sidebar rows.
    return rowH() * 2;
}

int FavoritesSidebar::FilterPanel::idealHeight() const
{
    // Roomier vertical rhythm between key grid / range blocks / hide-dupes.
    const int gap = metrics::scaled(12);
    const int rh = rangeH();
    return metrics::scaled(4) + keyGrid.idealHeight() + gap
         + rh + gap
         + rh + gap
         + rh + gap
         + rowH();
}

void FavoritesSidebar::FilterPanel::applyTo(BrowserSearch& s) const
{
    const int bpmLo = bpmRange.getLo();
    const int bpmHi = bpmRange.getHi();
    if (!bpmRange.isFullRange())
    {
        s.bpmMin = (double) bpmLo;
        s.bpmMax = (double) bpmHi;
    }
    const int barsLo = barsRange.getLo();
    const int barsHi = barsRange.getHi();
    if (!barsRange.isFullRange())
    {
        s.barsMin = barsLo;
        s.barsMax = barsHi;
    }
    if (!complexityRange.isFullRange())
    {
        s.complexityMin = complexityRange.getLo();
        s.complexityMax = complexityRange.getHi();
    }
    s.keyMask = keyGrid.getSelectedMask();
    s.keyRoot = -1;
    if (s.keyMask != 0)
    {
        // Prefer a single-bit keyRoot for legacy saved-search labels.
        for (int i = 0; i < 12; ++i)
            if ((s.keyMask & (uint16_t) (1u << i)) != 0)
            {
                s.keyRoot = i;
                break;
            }
        int bits = 0;
        for (int i = 0; i < 12; ++i)
            if ((s.keyMask & (uint16_t) (1u << i)) != 0) ++bits;
        if (bits > 1)
            s.keyRoot = -1;
    }
    s.removeDuplicates = hideDupesSwitch.getToggleState();
}

int FavoritesSidebar::FilterPanel::activeGroupCount() const
{
    int n = 0;
    if (keyGrid.getSelectedMask() != 0) ++n;
    if (!bpmRange.isFullRange()) ++n;
    if (!barsRange.isFullRange()) ++n;
    if (!complexityRange.isFullRange()) ++n;
    return n;
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

    uint16_t mask = s.keyMask;
    if (mask == 0 && s.keyRoot >= 0 && s.keyRoot < 12)
        mask = (uint16_t) (1u << s.keyRoot);
    keyGrid.setSelectedMask(mask, juce::dontSendNotification);
    hideDupesSwitch.setToggleState(s.removeDuplicates, juce::dontSendNotification);
}

void FavoritesSidebar::FilterPanel::setHistograms(const std::vector<StepClip>& clips)
{
    std::vector<int> bpm, bars, cx;
    bpm.reserve(clips.size());
    bars.reserve(clips.size());
    cx.reserve(clips.size());
    for (const auto& c : clips)
    {
        if (c.bpm > 0.0)
            bpm.push_back(juce::jlimit(40, 240, (int) std::lround(c.bpm)));
        if (c.bars > 0)
            bars.push_back(juce::jlimit(1, 64, c.bars));
        if (c.complexity > 0)
            cx.push_back(juce::jlimit(0, 100, c.complexity));
    }
    bpmRange.setHistogramFromSamples(bpm, 16);
    barsRange.setHistogramFromSamples(bars, 16);
    complexityRange.setHistogramFromSamples(cx, 16);
}

void FavoritesSidebar::FilterPanel::paint(juce::Graphics& g)
{
    if (!hideDupesRow.isEmpty())
    {
        auto area = hideDupesRow;
        area.removeFromRight(hideDupesSwitch.idealWidth() + 4);
        g.setColour(sl().text2);
        g.setFont(uiFontFixed(11.0f));
        g.drawText("Hide Duplicates", area, juce::Justification::centredLeft, true);
    }
}

void FavoritesSidebar::FilterPanel::resized()
{
    auto r = getLocalBounds();
    const int gap = metrics::scaled(12);
    const int h = rowH();
    const int rh = rangeH();

    r.removeFromTop(metrics::scaled(4)); // space under action-row divider
    keyGrid.setBounds(r.removeFromTop(keyGrid.idealHeight()));
    r.removeFromTop(gap);
    bpmRange.setBounds(r.removeFromTop(rh));
    r.removeFromTop(gap);
    barsRange.setBounds(r.removeFromTop(rh));
    r.removeFromTop(gap);
    complexityRange.setBounds(r.removeFromTop(rh));
    r.removeFromTop(gap);
    hideDupesRow = r.removeFromTop(h);
    {
        auto area = hideDupesRow;
        const int w = hideDupesSwitch.idealWidth();
        hideDupesSwitch.setBounds(area.removeFromRight(w).withSizeKeepingCentre(w, h));
    }
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
    };
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

    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::styleEditors()
{
    const auto p = sl();
    queryField.setTextToShowWhenEmpty({}, p.text3);
    queryField.setFont(uiFontFixed(11.0f));
    queryField.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::textColourId, p.text);
    queryField.setColour(juce::TextEditor::highlightedTextColourId, colours::accentInk());
    queryField.setColour(juce::TextEditor::highlightColourId, p.accent.withAlpha(0.35f));
    queryField.setColour(juce::CaretComponent::caretColourId, p.accent);
    queryField.setIndents(0, 2);
}

void FavoritesSidebar::SearchQueryPanel::lookAndFeelChanged()
{
    styleEditors();
    repaint();
}

void FavoritesSidebar::SearchQueryPanel::updateTrailingVisible()
{
    saveVisible = queryField.getText().isNotEmpty();
    clearBtn.setVisible(false); // 3c: Save chip replaces clear; Escape clears via onDeactivate
    resized();
    repaint();
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
    if (saveVisible && saveChipBounds.contains(e.getPosition()))
    {
        if (onSave) onSave();
        return;
    }
    if (!queryWell.contains(e.getPosition()))
        return;
    if (onActivate) onActivate();
    focusQuery();
}

void FavoritesSidebar::SearchQueryPanel::paint(juce::Graphics& g)
{
    const auto p = sl();
    const int iconCol = sidebarIconCol();
    const bool active = queryField.hasKeyboardFocus(true);

    if (!queryWell.isEmpty())
    {
        fillControlSurface(g, queryWell.toFloat(), 6.0f);
        if (active)
            drawFocusRing(g, queryWell.toFloat(), 6.0f);
    }

    auto iconArea = juce::Rectangle<float>((float) queryWell.getX() + 4.0f,
                                           (float) queryWell.getY(),
                                           (float) iconCol,
                                           (float) queryWell.getHeight())
                        .withSizeKeepingCentre(13.0f, 13.0f);
    drawIcon(g, icons::search, iconArea, active ? p.accent : p.text3, kSidebarIconStroke);

    if (saveVisible && !saveChipBounds.isEmpty())
    {
        g.setColour(p.accent);
        g.fillRoundedRectangle(saveChipBounds.toFloat(), (float) saveChipBounds.getHeight() * 0.5f);
        g.setColour(colours::accentInk());
        g.setFont(uiFontFixed(9.5f, true));
        g.drawText("Save", saveChipBounds, juce::Justification::centred, false);
    }
}

void FavoritesSidebar::SearchQueryPanel::resized()
{
    auto r = getLocalBounds();
    const int rowH = idealHeight();
    const int iconCol = sidebarIconCol();
    const int iconGap = sidebarRowIconGap();

    auto querySlot = r.removeFromTop(rowH);
    queryWell = querySlot;

    saveChipBounds = {};
    if (saveVisible)
    {
        constexpr int kChipW = 40;
        saveChipBounds = querySlot.removeFromRight(kChipW + 4)
                             .withTrimmedRight(4)
                             .withSizeKeepingCentre(kChipW, juce::jmin(18, rowH - 6));
    }

    queryField.setBounds(querySlot.withTrimmedLeft(iconCol + iconGap + 4).reduced(0, 3));
}

// ── FavoritesSidebar (3c compact action row) ─────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.ghost = true;
    btnToggle.iconScale = 1.0f;
    btnToggle.setWantsKeyboardFocus(true);
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
    btnToggle.onClick = [this] { setCollapsed(!collapsed); };
    addAndMakeVisible(btnToggle);

    btnSettings.ghost = true;
    btnSettings.iconScale = 1.0f;
    btnSettings.setWantsKeyboardFocus(true);
    btnSettings.setTooltip("Settings");
    btnSettings.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
    addAndMakeVisible(btnSettings);

    includeSwitch.setToggleState(false, juce::dontSendNotification);
    includeSwitch.setTooltip("Include MIDI from all subfolders");
    includeSwitch.onClick = [this]
    {
        includeSubdirs = includeSwitch.getToggleState();
        if (onIncludeSubdirsChanged) onIncludeSubdirsChanged(includeSubdirs);
        repaint();
    };
    addChildComponent(includeSwitch);

    expandedWidth = metrics::sidebarExpandedWidth();
    setWantsKeyboardFocus(true);

    filterPanel.onCriteriaChanged = [this]
    {
        scheduleFilterApply();
        repaint(); // filter badge
    };
    addChildComponent(filterPanel);

    searchQuery.onActivate = [this]
    {
        searchOpen = true;
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

    rebuildRows();
}

void FavoritesSidebar::notifyHeightChanged()
{
    layoutChildren();
    resized();
    repaint();
    if (onContentHeightChanged) onContentHeightChanged();
}

BrowserSearch FavoritesSidebar::getFilter() const
{
    BrowserSearch s;
    filterPanel.applyTo(s);
    s.subdirs = includeSubdirs;
    return s;
}

void FavoritesSidebar::setIncludeSubdirs(bool on)
{
    if (includeSubdirs == on) return;
    includeSubdirs = on;
    includeSwitch.setToggleState(on, juce::dontSendNotification);
    repaint();
}

void FavoritesSidebar::setCurrentClipCount(int n)
{
    currentClipCount = juce::jmax(0, n);
    repaint();
}

void FavoritesSidebar::setStarredCount(int n)
{
    starredCount = juce::jmax(0, n);
    repaint();
}

void FavoritesSidebar::setScanning(bool on)
{
    if (scanning == on) return;
    scanning = on;
    if (scanning)
    {
        startTimerHz(30); // spin Update icon; filter debounce waits until idle
    }
    else if (filterApplyPending)
    {
        startTimer(280);
    }
    else
    {
        stopTimer();
    }
    repaint();
}

void FavoritesSidebar::setFilterHistograms(const std::vector<StepClip>& clips)
{
    histogramClips = clips;
    filterPanel.setHistograms(clips);
}

BrowserSearch FavoritesSidebar::getCriteria() const
{
    BrowserSearch s = getFilter();
    s.query = searchQuery.getQuery();
    return s;
}

void FavoritesSidebar::setCriteria(const BrowserSearch& s)
{
    searchQuery.setQuery(s.query);
    filterPanel.loadFrom(s);
    repaint();
}

void FavoritesSidebar::scheduleFilterApply()
{
    filterApplyPending = true;
    if (!scanning)
        startTimer(280);
}

void FavoritesSidebar::timerCallback()
{
    if (scanning)
    {
        scanAngle += 0.28f;
        if (scanAngle > juce::MathConstants<float>::twoPi)
            scanAngle -= juce::MathConstants<float>::twoPi;
        repaint(updateBtnBounds);
        return;
    }
    stopTimer();
    if (filterApplyPending)
    {
        filterApplyPending = false;
        if (onFilterChanged) onFilterChanged();
    }
}

bool FavoritesSidebar::isCurrentFolderFavorited() const
{
    return active.isNotEmpty() && dirs.contains(active);
}

int FavoritesSidebar::activeFilterGroupCount() const
{
    return filterPanel.activeGroupCount();
}

int FavoritesSidebar::countMidiFilesQuick(const juce::File& dir)
{
    if (!dir.isDirectory()) return 0;
    // Cache per path so paint doesn't re-stat huge folders every frame.
    struct CacheEntry { juce::int64 modMs = 0; int count = 0; };
    static std::map<juce::String, CacheEntry> cache;
    const auto path = dir.getFullPathName();
    const auto modMs = dir.getLastModificationTime().toMilliseconds();
    if (auto it = cache.find(path); it != cache.end() && it->second.modMs == modMs)
        return it->second.count;
    const int n = dir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi").size();
    cache[path] = { modMs, n };
    return n;
}

int FavoritesSidebar::folderCardHeight() const
{
    if (active.isEmpty() || !juce::File(active).isDirectory())
        return 0;
    // header (~34) + subfolders row (22) + gap + action row (24) + pad
    int h = metrics::scaled(34 + 22 + 6 + kActionRowH + 22);
    if (filterOpen)
        h += metrics::scaled(8) + filterPanel.idealHeight() + metrics::scaled(6);
    return h;
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
    const int itemInset = padH + sidebarRowPadX();

    auto placeRow = [&](int rowIdx)
    {
        const auto r = collapsed
            ? juce::Rectangle<int>(0, y, W, rowH).withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize())
            : juce::Rectangle<int>(itemInset, y, W - itemInset * 2, rowH);
        out.push_back({ LayoutKind::Row, rowIdx, r });
        y += rowH + gap;
    };

    auto placeSection = [&](SectionId id)
    {
        if (collapsed) return;
        const auto header = juce::Rectangle<int>(itemInset, y, W - itemInset * 2, sidebarSectionH());
        out.push_back({ LayoutKind::Section, (int) id, header });
        y += sidebarSectionH() + sidebarHeaderToRows();
    };

    if (!collapsed)
    {
        out.push_back({ LayoutKind::OpenBtn, -1,
                        juce::Rectangle<int>(padH, y, W - padH * 2, openFolderH()) });
        y += openFolderH() + metrics::scaled(8);

        const int cardH = folderCardHeight();
        if (cardH > 0)
        {
            out.push_back({ LayoutKind::FolderCard, -1,
                            juce::Rectangle<int>(padH, y, W - padH * 2, cardH) });
            y += cardH + sidebarSectionGap();
        }
        else
        {
            y += metrics::scaled(4);
        }

        placeSection(SectionId::Library);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Starred
                || rows[(size_t) i].kind == RowKind::SavedDir)
                placeRow(i);

        y += sidebarSectionGap() - gap;
        placeSection(SectionId::Search);
        {
            const int qh = searchQuery.idealHeight();
            out.push_back({ LayoutKind::Query, -1,
                            juce::Rectangle<int>(itemInset, y, W - itemInset * 2, qh) });
            y += qh + gap;
        }

        y += sidebarSectionGap() - gap;
        placeSection(SectionId::Saved);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedSearch)
                placeRow(i);
    }
    else
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Open)
                placeRow(i);
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
    return metrics::sidebarExpandedWidth();
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
    notifyHeightChanged();
}

void FavoritesSidebar::setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeIdx)
{
    savedSearches = searches;
    activeSearchIdx = activeIdx;
    rebuildRows();
    layoutChildren();
    notifyHeightChanged();
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
    if (collapsed == shouldCollapse)
    {
        btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
        return;
    }
    collapsed = shouldCollapse;
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
    if (onCollapsedChanged) onCollapsedChanged();
    notifyHeightChanged();
}

void FavoritesSidebar::rebuildRows()
{
    rows.clear();
    rows.push_back({ RowKind::Open });
    rows.push_back({ RowKind::Starred });
    for (int i = 0; i < dirs.size(); ++i)
        rows.push_back({ RowKind::SavedDir, i });
    for (int i = 0; i < (int) savedSearches.size(); ++i)
        rows.push_back({ RowKind::SavedSearch, i });
    rebuildNavItems();
}

void FavoritesSidebar::rebuildNavItems()
{
    navItems.clear();
    if (!collapsed)
    {
        navItems.push_back({ NavKind::OpenFolder, -1 });
        if (folderCardHeight() > 0)
        {
            navItems.push_back({ NavKind::Keep, -1 });
            navItems.push_back({ NavKind::Update, -1 });
            navItems.push_back({ NavKind::Filters, -1 });
        }
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Starred
                || rows[(size_t) i].kind == RowKind::SavedDir)
                navItems.push_back({ NavKind::Row, i });
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedSearch)
                navItems.push_back({ NavKind::Row, i });
    }
    else
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind != RowKind::SavedSearch)
                navItems.push_back({ NavKind::Row, i });
    }
    navItems.push_back({ NavKind::Settings, -1 });
    if (!juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        navIndex = navItems.empty() ? -1 : 0;
}

juce::Rectangle<int> FavoritesSidebar::navItemBounds(const NavItem& item) const
{
    switch (item.kind)
    {
        case NavKind::OpenFolder: return openFolderBounds;
        case NavKind::Keep:       return keepBtnBounds;
        case NavKind::Update:     return updateBtnBounds;
        case NavKind::Filters:    return filtersBtnBounds;
        case NavKind::Settings:   return settingsRowBounds;
        case NavKind::Row:        return rowBounds(item.rowIdx);
    }
    return {};
}

void FavoritesSidebar::activateNavItem(const NavItem& item)
{
    switch (item.kind)
    {
        case NavKind::OpenFolder:
            if (onOpenFolder) onOpenFolder();
            break;
        case NavKind::Keep:
            if (onAddCurrent) onAddCurrent();
            break;
        case NavKind::Update:
            if (!scanning && onRefreshFolder) onRefreshFolder();
            break;
        case NavKind::Filters:
            toggleFilterPanel();
            break;
        case NavKind::Settings:
            if (onOpenSettings) onOpenSettings();
            break;
        case NavKind::Row:
        {
            if (!juce::isPositiveAndBelow(item.rowIdx, (int) rows.size()))
                break;
            const auto& row = rows[(size_t) item.rowIdx];
            switch (row.kind)
            {
                case RowKind::Open:
                    if (onOpenFolder) onOpenFolder();
                    break;
                case RowKind::SavedDir:
                    if (onPickDir) onPickDir(dirs[row.index]);
                    break;
                case RowKind::Starred:
                    if (onShowStarred) onShowStarred();
                    break;
                case RowKind::SavedSearch:
                    if (onPickSavedSearch) onPickSavedSearch(row.index);
                    break;
            }
            break;
        }
    }
    rebuildNavItems();
    repaint();
}

void FavoritesSidebar::moveNav(int delta)
{
    if (navItems.empty()) return;
    if (navIndex < 0) navIndex = 0;
    else navIndex = (navIndex + delta + (int) navItems.size()) % (int) navItems.size();
    repaint();
}

void FavoritesSidebar::updateTooltipForPos(juce::Point<int> pos)
{
    if (keepBtnBounds.contains(pos))
    {
        setTooltip(isCurrentFolderFavorited() ? "Remove from Library" : "Keep in Library");
        return;
    }
    if (updateBtnBounds.contains(pos))
    {
        setTooltip(scanning ? "Updating…" : "Update");
        return;
    }
    if (filtersBtnBounds.contains(pos))
    {
        setTooltip("Filters");
        return;
    }
    if (openFolderBounds.contains(pos))
    {
        setTooltip("Open Folder...");
        return;
    }
    const auto hit = rowHitAt(pos);
    if (hit.valid && hit.removeZone)
    {
        setTooltip("Remove");
        return;
    }
    setTooltip({});
}

void FavoritesSidebar::focusGained(FocusChangeType)
{
    if (navIndex < 0 && !navItems.empty())
        navIndex = 0;
    repaint();
}

void FavoritesSidebar::focusLost(FocusChangeType)
{
    repaint();
}

void FavoritesSidebar::layoutChildren()
{
    openFolderBounds = {};
    folderCardBounds = {};
    keepBtnBounds = {};
    updateBtnBounds = {};
    filtersBtnBounds = {};
    libraryHeaderBounds = {};
    searchHeaderBounds = {};
    savedHeaderBounds = {};

    filterPanel.setVisible(false);
    if (collapsed)
    {
        searchQuery.setVisible(false);
        includeSwitch.setVisible(false);
        return;
    }

    for (const auto& item : collectLayout())
    {
        switch (item.kind)
        {
            case LayoutKind::OpenBtn:
                openFolderBounds = item.bounds;
                break;
            case LayoutKind::FolderCard:
            {
                folderCardBounds = item.bounds;
                auto inner = item.bounds.reduced(metrics::scaled(10), metrics::scaled(10));
                inner.removeFromTop(metrics::scaled(34)); // header
                auto subRow = inner.removeFromTop(metrics::scaled(22));
                const int sw = includeSwitch.idealWidth();
                includeSwitch.setBounds(subRow.removeFromRight(sw).withSizeKeepingCentre(sw, 16));
                includeSwitch.setVisible(true);
                inner.removeFromTop(metrics::scaled(6));
                auto action = inner.removeFromTop(actionRowH());
                const int gap = actionGap();
                const int totalGaps = gap * 2;
                const int unit = juce::jmax(1, (action.getWidth() - totalGaps) / 4);
                keepBtnBounds = action.removeFromLeft(unit);
                action.removeFromLeft(gap);
                updateBtnBounds = action.removeFromLeft(unit);
                action.removeFromLeft(gap);
                filtersBtnBounds = action; // remaining ~½
                if (filterOpen)
                {
                    inner.removeFromTop(metrics::scaled(8));
                    const int fh = filterPanel.idealHeight();
                    filterPanel.setBounds(inner.removeFromTop(fh));
                    filterPanel.setVisible(true);
                }
                break;
            }
            case LayoutKind::Section:
                switch ((SectionId) item.id)
                {
                    case SectionId::Library: libraryHeaderBounds = item.bounds; break;
                    case SectionId::Search:  searchHeaderBounds = item.bounds; break;
                    case SectionId::Saved:   savedHeaderBounds = item.bounds; break;
                }
                break;
            case LayoutKind::Query:
                searchQuery.setBounds(item.bounds);
                searchQuery.setVisible(true);
                break;
            default:
                break;
        }
    }

    if (folderCardBounds.isEmpty())
        includeSwitch.setVisible(false);
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
        return hit;
    }
    return {};
}

FavoritesSidebar::ChromeHit FavoritesSidebar::chromeHitAt(juce::Point<int> pos) const
{
    if (settingsRowBounds.contains(pos)) return ChromeHit::Settings;
    if (collapsed) return ChromeHit::None;
    if (openFolderBounds.contains(pos)) return ChromeHit::OpenFolder;
    if (keepBtnBounds.contains(pos)) return ChromeHit::Keep;
    if (updateBtnBounds.contains(pos)) return ChromeHit::Update;
    if (filtersBtnBounds.contains(pos)) return ChromeHit::Filters;
    return ChromeHit::None;
}

void FavoritesSidebar::setSearchFormOpen(bool open)
{
    if (open && collapsed)
        setCollapsed(false);
    searchOpen = open;
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

void FavoritesSidebar::toggleFilterPanel()
{
    filterOpen = !filterOpen;
    if (filterOpen)
        filterPanel.setHistograms(histogramClips);
    notifyHeightChanged();
}

void FavoritesSidebar::resized()
{
    const int railW = metrics::sidebarRailWidth();
    const int headerH = sidebarHeaderH();
    const int padH = sidebarPadH();
    const int iconBtn = metrics::chromeIconButton();
    const int itemInset = padH + sidebarRowPadX();
    // Same icon column as Settings (left-aligned), vertically centred in the header.
    const int iconColX = itemInset + sidebarRowPadX();
    btnToggle.iconScale = 0.9f;
    if (collapsed)
        btnToggle.setBounds(juce::Rectangle<int>(0, 0, railW, headerH)
                                .withSizeKeepingCentre(iconBtn, iconBtn));
    else
        btnToggle.setBounds(juce::Rectangle<int>(iconColX, 0, sidebarIconCol(), headerH)
                                .withSizeKeepingCentre(iconBtn, iconBtn));

    const int footerH = footerHeight();
    const int contentBottom = contentBottomY();
    const int pad = juce::jmax(footerPad(), metrics::scaled(10));
    const int settingsTop = juce::jmax(contentBottom + pad,
                                       getHeight() - footerH + pad);
    if (collapsed)
    {
        settingsRowBounds = juce::Rectangle<int>(0, settingsTop, getWidth(), sidebarRowH())
                                .withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize());
    }
    else
    {
        settingsRowBounds = juce::Rectangle<int>(itemInset, settingsTop,
                                                 getWidth() - itemInset * 2,
                                                 sidebarRowH());
    }
    {
        if (collapsed)
            btnSettings.setBounds(settingsRowBounds);
        else
        {
            auto iconSlot = juce::Rectangle<int>(iconColX, settingsTop,
                                                 sidebarIconCol(), sidebarRowH());
            btnSettings.setBounds(iconSlot.withSizeKeepingCentre(iconBtn, iconBtn));
        }
        btnSettings.setVisible(true);
    }
    layoutChildren();
    rebuildNavItems();
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);

    const bool overSettings = settingsRowBounds.contains(e.getPosition());
    const auto chrome = chromeHitAt(e.getPosition());
    const auto hit = rowHitAt(e.getPosition());
    const int rowIdx = hit.valid ? [&]
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
                return i;
        return -1;
    }() : -1;
    if (rowIdx != hoverRow || hit.removeZone != hoverRemove
        || overSettings != hoverSettings || chrome != hoverChrome)
    {
        hoverRow = rowIdx;
        hoverRemove = hit.removeZone;
        hoverSettings = overSettings;
        hoverChrome = chrome;
        repaint();
    }
    updateTooltipForPos(e.getPosition());
}

void FavoritesSidebar::mouseExit(const juce::MouseEvent&)
{
    if (!resizing)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    hoverRow = -1;
    hoverRemove = false;
    hoverSettings = false;
    hoverChrome = ChromeHit::None;
    setTooltip({});
    repaint();
}

void FavoritesSidebar::mouseDown(const juce::MouseEvent& e)
{
    if (searchQuery.isVisible() && searchQuery.getBounds().contains(e.getPosition()))
        return;
    if (includeSwitch.isVisible() && includeSwitch.getBounds().contains(e.getPosition()))
        return;
    if (btnSettings.isVisible() && btnSettings.getBounds().contains(e.getPosition()))
        return;
    if (btnToggle.isVisible() && btnToggle.getBounds().contains(e.getPosition()))
        return;

    if (! hasKeyboardFocus(true) && ! searchQuery.isQueryFocused())
        grabKeyboardFocus();

    auto syncNav = [this](NavKind kind, int rowIdx = -1)
    {
        for (int i = 0; i < (int) navItems.size(); ++i)
            if (navItems[(size_t) i].kind == kind
                && (rowIdx < 0 || navItems[(size_t) i].rowIdx == rowIdx))
            {
                navIndex = i;
                break;
            }
        repaint();
    };

    switch (chromeHitAt(e.getPosition()))
    {
        case ChromeHit::Settings:
            syncNav(NavKind::Settings);
            if (onOpenSettings) onOpenSettings();
            return;
        case ChromeHit::OpenFolder:
            syncNav(NavKind::OpenFolder);
            if (onOpenFolder) onOpenFolder();
            return;
        case ChromeHit::Keep:
            syncNav(NavKind::Keep);
            if (onAddCurrent) onAddCurrent();
            return;
        case ChromeHit::Update:
            syncNav(NavKind::Update);
            if (!scanning && onRefreshFolder) onRefreshFolder();
            return;
        case ChromeHit::Filters:
            syncNav(NavKind::Filters);
            toggleFilterPanel();
            return;
        case ChromeHit::None:
            break;
    }

    const auto hit = rowHitAt(e.getPosition());
    if (!hit.valid) return;

    int rowIdx = -1;
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
            rowIdx = i;

    if (hit.removeZone)
    {
        if (hit.kind == RowKind::SavedDir && onRemoveDir)
            onRemoveDir(dirs[hit.index]);
        else if (hit.kind == RowKind::SavedSearch && onRemoveSavedSearch)
            onRemoveSavedSearch(hit.index);
        return;
    }

    syncNav(NavKind::Row, rowIdx);
    if (juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        activateNavItem({ NavKind::Row, rowIdx });
}

bool FavoritesSidebar::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (filterOpen)
        {
            filterOpen = false;
            notifyHeightChanged();
            return true;
        }
        if (searchQuery.isQueryFocused())
        {
            deactivateSearch(false);
            return true;
        }
        return false;
    }
    if (searchQuery.isQueryFocused())
        return false;

    if (navItems.empty())
        rebuildNavItems();

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::leftKey)
    {
        moveNav(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::rightKey)
    {
        moveNav(1);
        return true;
    }
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        if (juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
            activateNavItem(navItems[(size_t) navIndex]);
        return true;
    }
    return false;
}

void FavoritesSidebar::mouseDrag(const juce::MouseEvent&)
{
    // Fixed 212px width — no resize in 3c.
}

void FavoritesSidebar::mouseUp(const juce::MouseEvent&)
{
    resizing = false;
}

void FavoritesSidebar::paintSectionLabel(juce::Graphics& g, const juce::String& title,
                                         juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty()) return;
    const auto p = sl();
    g.setColour(p.text3);
    g.setFont(uiFontFixed(kCapPt, true));
    // Letter-spaced uppercase via drawText (JUCE lacks tracking; use spaced string).
    juce::String spaced;
    for (int i = 0; i < title.length(); ++i)
    {
        spaced += title[i];
        if (i + 1 < title.length()) spaced += " ";
    }
    g.drawText(spaced, bounds, juce::Justification::centredLeft, false);
}

void FavoritesSidebar::paintOpenFolderButton(juce::Graphics& g, juce::Rectangle<int> r,
                                             bool hover, bool focus)
{
    if (r.isEmpty()) return;
    const auto p = sl();
    fillControlSurface(g, r.toFloat(), 7.0f);
    if (hover)
    {
        g.setColour(p.accent.withAlpha(0.08f));
        g.fillRoundedRectangle(r.toFloat(), 7.0f);
    }
    auto area = r.reduced(10, 0);
    auto icon = area.removeFromLeft(16).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
    drawIcon(g, icons::plus, icon, p.accent, 1.8f);
    area.removeFromLeft(6);
    g.setColour(p.text);
    g.setFont(uiFontFixed(12.0f, true));
    // ASCII ellipsis — UI font may not include U+2026.
    g.drawText("Open Folder...", area, juce::Justification::centredLeft, false);
    if (focus)
        drawFocusRing(g, r.toFloat(), 7.0f);
}

void FavoritesSidebar::paintActionButton(juce::Graphics& g, juce::Rectangle<int> r,
                                         bool hover, bool active, bool disabled)
{
    if (r.isEmpty()) return;
    const auto p = sl();
    fillControlSurface(g, r.toFloat(), 6.0f);
    if (active)
    {
        g.setColour(p.accent.withAlpha(0.16f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }
    else if (hover && !disabled)
    {
        g.setColour(p.accent.withAlpha(0.08f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }
    if (disabled)
        g.setColour(p.text3.withAlpha(0.45f));
}

void FavoritesSidebar::paintFolderCard(juce::Graphics& g)
{
    if (folderCardBounds.isEmpty()) return;
    const auto p = sl();
    auto card = folderCardBounds.toFloat();
    // Same control face + border recipe as Open Folder / search field.
    g.setColour(p.control);
    g.fillRoundedRectangle(card, 9.0f);
    g.setColour(p.border);
    g.drawRoundedRectangle(card.reduced(0.5f), 9.0f, 1.0f);
    ds::shadow::control(g, card, 9.0f);

    auto inner = folderCardBounds.reduced(metrics::scaled(10), metrics::scaled(10));
    auto header = inner.removeFromTop(metrics::scaled(34));
    {
        auto iconSlot = header.removeFromLeft(18).toFloat().withSizeKeepingCentre(14.0f, 14.0f);
        drawIcon(g, icons::folder, iconSlot, p.accent, kSidebarIconStroke);
        header.removeFromLeft(6);

        const juce::File dir(active);
        const auto name = dir.getFileName().isNotEmpty() ? dir.getFileName() : active;
        const auto path = dir.getFullPathName();
        auto countArea = header.removeFromRight(36);
        g.setColour(p.text2);
        g.setFont(uiFontFixed(11.0f).withExtraKerningFactor(0.0f));
        // Tabular-ish count via mono if available.
        g.setFont(monoFont(11.0f));
        g.drawText(juce::String(currentClipCount), countArea,
                   juce::Justification::centredRight, false);

        auto nameR = header.removeFromTop(header.getHeight() / 2 + 1);
        auto pathR = header;
        g.setColour(p.text);
        g.setFont(uiFontFixed(12.5f, true));
        g.drawText(name, nameR, juce::Justification::centredLeft, true);
        g.setColour(p.text3);
        g.setFont(uiFontFixed(kPathPt));
        g.drawText(path, pathR, juce::Justification::centredLeft, true);
    }

    auto subRow = inner.removeFromTop(metrics::scaled(22));
    subRow.removeFromRight(includeSwitch.idealWidth() + 2);
    g.setColour(p.text2);
    g.setFont(uiFontFixed(11.0f));
    g.drawText("Include Subfolders", subRow, juce::Justification::centredLeft, true);

    inner.removeFromTop(metrics::scaled(6));

    // Action row icons/labels (bounds set in layoutChildren).
    const bool keepOn = isCurrentFolderFavorited();
    const bool kb = hasKeyboardFocus(true);
    const NavItem* focused = (kb && juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        ? &navItems[(size_t) navIndex] : nullptr;
    auto focusedKind = [&](NavKind k)
    {
        return focused != nullptr && focused->kind == k;
    };

    paintActionButton(g, keepBtnBounds, hoverChrome == ChromeHit::Keep || focusedKind(NavKind::Keep),
                      keepOn, false);
    drawIcon(g, keepOn ? icons::bookmarkFill : icons::bookmark,
             keepBtnBounds.toFloat().withSizeKeepingCentre(13.0f, 13.0f),
             p.accent, kSidebarIconStroke);
    if (focusedKind(NavKind::Keep))
        drawFocusRing(g, keepBtnBounds.toFloat(), 6.0f);

    paintActionButton(g, updateBtnBounds,
                      hoverChrome == ChromeHit::Update || focusedKind(NavKind::Update),
                      false, scanning);
    {
        auto iconR = updateBtnBounds.toFloat().withSizeKeepingCentre(13.0f, 13.0f);
        if (scanning)
        {
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(scanAngle, iconR.getCentreX(),
                                                           iconR.getCentreY()));
            drawIcon(g, icons::undo, iconR, p.accent.withAlpha(0.7f), kSidebarIconStroke);
            g.restoreState();
        }
        else
        {
            drawIcon(g, icons::undo, iconR, p.accent, kSidebarIconStroke);
        }
    }
    if (focusedKind(NavKind::Update))
        drawFocusRing(g, updateBtnBounds.toFloat(), 6.0f);

    paintActionButton(g, filtersBtnBounds,
                      hoverChrome == ChromeHit::Filters || focusedKind(NavKind::Filters),
                      filterOpen || activeFilterGroupCount() > 0, false);
    {
        auto area = filtersBtnBounds.reduced(6, 0);
        auto icon = area.removeFromLeft(14).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
        drawIcon(g, icons::sliders, icon, p.accent, kSidebarIconStroke);
        area.removeFromLeft(4);
        const int badge = activeFilterGroupCount();
        if (badge > 0)
        {
            auto badgeR = area.removeFromRight(16).toFloat()
                              .withSizeKeepingCentre(14.0f, 14.0f);
            g.setColour(p.accent);
            g.fillEllipse(badgeR);
            g.setColour(colours::accentInk());
            g.setFont(uiFontFixed(9.0f, true));
            g.drawText(juce::String(badge), badgeR.toNearestInt(),
                       juce::Justification::centred, false);
            area.removeFromRight(2);
        }
        g.setColour(p.text);
        g.setFont(uiFontFixed(11.0f, true));
        g.drawText("Filters", area, juce::Justification::centredLeft, false);
    }
    if (focusedKind(NavKind::Filters))
        drawFocusRing(g, filtersBtnBounds.toFloat(), 6.0f);

    if (filterOpen && filterPanel.isVisible())
    {
        // Hairline between action row and expanded filter body.
        const int lineY = filterPanel.getY() - metrics::scaled(4);
        g.setColour(p.border.withAlpha(usesDarkAppearance() ? 0.45f : 0.70f));
        g.fillRect(folderCardBounds.getX() + metrics::scaled(10), lineY,
                   folderCardBounds.getWidth() - metrics::scaled(20), 1);
    }
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone, bool keyboardFocus)
{
    if (r.isEmpty() || !juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return;

    const auto& row = rows[(size_t) rowIdx];
    const auto p = sl();
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeStar = row.kind == RowKind::Starred
        && (starredFilter || browseMode == 1);
    const bool activeSearch = row.kind == RowKind::SavedSearch && row.index == activeSearchIdx;
    const bool isActive = activeDir || activeStar || activeSearch;
    const bool lit = isActive || hovered || keyboardFocus;

    const char* icon = icons::folder;
    juce::String label;
    juce::String countText;
    juce::Colour iconCol = p.accent;

    switch (row.kind)
    {
        case RowKind::Open:
            icon = icons::folderOpen;
            label = "Open";
            iconCol = lit ? p.accent : p.text3;
            break;
        case RowKind::SavedDir:
        {
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            const int n = (dirs[row.index] == active) ? currentClipCount
                                                      : countMidiFilesQuick(dir);
            if (n > 0) countText = juce::String(n);
            break;
        }
        case RowKind::Starred:
            icon = icons::star;
            label = "Favorites";
            iconCol = p.gold;
            if (starredCount > 0) countText = juce::String(starredCount);
            break;
        case RowKind::SavedSearch:
        {
            icon = icons::search;
            const auto& entry = savedSearches[(size_t) row.index];
            label = entry.name;
            // Secondary summary: "query · N filters"
            int groups = 0;
            if (entry.search.keyMask != 0 || entry.search.keyRoot >= 0) ++groups;
            if (entry.search.bpmMin > 0.0 || entry.search.bpmMax > 0.0) ++groups;
            if (entry.search.barsMin > 0 || entry.search.barsMax > 0) ++groups;
            if (entry.search.complexityMin > 0 || entry.search.complexityMax > 0) ++groups;
            if (groups > 0)
                countText = juce::String(groups) + (groups == 1 ? " filter" : " filters");
            iconCol = lit ? p.accent : p.text3;
            break;
        }
    }

    if (collapsed)
    {
        if (hovered || keyboardFocus)
        {
            g.setColour(p.selectWash);
            g.fillRoundedRectangle(r.toFloat(), 6.0f);
        }
        drawIcon(g, icon, r.toFloat().withSizeKeepingCentre(16.0f, 16.0f),
                 iconCol, kSidebarIconStroke);
        if (keyboardFocus)
            drawFocusRing(g, r.toFloat(), 6.0f);
        return;
    }

    if (isActive || hovered)
    {
        g.setColour(isActive ? p.selectWash : p.selectWash.withAlpha(0.5f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }

    auto textArea = r.reduced(sidebarRowPadX(), 0);
    auto iconSlot = textArea.removeFromLeft(sidebarIconCol()).toFloat();
    drawIcon(g, icon, iconSlot.withSizeKeepingCentre(14.0f, 14.0f), iconCol, kSidebarIconStroke);
    textArea.removeFromLeft(sidebarRowIconGap());

    if ((hovered || keyboardFocus)
        && (row.kind == RowKind::SavedDir || row.kind == RowKind::SavedSearch))
    {
        auto xArea = textArea.removeFromRight(18).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, icons::x, xArea,
                 removeZone ? p.text : p.text3, kSidebarIconStroke);
    }
    else if (countText.isNotEmpty())
    {
        auto countArea = textArea.removeFromRight(28);
        g.setColour(p.text3);
        g.setFont(monoFont(11.0f));
        g.drawText(countText, countArea, juce::Justification::centredRight, false);
    }

    g.setColour(lit ? p.text : p.text2);
    g.setFont(uiFontFixed(kCtrlPt, isActive || keyboardFocus));
    g.drawText(label, textArea, juce::Justification::centredLeft, true);

    if (keyboardFocus)
        drawFocusRing(g, r.toFloat(), 6.0f);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    const auto p = sl();
    auto b = getLocalBounds().toFloat();
    g.setColour(p.tint);
    g.fillRect(b);
    g.setColour(p.border);
    g.fillRect(getLocalBounds().removeFromRight(1));

    const int padH = sidebarPadH();
    const int headerH = sidebarHeaderH();

    if (!collapsed)
    {
        g.setColour(p.text2);
        g.setFont(uiFontFixed(kCapPt, true));
        const int titleX = padH + sidebarRowPadX() + sidebarIconCol() + sidebarRowIconGap();
        g.drawText("Library", titleX, 0, 120, headerH,
                   juce::Justification::centredLeft, false);
    }

    const bool kb = hasKeyboardFocus(true);
    const NavItem* focused = (kb && juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        ? &navItems[(size_t) navIndex] : nullptr;

    if (!collapsed)
    {
        paintOpenFolderButton(g, openFolderBounds,
                              hoverChrome == ChromeHit::OpenFolder
                                  || (focused && focused->kind == NavKind::OpenFolder),
                              focused && focused->kind == NavKind::OpenFolder);
        paintFolderCard(g);
        paintSectionLabel(g, "LIBRARY", libraryHeaderBounds);
        paintSectionLabel(g, "SEARCH", searchHeaderBounds);
        paintSectionLabel(g, "SAVED SEARCHES", savedHeaderBounds);
    }

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        // Expanded: Open is the top button, not a list row.
        if (!collapsed && rows[(size_t) i].kind == RowKind::Open)
            continue;
        const auto r = rowBounds(i);
        const bool rowFocus = focused != nullptr && focused->kind == NavKind::Row
            && focused->rowIdx == i;
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove, rowFocus);
    }

    if (!settingsRowBounds.isEmpty() && !collapsed)
    {
        const bool lit = hoverSettings
            || (focused != nullptr && focused->kind == NavKind::Settings);
        auto area = settingsRowBounds;
        area.removeFromLeft(sidebarRowPadX() + sidebarIconCol() + sidebarRowIconGap());
        g.setColour(lit ? p.text : p.text2);
        g.setFont(uiFontFixed(kCtrlPt, lit));
        g.drawText("Settings", area, juce::Justification::centredLeft, true);
        if (focused != nullptr && focused->kind == NavKind::Settings)
            drawFocusRing(g, settingsRowBounds.toFloat(), 6.0f);
    }
    else if (!settingsRowBounds.isEmpty() && collapsed
             && focused != nullptr && focused->kind == NavKind::Settings)
    {
        drawFocusRing(g, settingsRowBounds.toFloat(), 6.0f);
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
    fitNameColumnToContents();
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
    int w = metrics::scaled(8) * 2 + metrics::scaled(nameColumnW) + metrics::scaled(14);
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

void FileListPanel::setNameColumnWidth(int logicalW)
{
    const int next = juce::jlimit(kNameWMin, kNameWMax, logicalW);
    if (next == nameColumnW)
        return;
    nameColumnW = next;
    updateContentSize();
    content.repaint();
    repaint();
}

void FileListPanel::fitNameColumnToContents()
{
    const float scale = juce::jmax(0.25f, contentScale());
    const auto font = ds::font(ds::Type::Body);
    float maxText = juce::GlyphArrangement::getStringWidth(font, "Name");
    for (const auto& e : entries)
        if (e.name.isNotEmpty())
            maxText = juce::jmax(maxText,
                                 juce::GlyphArrangement::getStringWidth(font, e.name));

    // Star/folder icon (18) + gap (6) + trailing pad so glyphs aren't clipped.
    const int pixelW = 18 + 6 + (int) std::ceil(maxText) + 16;
    const int logical = juce::jlimit(kNameWMin, kNameWMax,
                                     juce::roundToInt((float) pixelW / scale));
    if (logical == nameColumnW)
        return;

    nameColumnW = logical;
    updateContentSize();
    content.repaint();
    repaint();
    if (onNameColumnWidthChanged)
        onNameColumnWidthChanged(nameColumnW);
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

int FileListPanel::nameResizeHandleX() const
{
    return headerColumnBounds(SortColumn::Name).getRight();
}

int FileListPanel::nameColumnRightContentX() const
{
    const auto cols = splitRowColumns({ 0, 0, juce::jmax(getWidth(), totalContentWidth()), 1 });
    return cols.name.getRight();
}

bool FileListPanel::hitNameResizeHandle(juce::Point<int> pos) const
{
    if (pos.y < 0 || pos.y >= getHeight())
        return false;
    return std::abs(pos.x - nameResizeHandleX()) <= kResizeHitSlop;
}

bool FileListPanel::hitNameResizeHandleContent(juce::Point<int> pos) const
{
    return std::abs(pos.x - nameColumnRightContentX()) <= kResizeHitSlop;
}

void FileListPanel::beginNameColumnResize(int screenX)
{
    resizingNameColumn = true;
    nameResizeStartScreenX = screenX;
    nameResizeStartW = nameColumnW;
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    content.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    repaint();
}

void FileListPanel::dragNameColumnResize(int screenX)
{
    if (!resizingNameColumn)
        return;
    const float scale = juce::jmax(0.25f, contentScale());
    const int deltaLogical = juce::roundToInt((float) (screenX - nameResizeStartScreenX) / scale);
    setNameColumnWidth(nameResizeStartW + deltaLogical);
}

void FileListPanel::endNameColumnResize()
{
    if (!resizingNameColumn)
        return;
    resizingNameColumn = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    content.setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
    if (onNameColumnWidthChanged)
        onNameColumnWidthChanged(nameColumnW);
}

void FileListPanel::paintNameColumnDivider(juce::Graphics& g, int x, int y, int h) const
{
    g.setColour(resizingNameColumn ? ds::acc().withAlpha(0.65f) : ds::ctlb());
    g.fillRect(x, y, 1, h);
    // Wider invisible-feeling rail so the edge reads as a splitter.
    if (resizingNameColumn)
    {
        g.setColour(ds::acc().withAlpha(0.12f));
        g.fillRect(x - 2, y, 5, h);
    }
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
    cols.name = row.removeFromLeft(metrics::scaled(nameColumnW));
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
    // Match search-field / control border (--ctlb), not the darker hairline.
    g.setColour(ds::ctlb());
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

    if (hitNameResizeHandle(e.getPosition()))
    {
        beginNameColumnResize(e.getScreenX());
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

void FileListPanel::mouseDrag(const juce::MouseEvent& e)
{
    dragNameColumnResize(e.getScreenX());
}

void FileListPanel::mouseUp(const juce::MouseEvent&)
{
    endNameColumnResize();
}

void FileListPanel::mouseMove(const juce::MouseEvent& e)
{
    if (resizingNameColumn)
        return;
    setMouseCursor(hitNameResizeHandle(e.getPosition())
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

void FileListPanel::mouseExit(const juce::MouseEvent&)
{
    if (!resizingNameColumn)
        setMouseCursor(juce::MouseCursor::NormalCursor);
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
        g.setColour(ds::colour(ds::Type::TableHeader));
        g.setFont(ds::font(ds::Type::TableHeader));
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
            g.setColour(ds::acc());
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

    // Name-column resize rail (content coordinates; transform already applied).
    const int divX = splitRowColumns({ 0, 0, juce::jmax(getWidth(), totalContentWidth()),
                                       metrics::listHeaderH() }).name.getRight();
    paintNameColumnDivider(g, divX, 0, metrics::listHeaderH());
    g.restoreState();
}

void FileListPanel::paintRow(juce::Graphics& g, int displayIdx, juce::Rectangle<int> r,
                             bool hovered, bool hoverStar)
{
    const int entryIdx = displayToEntry(displayIdx);
    if (entryIdx < 0) return;
    const auto& e = entries[(size_t) entryIdx];
    const bool isSelected = entryIdx == selected;
    auto rowR = r.toFloat().reduced(2.0f, 1.0f);

    if (isSelected)
    {
        g.setColour(ds::acc());
        g.fillRoundedRectangle(rowR, 5.0f);
    }
    else if (hovered)
    {
        g.setColour(ds::hoverWash());
        g.fillRoundedRectangle(rowR, 5.0f);
    }
    else if ((displayIdx % 2) == 1)
    {
        g.setColour(ds::rowalt());
        g.fillRoundedRectangle(rowR, 5.0f);
    }

    // Selection text: on-accent ink (dark in dark mode so blue highlight stays readable).
    const auto ink = colours::accentInk();
    const auto textCol = isSelected ? ink : ds::tx();
    const auto metaCol = isSelected ? ink.withAlpha(0.80f) : ds::tx2();

    auto cols = splitRowColumns(r);
    auto row = cols.name;

    const float iconS = 13.0f;
    auto iconArea = row.removeFromLeft(18).toFloat().withSizeKeepingCentre(iconS, iconS);
    if (e.isDirectory)
    {
        drawIcon(g, icons::folder, iconArea,
                 isSelected ? ink : ds::acc(), 1.35f);
    }
    else
    {
        // Outline star (--trk tint) → --stron when favorited; on-accent when selected.
        const auto starCol = e.starred
            ? (isSelected ? ink : ds::stron())
            : (hoverStar ? textCol : (isSelected ? ink.withAlpha(0.55f)
                                                 : ds::trk()));
        drawIcon(g, e.starred ? icons::star : icons::starOutline, iconArea, starCol, 1.35f);
    }
    row.removeFromLeft(6);

    if (e.isDirectory)
    {
        g.setColour(textCol);
        g.setFont(ds::font(ds::Type::Body));
        g.drawText(e.name, row, juce::Justification::centredLeft, true);
        return;
    }

    g.setFont(ds::font(ds::Type::Metadata));
    g.setColour(metaCol);
    if (columnsVisible.key && e.rootName.isNotEmpty())
        g.drawText(e.rootName, cols.key, juce::Justification::centredLeft);
    if (columnsVisible.tempo && e.bpm > 0.0)
        g.drawText(juce::String((int) std::lround(e.bpm)), cols.tempo, juce::Justification::centredLeft);
    if (columnsVisible.bars && e.bars > 0)
        g.drawText(juce::String(e.bars), cols.bars, juce::Justification::centredLeft);
    if (columnsVisible.kind)
    {
        // §2 kind chip: 7×7 r2 at 75% opacity + 11px label.
        juce::Colour chip = ds::kindKeys();
        switch (e.kind)
        {
            case ClipKind::Drums:  chip = ds::kindDrums(); break;
            case ClipKind::Bass:   chip = ds::kindBass(); break;
            case ClipKind::Piano:  chip = ds::kindKeys(); break;
            case ClipKind::Lead:   chip = ds::kindKeys(); break;
            case ClipKind::Single: chip = ds::kindPerc(); break;
        }
        auto kindR = cols.kind;
        auto dot = kindR.removeFromLeft(10).toFloat().withSizeKeepingCentre(7.0f, 7.0f);
        g.setColour(isSelected ? ink.withAlpha(0.75f) : chip.withAlpha(0.75f));
        g.fillRoundedRectangle(dot, 2.0f);
        g.setColour(metaCol);
        g.setFont(ds::font(ds::Type::Metadata));
        g.drawText(clipKindName(e.kind), kindR, juce::Justification::centredLeft, true);
    }
    if (columnsVisible.complexity && e.complexity > 0)
        g.drawText(juce::String(e.complexity), cols.complexity, juce::Justification::centredLeft);
    if (columnsVisible.difNotes && e.difNotes > 0)
        g.drawText(juce::String(e.difNotes), cols.difNotes, juce::Justification::centredLeft);
    if (columnsVisible.timeSig)
        g.drawText(juce::String(e.timeSigNum) + "/" + juce::String(e.timeSigDen),
                   cols.timeSig, juce::Justification::centredLeft);
    if (columnsVisible.notes && e.noteCount > 0)
        g.drawText(juce::String(e.noteCount), cols.notes, juce::Justification::centredLeft);

    g.setColour(textCol);
    g.setFont(ds::font(ds::Type::Body));
    g.drawText(e.name, row, juce::Justification::centredLeft, true);
}

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    if (owner.entries.empty())
    {
        g.setColour(colours::text2());
        g.setFont(uiFont(13.0f, false));
        g.drawText("No files", getLocalBounds().reduced(24, 32),
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

    owner.paintNameColumnDivider(g, owner.nameColumnRightContentX(), 0, getHeight());
}

void FileListPanel::ListContent::mouseMove(const juce::MouseEvent& e)
{
    const auto local = getLocalPoint(nullptr, e.getScreenPosition());
    if (owner.hitNameResizeHandleContent(local))
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        if (hoverRow >= 0 || hoverStar)
        {
            hoverRow = -1;
            hoverStar = false;
            repaint();
        }
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);

    const int rowH = metrics::listRowH();
    // Screen→local avoids AffineTransform/content-scale drift in nested Viewport coords.
    const int y = local.y;
    const int disp = rowH > 0 ? y / rowH : -1;
    const bool valid = juce::isPositiveAndBelow(disp, (int) owner.sortOrder.size());
    const int entryIdx = valid ? owner.displayToEntry(disp) : -1;
    const bool isDir = entryIdx >= 0 && owner.entries[(size_t) entryIdx].isDirectory;
    // Star sits in the leading icon slot of the name column.
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();
    const int x = local.x;
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
    if (!owner.resizingNameColumn)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void FileListPanel::ListContent::mouseDown(const juce::MouseEvent& e)
{
    owner.grabBrowseFocus();
    if (owner.onActivated)
        owner.onActivated();

    const auto local = getLocalPoint(nullptr, e.getScreenPosition());
    if (owner.hitNameResizeHandleContent(local))
    {
        owner.beginNameColumnResize(e.getScreenX());
        return;
    }

    if (owner.entries.empty())
    {
        if (owner.onEmptyOpenFolder)
            owner.onEmptyOpenFolder();
        return;
    }

    // Column header clicks are handled by the parent panel.
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
    if (owner.resizingNameColumn)
    {
        owner.endNameColumnResize();
        return;
    }
    clearDragState();
}

void FileListPanel::ListContent::mouseDrag(const juce::MouseEvent& e)
{
    if (owner.resizingNameColumn)
    {
        owner.dragNameColumnResize(e.getScreenX());
        return;
    }

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
