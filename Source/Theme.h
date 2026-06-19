#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include "BuildInfo.h"

namespace pflow {

// ── Apple Human Interface Guidelines (macOS dark) ───────────────────────────

struct ThemeTokens
{
    bool isDark = true;

    juce::Colour primary;
    juce::Colour onPrimary;
    juce::Colour primaryContainer;
    juce::Colour onPrimaryContainer;

    juce::Colour surface;
    juce::Colour surfaceContainerLow;
    juce::Colour surfaceContainer;
    juce::Colour surfaceContainerHigh;
    juce::Colour onSurface;
    juce::Colour onSurfaceVariant;

    juce::Colour outline;
    juce::Colour outlineVariant;

    juce::Colour error;
    juce::Colour onError;
};

enum class TextStyle
{
    LargeTitle,     // 28pt Semibold
    Title2,         // 17pt Semibold
    Headline,       // 13pt Semibold
    Body,           // 13pt Regular
    Callout,        // 12pt Regular
    Subheadline,    // 11pt Regular
    Footnote,       // 10pt Regular
    Caption         // 10pt Regular secondary
};

inline juce::Font systemFont(float pt, bool semibold = false)
{
#if JUCE_MAC
    juce::FontOptions opts(".AppleSystemUIFont", pt, semibold ? juce::Font::bold : juce::Font::plain);
    if (semibold)
        opts = opts.withStyle("Semibold");
    return juce::Font(opts);
#else
    return juce::Font(juce::FontOptions(pt).withStyle(semibold ? "Semibold" : "Regular"));
#endif
}

inline juce::Font fontFor(TextStyle s)
{
    switch (s)
    {
        case TextStyle::LargeTitle:  return systemFont(28.0f, true);
        case TextStyle::Title2:      return systemFont(17.0f, true);
        case TextStyle::Headline:    return systemFont(13.0f, true);
        case TextStyle::Body:        return systemFont(13.0f, false);
        case TextStyle::Callout:     return systemFont(12.0f, false);
        case TextStyle::Subheadline: return systemFont(11.0f, false);
        case TextStyle::Footnote:    return systemFont(10.0f, false);
        case TextStyle::Caption:     return systemFont(10.0f, false);
    }
    return systemFont(13.0f, false);
}

inline std::atomic<int>& appThemeId()
{
    static std::atomic<int> id { 0 };
    return id;
}

inline std::atomic<bool>& darkModeEnabled()
{
    static std::atomic<bool> enabled { true };
    return enabled;
}

inline ThemeTokens makeHIGTokens(bool dark)
{
    ThemeTokens t;
    t.isDark = dark;

    if (dark)
    {
        t.primary              = juce::Colour(0xff0a84ff);
        t.onPrimary            = juce::Colours::white;
        t.primaryContainer     = juce::Colour(0xff0a84ff).withAlpha(0.22f);
        t.onPrimaryContainer   = juce::Colour(0xff64b5ff);

        t.surface              = juce::Colour(0xff1c1c1e);
        t.surfaceContainerLow  = juce::Colour(0xff2c2c2e);
        t.surfaceContainer     = juce::Colour(0xff3a3a3c);
        t.surfaceContainerHigh = juce::Colour(0xff48484a);

        t.onSurface            = juce::Colour(0xffffffff);
        t.onSurfaceVariant     = juce::Colour(0xffebebf5).withAlpha(0.60f);
        t.outline              = juce::Colour(0xff545458).withAlpha(0.65f);
        t.outlineVariant       = juce::Colour(0xff38383a);
    }
    else
    {
        t.primary              = juce::Colour(0xff007aff);
        t.onPrimary            = juce::Colours::white;
        t.primaryContainer     = juce::Colour(0xff007aff).withAlpha(0.12f);
        t.onPrimaryContainer   = juce::Colour(0xff007aff);

        t.surface              = juce::Colour(0xfff2f2f7);
        t.surfaceContainerLow  = juce::Colours::white;
        t.surfaceContainer     = juce::Colour(0xffe5e5ea);
        t.surfaceContainerHigh = juce::Colour(0xffd1d1d6);

        t.onSurface            = juce::Colour(0xff000000);
        t.onSurfaceVariant     = juce::Colour(0xff3c3c43).withAlpha(0.60f);
        t.outline              = juce::Colour(0xff3c3c43).withAlpha(0.29f);
        t.outlineVariant       = juce::Colour(0xffc6c6c8);
    }

    t.error    = juce::Colour(0xffff453a);
    t.onError  = juce::Colours::white;
    return t;
}

struct ThemePreset
{
    const char* name = "";
    bool isDark = true;
};

inline const std::array<ThemePreset, 1>& themePresets()
{
    static const std::array<ThemePreset, 1> presets {{
        { "macOS Dark", true },
    }};
    return presets;
}

inline ThemeTokens& currentThemeTokens()
{
    static ThemeTokens tokens = makeHIGTokens(true);
    return tokens;
}

inline void applyAppTheme(int themeId)
{
    const int clamped = juce::jlimit(0, (int) themePresets().size() - 1, themeId);
    appThemeId().store(clamped);
    const auto& p = themePresets()[(size_t) clamped];
    currentThemeTokens() = makeHIGTokens(p.isDark);
    darkModeEnabled().store(p.isDark);
}

namespace colours {
    inline juce::Colour bg()            { return currentThemeTokens().surface; }
    inline juce::Colour bgLight()       { return currentThemeTokens().surfaceContainerLow; }
    inline juce::Colour bgLighter()     { return currentThemeTokens().surfaceContainer; }
    inline juce::Colour panel()         { return currentThemeTokens().surfaceContainerLow; }
    inline juce::Colour panelBorder()   { return currentThemeTokens().outline; }
    inline juce::Colour borderHover()   { return currentThemeTokens().outlineVariant; }
    inline juce::Colour separator()     { return currentThemeTokens().outline; }

    inline juce::Colour accent()        { return currentThemeTokens().primary; }
    inline juce::Colour accentDim()     { return currentThemeTokens().primaryContainer; }
    inline juce::Colour accentBright()  { return currentThemeTokens().onPrimaryContainer; }
    inline juce::Colour onPrimary()     { return currentThemeTokens().onPrimary; }

    inline juce::Colour text()          { return currentThemeTokens().onSurface; }
    inline juce::Colour textDim()       { return currentThemeTokens().onSurfaceVariant; }
    inline juce::Colour textBright()    { return currentThemeTokens().onSurface; }
    inline juce::Colour textMuted()     { return currentThemeTokens().onSurfaceVariant.withAlpha(0.75f); }

    inline juce::Colour controlFill()   { return juce::Colour(0xff787880).withAlpha(0.36f); }
    inline juce::Colour knobTrack()     { return currentThemeTokens().outlineVariant; }

    inline juce::Colour compSelectionHighlight() { return accent().withAlpha(0.28f); }

    inline juce::Colour pianoWhiteKey() { return juce::Colour(0xfff2f2f7); }
    inline juce::Colour pianoBlackKey() { return juce::Colour(0xff1c1c1e); }
    inline juce::Colour pianoGrid()     { return separator().withAlpha(0.35f); }
    inline juce::Colour noteBlock()     { return accent(); }
    inline juce::Colour selection()     { return accent().withAlpha(0.28f); }
    inline juce::Colour playhead()      { return juce::Colour(0xffff453a); }

    inline juce::Colour muteInactive()  { return controlFill(); }
    inline juce::Colour muteYellow()    { return juce::Colour(0xffff9f0a); }
    inline juce::Colour soloBlue()      { return accent(); }
    inline juce::Colour recordRed()     { return juce::Colour(0xffff453a); }
    inline juce::Colour activeGreen()   { return juce::Colour(0xff30d158); }

    inline juce::Colour muteRed()       { return muteYellow(); }
    inline juce::Colour soloGreen()     { return activeGreen(); }
}

namespace metrics {
    constexpr int browserWidth      = 230;
    constexpr int browserMinWidth   = 160;
    constexpr int browserMaxWidth   = 420;
    constexpr float uiScale         = 1.0f;

    constexpr float cornerRadius    = 8.0f;
    constexpr float groupedRadius   = 10.0f;
    constexpr float chipRadius      = 6.0f;
    constexpr float browserFontSize = 13.0f;

    constexpr int grid              = 8;
    constexpr int pluginPad         = 16;
    constexpr int sectionHeaderH    = 20;
    constexpr int toolbarH          = 32;
    constexpr int toolbarGap        = 8;
    constexpr int previewH          = 168;
    constexpr int previewControlsH  = 32;
    constexpr int editorHeaderH     = 52;
    constexpr int iconButtonSize    = 28;
    constexpr int comboTextPadding  = 8;
}

inline void styleSectionLabel(juce::Label& lbl, const juce::String& text)
{
    lbl.setText(text, juce::dontSendNotification);
    lbl.setFont(fontFor(TextStyle::Headline));
    lbl.setJustificationType(juce::Justification::centredLeft);
    lbl.setColour(juce::Label::textColourId, colours::textDim());
    lbl.setInterceptsMouseClicks(false, false);
}

namespace higIcon {
    inline constexpr const char* star         = "star";
    inline constexpr const char* chevronUp    = "chevron.up";
    inline constexpr const char* chevronDown  = "chevron.down";
    inline constexpr const char* scissors     = "scissors";
    inline constexpr const char* speakerSlash = "speaker.slash";
}

class PatternFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PatternFlowLookAndFeel();
    void refreshColours();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg,
                              bool highlighted, bool down) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    void drawFileBrowserRow(juce::Graphics&, int width, int height,
                            const juce::File& file, const juce::String& filename, juce::Image* icon,
                            const juce::String& fileSizeDescription,
                            const juce::String& fileTimeDescription,
                            bool isDirectory, bool isItemSelected,
                            int itemIndex,
                            juce::DirectoryContentsDisplayComponent& dcc) override;

    void drawTextEditorOutline(juce::Graphics&, int width, int height,
                               juce::TextEditor&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override;

    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton& btn, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static void drawSymbol(juce::Graphics& g, const juce::String& name,
                           juce::Rectangle<float> bounds, juce::Colour colour, float stroke = 1.5f);
};

} // namespace pflow
