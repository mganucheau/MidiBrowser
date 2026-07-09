#include "Theme.h"
#include "BinaryData.h"
#include <unordered_map>

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

const ThemeTokens& themeTokens(ThemeId t)
{
    static const std::array<ThemeTokens, kNumThemes> themes {{
        {
            "charcoal",
            juce::Colour(0xff131419), juce::Colour(0xff181a20), juce::Colour(0xff1c1f26), juce::Colour(0xff24272f),
            rgba(255, 255, 255, 0.08f), rgba(255, 255, 255, 0.15f),
            juce::Colour(0xffe9eaee), juce::Colour(0xffa6a9b3), juce::Colour(0xff6c7079),
            juce::Colour(0xff14161b), rgba(255, 255, 255, 0.028f), rgba(255, 255, 255, 0.045f),
            rgba(0, 0, 0, 0.28f), rgba(255, 255, 255, 0.28f),
            juce::Colour(0xff2b2e36), juce::Colour(0xff1a1c22),
        },
        {
            "graphite",
            juce::Colour(0xff18160f), juce::Colour(0xff1e1b15), juce::Colour(0xff221e17), juce::Colour(0xff2b261d),
            rgba(255, 250, 235, 0.08f), rgba(255, 250, 235, 0.15f),
            juce::Colour(0xffece8df), juce::Colour(0xffaca598), juce::Colour(0xff726c5f),
            juce::Colour(0xff15130d), rgba(255, 248, 230, 0.03f), rgba(255, 248, 230, 0.05f),
            rgba(0, 0, 0, 0.30f), rgba(255, 248, 230, 0.28f),
            juce::Colour(0xff2f2a20), juce::Colour(0xff1c1810),
        },
        {
            "ink",
            juce::Colour(0xff0d0f16), juce::Colour(0xff11141d), juce::Colour(0xff141826), juce::Colour(0xff1b2030),
            rgba(180, 200, 255, 0.09f), rgba(180, 200, 255, 0.16f),
            juce::Colour(0xffe6e9f3), juce::Colour(0xff9ca3b8), juce::Colour(0xff636b82),
            juce::Colour(0xff0e1119), rgba(150, 180, 255, 0.03f), rgba(150, 180, 255, 0.05f),
            rgba(0, 0, 0, 0.32f), rgba(170, 190, 255, 0.30f),
            juce::Colour(0xff262c3d), juce::Colour(0xff161a26),
        },
    }};
    return themes[(size_t) juce::jlimit(0, kNumThemes - 1, (int) t)];
}

const AccentTokens& accentTokens(AccentId a)
{
    static const std::array<AccentTokens, kNumAccents> accents {{
        { "blue",  juce::Colour(0xff4d87ff), juce::Colour(0xffffffff),
          rgba(77, 135, 255, 0.16f), rgba(77, 135, 255, 0.42f), juce::Colour(0xff9cbcff) },
        { "amber", juce::Colour(0xfff0a93b), juce::Colour(0xff1c1304),
          rgba(240, 169, 59, 0.16f), rgba(240, 169, 59, 0.42f), juce::Colour(0xffffd089) },
        { "mint",  juce::Colour(0xff2bd49f), juce::Colour(0xff042019),
          rgba(43, 212, 159, 0.15f), rgba(43, 212, 159, 0.42f), juce::Colour(0xff79efc9) },
    }};
    return accents[(size_t) juce::jlimit(0, kNumAccents - 1, (int) a)];
}

// ── Fonts ────────────────────────────────────────────────────────────────────

namespace {

juce::Typeface::Ptr loadTypeface(const void* data, size_t size)
{
    return juce::Typeface::createSystemTypefaceFor(data, size);
}

juce::Font fontWithTypeface(juce::Typeface::Ptr tf, float pt, bool synthBold)
{
    if (tf != nullptr)
    {
        auto f = juce::Font(juce::FontOptions().withTypeface(tf).withHeight(pt));
        if (synthBold)
            f.setBold(true);
        return f;
    }
    return juce::Font(juce::FontOptions(pt).withStyle(synthBold ? "Semibold" : "Regular"));
}

} // namespace

juce::Font uiFont(float pt, bool semibold)
{
    static juce::Typeface::Ptr tf =
        loadTypeface(BinaryData::SchibstedGrotesk_ttf, (size_t) BinaryData::SchibstedGrotesk_ttfSize);
    return fontWithTypeface(tf, pt, semibold);
}

juce::Font monoFont(float pt, bool semibold)
{
    static juce::Typeface::Ptr regular =
        loadTypeface(BinaryData::JetBrainsMonoRegular_ttf, (size_t) BinaryData::JetBrainsMonoRegular_ttfSize);
    static juce::Typeface::Ptr semi =
        loadTypeface(BinaryData::JetBrainsMonoSemiBold_ttf, (size_t) BinaryData::JetBrainsMonoSemiBold_ttfSize);
    auto tf = semibold ? semi : regular;
    return fontWithTypeface(tf != nullptr ? tf : regular, pt, false);
}

juce::Font fontFor(TextStyle s)
{
    switch (s)
    {
        case TextStyle::LargeTitle:  return uiFont(28.0f, true);
        case TextStyle::Title2:      return uiFont(17.0f, true);
        case TextStyle::Headline:    return uiFont(13.0f, true);
        case TextStyle::Body:        return uiFont(13.0f, false);
        case TextStyle::Callout:     return uiFont(12.0f, false);
        case TextStyle::Subheadline: return uiFont(11.0f, false);
        case TextStyle::Footnote:    return uiFont(10.0f, false);
        case TextStyle::Caption:     return uiFont(10.0f, false);
    }
    return uiFont(13.0f, false);
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
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent());
    setColour(juce::PopupMenu::highlightedTextColourId, colours::onPrimary());
    setColour(juce::ScrollBar::thumbColourId, colours::controlFill());
    setColour(juce::ListBox::backgroundColourId, colours::bgLight());
    setColour(juce::ListBox::textColourId, colours::text());
    setColour(juce::Label::textColourId, colours::text());
    setColour(juce::ToggleButton::textColourId, colours::text());
    setColour(juce::ToggleButton::tickColourId, colours::accent());
    setColour(juce::TextButton::buttonColourId, colours::controlFill());
    setColour(juce::TextButton::textColourOffId, colours::text());
    setColour(juce::TextButton::textColourOnId, colours::onPrimary());

    setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TreeView::linesColourId, colours::separator().withAlpha(0.4f));
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::selection());
    setColour(juce::TreeView::dragAndDropIndicatorColourId, colours::accent());

    setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::selection());
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, colours::text());

    setColour(juce::TooltipWindow::backgroundColourId, colours::bgLighter());
    setColour(juce::TooltipWindow::textColourId, colours::text());
    setColour(juce::TooltipWindow::outlineColourId, colours::separator());
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
    label.setBounds(b.withTrimmedLeft(metrics::comboTextPadding).withTrimmedRight(16));
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
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const bool focused = ed.hasKeyboardFocus(true);
    g.setColour(focused ? colours::accent() : colours::separator());
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, focused ? 1.5f : 0.5f);
}

void PatternFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                              const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        g.setColour(colours::separator());
        g.fillRect(area.withHeight(1).withY(area.getCentreY()));
        return;
    }
    auto r = area.reduced(6, 1);
    const float targetA = isHighlighted ? 1.0f : 0.0f;
    const juce::int64 key = ((juce::int64) text.hashCode64() << 20) ^ (juce::int64) area.getY();
    const float a = animatedAlphaForKey(key, targetA, 80.0f);
    if (a > 0.0f)
    {
        g.setColour(colours::accent().withAlpha(0.85f * a));
        g.fillRoundedRectangle(r.toFloat(), metrics::chipRadius);
    }
    juce::Colour col = (textColourToUse != nullptr) ? *textColourToUse : colours::text();
    if (!isActive) col = colours::textDim();
    juce::LookAndFeel_V4::drawPopupMenuItem(g, area, false, isActive, false, isTicked, hasSubMenu,
                                           text, shortcutKeyText, icon, &col);
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
    g.fillRoundedRectangle(thumb, thumb.getWidth() * 0.5f);
}

} // namespace pflow
