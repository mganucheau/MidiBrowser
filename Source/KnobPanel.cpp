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
    // Prototype feel: full range over ~170px of vertical travel.
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
    const float norm = juce::jlimit(0.0f, 1.0f,
        (float) (value - def.min) / (float) juce::jmax(1, def.max - def.min));
    const float zeroNorm = def.min < 0
        ? (float) (0 - def.min) / (float) (def.max - def.min) : 0.0f;
    auto rad = [](float deg) { return juce::degreesToRadians(deg); };
    const float angle = rad(startDeg + norm * (endDeg - startDeg));
    const float zeroAngle = rad(startDeg + zeroNorm * (endDeg - startDeg));

    juce::PathStrokeType st(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Track
    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, rad(startDeg), rad(endDeg), true);
    g.setColour(colours::lineStrong());
    g.strokePath(track, st);

    // Value arc: unipolar fills from start, bipolar from center.
    const float lo = juce::jmin(zeroAngle, angle), hi = juce::jmax(zeroAngle, angle);
    if (hi - lo > 0.012f)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, r, r, 0.0f, lo, hi, true);
        g.setColour(colours::accent());
        g.strokePath(arc, st);
    }

    // Body + pointer
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

    // Label + readout
    auto below = getLocalBounds().withTrimmedTop((int) size + 6);
    g.setColour(colours::text3());
    g.setFont(uiFont(9.5f, true));
    g.drawText(juce::String(def.label).toUpperCase(), below.removeFromTop(12),
               juce::Justification::centred);
    const bool activeVal = value != def.def;
    g.setColour(activeVal ? colours::accent() : colours::text2());
    g.setFont(monoFont(11.5f, true));
    g.drawText(juce::String(value) + "%", below.removeFromTop(14), juce::Justification::centred);
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
        addAndMakeVisible(*knobs[(size_t) i]);
    }

    btnReset.onClick = [this]
    {
        setParams({}, juce::dontSendNotification);
        pushParams();
    };
    addAndMakeVisible(btnReset);
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
    btnReset.setVisible(open && params.activeCount() > 0);
    if (onOpenChanged)
        onOpenChanged();
}

int KnobsPanel::idealHeight() const
{
    if (!open)
        return headerH;
    const int perRow = juce::jmax(1, (getWidth() - 20) / GrooveKnob::totalW);
    const int rows = (kNumKnobs + perRow - 1) / perRow;
    return headerH + rows * (GrooveKnob::totalH + 8) + 8;
}

void KnobsPanel::resized()
{
    auto header = getLocalBounds().removeFromTop(headerH);
    btnReset.setBounds(header.removeFromRight(30).withSizeKeepingCentre(24, 24));

    if (!open) return;
    // Wrapping row, space-around per row.
    auto area = getLocalBounds().withTrimmedTop(headerH + 2).reduced(10, 0);
    const int perRow = juce::jmax(1, area.getWidth() / GrooveKnob::totalW);
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int row = i / perRow;
        const int col = i % perRow;
        const int inRow = juce::jmin(perRow, kNumKnobs - row * perRow);
        const float slot = (float) area.getWidth() / (float) inRow;
        const int x = area.getX() + (int) (slot * ((float) col + 0.5f)) - GrooveKnob::totalW / 2;
        const int y = area.getY() + row * (GrooveKnob::totalH + 8);
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

    auto sl = header.removeFromLeft(14).toFloat().withSizeKeepingCentre(13.0f, 13.0f);
    drawIcon(g, icons::sliders, sl, colours::text2(), 1.4f);
    header.removeFromLeft(7);

    g.setColour(colours::text2());
    g.setFont(uiFont(11.0f, true));
    g.drawText("GROOVE", header.removeFromLeft(52), juce::Justification::centredLeft);

    const int active = params.activeCount();
    if (active > 0)
    {
        auto badge = header.removeFromLeft(20).withSizeKeepingCentre(16, 15);
        g.setColour(colours::accentSoft());
        g.fillRoundedRectangle(badge.toFloat(), 7.0f);
        g.setColour(colours::accentBright());
        g.setFont(monoFont(9.5f, true));
        g.drawText(juce::String(active), badge, juce::Justification::centred);
    }

    if (!open)
    {
        if (btnReset.isVisible())
            header.removeFromRight(28);
        g.setColour(colours::text3());
        g.setFont(monoFont(10.0f, false));
        juce::String names;
        for (int i = 0; i < kNumKnobs; ++i)
            names << kKnobDefs[(size_t) i].label << (i < kNumKnobs - 1 ? juce::String::fromUTF8(" · ") : juce::String());
        g.drawText(names, header, juce::Justification::centredRight, true);
    }
}

} // namespace pflow
