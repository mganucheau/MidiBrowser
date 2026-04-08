#include "Theme.h"
#include <unordered_map>

namespace pflow {

namespace {

// ── Animated state transitions (M3 motion-inspired) ──────────────────────────
// Lightweight global animator: stores per-component animated values in Properties
// and repaints components until they converge.
class M3Animator : private juce::Timer
{
public:
    static M3Animator& instance()
    {
        static M3Animator a;
        return a;
    }

    void watch(juce::Component& c)
    {
        if (watching.contains(&c))
            return;
        watching.add(&c);
        startTimerHz(60);
    }

private:
    void timerCallback() override
    {
        for (int i = watching.size(); --i >= 0;)
        {
            auto* c = watching.getUnchecked(i);
            if (c == nullptr)
            {
                watching.remove(i);
                continue;
            }
            if (!c->isShowing())
            {
                watching.remove(i);
                continue;
            }
            c->repaint();
        }
        if (watching.isEmpty())
            stopTimer();
    }

    juce::Array<juce::Component*> watching;
};

static const juce::Identifier kM3Alpha("m3_alpha");
static const juce::Identifier kM3LastMs("m3_last_ms");

float animatedAlpha(juce::Component& c, float target, float tauMs)
{
    auto& props = c.getProperties();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double last = props.contains(kM3LastMs) ? (double)props[kM3LastMs] : now;
    const float cur = props.contains(kM3Alpha) ? (float)props[kM3Alpha] : 0.0f;

    const float dt = (float)juce::jlimit(0.0, 100.0, now - last);
    props.set(kM3LastMs, now);

    // 1st-order low-pass towards target
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    float next = cur + (target - cur) * k;
    if (std::abs(next - target) < 0.002f)
        next = target;
    props.set(kM3Alpha, next);

    if (next != target)
        M3Animator::instance().watch(c);
    return next;
}

// Same smoothing, but keyed for non-Component draw calls (e.g. PopupMenu items).
struct KeyAnimState { float v = 0.0f; double lastMs = 0.0; };
float animatedAlphaForKey(juce::int64 key, float target, float tauMs)
{
    static std::unordered_map<juce::int64, KeyAnimState> states;
    static double lastSweepMs = 0.0;

    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& st = states[key];
    if (st.lastMs <= 0.0) st.lastMs = now;

    const float dt = (float)juce::jlimit(0.0, 100.0, now - st.lastMs);
    st.lastMs = now;
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    st.v = st.v + (target - st.v) * k;
    if (std::abs(st.v - target) < 0.002f) st.v = target;

    // Occasional sweep of converged/old entries
    if (now - lastSweepMs > 2000.0)
    {
        lastSweepMs = now;
        for (auto it = states.begin(); it != states.end();)
        {
            if (now - it->second.lastMs > 4000.0 && std::abs(it->second.v - target) < 0.002f)
                it = states.erase(it);
            else
                ++it;
        }
    }
    return st.v;
}

// Material 3 "state layer" opacities (approx)
// hover 0.08, focus 0.12, pressed 0.12, dragged 0.16
float stateLayerAlpha(bool highlighted, bool down, bool focused)
{
    if (down) return 0.12f;
    if (focused) return 0.12f;
    if (highlighted) return 0.08f;
    return 0.0f;
}

juce::Colour stateLayerColourOnSurface()
{
    // Use onSurface as the state-layer ink (M3 spec uses onSurface/onPrimary depending on container)
    return colours::text().withAlpha(1.0f);
}

juce::Colour stateLayerColourOnPrimary()
{
    return currentThemeTokens().onPrimary.withAlpha(1.0f);
}

juce::Colour shadowColourForSurface()
{
    // Softer shadows for dark; slightly stronger for light.
    return juce::Colours::black.withAlpha(currentThemeTokens().isDark ? 0.35f : 0.22f);
}

void drawElevationShadow(juce::Graphics& g, juce::Rectangle<float> bounds, float r, int dp)
{
    if (dp <= 0) return;
    // Approximate dp -> radius/offset.
    const int radius = juce::jlimit(2, 18, 2 + dp * 2);
    const int offsetY = juce::jlimit(1, 10, 1 + dp);

    juce::Path p;
    p.addRoundedRectangle(bounds.translated(0.0f, (float)offsetY), r);

    juce::DropShadow ds(shadowColourForSurface(), radius, { 0, offsetY });
    ds.drawForPath(g, p);
}

} // namespace

PatternFlowLookAndFeel::PatternFlowLookAndFeel()
{
    refreshColours();
}

void PatternFlowLookAndFeel::refreshColours()
{
    setColour(juce::ResizableWindow::backgroundColourId,  colours::bg());
    setColour(juce::TextEditor::backgroundColourId,       colours::bg());
    setColour(juce::TextEditor::textColourId,             colours::text());
    setColour(juce::TextEditor::outlineColourId,          colours::knobTrack());
    setColour(juce::ComboBox::backgroundColourId,         colours::bg());
    setColour(juce::ComboBox::textColourId,               juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId,            colours::knobTrack());
    setColour(juce::ComboBox::arrowColourId,              colours::text());
    setColour(juce::PopupMenu::backgroundColourId,        colours::bgLight());
    setColour(juce::PopupMenu::textColourId,              juce::Colours::white);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent());
    setColour(juce::PopupMenu::highlightedTextColourId,   juce::Colours::white);
    setColour(juce::ScrollBar::thumbColourId,             colours::knobTrack());
    setColour(juce::ListBox::backgroundColourId,          colours::panel());
    setColour(juce::ListBox::textColourId,                colours::text());
    setColour(juce::Label::textColourId,                  colours::text());
    setColour(juce::ToggleButton::textColourId,           colours::text());
    setColour(juce::ToggleButton::tickColourId,           colours::accent());
    setColour(juce::ToggleButton::tickDisabledColourId,   colours::textDim());
    setColour(juce::TextButton::buttonColourId,           colours::bgLighter());
    setColour(juce::TextButton::textColourOffId,          colours::text());
    setColour(juce::TextButton::textColourOnId,           colours::textBright());

    // File tree / TreeView colours
    setColour(juce::TreeView::backgroundColourId,         colours::panel());
    setColour(juce::TreeView::linesColourId,              colours::panelBorder());
    // Base selection colour; animation is handled in TreeViewItem paint via repeated repaints,
    // but we keep this value conservative for balanced colour usage.
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::accent().withAlpha(0.14f));
    setColour(juce::TreeView::dragAndDropIndicatorColourId, colours::accent());

    // DirectoryContentsDisplayComponent (file browser text)
    setColour(juce::DirectoryContentsDisplayComponent::textColourId,    colours::text());
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);

    // Tooltip
    setColour(juce::TooltipWindow::backgroundColourId,    colours::bgLight());
    setColour(juce::TooltipWindow::textColourId,          colours::text());
    setColour(juce::TooltipWindow::outlineColourId,       colours::panelBorder());
}

void PatternFlowLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                               int w, int h, float sliderPos,
                                               float startAngle, float endAngle,
                                               juce::Slider&)
{
    auto radius  = (float)juce::jmin(w, h) * 0.38f;
    auto centreX = (float)x + (float)w * 0.5f;
    auto centreY = (float)y + (float)h * 0.5f;
    auto angle   = startAngle + sliderPos * (endAngle - startAngle);

    // Knob body (filled circle with subtle gradient feel)
    auto bodyRadius = radius - 2.0f;
    g.setColour(colours::bgLighter());
    g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(colours::panelBorder());
    g.drawEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    // Track arc (background)
    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(colours::knobTrack());
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    if (sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                               startAngle, angle, true);
        g.setColour(colours::accent());
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer indicator (dot on knob edge instead of line from center)
    float dotDist = bodyRadius * 0.65f;
    float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
    float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
    g.setColour(colours::textBright());
    g.fillEllipse(px - 3.0f, py - 3.0f, 6.0f, 6.0f);
}

void PatternFlowLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                    juce::Button& btn,
                                                    const juce::Colour&,
                                                    bool highlighted, bool down)
{
    if (btn.getComponentID() == "Settings")
        return;
    auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
    const float r = metrics::cornerRadius;

    juce::Colour baseColour = colours::bgLight();
    juce::Colour outlineCol = colours::panelBorder();
    juce::Colour ink = stateLayerColourOnSurface();

    const bool focused = btn.hasKeyboardFocus(true);

    int elevationDp = 0;
    if (btn.getComponentID() == "ActivePill")
    {
        baseColour = down ? colours::accentDim()
                  : (highlighted ? colours::accentBright() : colours::accent());
        outlineCol = juce::Colours::transparentBlack;
        ink = stateLayerColourOnPrimary();
        elevationDp = (down ? 1 : (highlighted ? 2 : 1));
    }
    else if (btn.getComponentID() == "ActionButton")
    {
        // Tonal button (primaryContainer)
        baseColour = colours::accentDim();
        outlineCol = juce::Colours::transparentBlack;
        ink = colours::accentBright();
        elevationDp = (down ? 0 : (highlighted ? 1 : 0));
    }
    else
    {
        // Outlined button
        baseColour = colours::bgLight();
        outlineCol = highlighted ? colours::borderHover() : colours::panelBorder();
    }

    // Elevation shadow + surface tint (dark theme)
    if (elevationDp > 0)
        drawElevationShadow(g, bounds, r, elevationDp);

    baseColour = elevatedSurface(baseColour, elevationDp);
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, r);

    const float targetA = stateLayerAlpha(highlighted, down, focused);
    const float a = animatedAlpha(btn, targetA, 110.0f);
    if (a > 0.0f)
    {
        g.setColour(ink.withAlpha(a));
        g.fillRoundedRectangle(bounds, r);
    }

    if (outlineCol != juce::Colours::transparentBlack)
    {
        g.setColour(outlineCol.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, r, focused ? 1.6f : 1.0f);
    }
}

void PatternFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                     juce::jmax(1, (int)((float)textArea.getHeight() / g.getCurrentFont().getHeight())),
                     juce::jmax(label.getMinimumHorizontalScale(), 1.0f));
}

juce::Font PatternFlowLookAndFeel::getLabelFont(juce::Label&)
{
    return fontFor(TextStyle::BodyMedium);
}

juce::Font PatternFlowLookAndFeel::getTextButtonFont(juce::TextButton& btn, int)
{
    if (btn.getComponentID() == "Settings")
        return fontFor(TextStyle::DisplaySmall);
    if (btn.getComponentID() == "ActivePill" || btn.getComponentID() == "ActionButton")
        return fontFor(TextStyle::LabelLarge);
    return juce::LookAndFeel_V4::getTextButtonFont(btn, 0);
}

juce::Font PatternFlowLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return fontFor(TextStyle::LabelLarge);
}

juce::Font PatternFlowLookAndFeel::getPopupMenuFont()
{
    return fontFor(TextStyle::LabelLarge);
}

void PatternFlowLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                                            bool, bool)
{
    g.setColour(btn.findColour(btn.getToggleState() ? juce::TextButton::textColourOnId
                                                    : juce::TextButton::textColourOffId));
    g.setFont(getTextButtonFont(btn, 0));
    auto r = btn.getLocalBounds().reduced(6, 2);
    g.drawFittedText(btn.getButtonText(), r, juce::Justification::centred, 1, 1.0f);
}

void PatternFlowLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const float fontSize = juce::jmin(15.0f, (float)button.getHeight() * 0.75f);
    const float tickWidth = fontSize * 1.1f;
    const bool focused = button.hasKeyboardFocus(true);
    const float targetA = stateLayerAlpha(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown, focused);
    const float a = animatedAlpha(button, targetA, 110.0f);

    // State layer over the whole toggle row (M3-like)
    if (a > 0.0f)
    {
        auto b = button.getLocalBounds().toFloat().reduced(1.0f, 2.0f);
        g.setColour(stateLayerColourOnSurface().withAlpha(a));
        g.fillRoundedRectangle(b, metrics::cornerRadius);
    }

    drawTickBox(g, button, 4.0f, ((float)button.getHeight() - tickWidth) * 0.5f,
                tickWidth, tickWidth,
                button.getToggleState(),
                button.isEnabled(),
                shouldDrawButtonAsHighlighted,
                shouldDrawButtonAsDown);

    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(fontFor(TextStyle::LabelLarge));
    if (!button.isEnabled())
        g.setOpacity(0.5f);

    const int leftTrim = juce::roundToInt(tickWidth) + 10;
    auto textArea = button.getLocalBounds().withTrimmedLeft(leftTrim).withTrimmedRight(6);
    g.drawFittedText(button.getButtonText(), textArea, juce::Justification::centredLeft,
                     1, 1.0f);
}

void PatternFlowLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
    const float r = metrics::cornerRadius;
    const bool focused = box.hasKeyboardFocus(true);

    const int elevationDp = box.isPopupActive() ? 2 : (box.isMouseOverOrDragging() ? 1 : 0);
    if (elevationDp > 0)
        drawElevationShadow(g, bounds, r, elevationDp);

    g.setColour(elevatedSurface(colours::bgLight(), elevationDp));
    g.fillRoundedRectangle(bounds, r);

    const float targetA = stateLayerAlpha(box.isMouseOverOrDragging(), isButtonDown, focused);
    const float a = animatedAlpha(box, targetA, 110.0f);
    if (a > 0.0f)
    {
        g.setColour(stateLayerColourOnSurface().withAlpha(a));
        g.fillRoundedRectangle(bounds, r);
    }

    g.setColour((focused ? colours::accent() : colours::panelBorder()).withAlpha(0.9f));
    g.drawRoundedRectangle(bounds, r, focused ? 1.6f : 1.0f);

    // Dropdown chevron
    const float cx = bounds.getRight() - 12.0f;
    const float cy = bounds.getCentreY();
    juce::Path p;
    p.startNewSubPath(cx - 5.0f, cy - 2.0f);
    p.lineTo(cx, cy + 3.0f);
    p.lineTo(cx + 5.0f, cy - 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PatternFlowLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                  juce::TextEditor& ed)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
    const float r = metrics::cornerRadius;
    const bool focused = ed.hasKeyboardFocus(true);

    // Subtle animated focus ring using state layer timing
    const float targetA = focused ? 1.0f : 0.0f;
    const float a = animatedAlpha(ed, targetA, 140.0f);
    auto ringCol = colours::accent().withAlpha(0.9f * a);
    auto baseCol = colours::panelBorder().withAlpha(0.9f * (1.0f - a));
    g.setColour(baseCol.overlaidWith(ringCol));
    g.drawRoundedRectangle(bounds, r, focused ? 1.6f : 1.0f);
}

void PatternFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                              const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        g.setColour(colours::panelBorder().withAlpha(0.6f));
        g.fillRect(area.withHeight(1).withY(area.getCentreY()));
        return;
    }

    auto r = area.reduced(6, 2);
    const float radius = metrics::cornerRadius;

    // Animated highlight state layer
    const float targetA = isHighlighted ? 0.10f : 0.0f;
    // Key uses menu item's text hash + y position (good enough for stable animation while menu open)
    const juce::int64 key = ((juce::int64)text.hashCode64() << 20) ^ (juce::int64)area.getY();
    const float a = animatedAlphaForKey(key, targetA, 90.0f);
    if (a > 0.0f)
    {
        g.setColour(colours::accent().withAlpha(a));
        g.fillRoundedRectangle(r.toFloat(), radius);
    }

    // Let JUCE lay out the text/icon/ticks correctly; we just provide animation + colors.
    juce::Colour col = (textColourToUse != nullptr) ? *textColourToUse : juce::Colours::white;
    if (!isActive) col = colours::textDim();
    juce::LookAndFeel_V4::drawPopupMenuItem(g, area,
                                           false, isActive, false,
                                           isTicked, hasSubMenu,
                                           text, shortcutKeyText, icon,
                                           &col);
}

void PatternFlowLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& sb, int x, int y, int width, int height,
                                          bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float r = juce::jmin(track.getWidth(), track.getHeight()) * 0.5f;

    // Track
    g.setColour(colours::panel().withAlpha(0.35f));
    g.fillRoundedRectangle(track, r);

    // Thumb rect
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { track.getX(), track.getY() + (float)thumbStartPosition, track.getWidth(), (float)thumbSize };
    else
        thumb = { track.getX() + (float)thumbStartPosition, track.getY(), (float)thumbSize, track.getHeight() };

    thumb = thumb.reduced(2.0f);

    const float targetA = isMouseDown ? 0.20f : (isMouseOver ? 0.12f : 0.0f);
    const float a = animatedAlpha(sb, targetA, 90.0f);

    auto base = colours::panelBorder().withAlpha(0.70f);
    auto ink = colours::accent().withAlpha(a);
    g.setColour(base.overlaidWith(ink));
    g.fillRoundedRectangle(thumb, r);
}

} // namespace pflow
