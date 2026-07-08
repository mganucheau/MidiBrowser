#include "PianoRollEditor.h"

namespace pflow {

// ── PitchRowMap ──────────────────────────────────────────────────────────────

void PitchRowMap::fit(const std::vector<RollNote>& notes, float height, int minRange)
{
    int lo = 127, hi = 0;
    for (const auto& n : notes)
    {
        lo = juce::jmin(lo, n.pitch);
        hi = juce::jmax(hi, n.pitch);
    }
    if (notes.empty()) { lo = 57; hi = 74; }

    // Pad by a row each side, enforce a minimum range, clamp to MIDI.
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
        g.setFont(uiFont(11.0f, false));
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

    for (int i = 0; i < kNumModes; ++i)
        modePicker.addItem(modeName((Mode) i), i + 1);
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

    // Toolbar
    btnRevert.onClick = [this]
    {
        selection.clear();
        applyEdit([](ClipEdit& e) { e = ClipEdit(); });
    };
    addAndMakeVisible(btnRevert);

    btnTrim.onClick = [this] { toggleTrim(); };
    addAndMakeVisible(btnTrim);

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
        zoomAround(1.0f / 1.25f, (float) rollViewport.getViewPositionX()
                                     + (float) rollViewport.getWidth() * 0.5f);
    };
    btnZoomIn.onClick = [this]
    {
        zoomAround(1.25f, (float) rollViewport.getViewPositionX()
                              + (float) rollViewport.getWidth() * 0.5f);
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
    rollViewport.setScrollBarsShown(false, true, false, true);
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
    if (!sameClip)
    {
        selection.clear();
        rollViewport.setViewPosition(0, 0);
    }
    knobsPanel.setParams(groove, juce::dontSendNotification);
    rebuildResolved();
    refreshControls();
    updateRollSize();
    repaint();
}

void PianoRollEditor::clearClip()
{
    hasClip = false;
    selection.clear();
    resolved = {};
    grooved.clear();
    refreshControls();
    repaint();
}

void PianoRollEditor::setPlayheadStep(double step, bool isPlaying)
{
    playheadStep = step;
    playing = isPlaying;
    rollContent.repaint();
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
    btnTrim.repaint();

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
    });
}

void PianoRollEditor::toggleTrim()
{
    const bool isTrimmed = edit.trimLead + edit.trimTail > 0;
    const auto lead = edges.lead, tail = edges.tail;
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

// ── geometry ─────────────────────────────────────────────────────────────────

float PianoRollEditor::pxPerStep() const
{
    const float fitPps = (float) juce::jmax(60, rollViewport.getMaximumVisibleWidth())
                         / (float) juce::jmax(1, totalSteps());
    return juce::jmax(1.5f, fitPps) * zoom;
}

void PianoRollEditor::updateRollSize()
{
    const int w = (int) std::ceil((float) totalSteps() * pxPerStep());
    const int h = juce::jmax(1, rollViewport.getMaximumVisibleHeight());
    rollContent.setSize(juce::jmax(w, 1), h);
    rowMap.fit(resolved.notes, (float) h, 16);
    gutter.repaint();
    velocityLane.repaint();
}

juce::Rectangle<float> PianoRollEditor::noteRect(const RollNote& n) const
{
    const float pps = pxPerStep();
    return { (float) n.start * pps,
             rowMap.yForPitchTop(n.pitch, (float) rollContent.getHeight()),
             juce::jmax(3.0f, (float) n.len * pps - 1.0f),
             juce::jmax(3.0f, rowMap.rowH - 1.0f) };
}

const RollNote* PianoRollEditor::noteAt(juce::Point<float> pos) const
{
    // Topmost = last drawn; search back to front.
    for (auto it = grooved.rbegin(); it != grooved.rend(); ++it)
        if (noteRect(*it).contains(pos))
            return &(*it);
    return nullptr;
}

void PianoRollEditor::zoomAround(float factor, float contentX)
{
    const float pps = pxPerStep();
    const float anchorStep = contentX / pps;
    const float cursorInView = contentX - (float) rollViewport.getViewPositionX();

    zoom = juce::jlimit(1.0f, 3.0f, zoom * factor);   // clamp ~1–3 per spec
    updateRollSize();

    const float newX = anchorStep * pxPerStep() - cursorInView;
    rollViewport.setViewPosition((int) std::round(newX), 0);
    rollContent.repaint();
}

void PianoRollEditor::commitNoteDrag()
{
    auto& rc = rollContent;
    if (rc.dragDPitch == 0 && rc.dragDStep == 0)
        return;

    std::vector<int> ids;
    if (selection.count(rc.dragNoteId) > 0)
        ids.assign(selection.begin(), selection.end());
    else
        ids.push_back(rc.dragNoteId);

    const int dPitch = rc.dragDPitch, dStep = rc.dragDStep;
    applyEdit([&ids, dPitch, dStep](ClipEdit& e)
    {
        for (int id : ids)
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

    // Instrument strip
    auto strip = r.removeFromTop(stripH).reduced(8, 4);
    octaveStepper.setBounds(strip.removeFromLeft(84).withSizeKeepingCentre(84, 26));
    strip.removeFromLeft(8);
    rootPicker.setBounds(strip.removeFromLeft(58).withSizeKeepingCentre(58, 26));
    strip.removeFromLeft(4);
    modePicker.setBounds(strip.removeFromLeft(96).withSizeKeepingCentre(96, 26));
    strip.removeFromLeft(12);
    fitSwitch.setBounds(strip.removeFromLeft(96));
    strip.removeFromLeft(8);
    mapSwitch.setBounds(strip.removeFromLeft(110));

    // Toolbar
    auto bar = r.removeFromTop(toolbarH).reduced(8, 5);
    btnRevert.setBounds(bar.removeFromLeft(26).withSizeKeepingCentre(24, 24));
    bar.removeFromLeft(6);
    for (auto& chip : badgeChips)
    {
        const int w = juce::jmin(chip->idealWidth(), 130);
        if (bar.getWidth() < w + 150) { chip->setVisible(false); continue; }
        chip->setVisible(true);
        chip->setBounds(bar.removeFromLeft(w).withSizeKeepingCentre(w, 22));
        bar.removeFromLeft(4);
    }
    btnTrim.setBounds(bar.removeFromLeft(juce::jmin(btnTrim.idealWidth(), 74))
                          .withSizeKeepingCentre(64, 22));
    bar.removeFromLeft(10);

    // Right side, packed from the right edge
    auto right = bar;
    if (selBadge.isVisible())
    {
        selBadge.setBounds(right.removeFromRight(juce::jmin(selBadge.idealWidth(), 76))
                               .withSizeKeepingCentre(juce::jmin(selBadge.idealWidth(), 76), 22));
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

    // Roll: fixed key gutter + horizontally scrolling content
    gutter.setBounds(r.removeFromLeft(gutterW));
    rollViewport.setBounds(r);
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
        g.setFont(uiFont(12.0f, false));
        g.drawText("Select a MIDI file to edit", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float pps = ed.pxPerStep();
    const float h = (float) getHeight();
    const auto clipB = g.getClipBounds().toFloat();

    // Pitch rows: shade black-key rows; row lines per style.
    for (int p = ed.rowMap.minPitch; p <= ed.rowMap.maxPitch; ++p)
    {
        const float y = ed.rowMap.yForPitchTop(p, h);
        if (y + ed.rowMap.rowH < clipB.getY() || y > clipB.getBottom()) continue;
        if (isBlackKeyPitch(p))
        {
            g.setColour(blueprint ? colours::bpRow() : colours::rollShade());
            g.fillRect(clipB.getX(), y, clipB.getWidth(), ed.rowMap.rowH);
        }
        if (gridStyle != GridStyle::Minimal || p % 12 == 0)
        {
            g.setColour(blueprint ? colours::bpRow() : colours::rollRowline());
            g.fillRect(clipB.getX(), y + ed.rowMap.rowH - 0.5f, clipB.getWidth(), 0.5f);
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
        g.fillRect(x, 0.0f, isBar ? 1.0f : 0.5f, h);
    }

    // Notes at grooved positions; opacity tracks velocity.
    const bool draggingNotes = drag == Drag::Note;
    for (const auto& n : ed.grooved)
    {
        RollNote shown = n;
        const bool isSelected = ed.selection.count(n.id) > 0;
        const bool inDrag = draggingNotes
            && (n.id == dragNoteId || (ed.selection.count(dragNoteId) > 0 && isSelected));
        if (inDrag)
        {
            shown.pitch += dragDPitch;
            shown.start += dragDStep;
        }
        auto r = ed.noteRect(shown);
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
            // Bright ring + dark halo, per spec.
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
        g.fillRect((float) ed.playheadStep * pps, 0.0f, 1.5f, h);
    }
}

void PianoRollEditor::RollContent::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;

    dragStart = e.position;
    dragDPitch = 0;
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
        const int pitchMoved = -(int) std::round(dy / ed.rowMap.rowH);
        if (stepsMoved != dragDStep || pitchMoved != dragDPitch)
        {
            dragDStep = stepsMoved;
            dragDPitch = pitchMoved;
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
    dragDPitch = dragDStep = 0;
    repaint();
}

void PianoRollEditor::RollContent::mouseWheelMove(const juce::MouseEvent& e,
                                                  const juce::MouseWheelDetails& wheel)
{
    // ⌘/ctrl+wheel = pinch-zoom toward the cursor. Anything else falls through
    // to the viewport for native horizontal two-finger panning.
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float factor = 1.0f + juce::jlimit(-0.4f, 0.4f, wheel.deltaY * 2.2f);
        owner.zoomAround(factor, e.position.x);
        return;
    }
    juce::Component::mouseWheelMove(e, wheel);
}

// ── KeyGutter ────────────────────────────────────────────────────────────────

void PianoRollEditor::KeyGutter::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(colours::panel());
    if (!ed.hasClip) return;

    const float h = (float) getHeight();

    std::set<int> selectedPitches;
    for (const auto& n : ed.resolved.notes)
        if (ed.selection.count(n.id) > 0)
            selectedPitches.insert(n.pitch);

    for (int p = ed.rowMap.minPitch; p <= ed.rowMap.maxPitch; ++p)
    {
        const float y = ed.rowMap.yForPitchTop(p, h);
        auto row = juce::Rectangle<float>(0.0f, y, (float) getWidth() - 1.0f, ed.rowMap.rowH);
        g.setColour(isBlackKeyPitch(p) ? colours::kbBlack() : colours::kbWhite());
        g.fillRect(row);

        if (selectedPitches.count(p) > 0)
        {
            g.setColour(colours::accentSoft());
            g.fillRect(row);
            g.setColour(colours::accent());
            g.fillRect(row.removeFromRight(2.0f));
        }

        g.setColour(colours::rollRowline());
        g.fillRect(0.0f, y, (float) getWidth(), 0.5f);

        if (p % 12 == 0 && ed.rowMap.rowH >= 7.0f)   // label C rows
        {
            g.setColour(colours::text3());
            g.setFont(monoFont(juce::jmin(9.0f, ed.rowMap.rowH - 1.0f), false));
            g.drawText(pitchName(p), 4, (int) y, getWidth() - 8, (int) ed.rowMap.rowH,
                       juce::Justification::centredLeft);
        }
    }
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));
}

void PianoRollEditor::KeyGutter::mouseDown(const juce::MouseEvent& e)
{
    if (!owner.hasClip) return;
    const int pitch = owner.rowMap.pitchForY(e.position.y, (float) getHeight());
    owner.selectPitch(pitch, e.mods.isShiftDown());
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
    g.setFont(uiFont(10.5f, true));
    g.drawText("VELOCITY", header.removeFromLeft(64), juce::Justification::centredLeft);

    g.setColour(colours::text3());
    g.setFont(monoFont(10.0f, false));
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
        if (x < (float) gutterW - 4.0f || x > (float) getWidth() + 4.0f) continue;
        const double v = effectiveVelocity(n, ed.groove);
        const float bh = juce::jmax(2.0f, (float) v * (float) lane.getHeight());
        const bool isSelected = ed.selection.count(n.id) > 0;
        g.setColour((isSelected ? colours::accentBright() : colours::accent())
                        .withAlpha(isSelected ? 1.0f : 0.75f));
        g.fillRoundedRectangle(x, (float) lane.getBottom() - bh, 3.0f, bh, 1.2f);
    }
}

void PianoRollEditor::VelocityLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
    {
        owner.velocityOpen = !owner.velocityOpen;
        owner.resized();
        owner.repaint();
    }
}

juce::String PianoRollEditor::divisionName(int steps) const
{
    switch (steps)
    {
        case 16: return "1 bar";
        case 8:  return "1/2";
        case 4:  return "1/4";
        case 2:  return "1/8";
        default: return "1/16";
    }
}

} // namespace pflow
