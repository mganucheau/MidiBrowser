#include "KnobPanel.h"

namespace pflow {

// ── GrooveKnob ───────────────────────────────────────────────────────────────

GrooveKnob::GrooveKnob(const KnobDef& d) : def(d), value(d.def)
{
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setTitle(def.label);
}

void GrooveKnob::setValue(int v, juce::NotificationType notify)
{
    v = juce::jlimit(def.min, def.max, v);
    if (v == value) return;
    value = v;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(value);
}

void GrooveKnob::mouseDown(const juce::MouseEvent& e)
{
    dragStartValue = value;
    dragStartY = e.position.y;
    grabKeyboardFocus();
}

void GrooveKnob::mouseDrag(const juce::MouseEvent& e)
{
    const float dy = dragStartY - e.position.y;
    const float range = (float) (def.max - def.min);
    setValue(dragStartValue + (int) std::round((dy / 170.0f) * range));
}

void GrooveKnob::mouseUp(const juce::MouseEvent&) {}

void GrooveKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    setValue(def.def);
}

bool GrooveKnob::keyPressed(const juce::KeyPress& key)
{
    const int step = juce::jmax(1, (def.max - def.min) / 100);
    if (key == juce::KeyPress::upKey)    { setValue(value + step); return true; }
    if (key == juce::KeyPress::downKey)  { setValue(value - step); return true; }
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::backspaceKey)
    {
        setValue(def.def);
        return true;
    }
    return false;
}

void GrooveKnob::paint(juce::Graphics& g)
{
    const float size = (float) knobSize;
    const float cx = (float) getWidth() * 0.5f;
    const float cy = size * 0.5f + 2.0f;
    const float r = size * 0.5f - 5.0f;

    constexpr float startDeg = -135.0f, endDeg = 135.0f;
    const float span = (float) juce::jmax(1, def.max - def.min);
    const float norm = juce::jlimit(0.0f, 1.0f, (float) (value - def.min) / span);

    float originNorm = 0.0f;
    if (def.bipolar())
        originNorm = (float) (0 - def.min) / span;
    else if (def.fillFromDefault())
        originNorm = (float) (def.def - def.min) / span;

    auto rad = [](float deg) { return juce::degreesToRadians(deg); };
    const float angle = rad(startDeg + norm * (endDeg - startDeg));
    const float originAngle = rad(startDeg + originNorm * (endDeg - startDeg));

    juce::PathStrokeType st(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, rad(startDeg), rad(endDeg), true);
    g.setColour(colours::lineStrong());
    g.strokePath(track, st);

    const float lo = juce::jmin(originAngle, angle), hi = juce::jmax(originAngle, angle);
    if (hi - lo > 0.012f)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, r, r, 0.0f, lo, hi, true);
        g.setColour(colours::accent());
        g.strokePath(arc, st);
    }

    g.setColour(colours::elev());
    g.fillEllipse(cx - (r - 3.0f), cy - (r - 3.0f), (r - 3.0f) * 2.0f, (r - 3.0f) * 2.0f);
    g.setColour(colours::line());
    g.drawEllipse(cx - (r - 3.0f), cy - (r - 3.0f), (r - 3.0f) * 2.0f, (r - 3.0f) * 2.0f, 1.0f);

    const float pr = r - 3.5f;
    g.setColour(colours::accentBright());
    g.drawLine(cx, cy, cx + pr * std::sin(angle), cy - pr * std::cos(angle), 2.0f);
    g.setColour(colours::text3());
    g.fillEllipse(cx - 1.6f, cy - 1.6f, 3.2f, 3.2f);

    if (hasKeyboardFocus(true))
    {
        g.setColour(colours::accentLine());
        g.drawEllipse(cx - r - 3.0f, cy - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, 1.0f);
    }

    auto below = getLocalBounds().withTrimmedTop((int) size + 6);
    g.setColour(colours::text3());
    g.setFont(uiFont(12.0f, true));
    g.drawText(juce::String(def.label).toUpperCase(), below.removeFromTop(14),
               juce::Justification::centred);
    const bool activeVal = value != def.def;
    g.setColour(activeVal ? colours::accent() : colours::text2());
    g.setFont(monoFont(13.0f, true));
    g.drawText(grooveValueText(def, value), below.removeFromTop(14), juce::Justification::centred);
}

// ── KnobsPanel ───────────────────────────────────────────────────────────────

KnobsPanel::KnobsPanel()
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        knobs[(size_t) i] = std::make_unique<GrooveKnob>(kKnobDefs[(size_t) i]);
        knobs[(size_t) i]->onChange = [this, i](int v)
        {
            params.set(i, v);
            pushParams();
        };
        addChildComponent(*knobs[(size_t) i]);   // hidden until expanded
    }

    btnReset.onClick = [this]
    {
        setParams({}, juce::dontSendNotification);
        pushParams();
    };
    addChildComponent(btnReset);

    btnSwing8.mono = true;
    btnSwing16.mono = true;
    btnSwing8.setTooltip("Swing base: delay offbeat 8ths (Ableton Base 1/8)");
    btnSwing16.setTooltip("Swing base: delay offbeat 16ths (Ableton Base 1/16)");
    btnSwing8.onClick = [this]
    {
        params.swingBase = SwingBase::Eighth;
        syncOptionButtons();
        pushParams();
    };
    btnSwing16.onClick = [this]
    {
        params.swingBase = SwingBase::Sixteenth;
        syncOptionButtons();
        pushParams();
    };
    addChildComponent(btnSwing8);
    addChildComponent(btnSwing16);

    syncOptionButtons();
}

void KnobsPanel::syncOptionButtons()
{
    btnSwing8.active = (params.swingBase == SwingBase::Eighth);
    btnSwing16.active = (params.swingBase == SwingBase::Sixteenth);
    btnSwing8.repaint();
    btnSwing16.repaint();
}

void KnobsPanel::pushParams()
{
    btnReset.setVisible(open && params.activeCount() > 0);
    repaint();
    if (onParamsChanged)
        onParamsChanged(params);
}

void KnobsPanel::setParams(const GrooveParams& p, juce::NotificationType notify)
{
    params = p;
    for (int i = 0; i < kNumKnobs; ++i)
        knobs[(size_t) i]->setValue(params.get(i), juce::dontSendNotification);
    syncOptionButtons();
    btnReset.setVisible(open && params.activeCount() > 0);
    repaint();
    if (notify != juce::dontSendNotification && onParamsChanged)
        onParamsChanged(params);
}

void KnobsPanel::setOpen(bool shouldOpen)
{
    if (open == shouldOpen) return;
    open = shouldOpen;
    for (auto& k : knobs)
        k->setVisible(open);
    btnSwing8.setVisible(open);
    btnSwing16.setVisible(open);
    btnReset.setVisible(open && params.activeCount() > 0);
    if (onOpenChanged)
        onOpenChanged();
}

int KnobsPanel::idealHeight() const
{
    if (!open)
        return headerH;
    // Swing block is wider than a single knob; remaining five wrap beside/below.
    const int avail = juce::jmax(1, getWidth() - 20 - swingBlockWidth());
    const int perRow = juce::jmax(1, avail / GrooveKnob::totalW);
    const int rows = 1 + (5 + perRow - 1) / perRow;   // swing row + wrapped rest
    return headerH + rows * (GrooveKnob::totalH + 8) + 8;
}

void KnobsPanel::resized()
{
    auto header = getLocalBounds().removeFromTop(headerH);
    btnReset.setBounds(header.removeFromRight(30).withSizeKeepingCentre(24, 24));

    if (!open)
    {
        for (auto& k : knobs) k->setVisible(false);
        btnSwing8.setVisible(false);
        btnSwing16.setVisible(false);
        return;
    }

    for (auto& k : knobs) k->setVisible(true);
    btnSwing8.setVisible(true);
    btnSwing16.setVisible(true);

    auto area = getLocalBounds().withTrimmedTop(headerH + 2).reduced(10, 0);
    const int blockW = swingBlockWidth();
    const int y0 = area.getY();

    // Swing knob + stacked 1/8 · 1/16 to its right
    knobs[0]->setBounds(area.getX(), y0, GrooveKnob::totalW, GrooveKnob::totalH);
    const int chipX = area.getX() + GrooveKnob::totalW + swingBlockGap;
    const int stackH = swingChipH * 2 + swingChipGap;
    const int chipY = y0 + (GrooveKnob::knobSize - stackH) / 2 + 2;
    btnSwing8.setBounds(chipX, chipY, swingChipW, swingChipH);
    btnSwing16.setBounds(chipX, chipY + swingChipH + swingChipGap, swingChipW, swingChipH);

    // Remaining knobs (1..5) wrap in the space to the right of the swing block
    auto rest = area.withTrimmedLeft(blockW + 8);
    const int perRow = juce::jmax(1, rest.getWidth() / GrooveKnob::totalW);
    for (int i = 1; i < kNumKnobs; ++i)
    {
        const int idx = i - 1;
        const int row = idx / perRow;
        const int col = idx % perRow;
        const int inRow = juce::jmin(perRow, (kNumKnobs - 1) - row * perRow);
        const float slot = (float) rest.getWidth() / (float) inRow;
        const int x = rest.getX() + (int) (slot * ((float) col + 0.5f)) - GrooveKnob::totalW / 2;
        const int y = rest.getY() + row * (GrooveKnob::totalH + 8);
        knobs[(size_t) i]->setBounds(x, y, GrooveKnob::totalW, GrooveKnob::totalH);
    }
}

void KnobsPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
        setOpen(!open);
}

void KnobsPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto header = getLocalBounds().removeFromTop(headerH).reduced(10, 0);

    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    drawIcon(g, open ? icons::caretDown : icons::caretUp, caret, colours::text3(), 1.6f);
    header.removeFromLeft(6);

    // Sliders icon only when collapsed — expanded view is title-only.
    if (!open)
    {
        auto sl = header.removeFromLeft(14).toFloat().withSizeKeepingCentre(13.0f, 13.0f);
        drawIcon(g, icons::sliders, sl, colours::text2(), 1.4f);
        header.removeFromLeft(7);
    }

    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText("GROOVE", header.removeFromLeft(64), juce::Justification::centredLeft);

    const int active = params.activeCount();
    if (active > 0)
    {
        auto badge = header.removeFromLeft(20).withSizeKeepingCentre(16, 15);
        g.setColour(colours::accentSoft());
        g.fillRoundedRectangle(badge.toFloat(), 7.0f);
        g.setColour(colours::accentBright());
        g.setFont(monoFont(12.0f, true));
        g.drawText(juce::String(active), badge, juce::Justification::centred);
    }

    if (!open)
    {
        if (btnReset.isVisible())
            header.removeFromRight(28);
        g.setColour(colours::text3());
        g.setFont(monoFont(12.0f, false));
        juce::String names;
        for (int i = 0; i < kNumKnobs; ++i)
            names << kKnobDefs[(size_t) i].label
                  << (i < kNumKnobs - 1 ? juce::String::fromUTF8(" · ") : juce::String());
        g.drawText(names, header, juce::Justification::centredRight, true);
    }
}

} // namespace pflow
