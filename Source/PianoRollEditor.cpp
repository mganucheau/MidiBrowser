#include "PianoRollEditor.h"
#include <algorithm>

namespace pflow {

// ── PitchRowMap (mini roll only) ─────────────────────────────────────────────

void PitchRowMap::fit(const std::vector<RollNote>& notes, float height, int minRange)
{
    int lo = 127, hi = 0;
    for (const auto& n : notes)
    {
        lo = juce::jmin(lo, n.pitch);
        hi = juce::jmax(hi, n.pitch);
    }
    if (notes.empty()) { lo = 57; hi = 74; }

    lo -= 1; hi += 1;
    while (hi - lo + 1 < minRange)
    {
        if (lo > 0) --lo;
        if (hi - lo + 1 < minRange && hi < 127) ++hi;
        if (lo == 0 && hi == 127) break;
    }
    minPitch = juce::jlimit(0, 127, lo);
    maxPitch = juce::jlimit(minPitch, 127, hi);
    rowH = height / (float) numRows();
}

double effectiveVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(juce::jlimit(0.04, 1.0, n.fileVelocity / 127.0), n, k);
}

// ── PianoRollMini ────────────────────────────────────────────────────────────

void PianoRollMini::setNotes(std::vector<RollNote> resolvedGrooved, int numBars,
                             const GrooveParams& groove)
{
    notes = std::move(resolvedGrooved);
    bars = juce::jmax(1, numBars);
    knobs = groove;
    repaint();
}

void PianoRollMini::setPlayheadStep(double step, bool isPlaying)
{
    playheadStep = step;
    playing = isPlaying;
    repaint();
}

void PianoRollMini::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::rollBg());
    g.fillRoundedRectangle(b, 6.0f);

    if (notes.empty())
    {
        g.setColour(colours::text3());
        g.setFont(uiFont(13.0f, false));
        g.drawText("No notes", getLocalBounds(), juce::Justification::centred);
        return;
    }

    PitchRowMap map;
    map.fit(notes, b.getHeight() - 8.0f, 14);
    const float pps = (b.getWidth() - 8.0f) / (float) (bars * kStepsPerBar);
    auto inner = b.reduced(4.0f);

    g.setColour(colours::rollRowline());
    for (int bar = 1; bar < bars; ++bar)
        g.fillRect(inner.getX() + (float) (bar * kStepsPerBar) * pps, inner.getY(),
                   1.0f, inner.getHeight());

    for (const auto& n : notes)
    {
        const double v = effectiveVelocity(n, knobs);
        auto r = juce::Rectangle<float>(
            inner.getX() + (float) n.start * pps,
            inner.getY() + map.yForPitchTop(n.pitch, inner.getHeight()),
            juce::jmax(2.0f, (float) n.len * pps - 0.5f),
            juce::jmax(2.0f, map.rowH - 0.8f));
        g.setColour((n.moved ? colours::accentBright() : colours::accent())
                        .withAlpha((float) (0.4 + 0.55 * v)));
        g.fillRoundedRectangle(r, 1.5f);
    }

    if (playing)
    {
        g.setColour(colours::playhead());
        g.fillRect(inner.getX() + (float) playheadStep * pps, inner.getY(), 1.5f, inner.getHeight());
    }
}

// ── PianoRollEditor ──────────────────────────────────────────────────────────

PianoRollEditor::PianoRollEditor()
{
    setWantsKeyboardFocus(true);

    // Instrument strip
    octaveStepper.minValue = -3;
    octaveStepper.maxValue = 3;
    octaveStepper.onChange = [this](int v) { applyEdit([v](ClipEdit& e) { e.octave = v; }); };
    addAndMakeVisible(octaveStepper);

    for (int i = 0; i < 12; ++i)
        rootPicker.addItem(kNoteNames[(size_t) i], i + 1);
    rootPicker.setTextWhenNothingSelected("-");
    rootPicker.onChange = [this]
    {
        const int id = rootPicker.getSelectedId();
        if (id > 0)
            applyEdit([id](ClipEdit& e) { e.root = id - 1; });
    };
    addAndMakeVisible(rootPicker);

    // Major / Minor first for discoverability; IDs still map to Mode enum values.
    static constexpr Mode kModeOrder[] = {
        Mode::Ionian, Mode::Aeolian, Mode::Dorian, Mode::Phrygian,
        Mode::Lydian, Mode::Mixolydian, Mode::Locrian
    };
    for (auto m : kModeOrder)
        modePicker.addItem(modeName(m), (int) m + 1);
    modePicker.onChange = [this]
    {
        const int id = modePicker.getSelectedId();
        if (id > 0)
            applyEdit([id](ClipEdit& e) { e.mode = (Mode) (id - 1); });
    };
    addAndMakeVisible(modePicker);

    fitSwitch.onText = "snapping";
    fitSwitch.onClick = [this]
    {
        const bool on = fitSwitch.getToggleState();
        applyEdit([on](ClipEdit& e)
        {
            e.fitScale = on;
            if (on && e.root < 0) e.root = 0;   // default to C
        });
    };
    addAndMakeVisible(fitSwitch);

    mapSwitch.onClick = [this]
    {
        const bool on = mapSwitch.getToggleState();
        applyEdit([on](ClipEdit& e)
        {
            e.mapToRoot = on;
            if (on && e.root < 0) e.root = 0;   // default to C
        });
    };
    addAndMakeVisible(mapSwitch);

    btnLock.setComponentID("btnLock");
    btnLock.onClick = [this]
    {
        if (onLockToggled)
            onLockToggled(!lockActive);
    };
    addAndMakeVisible(btnLock);

    // Toolbar
    btnRevert.onClick = [this]
    {
        selection.clear();
        applyEdit([](ClipEdit& e) { e = ClipEdit(); });
    };
    addAndMakeVisible(btnRevert);

    btnTrim.setComponentID("btnTrim");
    btnTrim.onClick = [this] { toggleTrim(); };
    addAndMakeVisible(btnTrim);

    btnFold.setComponentID("btnFold");
    btnFold.onClick = [this]
    {
        folded = !folded;
        btnFold.active = folded;
        btnFold.repaint();
        updateRollSize();
        scrollToContent();
        rollContent.repaint();
        gutter.repaint();
    };
    addAndMakeVisible(btnFold);

    divisionPicker.addItem("1 bar", 16);
    divisionPicker.addItem("1/2", 8);
    divisionPicker.addItem("1/4", 4);
    divisionPicker.addItem("1/8", 2);
    divisionPicker.addItem("1/16", 1);
    divisionPicker.setSelectedId(divisionSteps, juce::dontSendNotification);
    divisionPicker.onChange = [this]
    {
        if (divisionPicker.getSelectedId() > 0)
        {
            divisionSteps = divisionPicker.getSelectedId();
            rollContent.repaint();
        }
    };
    addAndMakeVisible(divisionPicker);

    btnZoomOut.onClick = [this]
    {
        zoomXAround(1.0f / 1.3f, (float) rollViewport.getViewPositionX()
                                     + (float) rollViewport.getMaximumVisibleWidth() * 0.5f);
    };
    btnZoomIn.onClick = [this]
    {
        zoomXAround(1.3f, (float) rollViewport.getViewPositionX()
                              + (float) rollViewport.getMaximumVisibleWidth() * 0.5f);
    };
    addAndMakeVisible(btnZoomOut);
    addAndMakeVisible(btnZoomIn);

    selBadge.trailingIcon = icons::x;
    selBadge.accentText = true;
    selBadge.onClick = [this]
    {
        selection.clear();
        refreshControls();
        repaint();
    };
    addChildComponent(selBadge);

    // Roll
    addAndMakeVisible(gutter);
    rollViewport.setViewedComponent(&rollContent, false);
    rollViewport.setScrollBarsShown(true, true, true, true);
    rollViewport.setScrollBarThickness(8);
    rollViewport.onScrolled = [this]
    {
        gutter.repaint();
        velocityLane.repaint();
    };
    addAndMakeVisible(rollViewport);

    addAndMakeVisible(velocityLane);

    knobsPanel.onParamsChanged = [this](const GrooveParams& p)
    {
        groove = p;
        rebuildResolved();
        rollContent.repaint();
        velocityLane.repaint();
        if (onGrooveChanged)
            onGrooveChanged(groove);
    };
    knobsPanel.onOpenChanged = [this] { resized(); };
    addAndMakeVisible(knobsPanel);
}

PianoRollEditor::~PianoRollEditor() = default;

// ── model → view ─────────────────────────────────────────────────────────────

void PianoRollEditor::setClip(const StepClip& c, const ClipEdit& e, const GrooveParams& k)
{
    const bool sameClip = hasClip && clip.filePath == c.filePath && clip.name == c.name;
    hasClip = true;
    clip = c;
    edit = e;
    groove = k;
    knobsPanel.setParams(groove, juce::dontSendNotification);
    rebuildResolved();
    refreshControls();
    if (!sameClip)
    {
        selection.clear();
        computePxPerStepBase();
        updateRollSize();
        scrollToContent();
    }
    else
    {
        updateRollSize();
    }
    repaint();
}

void PianoRollEditor::clearClip()
{
    hasClip = false;
    selection.clear();
    resolved = {};
    grooved.clear();
    foldPitches.clear();
    refreshControls();
    repaint();
}

void PianoRollEditor::setPlayheadStep(double step, bool isPlaying)
{
    playheadStep = step;
    playing = isPlaying;
    rollContent.repaint();
}

void PianoRollEditor::setLockActive(bool locked)
{
    lockActive = locked;
    btnLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnLock.active = locked;
    btnLock.setTooltip(locked ? "Pitch edits locked: applied to every clip while browsing"
                              : "Lock pitch edits while browsing");
    btnLock.repaint();
}

void PianoRollEditor::rebuildResolved()
{
    if (!hasClip) return;
    resolved = resolveClip(clip, edit);
    grooved = applyGroove(resolved.notes, groove);

    ClipEdit noTrim = edit;
    noTrim.trimLead = noTrim.trimTail = 0;
    const auto preTrim = resolveClip(clip, noTrim);
    edges = emptyEdgeBars(preTrim.notes, clip.bars);

    std::set<int> pitches;
    for (const auto& n : resolved.notes)
        pitches.insert(juce::jlimit(0, 127, n.pitch));
    foldPitches.assign(pitches.begin(), pitches.end());
}

void PianoRollEditor::applyEdit(std::function<void(ClipEdit&)> mutate)
{
    if (!hasClip) return;
    mutate(edit);
    rebuildResolved();
    refreshControls();
    updateRollSize();
    repaint();
    if (onEditChanged)
        onEditChanged(edit);
}

void PianoRollEditor::refreshControls()
{
    octaveStepper.setValue(edit.octave, juce::dontSendNotification);
    rootPicker.setSelectedId(edit.root >= 0 ? edit.root + 1 : 0, juce::dontSendNotification);
    modePicker.setSelectedId((int) edit.mode + 1, juce::dontSendNotification);
    fitSwitch.setToggleState(edit.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(edit.mapToRoot, juce::dontSendNotification);
    mapSwitch.onText = (edit.mapToRoot && edit.root >= 0 && clip.root >= 0)
        ? juce::String(kNoteNames[(size_t) clip.root]) + juce::String::fromUTF8(" → ")
              + kNoteNames[(size_t) edit.root]
        : juce::String("on");
    mapSwitch.repaint();
    fitSwitch.repaint();

    // Trim: enabled when there is something to trim or restore.
    const bool isTrimmed = edit.trimLead + edit.trimTail > 0;
    const bool canTrim = edges.lead + edges.tail > edit.trimLead + edit.trimTail;
    btnTrim.setEnabled(canTrim || isTrimmed);
    btnTrim.active = isTrimmed;
    if (isTrimmed)
    {
        btnTrim.label = "Restore";
        btnTrim.setTooltip("Restore trimmed bars");
    }
    else if (canTrim)
    {
        btnTrim.label = "Trim";
        btnTrim.setTooltip("Trim empty edge bars");
    }
    else
    {
        btnTrim.label = "No empty bars";
        btnTrim.setTooltip("No empty edge bars to trim");
    }
    btnTrim.repaint();

    btnFold.setEnabled(hasClip && !foldPitches.empty());
    btnFold.active = folded;
    btnFold.repaint();

    // Selection badge
    selBadge.label = juce::String((int) selection.size()) + " sel";
    selBadge.setVisible(!selection.empty());
    selBadge.repaint();

    // Badges
    badgeChips.clear();
    for (const auto& b : editBadges(clip, edit))
    {
        auto chip = std::make_unique<ChipBtn>(b.label);
        chip->trailingIcon = icons::x;
        chip->accentText = true;
        const juce::String key = b.key;
        chip->onClick = [this, key] { removeBadge(key); };
        addAndMakeVisible(*chip);
        badgeChips.push_back(std::move(chip));
    }
    resized();
}

void PianoRollEditor::removeBadge(const juce::String& key)
{
    applyEdit([&key](ClipEdit& e)
    {
        if (key == "oct")        e.octave = 0;
        else if (key == "scale") e.fitScale = false;
        else if (key == "map")   e.mapToRoot = false;
        else if (key == "moves") e.moves.clear();
        else if (key == "trim")  { e.trimLead = 0; e.trimTail = 0; }
        else if (key == "vel")   e.velocities.clear();
        else if (key == "del")   e.deleted.clear();
    });
}

void PianoRollEditor::deleteSelectedNotes()
{
    if (!hasClip || selection.empty())
        return;
    const auto ids = selection;
    selection.clear();
    applyEdit([&ids](ClipEdit& e)
    {
        for (int id : ids)
            e.deleted.insert(id);
    });
}

bool PianoRollEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selection.empty())
        {
            deleteSelectedNotes();
            return true;
        }
    }
    return false;
}

void PianoRollEditor::toggleTrim()
{
    const bool isTrimmed = edit.trimLead + edit.trimTail > 0;
    // Prefer live edge detection; if somehow zero while trimmed, restore.
    auto lead = edges.lead;
    auto tail = edges.tail;
    if (!isTrimmed && lead + tail == 0)
    {
        // Recompute from the current clip in case edges were stale.
        ClipEdit noTrim = edit;
        noTrim.trimLead = noTrim.trimTail = 0;
        const auto preTrim = resolveClip(clip, noTrim);
        const auto fresh = emptyEdgeBars(preTrim.notes, clip.bars);
        lead = fresh.lead;
        tail = fresh.tail;
    }
    if (!isTrimmed && lead + tail == 0)
        return;   // nothing to trim
    applyEdit([isTrimmed, lead, tail](ClipEdit& e)
    {
        e.trimLead = isTrimmed ? 0 : lead;
        e.trimTail = isTrimmed ? 0 : tail;
    });
}

void PianoRollEditor::selectPitch(int pitch, bool additive)
{
    if (!additive)
        selection.clear();
    for (const auto& n : resolved.notes)
        if (n.pitch == pitch)
            selection.insert(n.id);
    refreshControls();
    rollContent.repaint();
    gutter.repaint();
    velocityLane.repaint();
}

// ── row geometry ─────────────────────────────────────────────────────────────

int PianoRollEditor::rowForPitch(int pitch) const
{
    if (!folded)
        return 127 - juce::jlimit(0, 127, pitch);
    if (foldPitches.empty())
        return 0;
    // Nearest note-bearing row (exact for resolved pitches).
    auto it = std::lower_bound(foldPitches.begin(), foldPitches.end(), pitch);
    if (it == foldPitches.end()) --it;
    const int idx = (int) std::distance(foldPitches.begin(), it);
    return (int) foldPitches.size() - 1 - idx;
}

int PianoRollEditor::pitchForRow(int row) const
{
    if (!folded)
        return 127 - juce::jlimit(0, 127, row);
    if (foldPitches.empty())
        return 60;
    const int idx = juce::jlimit(0, (int) foldPitches.size() - 1,
                                 (int) foldPitches.size() - 1 - row);
    return foldPitches[(size_t) idx];
}

juce::Rectangle<float> PianoRollEditor::noteRect(const RollNote& n, int rowOffset,
                                                 double stepOffset) const
{
    const float pps = pxPerStep();
    const int row = juce::jlimit(0, numRows() - 1, rowForPitch(n.pitch) + rowOffset);
    return { (float) (n.start + stepOffset) * pps,
             (float) row * effRowH(),
             juce::jmax(3.0f, (float) n.len * pps - 1.0f),
             juce::jmax(3.0f, effRowH() - 1.0f) };
}

const RollNote* PianoRollEditor::noteAt(juce::Point<float> pos) const
{
    for (auto it = grooved.rbegin(); it != grooved.rend(); ++it)
        if (noteRect(*it).contains(pos))
            return &(*it);
    return nullptr;
}

void PianoRollEditor::computePxPerStepBase()
{
    const int visW = juce::jmax(60, rollViewport.getMaximumVisibleWidth());
    pxPerStepBase = juce::jmax(0.75f, (float) visW / (float) juce::jmax(1, totalSteps()));
}

void PianoRollEditor::updateRollSize()
{
    const int visW = juce::jmax(1, rollViewport.getMaximumVisibleWidth());
    const int visH = juce::jmax(1, rollViewport.getMaximumVisibleHeight());
    const int w = juce::jmax((int) std::ceil((float) totalSteps() * pxPerStep()), visW);
    const int h = juce::jmax((int) std::ceil((float) numRows() * effRowH()), visH);
    rollContent.setSize(w, h);
    gutter.repaint();
    velocityLane.repaint();
}

void PianoRollEditor::scrollToContent()
{
    if (resolved.notes.empty())
    {
        rollViewport.setViewPosition(0, juce::jmax(0, (int) (rowForPitch(66) * effRowH())
                                                          - rollViewport.getMaximumVisibleHeight() / 2));
        return;
    }
    long sum = 0;
    for (const auto& n : resolved.notes)
        sum += n.pitch;
    const int meanPitch = (int) (sum / (long) resolved.notes.size());
    const int y = (int) ((float) rowForPitch(meanPitch) * effRowH())
                  - rollViewport.getMaximumVisibleHeight() / 2;
    rollViewport.setViewPosition(0, juce::jmax(0, y));
}

void PianoRollEditor::zoomXAround(float factor, float contentX)
{
    const float pps = pxPerStep();
    const float anchorStep = contentX / pps;
    const float cursorInView = contentX - (float) rollViewport.getViewPositionX();

    zoomX = juce::jlimit(1.0f, kMaxZoomX, zoomX * factor);
    updateRollSize();

    rollViewport.setViewPosition(
        (int) std::round(anchorStep * pxPerStep() - cursorInView),
        rollViewport.getViewPositionY());
    rollContent.repaint();
}

void PianoRollEditor::zoomRowsAround(float factor, float contentY)
{
    const float anchorRow = contentY / effRowH();
    const float cursorInView = contentY - (float) rollViewport.getViewPositionY();

    rowH = juce::jlimit(kMinRowH, kMaxRowH, rowH * factor);
    updateRollSize();

    rollViewport.setViewPosition(
        rollViewport.getViewPositionX(),
        (int) std::round(anchorRow * effRowH() - cursorInView));
    rollContent.repaint();
}

void PianoRollEditor::commitNoteDrag()
{
    auto& rc = rollContent;
    if (rc.dragDRows == 0 && rc.dragDStep == 0)
        return;

    std::vector<int> ids;
    if (selection.count(rc.dragNoteId) > 0)
        ids.assign(selection.begin(), selection.end());
    else
        ids.push_back(rc.dragNoteId);

    // Per-note pitch delta: rows map 1:1 to semitones unfolded, and to the
    // note-bearing rows when folded (Live-style fold drag).
    std::map<int, int> pitchDelta;
    for (int id : ids)
        for (const auto& n : resolved.notes)
            if (n.id == id)
            {
                const int row = juce::jlimit(0, numRows() - 1, rowForPitch(n.pitch) + rc.dragDRows);
                pitchDelta[id] = pitchForRow(row) - n.pitch;
                break;
            }

    const int dStep = rc.dragDStep;
    applyEdit([&pitchDelta, dStep](ClipEdit& e)
    {
        for (const auto& [id, dPitch] : pitchDelta)
        {
            auto& mv = e.moves[id];
            mv.dPitch += dPitch;
            mv.dStep += dStep;
            if (mv.dPitch == 0 && mv.dStep == 0)
                e.moves.erase(id);
        }
    });
}

// ── layout / paint ───────────────────────────────────────────────────────────

void PianoRollEditor::resized()
{
    auto r = getLocalBounds();

    // Instrument strip: primary pitch tools left; Fit/Map demoted as secondary.
    auto strip = r.removeFromTop(stripH).reduced(8, 4);
    octaveStepper.setBounds(strip.removeFromLeft(80).withSizeKeepingCentre(80, 26));
    strip.removeFromLeft(6);
    rootPicker.setBounds(strip.removeFromLeft(96).withSizeKeepingCentre(96, 26));
    strip.removeFromLeft(4);
    modePicker.setBounds(strip.removeFromLeft(118).withSizeKeepingCentre(118, 26));
    strip.removeFromLeft(10);
    btnLock.setBounds(strip.removeFromRight(26).withSizeKeepingCentre(24, 24));
    strip.removeFromRight(8);
    // Secondary density: Fit / Map share remaining space at lower visual weight.
    const int secondaryW = juce::jmax(0, strip.getWidth());
    const int fitW = juce::jmin(fitSwitch.idealWidth(), secondaryW / 2);
    fitSwitch.setBounds(strip.removeFromLeft(fitW));
    strip.removeFromLeft(8);
    mapSwitch.setBounds(strip.removeFromLeft(juce::jmin(mapSwitch.idealWidth(), strip.getWidth())));

    // Toolbar: Trim / Fold first so they stay reachable even with many badges.
    auto bar = r.removeFromTop(toolbarH).reduced(8, 5);
    btnRevert.setBounds(bar.removeFromLeft(26).withSizeKeepingCentre(24, 24));
    bar.removeFromLeft(6);
    btnTrim.setBounds(bar.removeFromLeft(juce::jmin(btnTrim.idealWidth(), 110))
                          .withSizeKeepingCentre(juce::jmin(btnTrim.idealWidth(), 110), 22));
    bar.removeFromLeft(4);
    btnFold.setBounds(bar.removeFromLeft(juce::jmin(btnFold.idealWidth(), 78))
                          .withSizeKeepingCentre(juce::jmin(btnFold.idealWidth(), 78), 22));
    bar.removeFromLeft(8);
    for (auto& chip : badgeChips)
    {
        const int w = juce::jmin(chip->idealWidth(), 130);
        if (bar.getWidth() < w + 160) { chip->setVisible(false); continue; }
        chip->setVisible(true);
        chip->setBounds(bar.removeFromLeft(w).withSizeKeepingCentre(w, 22));
        bar.removeFromLeft(4);
    }

    // Right side, packed from the right edge
    auto right = bar;
    if (selBadge.isVisible())
    {
        selBadge.setBounds(right.removeFromRight(juce::jmin(selBadge.idealWidth(), 84))
                               .withSizeKeepingCentre(juce::jmin(selBadge.idealWidth(), 84), 22));
        right.removeFromRight(6);
    }
    btnZoomIn.setBounds(right.removeFromRight(24).withSizeKeepingCentre(22, 22));
    btnZoomOut.setBounds(right.removeFromRight(24).withSizeKeepingCentre(22, 22));
    right.removeFromRight(6);
    divisionPicker.setBounds(right.removeFromRight(74).withSizeKeepingCentre(74, 24));

    // Groove footer
    knobsPanel.setBounds(r.removeFromBottom(knobsPanel.idealHeight()));

    // Velocity lane
    const int laneH = velocityOpen ? VelocityLane::headerH + VelocityLane::laneH
                                   : VelocityLane::headerH;
    velocityLane.setBounds(r.removeFromBottom(laneH));

    // Roll: fixed key gutter + scrolling content
    gutter.setBounds(r.removeFromLeft(gutterW));
    rollViewport.setBounds(r);
    computePxPerStepBase();
    updateRollSize();
}

void PianoRollEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel2());
    g.setColour(colours::line());
    g.fillRect(juce::Rectangle<int>(0, stripH - 1, getWidth(), 1));
    g.fillRect(juce::Rectangle<int>(0, stripH + toolbarH - 1, getWidth(), 1));
}

// ── RollContent ──────────────────────────────────────────────────────────────

void PianoRollEditor::RollContent::paint(juce::Graphics& g)
{
    auto& ed = owner;
    const auto gridStyle = currentGrid();
    const bool blueprint = gridStyle == GridStyle::Blueprint;

    g.fillAll(blueprint ? colours::bpBg() : colours::rollBg());
    if (!ed.hasClip)
    {
        g.setColour(colours::text3());
        g.setFont(uiFont(14.0f, false));
        g.drawText("Select a MIDI file to edit", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float pps = ed.pxPerStep();
    const auto clipB = g.getClipBounds().toFloat();

    // Pitch rows: shade black-key rows; row lines per style.
    const int firstRow = juce::jmax(0, (int) std::floor(clipB.getY() / ed.effRowH()));
    const int lastRow = juce::jmin(ed.numRows() - 1, (int) std::ceil(clipB.getBottom() / ed.effRowH()));
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const float y = (float) row * ed.effRowH();
        const int pitch = ed.pitchForRow(row);
        if (isBlackKeyPitch(pitch))
        {
            g.setColour(blueprint ? colours::bpRow() : colours::rollShade());
            g.fillRect(clipB.getX(), y, clipB.getWidth(), ed.effRowH());
        }
        if (gridStyle != GridStyle::Minimal || pitch % 12 == 0)
        {
            g.setColour(blueprint ? colours::bpRow() : colours::rollRowline());
            g.fillRect(clipB.getX(), y + ed.effRowH() - 0.5f, clipB.getWidth(), 0.5f);
        }
    }

    // Vertical grid from the division picker; bar lines heavier.
    const int total = ed.totalSteps();
    for (int s = 0; s <= total; s += ed.divisionSteps)
    {
        const float x = (float) s * pps;
        if (x < clipB.getX() - 2.0f || x > clipB.getRight() + 2.0f) continue;
        const bool isBar = (s % kStepsPerBar) == 0;
        if (blueprint)
            g.setColour(isBar ? colours::bpBar() : colours::bpBeat());
        else
            g.setColour(isBar ? colours::lineStrong() : colours::rollRowline());
        g.fillRect(x, clipB.getY(), isBar ? 1.0f : 0.5f, clipB.getHeight());
    }

    // Notes at grooved positions; opacity tracks velocity.
    const bool draggingNotes = drag == Drag::Note;
    for (const auto& n : ed.grooved)
    {
        const bool isSelected = ed.selection.count(n.id) > 0;
        const bool inDrag = draggingNotes
            && (n.id == dragNoteId || (ed.selection.count(dragNoteId) > 0 && isSelected));
        auto r = ed.noteRect(n, inDrag ? dragDRows : 0, inDrag ? (double) dragDStep : 0.0);
        if (!r.intersects(clipB)) continue;

        const double v = effectiveVelocity(n, ed.groove);
        juce::Colour fill = blueprint ? colours::bpNote()
                          : (n.moved || inDrag) ? colours::accentBright()
                                                : colours::accent();
        g.setColour(fill.withAlpha((float) (0.4 + 0.55 * v)));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(blueprint ? colours::bpEdge() : colours::rollNoteEdge());
        g.drawRoundedRectangle(r, 2.0f, 0.8f);

        if (isSelected)
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawRoundedRectangle(r.expanded(2.2f), 3.0f, 1.6f);
            g.setColour(colours::accentBright());
            g.drawRoundedRectangle(r.expanded(0.8f), 2.5f, 1.5f);
        }
    }

    // Marquee band
    if (drag == Drag::Marquee)
    {
        g.setColour(colours::accentSoft());
        g.fillRect(marquee);
        g.setColour(colours::accentLine());
        g.drawRect(marquee, 1.0f);
    }

    // Playhead
    if (ed.playing)
    {
        g.setColour(colours::playhead());
        g.fillRect((float) ed.playheadStep * pps, clipB.getY(), 1.5f, clipB.getHeight());
    }
}

void PianoRollEditor::RollContent::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;

    dragStart = e.position;
    dragDRows = 0;
    dragDStep = 0;

    if (const auto* n = ed.noteAt(e.position))
    {
        dragNoteId = n->id;
        drag = Drag::Note;
        if (e.mods.isShiftDown())
        {
            if (!ed.selection.insert(n->id).second)
                ed.selection.erase(n->id);
        }
        else if (ed.selection.count(n->id) == 0)
        {
            ed.selection.clear();
            ed.selection.insert(n->id);
        }
        ed.refreshControls();
        repaint();
        ed.gutter.repaint();
        ed.velocityLane.repaint();
        return;
    }

    drag = Drag::Marquee;
    marqueeAdditive = e.mods.isShiftDown();
    marquee = { e.position.x, e.position.y, 0.0f, 0.0f };
    if (!marqueeAdditive && !ed.selection.empty())
    {
        ed.selection.clear();
        ed.refreshControls();
        ed.gutter.repaint();
        ed.velocityLane.repaint();
    }
    repaint();
}

void PianoRollEditor::RollContent::mouseDrag(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (drag == Drag::Note)
    {
        const float dx = e.position.x - dragStart.x;
        const float dy = e.position.y - dragStart.y;
        const int div = juce::jmax(1, ed.divisionSteps);
        const int stepsMoved = (int) std::round(dx / ed.pxPerStep() / (float) div) * div;
        const int rowsMoved = (int) std::round(dy / ed.effRowH());
        if (stepsMoved != dragDStep || rowsMoved != dragDRows)
        {
            dragDStep = stepsMoved;
            dragDRows = rowsMoved;
            repaint();
        }
        return;
    }
    if (drag == Drag::Marquee)
    {
        marquee = juce::Rectangle<float>(dragStart, e.position);
        repaint();
    }
}

void PianoRollEditor::RollContent::mouseUp(const juce::MouseEvent&)
{
    auto& ed = owner;
    if (drag == Drag::Note)
    {
        ed.commitNoteDrag();
        dragNoteId = -1;
    }
    else if (drag == Drag::Marquee)
    {
        if (marquee.getWidth() > 2.0f || marquee.getHeight() > 2.0f)
        {
            if (!marqueeAdditive)
                ed.selection.clear();
            for (const auto& n : ed.grooved)
                if (ed.noteRect(n).intersects(marquee))
                    ed.selection.insert(n.id);
            ed.refreshControls();
            ed.gutter.repaint();
            ed.velocityLane.repaint();
        }
    }
    drag = Drag::None;
    dragDRows = dragDStep = 0;
    repaint();
}

void PianoRollEditor::RollContent::mouseWheelMove(const juce::MouseEvent& e,
                                                  const juce::MouseWheelDetails& wheel)
{
    // ⌘/ctrl+wheel = horizontal zoom toward the cursor; alt+wheel = vertical
    // zoom. Anything else bubbles to the viewport, which pans both axes
    // (trackpad two-finger scrolling).
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float factor = 1.0f + juce::jlimit(-0.4f, 0.4f, wheel.deltaY * 2.2f);
        owner.zoomXAround(factor, e.position.x);
        return;
    }
    if (e.mods.isAltDown())
    {
        const float factor = 1.0f + juce::jlimit(-0.4f, 0.4f, wheel.deltaY * 2.2f);
        owner.zoomRowsAround(factor, e.position.y);
        return;
    }
    juce::Component::mouseWheelMove(e, wheel);
}

void PianoRollEditor::RollContent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    // Trackpad pinch: zoom time toward the cursor (Live/Logic feel).
    owner.zoomXAround(scaleFactor, e.position.x);
}

// ── KeyGutter ────────────────────────────────────────────────────────────────

void PianoRollEditor::KeyGutter::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(colours::panel());
    if (!ed.hasClip)
    {
        g.setColour(colours::line());
        g.fillRect(getLocalBounds().removeFromRight(1));
        return;
    }

    const int viewY = ed.rollViewport.getViewPositionY();

    std::set<int> selectedPitches;
    for (const auto& n : ed.resolved.notes)
        if (ed.selection.count(n.id) > 0)
            selectedPitches.insert(n.pitch);

    const int firstRow = juce::jmax(0, (int) std::floor((float) viewY / ed.effRowH()));
    const int lastRow = juce::jmin(ed.numRows() - 1,
                                   (int) std::ceil((float) (viewY + getHeight()) / ed.effRowH()));

    for (int row = firstRow; row <= lastRow; ++row)
    {
        const float y = (float) row * ed.effRowH() - (float) viewY;
        const int pitch = ed.pitchForRow(row);
        auto rowRect = juce::Rectangle<float>(0.0f, y, (float) getWidth() - 1.0f, ed.effRowH());

        g.setColour(isBlackKeyPitch(pitch) ? colours::kbBlack() : colours::kbWhite());
        g.fillRect(rowRect);

        if (selectedPitches.count(pitch) > 0)
        {
            g.setColour(colours::accentSoft());
            g.fillRect(rowRect);
            g.setColour(colours::accent());
            g.fillRect(rowRect.removeFromRight(2.0f));
        }

        g.setColour(colours::rollRowline());
        g.fillRect(0.0f, y, (float) getWidth(), 0.5f);

        // Label C rows; when folded, label every row (Live-style).
        if ((ed.folded || pitch % 12 == 0) && ed.effRowH() >= 8.0f)
        {
            g.setColour(colours::text3());
            g.setFont(monoFont(juce::jmin(12.0f, ed.effRowH() - 1.5f), false));
            g.drawText(pitchName(pitch), 4, (int) y, getWidth() - 8, (int) ed.effRowH(),
                       juce::Justification::centredLeft);
        }
    }
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));
}

void PianoRollEditor::KeyGutter::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;
    const int row = (int) std::floor((e.position.y + (float) ed.rollViewport.getViewPositionY())
                                     / ed.effRowH());
    if (row < 0 || row >= ed.numRows()) return;
    ed.selectPitch(ed.pitchForRow(row), e.mods.isShiftDown());
}

// ── VelocityLane ─────────────────────────────────────────────────────────────

void PianoRollEditor::VelocityLane::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto header = getLocalBounds().removeFromTop(headerH).reduced(10, 0);
    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    drawIcon(g, ed.velocityOpen ? icons::caretDown : icons::caretUp, caret, colours::text3(), 1.5f);
    header.removeFromLeft(6);
    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText("VELOCITY", header.removeFromLeft(72), juce::Justification::centredLeft);

    g.setColour(colours::text3());
    g.setFont(monoFont(12.0f, false));
    g.drawText("Intensity " + juce::String(ed.groove.intensity) + juce::String::fromUTF8("% · Dyn ")
                   + juce::String(ed.groove.dynamics) + "%",
               header, juce::Justification::centredRight);

    if (!ed.velocityOpen || !ed.hasClip) return;

    auto lane = getLocalBounds().withTrimmedTop(headerH).reduced(0, 3);
    const float pps = ed.pxPerStep();
    const int scrollX = ed.rollViewport.getViewPositionX() - gutterW;

    for (const auto& n : ed.grooved)
    {
        const float x = (float) n.start * pps - (float) scrollX;
        const float w = juce::jmax(2.0f, (float) n.len * pps - 1.0f);   // match note width
        if (x + w < (float) gutterW - 4.0f || x > (float) getWidth() + 4.0f) continue;
        const double v = effectiveVelocity(n, ed.groove);
        const float bh = juce::jmax(2.0f, (float) v * (float) lane.getHeight());
        const bool isSelected = ed.selection.count(n.id) > 0;
        g.setColour((isSelected ? colours::accentBright() : colours::accent())
                        .withAlpha(isSelected ? 0.95f : 0.6f));
        g.fillRoundedRectangle(x, (float) lane.getBottom() - bh, w, bh, 1.5f);
    }
}

const RollNote* PianoRollEditor::VelocityLane::noteAtX(float x) const
{
    // Content-space x → the note whose bar spans it (nearest start wins).
    auto& ed = owner;
    const float pps = ed.pxPerStep();
    const float contentX = x + (float) ed.rollViewport.getViewPositionX() - (float) gutterW;
    const RollNote* best = nullptr;
    float bestDist = 1.0e9f;
    for (const auto& n : ed.grooved)
    {
        const float nx = (float) n.start * pps;
        const float w = juce::jmax(2.0f, (float) n.len * pps - 1.0f);
        if (contentX >= nx - 2.0f && contentX <= nx + w + 2.0f)
        {
            const float dist = std::abs(contentX - nx);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = &n;
            }
        }
    }
    return best;
}

void PianoRollEditor::VelocityLane::applyDragVelocity(const juce::MouseEvent& e)
{
    if (dragNoteId < 0) return;
    auto lane = getLocalBounds().withTrimmedTop(headerH).reduced(0, 3);
    const double frac = juce::jlimit(0.0, 1.0,
        (double) (lane.getBottom() - e.position.y) / (double) juce::jmax(1, lane.getHeight()));
    const int vel = juce::jlimit(1, 127, (int) std::lround(frac * 127.0));
    const int id = dragNoteId;
    owner.applyEdit([id, vel](ClipEdit& ed) { ed.velocities[id] = vel; });
}

void PianoRollEditor::VelocityLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
    {
        owner.velocityOpen = !owner.velocityOpen;
        owner.resized();
        owner.repaint();
        return;
    }
    if (!owner.velocityOpen || !owner.hasClip) return;
    if (const auto* n = noteAtX(e.position.x))
    {
        dragNoteId = n->id;
        applyDragVelocity(e);
    }
}

void PianoRollEditor::VelocityLane::mouseDrag(const juce::MouseEvent& e)
{
    applyDragVelocity(e);
}

void PianoRollEditor::VelocityLane::mouseUp(const juce::MouseEvent&)
{
    dragNoteId = -1;
}

} // namespace pflow
