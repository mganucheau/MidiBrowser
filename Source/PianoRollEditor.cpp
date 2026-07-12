#include "PianoRollEditor.h"
#include <algorithm>

namespace pflow {

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

void PianoRollMini::setTimeStretch(double stretch)
{
    timeStretch = juce::jmax(0.25, stretch);
    repaint();
}

void PianoRollMini::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::rollBg());
    g.fillRect(b);
    g.setColour(colours::line());
    g.drawRect(b.reduced(0.5f), 1.0f);

    if (notes.empty())
    {
        g.setColour(colours::text3());
        g.setFont(uiFont(13.0f, false));
        g.drawText("Select a MIDI file", getLocalBounds(), juce::Justification::centred);
        return;
    }

    PitchRowMap map;
    map.fit(notes, b.getHeight(), 14);
    const double totalSteps = (double) bars * kStepsPerBar * timeStretch;
    const float pps = b.getWidth() / (float) juce::jmax(1.0, totalSteps);
    auto inner = b;

    g.setColour(colours::rollRowline());
    const int displayBars = juce::jmax(1, (int) std::lround((double) bars * timeStretch));
    for (int bar = 1; bar < displayBars; ++bar)
        g.fillRect(inner.getX() + (float) (bar * kStepsPerBar) * pps, inner.getY(),
                   1.0f, inner.getHeight());

    for (const auto& n : notes)
    {
        const double v = effectiveVelocity(n, knobs);
        auto r = juce::Rectangle<float>(
            inner.getX() + (float) (n.start * timeStretch) * pps,
            inner.getY() + map.yForPitchTop(n.pitch, inner.getHeight()),
            juce::jmax(2.0f, (float) (n.len * timeStretch) * pps - 0.5f),
            juce::jmax(2.0f, map.rowH - 0.8f));
        g.setColour(colours::accent().withAlpha((float) (0.35 + 0.6 * v)));
        g.fillRoundedRectangle(r, 1.5f);
    }

    if (playing)
    {
        g.setColour(colours::playhead());
        g.fillRect(inner.getX() + (float) (playheadStep * timeStretch) * pps,
                   inner.getY(), 1.5f, inner.getHeight());
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

    // Pitch lock lives in EffectsInspector; keep member for ABI but hide it.
    btnLock.setComponentID({});
    btnLock.setVisible(false);
    btnLock.onClick = nullptr;
    addChildComponent(btnLock);

    // Toolbar
    btnRevert.onClick = [this]
    {
        selection.clear();
        applyEdit([](ClipEdit& e) { e = ClipEdit(); });
    };
    addAndMakeVisible(btnRevert);

    btnTrim.onClick = [this] { toggleTrim(); };
    addChildComponent(btnTrim);
    btnTrim.setVisible(false);

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
            timeRuler.repaint();
        }
    };
    addAndMakeVisible(divisionPicker);

    for (int b : { 2, 4, 8, 16 })
        barsZoomPicker.addItem(juce::String(b), b);
    barsZoomPicker.setSelectedId(visibleBarsZoom, juce::dontSendNotification);
    barsZoomPicker.onChange = [this]
    {
        const int id = barsZoomPicker.getSelectedId();
        if (id > 0)
        {
            visibleBarsZoom = id;
            computePxPerStepBase();
            updateRollSize();
            rollContent.repaint();
            timeRuler.repaint();
            velocityLane.repaint();
        }
    };
    addAndMakeVisible(barsZoomPicker);

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
    addAndMakeVisible(timeRuler);
    rollViewport.setViewedComponent(&rollContent, false);
    rollViewport.setScrollBarsShown(true, true, true, true);
    rollViewport.setScrollBarThickness(8);
    rollViewport.onScrolled = [this]
    {
        gutter.repaint();
        velocityLane.repaint();
        timeRuler.repaint();
    };
    addAndMakeVisible(rollViewport);

    addAndMakeVisible(velocityLane);

    addAndMakeVisible(scalePanel);
    scalePanel.setVisible(false);
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
    rebuildResolved();
    refreshControls();
    if (!sameClip)
    {
        selection.clear();
        resetLoopToClip();
        visibleBarsZoom = snapBarsZoom(resolved.bars);
        barsZoomPicker.setSelectedId(visibleBarsZoom, juce::dontSendNotification);
        computePxPerStepBase();
        updateRollSize();
        scrollToContent();
    }
    else
    {
        // Keep loop inside the (possibly trimmed) clip length.
        setLoopSteps(loopStartStep, loopEndStep, false);
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
    btnLock.setTooltip(locked ? "Pitch edits + trim locked while browsing"
                              : "Lock pitch edits (and trim, if active) while browsing");
    btnLock.repaint();
}

void PianoRollEditor::setTimeStretch(double stretch)
{
    const double s = juce::jmax(0.25, stretch);
    if (std::abs(s - timeStretch) < 1.0e-6) return;
    timeStretch = s;
    computePxPerStepBase();
    updateRollSize();
    rollContent.repaint();
    timeRuler.repaint();
    velocityLane.repaint();
    notifyLoopChanged();
}

void PianoRollEditor::setScalePlacement(ScalePlacement)
{
    // Pitch & Scale lives in the Effects inspector (Cupertino).
}

void PianoRollEditor::rebuildResolved()
{
    if (!hasClip) return;
    resolved = resolveClip(clip, edit);
    grooved = applyGroove(resolved.notes, groove);

    ClipEdit noTrim = edit;
    noTrim.clearTrim();
    const auto preTrim = resolveClip(clip, noTrim);
    emptyBarIndices = emptyBars(preTrim.notes, clip.bars);

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
    setLoopSteps(loopStartStep, loopEndStep, true);
    refreshControls();
    updateRollSize();
    repaint();
    if (onEditChanged)
        onEditChanged(edit);
}

void PianoRollEditor::refreshTrimButtonState()
{
    const bool isTrimmed = edit.hasTrim();
    const bool hasNewEmpties = !emptyBarIndices.empty()
                               && emptyBarIndices != edit.removedBars;
    const bool showRestore = isTrimmed && !hasNewEmpties;
    const bool enabled = hasNewEmpties || isTrimmed;
    const juce::String label = showRestore ? "Restore" : "Trim";
    btnTrim.setEnabled(enabled);
    btnTrim.active = isTrimmed;
    btnTrim.label = label;
    btnTrim.setTooltip(showRestore ? "Restore trimmed bars"
                      : hasNewEmpties ? "Trim all empty measures"
                                      : "No empty measures to trim");
    btnTrim.repaint();
    if (onTrimStateChanged)
        onTrimStateChanged(isTrimmed, enabled, label);
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

    refreshTrimButtonState();

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
        else if (key == "trim")  e.clearTrim();
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

    if (!selection.empty() && key.getModifiers().isShiftDown())
    {
        if (key == juce::KeyPress::upKey)    { nudgeSelection(1, 0); return true; }
        if (key == juce::KeyPress::downKey)  { nudgeSelection(-1, 0); return true; }
        if (key == juce::KeyPress::leftKey)  { nudgeSelection(0, -divisionSteps); return true; }
        if (key == juce::KeyPress::rightKey) { nudgeSelection(0, divisionSteps); return true; }
    }
    return false;
}

void PianoRollEditor::nudgeSelection(int dPitch, int dStep)
{
    if (selection.empty() || (dPitch == 0 && dStep == 0)) return;
    const auto ids = selection;
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

void PianoRollEditor::toggleTrim()
{
    ClipEdit noTrim = edit;
    noTrim.clearTrim();
    const auto preTrim = resolveClip(clip, noTrim);
    const auto toRemove = emptyBars(preTrim.notes, clip.bars);
    emptyBarIndices = toRemove;

    const bool isTrimmed = edit.hasTrim();
    const bool hasNewEmpties = !toRemove.empty() && toRemove != edit.removedBars;

    if (isTrimmed && !hasNewEmpties)
    {
        applyEdit([](ClipEdit& e) { e.clearTrim(); });
        return;
    }
    if (toRemove.empty())
        return;

    applyEdit([toRemove](ClipEdit& e)
    {
        e.clearTrim();
        e.removedBars = toRemove;
    });
}

void PianoRollEditor::resetLoopToClip()
{
    loopStartStep = 0.0;
    loopEndStep = juce::jmax(minLoopSteps(), clipSteps());
    notifyLoopChanged();
    timeRuler.repaint();
    rollContent.repaint();
}

void PianoRollEditor::setLoopSteps(double start, double end, bool notify)
{
    const double minLen = minLoopSteps();
    const double maxEnd = juce::jmax(minLen, clipSteps());
    start = juce::jlimit(0.0, maxEnd - minLen, start);
    end = juce::jlimit(start + minLen, maxEnd, end);
    if (std::abs(start - loopStartStep) < 1.0e-9 && std::abs(end - loopEndStep) < 1.0e-9)
        return;
    loopStartStep = start;
    loopEndStep = end;
    if (notify)
        notifyLoopChanged();
    timeRuler.repaint();
    rollContent.repaint();
}

void PianoRollEditor::notifyLoopChanged()
{
    if (onLoopChanged)
        onLoopChanged(loopStartStep, loopEndStep);
}

double PianoRollEditor::stepFromContentX(float x) const
{
    const float pps = pxPerStep();
    if (pps <= 0.0f || timeStretch <= 0.0)
        return 0.0;
    return (double) x / ((double) pps * timeStretch);
}

float PianoRollEditor::contentXFromStep(double step) const
{
    return (float) (step * timeStretch) * pxPerStep();
}

int PianoRollEditor::hitLoopHandle(float contentX, float hitPx) const
{
    const float xs = contentXFromStep(loopStartStep);
    const float xe = contentXFromStep(loopEndStep);
    if (std::abs(contentX - xs) <= hitPx) return 0;
    if (std::abs(contentX - xe) <= hitPx) return 1;
    return -1;
}

void PianoRollEditor::paintLoopOverlay(juce::Graphics& g, juce::Rectangle<float> clipB,
                                       float top, float bottom) const
{
    const float xs = contentXFromStep(loopStartStep);
    const float xe = contentXFromStep(loopEndStep);
    const float h = bottom - top;

    // Dim outside the loop region.
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    if (xs > clipB.getX())
        g.fillRect(clipB.getX(), top, xs - clipB.getX(), h);
    if (xe < clipB.getRight())
        g.fillRect(xe, top, clipB.getRight() - xe, h);

    g.setColour(colours::accent().withAlpha(0.85f));
    g.fillRect(xs - 1.0f, top, 2.0f, h);
    g.fillRect(xe - 1.0f, top, 2.0f, h);
}

void PianoRollEditor::paintTimeRulerLabels(juce::Graphics& g, float viewX,
                                           float width, float height) const
{
    if (!hasClip) return;

    const float pps = pxPerStep();
    if (pps <= 0.0f) return;

    const int bars = juce::jmax(1, resolved.bars);
    const float pxPerBar = (float) kStepsPerBar * (float) timeStretch * pps;
    const float pxPerBeat = pxPerBar / 4.0f;
    const float pxPerDiv = (float) juce::jmax(1, divisionSteps) * (float) timeStretch * pps;
    constexpr float kMinLabelPx = 30.0f;

    // Label resolution scales with how much room each measure has on screen
    // (clip length + zoom), not with the grid picker alone — a 32-bar clip on
    // 1/16 only gets measure numbers; short clips can show beats / divisions.
    enum class Res { EveryNBars, Bars, Beats, Divisions };
    Res res = Res::Bars;
    int barStride = 1;

    if (pxPerBar < kMinLabelPx)
    {
        res = Res::EveryNBars;
        while ((float) barStride * pxPerBar < kMinLabelPx && barStride < 64)
            barStride *= 2;
    }
    else if (bars <= 8 && pxPerBeat >= kMinLabelPx)
    {
        res = (bars <= 4 && pxPerDiv >= kMinLabelPx && divisionSteps < kStepsPerBar)
                  ? Res::Divisions
                  : Res::Beats;
    }
    else if (bars <= 16 && pxPerBeat >= kMinLabelPx * 1.15f)
    {
        res = Res::Beats;
    }

    g.setFont(monoFont(11.0f, true));

    auto drawLabel = [&](float x, const juce::String& text, bool strong)
    {
        g.setColour(strong ? colours::text2() : colours::text3());
        g.fillRect(x, height - (strong ? 4.0f : 3.0f), strong ? 1.0f : 0.5f,
                   strong ? 4.0f : 3.0f);
        if (text.isNotEmpty())
            g.drawText(text, juce::Rectangle<float>(x + 3.0f, 1.0f, 40.0f, height - 5.0f),
                       juce::Justification::centredLeft, false);
    };

    if (res == Res::EveryNBars || res == Res::Bars)
    {
        for (int bar = 0; bar <= bars; ++bar)
        {
            if (res == Res::EveryNBars && (bar % barStride) != 0)
                continue;
            const double step = (double) bar * (double) kStepsPerBar;
            const float x = contentXFromStep(step) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            drawLabel(x, bar < bars ? juce::String(bar + 1) : juce::String(), true);
        }
    }
    else if (res == Res::Beats)
    {
        const int totalBeats = bars * 4;
        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const double step = (double) beat * 4.0; // 4 steps per beat
            const float x = contentXFromStep(step) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            const int bar = beat / 4;
            const int beatInBar = beat % 4;
            const bool isBar = beatInBar == 0;
            const juce::String label = beat < totalBeats
                ? (isBar ? juce::String(bar + 1)
                         : juce::String(bar + 1) + "." + juce::String(beatInBar + 1))
                : juce::String();
            drawLabel(x, label, isBar);
        }
    }
    else // Divisions
    {
        const int div = juce::jmax(1, divisionSteps);
        const int total = (int) std::lround((double) bars * (double) kStepsPerBar);
        for (int s = 0; s <= total; s += div)
        {
            const float x = contentXFromStep((double) s) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            const int bar = s / kStepsPerBar;
            const int stepInBar = s % kStepsPerBar;
            const bool isBar = stepInBar == 0;
            const bool isBeat = (stepInBar % 4) == 0;
            juce::String label;
            if (s < total)
            {
                if (isBar)
                    label = juce::String(bar + 1);
                else if (isBeat)
                    label = juce::String(bar + 1) + "." + juce::String(stepInBar / 4 + 1);
            }
            drawLabel(x, label, isBar);
        }
    }

    // Loop handles on the ruler.
    const float xs = contentXFromStep(loopStartStep) - viewX;
    const float xe = contentXFromStep(loopEndStep) - viewX;
    g.setColour(colours::accent());
    auto drawHandle = [&](float x)
    {
        juce::Path tri;
        tri.addTriangle(x - 5.0f, 2.0f, x + 5.0f, 2.0f, x, height - 1.0f);
        g.fillPath(tri);
    };
    drawHandle(xs);
    drawHandle(xe);
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
    return { (float) ((n.start + stepOffset) * timeStretch) * pps,
             (float) row * effRowH(),
             juce::jmax(3.0f, (float) (n.len * timeStretch) * pps - 1.0f),
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
    const int bars = juce::jmax(1, juce::jmin(visibleBarsZoom, juce::jmax(1, resolved.bars)));
    const int stepsToShow = bars * kStepsPerBar;
    pxPerStepBase = juce::jmax(0.75f, (float) visW / (float) juce::jmax(1, stepsToShow));
}

int PianoRollEditor::snapBarsZoom(int clipBars) const
{
    const int t = juce::jmin(16, juce::jmax(1, clipBars));
    for (int o : { 2, 4, 8, 16 })
        if (o >= t) return o;
    return 16;
}

int PianoRollEditor::velocityForNote(const RollNote& n) const
{
    if (auto it = edit.velocities.find(n.id); it != edit.velocities.end())
        return it->second;
    return juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
}

const RollNote* PianoRollEditor::noteAtVelocityX(float laneX) const
{
    const float viewX = (float) rollViewport.getViewPositionX();
    const float pps = pxPerStep();
    const RollNote* best = nullptr;
    float bestDist = 1.0e9f;
    for (const auto& n : grooved)
    {
        const float nx = (float) gutterW + contentXFromStep(n.start) - viewX;
        const float nw = juce::jmax(4.0f, (float) (n.len * timeStretch) * pps);
        if (laneX < nx - 3.0f || laneX > nx + nw + 3.0f) continue;
        const float cx = nx + nw * 0.5f;
        const float d = std::abs(laneX - cx);
        if (d < bestDist)
        {
            bestDist = d;
            best = &n;
        }
    }
    return best;
}

void PianoRollEditor::setVelocityFromLaneY(int noteId, float laneY)
{
    const int laneTop = VelocityLane::headerH;
    const int laneH = VelocityLane::laneH;
    const float t = 1.0f - juce::jlimit(0.0f, 1.0f, (laneY - (float) laneTop) / (float) laneH);
    const int vel = juce::jlimit(1, 127, (int) std::lround(t * 127.0));
    applyEdit([noteId, vel](ClipEdit& e) { e.velocities[noteId] = vel; });
    velocityLane.repaint();
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

    // Clip header: name + meta + revert
    auto strip = r.removeFromTop(stripH).reduced(10, 4);
    btnRevert.setBounds(strip.removeFromRight(26).withSizeKeepingCentre(24, 24));
    btnLock.setVisible(false);
    octaveStepper.setVisible(false);
    rootPicker.setVisible(false);
    modePicker.setVisible(false);
    fitSwitch.setVisible(false);
    mapSwitch.setVisible(false);
    scalePanel.setVisible(false);

    // Toolbar
    auto bar = r.removeFromTop(toolbarH).reduced(8, 5);
    barsZoomPicker.setBounds(bar.removeFromLeft(52).withSizeKeepingCentre(52, 24));
    bar.removeFromLeft(6);
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

    const int laneH = velocityOpen ? VelocityLane::headerH + VelocityLane::laneH
                                   : VelocityLane::headerH;
    velocityLane.setBounds(r.removeFromBottom(laneH));

    // Ruler + gutter + roll
    auto rollArea = r;
    auto rulerRow = rollArea.removeFromTop(rulerH);
    gutter.setBounds(rollArea.removeFromLeft(gutterW));
    timeRuler.setBounds(rulerRow.withTrimmedLeft(gutterW));
    rollViewport.setBounds(rollArea);
    computePxPerStepBase();
    updateRollSize();
}

void PianoRollEditor::layoutScaleControls(juce::Rectangle<int> strip)
{
    octaveStepper.setVisible(true);
    rootPicker.setVisible(true);
    modePicker.setVisible(true);
    fitSwitch.setVisible(true);
    mapSwitch.setVisible(true);

    octaveStepper.setBounds(strip.removeFromLeft(80).withSizeKeepingCentre(80, 26));
    strip.removeFromLeft(6);
    rootPicker.setBounds(strip.removeFromLeft(96).withSizeKeepingCentre(96, 26));
    strip.removeFromLeft(4);
    modePicker.setBounds(strip.removeFromLeft(118).withSizeKeepingCentre(118, 26));
    strip.removeFromLeft(10);
    const int secondaryW = juce::jmax(0, strip.getWidth());
    const int fitW = juce::jmin(fitSwitch.idealWidth(), secondaryW / 2);
    fitSwitch.setBounds(strip.removeFromLeft(fitW));
    strip.removeFromLeft(8);
    mapSwitch.setBounds(strip.removeFromLeft(juce::jmin(mapSwitch.idealWidth(), strip.getWidth())));
}

void PianoRollEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::Path roundClip;
    roundClip.addRoundedRectangle(bounds, metrics::cornerRadius);
    g.reduceClipRegion(roundClip);

    g.setColour(colours::panel());
    g.fillRect(getLocalBounds());
    g.setColour(colours::line());
    g.fillRect(juce::Rectangle<int>(0, stripH - 1, getWidth(), 1));
    g.fillRect(juce::Rectangle<int>(0, stripH + toolbarH - 1, getWidth(), 1));

    if (hasClip)
    {
        // Prototype: single compact title line — "Name · Key · N bars"
        juce::String line = clip.name;
        juce::String meta;
        if (edit.root >= 0)
            meta << kNoteNames[(size_t) edit.root] << " " << modeName(edit.mode);
        else if (clip.root >= 0)
            meta << kNoteNames[(size_t) clip.root];
        if (meta.isNotEmpty())
            line << "  ·  " << meta;
        line << "  ·  " << resolved.bars << " bars";
        if (!editIsClean(edit))
            line << "  ·  edited";

        g.setColour(colours::text());
        g.setFont(uiFont(12.5f, true));
        g.drawText(line, juce::Rectangle<int>(12, 0, getWidth() - 52, stripH),
                   juce::Justification::centredLeft, true);
    }
}

void PianoRollEditor::paintOverChildren(juce::Graphics& g)
{
    g.setColour(colours::line());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),
                           metrics::cornerRadius, 0.5f);
}

// ── RollContent ──────────────────────────────────────────────────────────────

void PianoRollEditor::RollContent::paint(juce::Graphics& g)
{
    auto& ed = owner;

    g.fillAll(colours::rollBg());
    if (!ed.hasClip)
    {
        g.setColour(colours::text3());
        g.setFont(uiFont(14.0f, false));
        g.drawText("Select a MIDI file to edit", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float pps = ed.pxPerStep();
    const auto clipB = g.getClipBounds().toFloat();

    // Pitch rows: shade black-key rows; row lines.
    const int firstRow = juce::jmax(0, (int) std::floor(clipB.getY() / ed.effRowH()));
    const int lastRow = juce::jmin(ed.numRows() - 1, (int) std::ceil(clipB.getBottom() / ed.effRowH()));
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const float y = (float) row * ed.effRowH();
        const int pitch = ed.pitchForRow(row);
        if (isBlackKeyPitch(pitch))
        {
            g.setColour(colours::rollShade());
            g.fillRect(clipB.getX(), y, clipB.getWidth(), ed.effRowH());
        }
        if (pitch % 12 == 0)
        {
            g.setColour(colours::rollRowline());
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
        juce::Colour fill = (n.moved || inDrag) ? colours::accentBright() : colours::accent();
        g.setColour(fill.withAlpha((float) (0.35 + 0.6 * v)));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(colours::rollNoteEdge());
        g.drawRoundedRectangle(r, 3.0f, 0.8f);

        if (isSelected)
        {
            // Cupertino selection ring: bright blue outline.
            g.setColour(colours::accentBright());
            g.drawRoundedRectangle(r.expanded(1.5f), 3.5f, 1.5f);
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
        g.fillRect((float) (ed.playheadStep * ed.timeStretch) * pps, clipB.getY(), 1.5f, clipB.getHeight());
    }

    ed.paintLoopOverlay(g, clipB, clipB.getY(), clipB.getBottom());
}

void PianoRollEditor::RollContent::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;

    dragStart = e.position;
    dragDRows = 0;
    dragDStep = 0;

    if (e.mods.isMiddleButtonDown())
    {
        drag = Drag::Pan;
        panStartView = ed.rollViewport.getViewPosition();
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (const int handle = ed.hitLoopHandle(e.position.x); handle >= 0)
    {
        drag = handle == 0 ? Drag::LoopStart : Drag::LoopEnd;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

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
        ed.velocityLane.repaint();
        return;
    }

    drag = Drag::Marquee;
    marqueeAdditive = e.mods.isShiftDown();
    marquee = juce::Rectangle<float>(dragStart, dragStart);
    if (!marqueeAdditive)
        ed.selection.clear();
    ed.refreshControls();
    repaint();
}

void PianoRollEditor::RollContent::mouseDrag(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (drag == Drag::Pan)
    {
        const auto d = e.getOffsetFromDragStart();
        ed.rollViewport.setViewPosition(panStartView.x - d.x, panStartView.y - d.y);
        return;
    }
    if (drag == Drag::LoopStart || drag == Drag::LoopEnd)
    {
        const double step = ed.stepFromContentX(e.position.x);
        if (drag == Drag::LoopStart)
            ed.setLoopSteps(step, ed.loopEndStep, false);
        else
            ed.setLoopSteps(ed.loopStartStep, step, false);
        return;
    }
    if (drag == Drag::Note)
    {
        const float dx = e.position.x - dragStart.x;
        const float dy = e.position.y - dragStart.y;
        const int div = juce::jmax(1, ed.divisionSteps);
        // Account for time-stretch so drag distance matches visual grid.
        const float stepPx = ed.pxPerStep() * (float) ed.timeStretch;
        const int stepsMoved = (int) std::round(dx / juce::jmax(0.01f, stepPx) / (float) div) * div;
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
    if (drag == Drag::LoopStart || drag == Drag::LoopEnd)
    {
        const double div = (double) juce::jmax(1, ed.divisionSteps);
        auto snap = [div](double s) { return std::round(s / div) * div; };
        if (drag == Drag::LoopStart)
            ed.setLoopSteps(snap(ed.loopStartStep), ed.loopEndStep, true);
        else
            ed.setLoopSteps(ed.loopStartStep, snap(ed.loopEndStep), true);
    }
    else if (drag == Drag::Note)
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
    setMouseCursor(juce::MouseCursor::NormalCursor);
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

// ── TimeRuler ────────────────────────────────────────────────────────────────

void PianoRollEditor::TimeRuler::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromBottom(1));

    if (!ed.hasClip) return;

    const float viewX = (float) ed.rollViewport.getViewPositionX();
    ed.paintTimeRulerLabels(g, viewX, (float) getWidth(), (float) getHeight());
}

void PianoRollEditor::TimeRuler::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;
    const float contentX = e.position.x + (float) ed.rollViewport.getViewPositionX();
    if (const int handle = ed.hitLoopHandle(contentX, 8.0f); handle >= 0)
    {
        drag = handle == 0 ? Drag::LoopStart : Drag::LoopEnd;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
}

void PianoRollEditor::TimeRuler::mouseDrag(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (drag == Drag::None) return;
    const float contentX = e.position.x + (float) ed.rollViewport.getViewPositionX();
    const double step = ed.stepFromContentX(contentX);
    if (drag == Drag::LoopStart)
        ed.setLoopSteps(step, ed.loopEndStep, false);
    else
        ed.setLoopSteps(ed.loopStartStep, step, false);
}

void PianoRollEditor::TimeRuler::mouseUp(const juce::MouseEvent&)
{
    auto& ed = owner;
    if (drag == Drag::None) return;
    const double div = (double) juce::jmax(1, ed.divisionSteps);
    auto snap = [div](double s) { return std::round(s / div) * div; };
    if (drag == Drag::LoopStart)
        ed.setLoopSteps(snap(ed.loopStartStep), ed.loopEndStep, true);
    else
        ed.setLoopSteps(ed.loopStartStep, snap(ed.loopEndStep), true);
    drag = Drag::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
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
    header.removeFromLeft(4);
    g.setColour(colours::text3());
    g.setFont(uiFont(9.5f, true));
    g.drawText("VELOCITY", header, juce::Justification::centredLeft);

    if (!ed.hasClip || !ed.velocityOpen) return;

    auto lane = getLocalBounds().withTrimmedTop(headerH);
    const float pps = ed.pxPerStep();
    const float viewX = (float) ed.rollViewport.getViewPositionX();

    for (const auto& n : ed.grooved)
    {
        const float noteW = juce::jmax(4.0f, (float) (n.len * ed.timeStretch) * pps);
        const float x = (float) gutterW + ed.contentXFromStep(n.start) - viewX;
        if (x + noteW < (float) gutterW - 4.0f || x > (float) getWidth() + 4.0f) continue;
        const int vel = ed.velocityForNote(n);
        const float bh = juce::jmax(2.0f, (float) vel / 127.0f * (float) lane.getHeight() * 0.92f);
        g.setColour(colours::accent().withAlpha(0.85f));
        g.fillRect(x, (float) lane.getBottom() - bh, noteW, bh);
        g.setColour(colours::accent().brighter(0.15f));
        g.fillRect(x, (float) lane.getBottom() - bh, noteW, 2.0f);
    }
}

void PianoRollEditor::VelocityLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
    {
        owner.velocityOpen = !owner.velocityOpen;
        owner.resized();
        return;
    }
    if (!owner.hasClip || !owner.velocityOpen) return;
    if (const auto* n = owner.noteAtVelocityX(e.position.x))
    {
        dragNoteId = n->id;
        owner.setVelocityFromLaneY(dragNoteId, e.position.y);
    }
}

void PianoRollEditor::VelocityLane::mouseDrag(const juce::MouseEvent& e)
{
    if (dragNoteId >= 0)
        owner.setVelocityFromLaneY(dragNoteId, e.position.y);
}

void PianoRollEditor::VelocityLane::mouseUp(const juce::MouseEvent&)
{
    dragNoteId = -1;
}

// ── ScalePanel (Bottom placement) ────────────────────────────────────────────

int PianoRollEditor::ScalePanel::idealHeight() const
{
    return open ? headerH + bodyH : headerH;
}

void PianoRollEditor::ScalePanel::setOpen(bool shouldOpen)
{
    if (open == shouldOpen) return;
    open = shouldOpen;
    owner.resized();
}

bool PianoRollEditor::ScalePanel::hitTest(int x, int y)
{
    // Only the header captures clicks (fold toggle). The body lets sibling
    // scale controls (laid out on top) receive mouse input.
    juce::ignoreUnused(x);
    return y >= 0 && y < headerH;
}

void PianoRollEditor::ScalePanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
        setOpen(!open);
}

void PianoRollEditor::ScalePanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto header = getLocalBounds().removeFromTop(headerH).reduced(10, 0);
    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    drawIcon(g, open ? icons::caretDown : icons::caretUp, caret, colours::text3(), 1.6f);
    header.removeFromLeft(6);
    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText("SCALE", header, juce::Justification::centredLeft);
}

void PianoRollEditor::ScalePanel::resized()
{
    // Controls are siblings of this panel; PianoRollEditor::resized lays them out.
}

} // namespace pflow
