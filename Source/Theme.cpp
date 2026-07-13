#include "Theme.h"
#include <unordered_map>
#include <cmath>

namespace pflow {

// ── Tokens ───────────────────────────────────────────────────────────────────

Tweaks& tweaks()
{
    static Tweaks t;
    return t;
}

namespace {

juce::Colour rgba(juce::uint8 r, juce::uint8 g, juce::uint8 b, float a)
{
    return juce::Colour(r, g, b).withAlpha(a);
}

} // namespace

bool usesDarkAppearance()
{
    switch (currentAppearance())
    {
        case Appearance::Light: return false;
        case Appearance::Dark:  return true;
        default: break;
    }
#if JUCE_MAC
    return juce::Desktop::getInstance().isDarkModeActive();
#else
    return false;
#endif
}

const ThemeTokens& themeTokens()
{
    // macOS light — independently tuned (not an invert of dark).
    static const ThemeTokens light {
        juce::Colour(0xfff5f5f7),   // bg (window)
        juce::Colour(0xffffffff),   // panel (content)
        juce::Colour(0xfff5f5f7),   // panel2
        juce::Colour(0xffe8e8ed),   // elev
        rgba(0, 0, 0, 0.08f),       // line
        rgba(0, 0, 0, 0.15f),       // lineStrong
        rgba(0, 0, 0, 0.06f),       // lineSoft
        juce::Colour(0xff1d1d1f),   // text
        juce::Colour(0xff6e6e73),   // text2
        juce::Colour(0xffaeaeb2),   // text3
        juce::Colour(0xffeef0f4),   // rollBg — cool gray so blue notes read clearly
        juce::Colour(0xffe2e5eb),   // rollShade — black-key lanes
        juce::Colour(0xffd0d4dc),   // rollRowline
        rgba(0, 0, 0, 0.16f),       // rollNoteEdge
        rgba(0, 0, 0, 0.22f),       // rollGhost
        juce::Colour(0xfff7f8fa),   // kbWhite
        juce::Colour(0xff2c2c30),   // kbBlack — real piano black
        juce::Colour(0xffece9e5),   // toolbarTop
        juce::Colour(0xffe3e0db),   // toolbarBot
        rgba(245, 245, 247, 0.94f), // sidebarTop
        rgba(232, 232, 237, 0.94f), // sidebarBot
        juce::Colour(0xfff7f6f4),   // tableAlt
        juce::Colour(0xffc9cfd8),   // desktopTop
        juce::Colour(0xffaeb6c2),   // desktopBot
        rgba(0, 0, 0, 0.18f),       // windowBorder
    };
    // macOS dark — more separation between surface levels (HIG / SwiftUI).
    static const ThemeTokens dark {
        juce::Colour(0xff1c1c1e),   // bg
        juce::Colour(0xff2c2c2e),   // panel
        juce::Colour(0xff3a3a3c),   // panel2
        juce::Colour(0xff48484a),   // elev
        rgba(255, 255, 255, 0.08f),
        rgba(255, 255, 255, 0.14f),
        rgba(255, 255, 255, 0.05f),
        juce::Colour(0xfff5f5f7),   // text
        juce::Colour(0xff98989d),   // text2
        juce::Colour(0xff636366),   // text3
        juce::Colour(0xff2c2c2e),   // rollBg
        juce::Colour(0xff333335),   // rollShade
        rgba(255, 255, 255, 0.06f), // rollRowline
        rgba(255, 255, 255, 0.14f),
        rgba(255, 255, 255, 0.22f),
        juce::Colour(0xffe8e8ed),   // kbWhite
        juce::Colour(0xff48484a),   // kbBlack
        juce::Colour(0xff323234),   // toolbarTop
        juce::Colour(0xff2a2a2c),   // toolbarBot
        rgba(44, 44, 46, 0.96f),    // sidebarTop
        rgba(36, 36, 38, 0.96f),    // sidebarBot
        juce::Colour(0xff333335),   // tableAlt
        juce::Colour(0xff1c1c1e),   // desktopTop
        juce::Colour(0xff0d0d0f),   // desktopBot
        rgba(255, 255, 255, 0.12f),
    };
    return usesDarkAppearance() ? dark : light;
}

const AccentTokens& accentTokens()
{
    static const AccentTokens lightBlue {
        juce::Colour(0xff0a66e0),           // accent — slightly deeper for roll contrast
        juce::Colour(0xffffffff),           // ink
        rgba(10, 102, 224, 0.16f),          // soft
        rgba(10, 102, 224, 0.48f),          // line
        juce::Colour(0xff2f86f5),           // bright
    };
    static const AccentTokens darkBlue {
        juce::Colour(0xff0a84ff),           // accent (system blue dark)
        juce::Colour(0xffffffff),
        rgba(10, 132, 255, 0.18f),
        rgba(10, 132, 255, 0.45f),
        juce::Colour(0xff64b5ff),
    };
    return usesDarkAppearance() ? darkBlue : lightBlue;
}

const InspectorTokens& inspectorTokens()
{
    static const InspectorTokens light {
        juce::Colour(0xfff6f5f3),   // panelBg — secondary grouped background
        juce::Colour(0xff007aff),   // accent
        juce::Colour(0xff1d1d1f),   // headerText
        juce::Colour(0xff3c3c3e),   // rowLabel
        juce::Colour(0xff8e8e93),   // valueText
        juce::Colour(0xffe5e2dd),   // divider
        juce::Colour(0xffd8d5cf),   // sliderTrack
        juce::Colours::white,       // sliderKnob
        juce::Colours::white,       // controlSurface (raised)
        juce::Colour(0xfffbfbfa),   // controlSurfaceHi
        juce::Colour(0xffe5e2dd),   // controlSeparator
        juce::Colour(0xffd1cdc7),   // switchOffTrack
        juce::Colour(0xffa3a29e),   // chevron
        juce::Colour(0xff58585c),   // segmentText
        rgba(0, 0, 0, 0.14f),       // controlHairline
        false,
    };
    static const InspectorTokens dark {
        juce::Colour(0xff2c2c2e),   // panelBg — matches SwiftUI Form sidebar
        juce::Colour(0xff0a84ff),
        juce::Colour(0xfff2f2f4),
        rgba(255, 255, 255, 0.78f),
        rgba(255, 255, 255, 0.45f),
        rgba(255, 255, 255, 0.08f),
        rgba(255, 255, 255, 0.14f),
        juce::Colour(0xffd4d4d8),
        rgba(255, 255, 255, 0.11f), // controlSurface (inset translucent)
        rgba(255, 255, 255, 0.15f),
        rgba(255, 255, 255, 0.10f),
        rgba(255, 255, 255, 0.18f),
        rgba(255, 255, 255, 0.35f),
        rgba(255, 255, 255, 0.60f),
        rgba(255, 255, 255, 0.10f),
        true,
    };
    return usesDarkAppearance() ? dark : light;
}

void drawInspectorControlSurface(juce::Graphics& g, juce::Rectangle<float> r,
                                 bool over, bool down)
{
    const auto& t = inspectorTokens();
    auto fill = t.controlSurface;
    if (down) fill = fill.darker(t.dark ? 0.10f : 0.04f);
    else if (over) fill = t.controlSurfaceHi;
    g.setColour(fill);
    g.fillRoundedRectangle(r, metrics::controlRadius);

    g.setColour(t.controlHairline);
    g.drawRoundedRectangle(r.reduced(0.25f), metrics::controlRadius, 0.5f);

    if (!t.dark)
    {
        // Light: raised control — subtle bottom edge (SwiftUI default button).
        g.setColour(rgba(0, 0, 0, 0.05f));
        g.drawHorizontalLine((int) r.getBottom() - 0.5f, r.getX() + 1.5f, r.getRight() - 1.5f);
    }
    else
    {
        // Dark: inset control — inner top highlight.
        g.setColour(rgba(255, 255, 255, 0.06f));
        g.drawHorizontalLine((int) r.getY() + 0.5f, r.getX() + 1.5f, r.getRight() - 1.5f);
    }
}

void drawInspectorDivider(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(inspectorTokens().divider);
    g.fillRect(bounds.getX(), bounds.getBottom() - 1, bounds.getWidth(), 1);
}

// ── Fonts (system stack) ─────────────────────────────────────────────────────

// Resolve the San Francisco system font. "-apple-system" is a CSS alias, not a
// CoreText family name, so JUCE would silently fall back to Helvetica.
static const juce::String& systemFontFamily(bool display)
{
    struct Families
    {
        Families()
        {
            const auto all = juce::Font::findAllTypefaceNames();
            text = all.contains("SF Pro Text") ? "SF Pro Text"
                 : all.contains("Helvetica Neue") ? "Helvetica Neue"
                 : juce::Font::getDefaultSansSerifFontName();
            displayFamily = all.contains("SF Pro Display") ? "SF Pro Display" : text;
            sf = text.startsWith("SF ");
        }
        juce::String text, displayFamily;
        bool sf = false;
    };
    static const Families f;
    return display ? f.displayFamily : f.text;
}

static bool systemFontIsSF()
{
    return systemFontFamily(false).startsWith("SF ");
}

juce::Font uiFont(float pt, bool semibold)
{
    // SF Pro Display for large text, SF Pro Text for body (HIG threshold 20pt).
    const float scaledPt = pt * contentScale();
    const auto& family = systemFontFamily(scaledPt >= 20.0f);
    const char* style = semibold ? (systemFontIsSF() ? "Semibold" : "Medium")
                                 : "Regular";
    return juce::Font(juce::FontOptions(scaledPt).withName(family).withStyle(style));
}

juce::Font monoFont(float pt, bool semibold)
{
    // Tabular numerals via system font (no JetBrains Mono).
    auto f = uiFont(pt, semibold);
    f.setExtraKerningFactor(0.0f);
    return f;
}

juce::Font fontFor(TextStyle s)
{
    switch (s)
    {
        case TextStyle::LargeTitle:  return uiFont(28.0f, true);
        case TextStyle::Title2:      return uiFont(17.0f, true);
        case TextStyle::Headline:    return uiFont(14.5f, true);
        case TextStyle::Body:        return uiFont(12.0f, false);
        case TextStyle::Callout:     return uiFont(13.0f, false);
        case TextStyle::Subheadline: return uiFont(12.5f, false);
        case TextStyle::Footnote:    return uiFont(11.0f, false);
        case TextStyle::Caption:     return uiFont(10.5f, false);
    }
    return uiFont(12.0f, false);
}

namespace {

class HIGAnimator : private juce::Timer
{
public:
    static HIGAnimator& instance()
    {
        static HIGAnimator a;
        return a;
    }

    void watch(juce::Component& c)
    {
        if (watching.contains(&c)) return;
        watching.add(&c);
        startTimerHz(60);
    }

private:
    void timerCallback() override
    {
        for (int i = watching.size(); --i >= 0;)
        {
            auto* c = watching.getUnchecked(i);
            if (c == nullptr || !c->isShowing()) { watching.remove(i); continue; }
            c->repaint();
        }
        if (watching.isEmpty()) stopTimer();
    }
    juce::Array<juce::Component*> watching;
};

static const juce::Identifier kHigAlpha("hig_alpha");
static const juce::Identifier kHigLastMs("hig_last_ms");

float animatedAlpha(juce::Component& c, float target, float tauMs)
{
    auto& props = c.getProperties();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double last = props.contains(kHigLastMs) ? (double) props[kHigLastMs] : now;
    const float cur = props.contains(kHigAlpha) ? (float) props[kHigAlpha] : 0.0f;
    const float dt = (float) juce::jlimit(0.0, 100.0, now - last);
    props.set(kHigLastMs, now);
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    float next = cur + (target - cur) * k;
    if (std::abs(next - target) < 0.002f) next = target;
    props.set(kHigAlpha, next);
    if (next != target) HIGAnimator::instance().watch(c);
    return next;
}

struct KeyAnimState { float v = 0.0f; double lastMs = 0.0; };
float animatedAlphaForKey(juce::int64 key, float target, float tauMs)
{
    static std::unordered_map<juce::int64, KeyAnimState> states;
    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& st = states[key];
    if (st.lastMs <= 0.0) st.lastMs = now;
    const float dt = (float) juce::jlimit(0.0, 100.0, now - st.lastMs);
    st.lastMs = now;
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    st.v = st.v + (target - st.v) * k;
    if (std::abs(st.v - target) < 0.002f) st.v = target;
    return st.v;
}

float higPressAlpha(bool highlighted, bool down, bool focused)
{
    if (down) return 0.16f;
    if (focused) return 0.10f;
    if (highlighted) return 0.06f;
    return 0.0f;
}

} // namespace

void PatternFlowLookAndFeel::drawSymbol(juce::Graphics& g, const juce::String& name,
                                        juce::Rectangle<float> b, juce::Colour colour, float stroke)
{
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float s = juce::jmin(b.getWidth(), b.getHeight()) * 0.38f;
    g.setColour(colour);
    juce::PathStrokeType st(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    if (name == higIcon::star)
    {
        juce::Path p;
        for (int i = 0; i < 5; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 5.0f - juce::MathConstants<float>::halfPi;
            const float r = (i % 2 == 0) ? s : s * 0.42f;
            const float x = cx + r * std::cos(a);
            const float y = cy + r * std::sin(a);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        p.closeSubPath();
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::chevronUp)
    {
        juce::Path p;
        p.startNewSubPath(cx - s * 0.55f, cy + s * 0.3f);
        p.lineTo(cx, cy - s * 0.45f);
        p.lineTo(cx + s * 0.55f, cy + s * 0.3f);
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::chevronDown)
    {
        juce::Path p;
        p.startNewSubPath(cx - s * 0.55f, cy - s * 0.3f);
        p.lineTo(cx, cy + s * 0.45f);
        p.lineTo(cx + s * 0.55f, cy - s * 0.3f);
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::scissors)
    {
        g.drawLine(cx - s, cy - s * 0.5f, cx + s, cy + s * 0.5f, stroke);
        g.drawLine(cx - s, cy + s * 0.5f, cx + s, cy - s * 0.5f, stroke);
        g.drawEllipse(cx - s * 0.85f, cy - s * 0.95f, s * 0.55f, s * 0.55f, stroke);
        g.drawEllipse(cx + s * 0.30f, cy + s * 0.40f, s * 0.55f, s * 0.55f, stroke);
        return;
    }

    if (name == higIcon::speakerSlash)
    {
        juce::Path spk;
        spk.addTriangle(cx - s * 0.55f, cy - s * 0.35f, cx - s * 0.55f, cy + s * 0.35f, cx - s * 0.05f, cy);
        g.strokePath(spk, st);
        g.drawLine(cx - s * 0.02f, cy - s * 0.45f, cx + s * 0.75f, cy - s * 0.95f, stroke * 0.9f);
        g.drawLine(cx - s * 0.02f, cy + s * 0.45f, cx + s * 0.75f, cy + s * 0.95f, stroke * 0.9f);
        g.drawLine(cx - s * 0.3f, cy + s * 0.95f, cx + s * 0.95f, cy - s * 0.75f, stroke);
    }
}

PatternFlowLookAndFeel::PatternFlowLookAndFeel()
{
    refreshColours();
}

void PatternFlowLookAndFeel::refreshColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::bg());
    setColour(juce::TextEditor::backgroundColourId, colours::bgLight());
    setColour(juce::TextEditor::textColourId, colours::text());
    setColour(juce::TextEditor::outlineColourId, colours::separator());
    setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    setColour(juce::ComboBox::textColourId, colours::text());
    setColour(juce::ComboBox::outlineColourId, colours::separator());
    setColour(juce::ComboBox::arrowColourId, colours::textDim());
    setColour(juce::PopupMenu::backgroundColourId, colours::bgLight());
    setColour(juce::PopupMenu::textColourId, colours::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accentSoft());
    setColour(juce::PopupMenu::highlightedTextColourId, colours::accent());
    setColour(juce::ScrollBar::thumbColourId, colours::lineStrong().withAlpha(0.55f));
    setColour(juce::ListBox::backgroundColourId, colours::panel());
    setColour(juce::ListBox::textColourId, colours::text());
    setColour(juce::Label::textColourId, colours::text());
    setColour(juce::ToggleButton::textColourId, colours::text());
    setColour(juce::ToggleButton::tickColourId, colours::accent());
    setColour(juce::TextButton::buttonColourId, colours::elev());
    setColour(juce::TextButton::textColourOffId, colours::text());
    setColour(juce::TextButton::textColourOnId, colours::accentInk());

    setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TreeView::linesColourId, colours::separator().withAlpha(0.4f));
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::selection());
    setColour(juce::TreeView::dragAndDropIndicatorColourId, colours::accent());

    setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::selection());
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, colours::text());

    setColour(juce::TooltipWindow::backgroundColourId, colours::panel());
    setColour(juce::TooltipWindow::textColourId, colours::text());
    setColour(juce::TooltipWindow::outlineColourId, colours::line());
}

void PatternFlowLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                               int w, int h, float sliderPos,
                                               float startAngle, float endAngle, juce::Slider&)
{
    auto radius = (float) juce::jmin(w, h) * 0.38f;
    auto centreX = (float) x + (float) w * 0.5f;
    auto centreY = (float) y + (float) h * 0.5f;
    auto angle = startAngle + sliderPos * (endAngle - startAngle);
    auto bodyRadius = radius - 2.0f;
    g.setColour(colours::controlFill());
    g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(colours::knobTrack());
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(colours::accent());
        g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void PatternFlowLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                                    const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat().reduced(0.5f);
    const juce::String id = btn.getComponentID();
    const bool focused = btn.hasKeyboardFocus(true);
    const bool toggled = btn.getToggleState();
    const float pressA = higPressAlpha(highlighted, down, focused);

    if (id == "HIGPrimaryButton")
    {
        juce::Colour fill = colours::accent();
        if (down) fill = fill.darker(0.12f);
        else if (highlighted) fill = fill.brighter(0.06f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, metrics::chipRadius);
        if (pressA > 0.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(pressA));
            g.fillRoundedRectangle(bounds, metrics::chipRadius);
        }
        return;
    }

    if (id == "HIGIconButton")
    {
        if (pressA > 0.0f)
        {
            g.setColour(colours::controlFill());
            g.fillRoundedRectangle(bounds, metrics::chipRadius);
        }
        return;
    }

    if (id == "HIGChipToggle")
    {
        const bool active = toggled && btn.getClickingTogglesState();
        juce::Colour fill = active ? colours::accent() : colours::controlFill();
        if (down) fill = fill.darker(active ? 0.1f : 0.05f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
        return;
    }

    if (pressA > 0.0f)
    {
        g.setColour(colours::controlFill().withAlpha(0.5f + pressA));
        g.fillRoundedRectangle(bounds, metrics::chipRadius);
    }
}

void PatternFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(), 1, 1.0f);
}

juce::Font PatternFlowLookAndFeel::getLabelFont(juce::Label&) { return fontFor(TextStyle::Body); }

juce::Font PatternFlowLookAndFeel::getTextButtonFont(juce::TextButton& btn, int)
{
    const auto id = btn.getComponentID();
    if (id == "HIGPrimaryButton") return fontFor(TextStyle::Body);
    if (id == "HIGChipToggle") return fontFor(TextStyle::Callout);
    return fontFor(TextStyle::Callout);
}

juce::Font PatternFlowLookAndFeel::getComboBoxFont(juce::ComboBox&) { return fontFor(TextStyle::Body); }

void PatternFlowLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(1.0f);
    auto b = box.getLocalBounds();
    label.setBounds(b.withTrimmedLeft(metrics::comboTextPadding).withTrimmedRight(18));
}

juce::Font PatternFlowLookAndFeel::getPopupMenuFont() { return fontFor(TextStyle::Body); }

void PatternFlowLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    const auto id = btn.getComponentID();

    if (id == "HIGIconButton")
    {
        const auto icon = btn.getProperties()["hig_icon"].toString();
        drawSymbol(g, icon, btn.getLocalBounds().toFloat(), colours::text(), 1.45f);
        return;
    }

    if (id == "HIGChipToggle" && btn.getButtonText().isEmpty())
    {
        const bool muted = btn.getToggleState();
        drawSymbol(g, higIcon::speakerSlash, btn.getLocalBounds().toFloat(),
                   muted ? colours::onPrimary() : colours::text(), 1.35f);
        return;
    }

    const bool active = btn.getToggleState() && btn.getClickingTogglesState();
    juce::Colour col = colours::text();
    if (id == "HIGPrimaryButton") col = colours::onPrimary();
    else if (id == "HIGChipToggle" && active) col = colours::onPrimary();
    g.setColour(col);
    g.setFont(getTextButtonFont(btn, 0));

    if (id == "HIGChipToggle" && btn.getButtonText() == "Trim")
    {
        auto r = btn.getLocalBounds().reduced(8, 2);
        auto iconArea = r.removeFromLeft(18);
        drawSymbol(g, higIcon::scissors, iconArea.toFloat(),
                   active ? colours::onPrimary() : colours::text(), 1.25f);
        g.drawFittedText(btn.getButtonText(), r, juce::Justification::centredLeft, 1, 1.0f);
        return;
    }

    g.drawFittedText(btn.getButtonText(), btn.getLocalBounds().reduced(8, 2),
                     juce::Justification::centred, 1, 1.0f);
}

void PatternFlowLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                              bool highlighted, bool down)
{
    juce::ignoreUnused(highlighted, down);
    const bool on = button.getToggleState();
    auto b = button.getLocalBounds().toFloat().reduced(1.0f);
    const float h = b.getHeight();
    const float w = h * 1.75f;
    auto track = juce::Rectangle<float>(b.getX(), b.getY() + (b.getHeight() - h * 0.72f) * 0.5f, w, h * 0.72f);
    g.setColour(on ? colours::accent() : colours::controlFill());
    g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
    const float knob = track.getHeight() - 4.0f;
    const float kx = on ? track.getRight() - knob - 2.0f : track.getX() + 2.0f;
    g.setColour(juce::Colours::white);
    g.fillEllipse(kx, track.getCentreY() - knob * 0.5f, knob, knob);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(fontFor(TextStyle::Body));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft((int) w + 8),
                     juce::Justification::centredLeft, 1, 1.0f);
}

void PatternFlowLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                         bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const bool focused = box.hasKeyboardFocus(true);
    g.setColour(colours::bgLight());
    g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    g.setColour(focused ? colours::accent() : colours::separator());
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, focused ? 1.5f : 0.5f);
}

void PatternFlowLookAndFeel::drawFileBrowserRow(juce::Graphics& g, int width, int height,
                                               const juce::File&, const juce::String& filename,
                                               juce::Image* icon, const juce::String&,
                                               const juce::String&, bool,
                                               bool isItemSelected, int, juce::DirectoryContentsDisplayComponent& dcc)
{
    auto* fileListComp = dynamic_cast<juce::Component*>(&dcc);
    if (isItemSelected)
    {
        g.setColour(fileListComp != nullptr
            ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightColourId)
            : findColour(juce::DirectoryContentsDisplayComponent::highlightColourId));
        g.fillRect(0, 1, width, height - 2);
    }

    const int x = 28;
    if (icon != nullptr && icon->isValid())
        g.drawImageWithin(*icon, 4, 2, x - 6, height - 4,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, false);

    g.setColour(colours::text());
    g.setFont(systemFont(metrics::browserFontSize, isItemSelected));
    g.drawText(filename, x, 0, width - x - 4, height, juce::Justification::centredLeft, true);
}

void PatternFlowLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    // Transparent-background editors (search form) only need a soft focus ring;
    // the parent paints the control surface.
    if (ed.findColour(juce::TextEditor::backgroundColourId).getAlpha() < 8)
    {
        if (ed.hasKeyboardFocus(true))
        {
            auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
            g.setColour(colours::accent());
            g.drawRoundedRectangle(bounds, metrics::controlRadius, 1.2f);
        }
        return;
    }

    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const bool focused = ed.hasKeyboardFocus(true);
    g.setColour(focused ? colours::accent() : colours::separator());
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, focused ? 1.5f : 0.5f);
}

void PatternFlowLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(1.0f);
    g.setColour(colours::panel());
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(colours::line());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 0.5f);
}

void PatternFlowLookAndFeel::preparePopupMenuWindow(juce::Component& window)
{
    window.setOpaque(false);
}

int PatternFlowLookAndFeel::getPopupMenuBorderSize()
{
    return 6;
}

void PatternFlowLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                       int standardMenuItemHeight,
                                                       int& idealWidth, int& idealHeight)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight,
                                              idealWidth, idealHeight);
    if (!isSeparator)
        idealHeight = 26;
    idealWidth = juce::jmax(idealWidth,
                            (int) std::ceil(juce::GlyphArrangement::getStringWidth(uiFont(12.5f, false), text)) + 36);
}

void PatternFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                              const juce::Colour* textColourToUse)
{
    juce::ignoreUnused(icon, shortcutKeyText);

    if (isSeparator)
    {
        g.setColour(colours::separator());
        g.fillRect(area.reduced(10, 0).withHeight(1).withY(area.getCentreY()));
        return;
    }

    auto r = area.reduced(6, 1).toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour(colours::accent());
        g.fillRoundedRectangle(r, metrics::chipRadius);
    }
    else if (isTicked)
    {
        g.setColour(colours::accent().withAlpha(0.14f));
        g.fillRoundedRectangle(r, metrics::chipRadius);
    }

    auto textArea = area.reduced(12, 0);
    if (isTicked || hasSubMenu)
        textArea.removeFromLeft(16);

    if (isTicked)
    {
        // ASCII-safe checkmark drawn as geometry (avoids missing-glyph boxes).
        juce::Path tick;
        const float x = (float) area.getX() + 12.0f;
        const float y = (float) area.getCentreY();
        tick.startNewSubPath(x - 3.5f, y);
        tick.lineTo(x - 0.5f, y + 3.0f);
        tick.lineTo(x + 4.5f, y - 3.5f);
        g.setColour(isHighlighted ? juce::Colours::white : colours::accent());
        g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    if (hasSubMenu)
    {
        auto chev = juce::Rectangle<float>((float) area.getRight() - 16.0f,
                                           (float) area.getCentreY() - 4.0f, 8.0f, 8.0f);
        juce::Path p;
        p.addTriangle(chev.getX(), chev.getY(),
                      chev.getX(), chev.getBottom(),
                      chev.getRight(), chev.getCentreY());
        g.setColour(isHighlighted ? juce::Colours::white : colours::text3());
        g.fillPath(p);
    }

    juce::Colour col = (textColourToUse != nullptr) ? *textColourToUse : colours::text();
    if (!isActive)
        col = colours::textDim();
    else if (isHighlighted)
        col = juce::Colours::white;

    g.setFont(uiFont(12.5f, false));
    g.setColour(col);
    // Draw with fitted text so missing glyphs never leave empty slots.
    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);
}

void PatternFlowLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                          int width, int height, bool isScrollbarVertical,
                                          int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { track.getX(), track.getY() + (float) thumbStartPosition, track.getWidth(), (float) thumbSize };
    else
        thumb = { track.getX() + (float) thumbStartPosition, track.getY(), (float) thumbSize, track.getHeight() };
    thumb = thumb.reduced(3.0f);
    const float alpha = isMouseDown ? 0.9f : (isMouseOver ? 0.65f : 0.35f);
    g.setColour(colours::textDim().withAlpha(alpha));
    // Use the short axis so horizontal thumbs are pills like vertical ones.
    const float r = juce::jmin(thumb.getWidth(), thumb.getHeight()) * 0.5f;
    g.fillRoundedRectangle(thumb, r);
}

void PatternFlowLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    const float radius = 6.0f;
    g.setColour(colours::elev());
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(colours::line());
    g.drawRoundedRectangle(bounds, radius, 0.5f);

    g.setColour(colours::text());
    g.setFont(uiFont(11.5f, false));
    g.drawFittedText(text, bounds.reduced(9.0f, 6.0f).toNearestInt(),
                     juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> PatternFlowLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                              juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
{
    const auto font = uiFont(11.5f, false);
    const int maxW = 260;
    int textW = 0;
    int lines = 1;
    {
        juce::AttributedString as;
        as.setJustification(juce::Justification::centredLeft);
        as.append(tipText, font, colours::text());
        juce::TextLayout layout;
        layout.createLayout(as, (float) maxW);
        textW = (int) std::ceil(layout.getWidth());
        lines = juce::jmax(1, (int) std::ceil(layout.getHeight() / font.getHeight()));
    }
    const int padX = 18;
    const int padY = 12;
    const int w = juce::jlimit(40, maxW + padX, textW + padX);
    const int h = (int) std::ceil(font.getHeight() * (float) lines) + padY;

    int x = screenPos.x;
    int y = screenPos.y + 14;
    if (x + w > parentArea.getRight())
        x = parentArea.getRight() - w - 4;
    if (y + h > parentArea.getBottom())
        y = screenPos.y - h - 6;
    x = juce::jmax(parentArea.getX() + 4, x);
    y = juce::jmax(parentArea.getY() + 4, y);
    return { x, y, w, h };
}

} // namespace pflow
