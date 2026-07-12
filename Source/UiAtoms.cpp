#include "UiAtoms.h"

namespace pflow {

// ── Icons ────────────────────────────────────────────────────────────────────

void drawIcon(juce::Graphics& g, const juce::String& name,
              juce::Rectangle<float> b, juce::Colour colour, float px)
{
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float s = juce::jmin(b.getWidth(), b.getHeight()) * 0.36f;
    g.setColour(colour);
    juce::PathStrokeType st(px, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;

    if (name == icons::play)
    {
        p.addTriangle(cx - s * 0.62f, cy - s, cx - s * 0.62f, cy + s, cx + s, cy);
        g.fillPath(p);
    }
    else if (name == icons::pause)
    {
        const float w = s * 0.55f;
        g.fillRoundedRectangle(cx - s * 0.8f, cy - s, w, s * 2.0f, px * 0.8f);
        g.fillRoundedRectangle(cx + s * 0.25f, cy - s, w, s * 2.0f, px * 0.8f);
    }
    else if (name == icons::stop)
    {
        g.fillRoundedRectangle(cx - s * 0.85f, cy - s * 0.85f, s * 1.7f, s * 1.7f, px);
    }
    else if (name == icons::caretUp || name == icons::caretDown)
    {
        const float dir = (name == icons::caretUp) ? -1.0f : 1.0f;
        p.startNewSubPath(cx - s * 0.8f, cy - dir * s * 0.4f);
        p.lineTo(cx, cy + dir * s * 0.4f);
        p.lineTo(cx + s * 0.8f, cy - dir * s * 0.4f);
        g.strokePath(p, st);
    }
    else if (name == icons::caretLeft || name == icons::caretRight)
    {
        const float dir = (name == icons::caretLeft) ? -1.0f : 1.0f;
        p.startNewSubPath(cx - dir * s * 0.4f, cy - s * 0.8f);
        p.lineTo(cx + dir * s * 0.4f, cy);
        p.lineTo(cx - dir * s * 0.4f, cy + s * 0.8f);
        g.strokePath(p, st);
    }
    else if (name == icons::arrowsIn || name == icons::arrowsOut)
    {
        // Center line + two horizontal arrows pointing in or out.
        g.drawLine(cx, cy - s, cx, cy + s, px);
        const bool in = (name == icons::arrowsIn);
        auto arrow = [&](float side)   // side -1 = left, +1 = right
        {
            const float x0 = cx + side * s * 1.35f;
            const float x1 = cx + side * s * 0.35f;
            g.drawLine(x0, cy, x1, cy, px);
            const float hx = in ? x1 : x0;
            const float hd = in ? side : -side;
            juce::Path h;
            h.startNewSubPath(hx + hd * s * 0.45f, cy - s * 0.45f);
            h.lineTo(hx, cy);
            h.lineTo(hx + hd * s * 0.45f, cy + s * 0.45f);
            g.strokePath(h, st);
        };
        arrow(-1.0f);
        arrow(1.0f);
    }
    else if (name == icons::scissors)
    {
        g.drawLine(cx - s * 0.15f, cy - s * 0.1f, cx + s, cy - s * 0.9f, px);
        g.drawLine(cx - s * 0.15f, cy + s * 0.1f, cx + s, cy + s * 0.9f, px);
        g.drawEllipse(cx - s * 1.05f, cy - s * 0.85f, s * 0.75f, s * 0.75f, px);
        g.drawEllipse(cx - s * 1.05f, cy + s * 0.1f, s * 0.75f, s * 0.75f, px);
    }
    else if (name == icons::sliders)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float y = cy - s + (float) i * s;
            g.drawLine(cx - s, y, cx + s, y, px);
            const float kx = cx + (i == 0 ? s * 0.4f : i == 1 ? -s * 0.35f : s * 0.05f);
            g.fillEllipse(kx - px, y - px, px * 2.0f, px * 2.0f);
        }
    }
    else if (name == icons::folder || name == icons::folderOpen)
    {
        p.startNewSubPath(cx - s, cy + s * 0.7f);
        p.lineTo(cx - s, cy - s * 0.55f);
        p.lineTo(cx - s * 0.25f, cy - s * 0.55f);
        p.lineTo(cx - s * 0.02f, cy - s * 0.25f);
        p.lineTo(cx + s, cy - s * 0.25f);
        p.lineTo(cx + s, cy + s * 0.7f);
        p.closeSubPath();
        g.strokePath(p, st);
    }
    else if (name == icons::star)
    {
        for (int i = 0; i < 10; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 10.0f
                            - juce::MathConstants<float>::halfPi;
            const float r = (i % 2 == 0) ? s : s * 0.45f;
            const float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        p.closeSubPath();
        g.strokePath(p, st);
    }
    else if (name == icons::plus)
    {
        g.drawLine(cx - s * 0.85f, cy, cx + s * 0.85f, cy, px);
        g.drawLine(cx, cy - s * 0.85f, cx, cy + s * 0.85f, px);
    }
    else if (name == icons::minus)
    {
        g.drawLine(cx - s * 0.85f, cy, cx + s * 0.85f, cy, px);
    }
    else if (name == icons::x)
    {
        g.drawLine(cx - s * 0.7f, cy - s * 0.7f, cx + s * 0.7f, cy + s * 0.7f, px);
        g.drawLine(cx - s * 0.7f, cy + s * 0.7f, cx + s * 0.7f, cy - s * 0.7f, px);
    }
    else if (name == icons::undo)
    {
        p.addCentredArc(cx, cy, s * 0.85f, s * 0.85f, 0.0f, 0.6f,
                        juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath(p, st);
        juce::Path h;
        const float ax = cx + s * 0.85f * std::sin(0.6f);
        const float ay = cy - s * 0.85f * std::cos(0.6f);
        h.startNewSubPath(ax - s * 0.5f, ay - s * 0.05f);
        h.lineTo(ax, ay);
        h.lineTo(ax + s * 0.15f, ay - s * 0.55f);
        g.strokePath(h, st);
    }
    else if (name == icons::zoomIn || name == icons::zoomOut)
    {
        g.drawEllipse(cx - s, cy - s, s * 1.5f, s * 1.5f, px);
        g.drawLine(cx + s * 0.15f, cy + s * 0.15f, cx + s * 0.95f, cy + s * 0.95f, px);
        const float mx = cx - s * 0.25f, my = cy - s * 0.25f;
        g.drawLine(mx - s * 0.4f, my, mx + s * 0.4f, my, px);
        if (name == icons::zoomIn)
            g.drawLine(mx, my - s * 0.4f, mx, my + s * 0.4f, px);
    }
    else if (name == icons::noteBass)
    {
        p.startNewSubPath(cx - s, cy);
        p.cubicTo(cx - s * 0.55f, cy - s * 1.4f, cx - s * 0.1f, cy + s * 1.4f, cx + s * 0.35f, cy);
        p.cubicTo(cx + s * 0.6f, cy - s * 0.8f, cx + s * 0.85f, cy + s * 0.8f, cx + s, cy);
        g.strokePath(p, st);
    }
    else if (name == icons::noteKeys)
    {
        auto r = juce::Rectangle<float>(cx - s, cy - s * 0.8f, s * 2.0f, s * 1.6f);
        g.drawRoundedRectangle(r, px, px);
        g.drawLine(cx - s * 0.33f, cy - s * 0.8f, cx - s * 0.33f, cy + s * 0.8f, px * 0.8f);
        g.drawLine(cx + s * 0.33f, cy - s * 0.8f, cx + s * 0.33f, cy + s * 0.8f, px * 0.8f);
        g.fillRect(cx - s * 0.5f, cy - s * 0.8f, s * 0.34f, s * 0.85f);
        g.fillRect(cx + s * 0.16f, cy - s * 0.8f, s * 0.34f, s * 0.85f);
    }
    else if (name == icons::noteDrums)
    {
        g.drawEllipse(cx - s, cy - s * 0.55f, s * 2.0f, s * 0.75f, px);
        g.drawLine(cx - s, cy - s * 0.18f, cx - s, cy + s * 0.6f, px);
        g.drawLine(cx + s, cy - s * 0.18f, cx + s, cy + s * 0.6f, px);
        juce::Path arc;
        arc.addCentredArc(cx, cy + s * 0.28f, s, s * 0.4f, 0.0f,
                          juce::MathConstants<float>::halfPi,
                          juce::MathConstants<float>::pi * 1.5f, true);
        g.strokePath(arc, st);
    }
    else if (name == icons::sidebar)
    {
        auto r = juce::Rectangle<float>(cx - s, cy - s * 0.8f, s * 2.0f, s * 1.6f);
        g.drawRoundedRectangle(r, px, px);
        g.drawLine(cx - s * 0.3f, cy - s * 0.8f, cx - s * 0.3f, cy + s * 0.8f, px);
    }
    else if (name == icons::lockOpen || name == icons::lockClosed)
    {
        const bool closed = (name == icons::lockClosed);
        auto body = juce::Rectangle<float>(cx - s * 0.75f, cy - s * 0.15f, s * 1.5f, s * 1.05f);
        g.drawRoundedRectangle(body, px, px);
        juce::Path shackle;
        const float top = closed ? cy - s * 0.95f : cy - s * 1.15f;
        shackle.startNewSubPath(cx - s * 0.45f, body.getY());
        shackle.lineTo(cx - s * 0.45f, top + s * 0.35f);
        shackle.addCentredArc(cx, top + s * 0.35f, s * 0.45f, s * 0.35f, 0.0f,
                              -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, false);
        if (closed)
            shackle.lineTo(cx + s * 0.45f, body.getY());
        else
            shackle.lineTo(cx + s * 0.45f, body.getY() - s * 0.35f);
        g.strokePath(shackle, st);
        g.fillEllipse(cx - px, cy + s * 0.3f, px * 2.0f, px * 2.0f);
    }
    else if (name == icons::foldRows)
    {
        // Rows collapsing together: three lines with arrows pointing inward.
        g.drawLine(cx - s, cy - s * 0.85f, cx + s, cy - s * 0.85f, px);
        g.drawLine(cx - s, cy + s * 0.85f, cx + s, cy + s * 0.85f, px);
        g.drawLine(cx - s, cy, cx + s, cy, px);
        juce::Path a1, a2;
        a1.startNewSubPath(cx - s * 0.3f, cy - s * 0.55f);
        a1.lineTo(cx, cy - s * 0.28f);
        a1.lineTo(cx + s * 0.3f, cy - s * 0.55f);
        a2.startNewSubPath(cx - s * 0.3f, cy + s * 0.55f);
        a2.lineTo(cx, cy + s * 0.28f);
        a2.lineTo(cx + s * 0.3f, cy + s * 0.55f);
        g.strokePath(a1, st);
        g.strokePath(a2, st);
    }
    else if (name == icons::infinity)
    {
        // Lemniscate (∞) for DAW sync.
        juce::Path lemni;
        lemni.startNewSubPath(cx - s, cy);
        lemni.cubicTo(cx - s, cy - s * 0.95f, cx - s * 0.15f, cy - s * 0.95f, cx, cy);
        lemni.cubicTo(cx + s * 0.15f, cy + s * 0.95f, cx + s, cy + s * 0.95f, cx + s, cy);
        lemni.cubicTo(cx + s, cy - s * 0.95f, cx + s * 0.15f, cy - s * 0.95f, cx, cy);
        lemni.cubicTo(cx - s * 0.15f, cy + s * 0.95f, cx - s, cy + s * 0.95f, cx - s, cy);
        g.strokePath(lemni, st);
    }
    else if (name == icons::search)
    {
        g.drawEllipse(cx - s * 0.55f, cy - s * 0.65f, s * 1.05f, s * 1.05f, px);
        g.drawLine(cx + s * 0.35f, cy + s * 0.45f, cx + s * 0.95f, cy + s * 1.05f, px * 1.2f);
    }
    else if (name == icons::moon)
    {
        // Crescent moon (appearance toggle).
        juce::Path disc, cut;
        disc.addEllipse(cx - s * 0.7f, cy - s * 0.7f, s * 1.4f, s * 1.4f);
        cut.addEllipse(cx - s * 0.15f, cy - s * 0.85f, s * 1.35f, s * 1.35f);
        disc.setUsingNonZeroWinding(false);
        disc.addPath(cut);
        g.fillPath(disc);
    }
    else if (name == icons::sun)
    {
        g.drawEllipse(cx - s * 0.4f, cy - s * 0.4f, s * 0.8f, s * 0.8f, px);
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
            g.drawLine(cx + s * 0.55f * std::cos(a), cy + s * 0.55f * std::sin(a),
                       cx + s * 0.9f * std::cos(a), cy + s * 0.9f * std::sin(a), px);
        }
    }
    else if (name == icons::gear)
    {
        g.drawEllipse(cx - s * 0.35f, cy - s * 0.35f, s * 0.7f, s * 0.7f, px);
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
            g.drawLine(cx + s * 0.6f * std::cos(a), cy + s * 0.6f * std::sin(a),
                       cx + s * 0.95f * std::cos(a), cy + s * 0.95f * std::sin(a), px);
        }
    }
}

// ── IconBtn ──────────────────────────────────────────────────────────────────

IconBtn::IconBtn(const juce::String& iconName, const juce::String& tip)
    : juce::Button(iconName), icon(iconName)
{
    if (tip.isNotEmpty())
        setTooltip(tip);
    setWantsKeyboardFocus(true);
}

void IconBtn::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    if (active)
    {
        g.setColour(colours::accent().withAlpha(down ? 0.85f : 1.0f));
        g.fillRoundedRectangle(b, metrics::chipRadius);
    }
    else if (down || (over && !ghost) || (over && ghost))
    {
        g.setColour(down ? colours::elev().brighter(0.08f) : colours::elev());
        g.fillRoundedRectangle(b, metrics::chipRadius);
    }
    const juce::Colour col = active ? colours::accentInk()
                           : isEnabled() ? (over ? colours::text() : colours::text2())
                                         : colours::text2();
    const float inset = juce::jmin(b.getWidth(), b.getHeight()) * (0.5f - 0.32f * iconScale);
    drawIcon(g, icon, b.reduced(inset), col, 1.6f);
    if (!isEnabled())
    {
        // Dim via fill overlay so glyph contrast stays ≥ AA on elev.
        g.setColour(colours::elev().withAlpha(0.35f));
        g.fillRoundedRectangle(b, metrics::chipRadius);
    }
    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, metrics::chipRadius);
}

// ── ChipBtn ──────────────────────────────────────────────────────────────────

ChipBtn::ChipBtn(const juce::String& text, const juce::String& iconName)
    : juce::Button(text), label(text), icon(iconName)
{
    setWantsKeyboardFocus(true);
}

int ChipBtn::idealWidth() const
{
    const auto f = mono ? monoFont(13.0f, true) : uiFont(13.0f, true);
    int w = (int) std::ceil(juce::GlyphArrangement::getStringWidth(f, label)) + 26;
    if (icon.isNotEmpty()) w += 16;
    if (trailingIcon.isNotEmpty()) w += 15;
    return w;
}

void ChipBtn::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float r = b.getHeight() * 0.5f;

    juce::Colour fill = active ? colours::accent() : colours::elev();
    if (!isEnabled()) fill = fill.withAlpha(0.5f);
    else if (down) fill = fill.darker(0.12f);
    else if (over) fill = fill.brighter(0.06f);
    g.setColour(fill);
    g.fillRoundedRectangle(b, r);
    if (!active)
    {
        g.setColour(accentText ? colours::accentLine() : colours::line());
        g.drawRoundedRectangle(b, r, 1.0f);
    }

    juce::Colour col = active ? colours::accentInk()
                     : accentText ? colours::accent()
                                  : colours::text2();
    if (!isEnabled())
        col = colours::text2();

    auto area = getLocalBounds().reduced(9, 0);
    if (icon.isNotEmpty())
    {
        auto ia = area.removeFromLeft(13).toFloat().withSizeKeepingCentre(13.0f, 13.0f);
        drawIcon(g, icon, ia, col, 1.4f);
        area.removeFromLeft(3);
    }
    if (trailingIcon.isNotEmpty())
    {
        auto ta = area.removeFromRight(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, trailingIcon, ta, col.withAlpha(over ? 1.0f : 0.85f), 1.3f);
    }
    g.setColour(col);
    g.setFont(mono ? monoFont(13.0f, true) : uiFont(13.0f, true));
    g.drawFittedText(label, area, juce::Justification::centred, 1, 1.0f);
    if (!isEnabled())
    {
        g.setColour(colours::elev().withAlpha(0.35f));
        g.fillRoundedRectangle(b, r);
    }
    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, r);
}

// ── PillToggle ───────────────────────────────────────────────────────────────

PillToggle::PillToggle(const juce::String& onText, const juce::String& offText)
    : juce::Button("sync"), onLabel(onText), offLabel(offText)
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

int PillToggle::idealWidth() const
{
    const auto f = uiFont(13.0f, true);
    const float w = juce::jmax(juce::GlyphArrangement::getStringWidth(f, onLabel),
                               juce::GlyphArrangement::getStringWidth(f, offLabel));
    return (int) std::ceil(w) + 34;
}

void PillToggle::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float r = b.getHeight() * 0.5f;
    const bool on = getToggleState();

    juce::Colour fill = on ? colours::accentSoft() : colours::elev();
    if (down) fill = fill.darker(0.1f);
    else if (over) fill = fill.brighter(0.05f);
    g.setColour(fill);
    g.fillRoundedRectangle(b, r);
    g.setColour(on ? colours::accentLine() : colours::line());
    g.drawRoundedRectangle(b, r, 1.0f);

    auto area = getLocalBounds().reduced(10, 0);
    auto dot = area.removeFromLeft(8).toFloat().withSizeKeepingCentre(6.0f, 6.0f);
    g.setColour(on ? colours::accent() : colours::text3());
    g.fillEllipse(dot);
    area.removeFromLeft(6);

    g.setColour(on ? colours::accentBright() : colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawFittedText(on ? onLabel : offLabel, area, juce::Justification::centredLeft, 1, 1.0f);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, r);
}

// ── MiniSwitch ───────────────────────────────────────────────────────────────

MiniSwitch::MiniSwitch(const juce::String& c) : juce::Button(c), caption(c)
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

int MiniSwitch::idealWidth() const
{
    const float capW = juce::GlyphArrangement::getStringWidth(uiFont(14.0f, true), caption);
    const float valW = juce::jmax(
        juce::GlyphArrangement::getStringWidth(monoFont(13.0f, false), onText),
        juce::GlyphArrangement::getStringWidth(monoFont(13.0f, false), offText));
    return (int) std::ceil(capW + valW) + 26 + 18;   // track + gaps
}

void MiniSwitch::paintButton(juce::Graphics& g, bool over, bool down)
{
    juce::ignoreUnused(down);
    const bool on = getToggleState();
    auto row = getLocalBounds();
    auto bounds = getLocalBounds().toFloat();

    // Caption · track · value, all on one row.
    g.setColour(on ? colours::text() : colours::text2());
    g.setFont(uiFont(14.0f, true));
    const int capW = (int) std::ceil(
        juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), caption));
    g.drawFittedText(caption, row.removeFromLeft(capW), juce::Justification::centredLeft, 1, 1.0f);
    row.removeFromLeft(7);

    auto track = row.removeFromLeft(26).toFloat().withSizeKeepingCentre(24.0f, 14.0f);
    g.setColour(on ? colours::accent() : (over ? colours::elev().brighter(0.1f) : colours::elev()));
    g.fillRoundedRectangle(track, 7.0f);
    g.setColour(on ? colours::accentInk() : colours::text3());
    const float kx = on ? track.getRight() - 12.0f : track.getX() + 2.0f;
    g.fillEllipse(kx, track.getCentreY() - 5.0f, 10.0f, 10.0f);

    row.removeFromLeft(6);
    g.setColour(on ? colours::accentBright() : colours::text3());
    g.setFont(monoFont(13.0f, true));
    g.drawFittedText(on ? onText : offText, row, juce::Justification::centredLeft, 1, 1.0f);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, bounds, 6.0f);
}

// ── Stepper ──────────────────────────────────────────────────────────────────

Stepper::Stepper()
{
    btnDown.ghost = false;
    btnUp.ghost = false;
    btnDown.onClick = [this] { setValue(value - 1); };
    btnUp.onClick = [this] { setValue(value + 1); };
    addAndMakeVisible(btnDown);
    addAndMakeVisible(btnUp);
}

void Stepper::setValue(int v, juce::NotificationType notify)
{
    v = juce::jlimit(minValue, maxValue, v);
    if (v == value) return;
    value = v;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(value);
}

void Stepper::resized()
{
    auto b = getLocalBounds();
    btnDown.setBounds(b.removeFromLeft(b.getHeight()));
    btnUp.setBounds(b.removeFromRight(b.getHeight()));
}

void Stepper::paint(juce::Graphics& g)
{
    auto mid = getLocalBounds().withTrimmedLeft(getHeight()).withTrimmedRight(getHeight());
    g.setColour(colours::elev());
    g.fillRoundedRectangle(mid.toFloat().reduced(1.0f, 2.0f), 4.0f);
    g.setColour(value != 0 ? colours::accent() : colours::text2());
    g.setFont(monoFont(14.0f, true));
    const juce::String text = format ? format(value)
                                     : (value > 0 ? "+" + juce::String(value) : juce::String(value));
    g.drawFittedText(text, mid, juce::Justification::centred, 1, 1.0f);
}

} // namespace pflow
