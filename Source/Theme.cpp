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
    setColour(juce::ComboBox::backgroundColourId,         colours::bgLighter());
    setColour(juce::ComboBox::textColourId,               juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId,            colours::panelBorder());
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
    // Default: FigmaExample filled button base (#252525)
    setColour(juce::TextButton::buttonColourId,           colours::bgLighter());
    setColour(juce::TextButton::textColourOffId,          colours::text());
    setColour(juce::TextButton::textColourOnId,           colours::textBright());

    // File tree / TreeView colours
    setColour(juce::TreeView::backgroundColourId,         colours::panel());
    setColour(juce::TreeView::linesColourId,              colours::panelBorder());
    // Base selection colour; animation is handled in TreeViewItem paint via repeated repaints,
    // but we keep this value conservative for balanced colour usage.
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::accent().withAlpha(0.18f));
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
    auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
    const juce::String id = btn.getComponentID();
    const float r = (id == "BrowserFolderButton") ? 0.0f : metrics::cornerRadius;

    const bool focused = btn.hasKeyboardFocus(true);
    const bool toggled = btn.getToggleState();

    const bool isGhost = (id == "HeaderGhost" || id == "HeaderIcon");

    if (isGhost)
    {
        // Header ghost buttons: transparent base, hover bg #2a2a2a, no border.
        if (highlighted || down || focused)
        {
            g.setColour(colours::bgLighter().brighter(0.05f));   // ~#2a2a2a feel
            g.fillRoundedRectangle(bounds, r);
        }
        return;
    }

    // Filled button (control strip + app-wide): bg #252525, border #333, hover -> #2a2a2a / #444
    juce::Colour base = colours::bgLighter();                 // #252525
    juce::Colour border = juce::Colour(0xff333333);
    juce::Colour activeBg = colours::accentDim().withAlpha(0.30f); // blue-900 @ 30% alpha feel
    juce::Colour activeBorder = colours::accent().withAlpha(0.50f);

    const bool active = toggled && btn.getClickingTogglesState();
    if (active)
        base = activeBg;
    else if (highlighted || down)
        base = colours::bgLighter().brighter(0.05f); // ~#2a2a2a

    g.setColour(base);
    g.fillRoundedRectangle(bounds, r);

    // Border
    juce::Colour useBorder = active ? activeBorder : (highlighted ? juce::Colour(0xff444444) : border);
    g.setColour(useBorder);
    g.drawRoundedRectangle(bounds, r, focused ? 1.6f : 1.0f);
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
    if (btn.getComponentID() == "HeaderIcon")
        return juce::Font(juce::FontOptions(40.0f));
    if (btn.getComponentID() == "HeaderGhost")
        return juce::Font(juce::FontOptions(15.0f));
    return juce::Font(juce::FontOptions(15.0f));
}

juce::Font PatternFlowLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return fontFor(TextStyle::LabelLarge);
}

void PatternFlowLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // FigmaExample: select text has comfortable left padding (matches buttons' px-2/px-3 feel).
    // Leave room for chevron at the right.
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    // Avoid "..." / multi-line shrinking for short labels (especially when paired with icons in the header).
    label.setMinimumHorizontalScale(1.0f);

    auto b = box.getLocalBounds();
    const int leftPad = juce::roundToInt((float)metrics::comboTextPadding * metrics::uiScale);
    const int rightPad = juce::roundToInt(12.0f * metrics::uiScale); // chevron + breathing room
    label.setBounds(b.withTrimmedLeft(leftPad).withTrimmedRight(rightPad));
}

juce::Font PatternFlowLookAndFeel::getPopupMenuFont()
{
    return fontFor(TextStyle::LabelLarge);
}

void PatternFlowLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                                            bool, bool)
{
    const bool active = btn.getToggleState() && btn.getClickingTogglesState();
    juce::Colour col = colours::text();
    if (active)
        col = colours::accentBright();
    g.setColour(col);
    g.setFont(getTextButtonFont(btn, 0));
    auto text = btn.getButtonText();

    if (btn.getComponentID() == "HeaderIcon")
    {
        g.drawText(text, btn.getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (btn.getComponentID() == "HeaderGhost")
    {
        // Icon + label (FigmaExample: size-3 icon, gap-1.5, px-3, h-8)
        auto r = btn.getLocalBounds().reduced(12, 2);
        auto iconArea = r.removeFromLeft(13);
        r.removeFromLeft(6);

        const float cx = (float)iconArea.getCentreX();
        const float cy = (float)iconArea.getCentreY();
        g.setColour(col);

        if (text == "Trim")
        {
            // Simple "scissors" approximation: X glyph
            g.drawLine(cx - 4.0f, cy - 4.0f, cx + 4.0f, cy + 4.0f, 1.6f);
            g.drawLine(cx - 4.0f, cy + 4.0f, cx + 4.0f, cy - 4.0f, 1.6f);
        }
        else
        {
            // Plus icon (Step/Extend)
            g.drawLine(cx - 4.0f, cy, cx + 4.0f, cy, 1.6f);
            g.drawLine(cx, cy - 4.0f, cx, cy + 4.0f, 1.6f);
        }

        g.drawFittedText(text, r, juce::Justification::centredLeft, 1, 1.0f);
        return;
    }

    auto r = btn.getLocalBounds().reduced(6, 2);
    g.drawFittedText(text, r, juce::Justification::centred, 1, 1.0f);
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

    // If this ComboBox lives inside our HeaderSelect wrapper, the wrapper paints the chip.
    // Drawing another filled rounded-rect here makes it look like there's a button behind.
    const bool isHeaderSelect = box.getProperties().contains("header_select");
    if (!isHeaderSelect)
    {
        // FigmaExample select style: bg #252525, border #333, hover border #444.
        const bool hover = box.isMouseOverOrDragging();
        g.setColour(colours::bgLighter());
        g.fillRoundedRectangle(bounds, r);

        juce::Colour border = focused ? colours::accent().withAlpha(0.80f)
                                      : (hover ? juce::Colour(0xff444444) : juce::Colour(0xff333333));
        g.setColour(border);
        g.drawRoundedRectangle(bounds, r, focused ? 1.6f : 1.0f);
    }

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

void PatternFlowLookAndFeel::drawFileBrowserRow(juce::Graphics& g, int width, int height,
                                               const juce::File&, const juce::String& filename, juce::Image* icon,
                                               const juce::String& fileSizeDescription,
                                               const juce::String& fileTimeDescription,
                                               bool isDirectory, bool isItemSelected,
                                               int /*itemIndex*/, juce::DirectoryContentsDisplayComponent& dcc)
{
    // Based on LookAndFeel_V2::drawFileBrowserRow, but with explicit typography for the sidebar.
    auto* fileListComp = dynamic_cast<juce::Component*>(&dcc);

    if (isItemSelected)
        g.fillAll(fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightColourId)
                                          : findColour(juce::DirectoryContentsDisplayComponent::highlightColourId));

    const int x = 32;

    if (icon != nullptr && icon->isValid())
    {
        g.drawImageWithin(*icon, 2, 2, x - 4, height - 4,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                          false);
    }
    else
    {
        if (auto* d = isDirectory ? getDefaultFolderImage()
                                  : getDefaultDocumentFileImage())
            d->drawWithin(g, juce::Rectangle<float>(2.0f, 2.0f, (float)(x - 4), (float)height - 4.0f),
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }

    if (isItemSelected)
        g.setColour(fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId)
                                            : findColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId));
    else
        g.setColour(fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::textColourId)
                                            : findColour(juce::DirectoryContentsDisplayComponent::textColourId));

    g.setFont(juce::Font(juce::FontOptions(metrics::browserFontSize)));

    if (width > 450 && ! isDirectory)
    {
        auto sizeX = juce::roundToInt((float)width * 0.7f);
        auto dateX = juce::roundToInt((float)width * 0.8f);

        g.drawText(filename,
                   x, 0, sizeX - x, height,
                   juce::Justification::centredLeft, false);

        g.setFont(juce::Font(juce::FontOptions(juce::jmax(10.0f, metrics::browserFontSize - 2.0f))));
        g.setColour(colours::textDim());

        if (! isDirectory)
        {
            g.drawText(fileSizeDescription,
                       sizeX, 0, dateX - sizeX - 8, height,
                       juce::Justification::centredRight, false);

            g.drawText(fileTimeDescription,
                       dateX, 0, width - 8 - dateX, height,
                       juce::Justification::centredRight, false);
        }
    }
    else
    {
        g.drawText(filename,
                   x, 0, width - x, height,
                   juce::Justification::centredLeft, false);
    }
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
