#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include "BuildInfo.h"

namespace pflow {

// ── Material Design 3-inspired theming ───────────────────────────────────────
// We use token-based colour roles (surface, primary, outline, etc.) and map
// the app's existing `colours::...()` helpers onto those roles.

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

// ── Typography scale (M3-inspired) ───────────────────────────────────────────
enum class TextStyle
{
    DisplaySmall,
    HeadlineSmall,
    TitleLarge,
    TitleMedium,
    BodyLarge,
    BodyMedium,
    LabelLarge,
    LabelMedium,
    LabelSmall
};

inline juce::Font fontFor(TextStyle s)
{
    // Sizes tuned for a compact audio tool UI while keeping M3 hierarchy.
    switch (s)
    {
        case TextStyle::DisplaySmall:   return juce::Font(juce::FontOptions(29.0f).withStyle("Bold"));
        case TextStyle::HeadlineSmall:  return juce::Font(juce::FontOptions(21.0f).withStyle("SemiBold"));
        case TextStyle::TitleLarge:     return juce::Font(juce::FontOptions(17.0f).withStyle("SemiBold"));
        case TextStyle::TitleMedium:    return juce::Font(juce::FontOptions(15.0f).withStyle("SemiBold"));
        case TextStyle::BodyLarge:      return juce::Font(juce::FontOptions(14.0f));
        case TextStyle::BodyMedium:     return juce::Font(juce::FontOptions(13.0f));
        case TextStyle::LabelLarge:     return juce::Font(juce::FontOptions(15.0f).withStyle("SemiBold"));
        case TextStyle::LabelMedium:    return juce::Font(juce::FontOptions(12.0f).withStyle("SemiBold"));
        case TextStyle::LabelSmall:     return juce::Font(juce::FontOptions(11.0f));
    }
    return juce::Font(juce::FontOptions(12.0f));
}

inline std::atomic<int>& appThemeId()
{
    static std::atomic<int> id { 0 }; // Single DAW theme
    return id;
}

inline std::atomic<bool>& darkModeEnabled()
{
    static std::atomic<bool> enabled { true }; // kept for legacy call sites
    return enabled;
}

inline juce::Colour mix(juce::Colour a, juce::Colour b, float t)
{
    return a.interpolatedWith(b, juce::jlimit(0.0f, 1.0f, t));
}

inline ThemeTokens makeM3Tokens(juce::Colour seedPrimary, bool dark)
{
    ThemeTokens t;
    t.isDark = dark;

    // Surface palette (simple, deterministic derivation; avoids runtime dependency on Material Color Utilities)
    const auto white = juce::Colours::white;
    const auto black = juce::Colours::black;

    // Balance chroma so themes don't swing between neon and muted.
    // Pull seed a bit toward the neutral onSurfaceVariant.
    const auto neutralPull = dark ? juce::Colour(0xffb9c0cc) : juce::Colour(0xff44474f);
    t.primary = mix(seedPrimary, neutralPull, 0.18f);
    t.onPrimary = dark ? juce::Colour(0xff0b0f14) : juce::Colours::white;
    t.primaryContainer = dark ? mix(seedPrimary, black, 0.55f) : mix(seedPrimary, white, 0.70f);
    t.onPrimaryContainer = dark ? mix(seedPrimary, white, 0.70f) : mix(seedPrimary, black, 0.82f);

    // FigmaExample is a fixed DAW palette: in dark mode, prefer the canonical blue accent.
    if (dark)
    {
        t.primary = juce::Colour(0xff3b82f6);                // blue-500
        t.onPrimary = juce::Colours::white;
        t.primaryContainer = juce::Colour(0xff1e3a8a);       // blue-900 (used as "active tint" source)
        t.onPrimaryContainer = juce::Colour(0xff60a5fa);     // blue-400/300-ish for active text
    }

    // DAW-style surfaces (matches Design/FigmaExample palette + style guide).
    // Light themes still use a softer M3-like palette; dark is deliberately "studio neutral".
    t.surface = dark ? juce::Colour(0xff1a1a1a) : juce::Colour(0xfffffbfe);
    t.surfaceContainerLow  = dark ? juce::Colour(0xff1e1e1e) : juce::Colour(0xfff7f2fa);
    t.surfaceContainer     = dark ? juce::Colour(0xff252525) : juce::Colour(0xfff2ecf5);
    t.surfaceContainerHigh = dark ? juce::Colour(0xff2a2a2a) : juce::Colour(0xffece6ef);

    t.onSurface = dark ? juce::Colours::white : juce::Colour(0xff1a1c1e);
    t.onSurfaceVariant = dark ? juce::Colour(0xffa0a0a0) : juce::Colour(0xff44474f);

    // Borders/separators
    t.outline = dark ? juce::Colour(0xff444444) : juce::Colour(0xff74777f);
    t.outlineVariant = dark ? juce::Colour(0xff2d2d2d) : juce::Colour(0xffc4c6d0);

    t.error = juce::Colour(0xffef4444);
    t.onError = juce::Colours::white;

    return t;
}

struct ThemePreset
{
    const char* name = "";
    bool isDark = true;
    juce::Colour seedPrimary;
};

inline const std::array<ThemePreset, 1>& themePresets()
{
    static const std::array<ThemePreset, 1> presets {{
        { "Dark - DAW (FigmaExample)", true, juce::Colour(0xff3b82f6) },
    }};
    return presets;
}

inline ThemeTokens& currentThemeTokens()
{
    static ThemeTokens tokens = makeM3Tokens(themePresets()[0].seedPrimary, true);
    return tokens;
}

inline void applyAppTheme(int themeId)
{
    const int clamped = juce::jlimit(0, (int)themePresets().size() - 1, themeId);
    appThemeId().store(clamped);
    const auto& p = themePresets()[(size_t)clamped];
    currentThemeTokens() = makeM3Tokens(p.seedPrimary, p.isDark);
    darkModeEnabled().store(p.isDark);
}

// ── Colour palette ───────────────────────────────────────────────────────────
// Material Design 3-inspired token roles + JUCE component mapping.
//
//  DEFAULT THEMES
//  ──────────────
//  Backgrounds:
//    bg           #1a1a1a  Main dark background
//    bgLight      #1e1e1e  Panels, tracks
//    bgLighter    #252525  Headers, sidebars, elevated
//    panel        #252525  Panel backgrounds
//    panelBorder  #2d2d2d  Primary border
//    borderHover  #444     Hover border
//
//  Accent (blue – selection/info):
//    accent       #3b82f6
//    accentDim    #2563eb
//    accentBright #60a5fa
//
//  Text:
//    text         #ffffff  Primary
//    textDim      #a0a0a0  Secondary
//    textBright   #ffffff
//    textMuted    #717182  Very subtle
//
//  Status:
//    recordRed    #ef4444  Record, critical
//    muteYellow   #facc15  Mute active
//    soloBlue     #3b82f6  Solo active
//    activeGreen  #22c55e  Active, safe
//
namespace colours {
    inline juce::Colour bg()            { return currentThemeTokens().surface; }
    inline juce::Colour bgLight()       { return currentThemeTokens().surfaceContainerLow; }
    inline juce::Colour bgLighter()     { return currentThemeTokens().surfaceContainer; }
    inline juce::Colour panel()         { return currentThemeTokens().surfaceContainer; }
    inline juce::Colour panelBorder()   { return currentThemeTokens().outlineVariant; }
    inline juce::Colour borderHover()   { return currentThemeTokens().outline; }

    inline juce::Colour accent()        { return currentThemeTokens().primary; }
    inline juce::Colour accentDim()     { return currentThemeTokens().primaryContainer; }
    inline juce::Colour accentBright()
    {
        // Brighter, but avoid washing out in light themes.
        return mix(currentThemeTokens().primary, juce::Colours::white, currentThemeTokens().isDark ? 0.22f : 0.06f);
    }

    inline juce::Colour text()          { return currentThemeTokens().onSurface; }
    inline juce::Colour textDim()       { return currentThemeTokens().onSurfaceVariant; }
    inline juce::Colour textBright()    { return currentThemeTokens().onSurface; }
    inline juce::Colour textMuted()     { return currentThemeTokens().outline; }

    inline juce::Colour knobTrack()     { return currentThemeTokens().outlineVariant; }

    inline juce::Colour compSelectionHighlight() { return accent().withAlpha(0.22f); }

    // Piano roll — realistic piano key colours (high-contrast black & white)
    inline juce::Colour pianoWhiteKey() { return currentThemeTokens().isDark ? juce::Colour(0xffe8e8e8) : juce::Colour(0xfff8f8f8); }
    inline juce::Colour pianoBlackKey() { return currentThemeTokens().isDark ? juce::Colour(0xff1a1a1a) : juce::Colour(0xff1a1a1a); }
    inline juce::Colour pianoGrid()     { return currentThemeTokens().outlineVariant; }
    inline juce::Colour noteBlock()     { return currentThemeTokens().isDark ? juce::Colour(0xffd0d0d0) : juce::Colour(0xff2a2a2a); }
    inline juce::Colour selection()     { return accent().withAlpha(0.25f); }
    inline juce::Colour playhead()      { return juce::Colour(0xffef4444); } // Red playhead per DAW style guide

    // Lane mute/solo – Mute=Orange (less "warning"), Solo=Blue
    /** Preview / UI mute (dark neutral — use in file preview, etc.). */
    inline juce::Colour muteInactive()  { return juce::Colour(0xff3d3d3d); }
    inline juce::Colour muteYellow()    { return juce::Colour(0xfff59e0b); } // orange-500 (lanes)
    inline juce::Colour soloBlue()      { return juce::Colour(0xff3b82f6); }
    inline juce::Colour recordRed()     { return juce::Colour(0xffef4444); }
    inline juce::Colour activeGreen()   { return juce::Colour(0xff22c55e); }

    // Legacy aliases for compatibility
    inline juce::Colour muteRed()       { return muteYellow(); }
    inline juce::Colour soloGreen()     { return activeGreen(); }   // Legacy name; solo is now blue in UI
}

// ── Elevation helpers (M3-inspired) ──────────────────────────────────────────
inline juce::Colour elevatedSurface(juce::Colour surface, int dp)
{
    // In M3 dark theme, elevation is represented via a surface tint overlay.
    // Approximation: blend primary into surface with increasing alpha.
    if (!currentThemeTokens().isDark || dp <= 0)
        return surface;

    const float t = juce::jlimit(0.0f, 1.0f, (float)dp / 6.0f);
    const float alpha = 0.04f + 0.10f * t; // ~4%..14%
    return surface.overlaidWith(currentThemeTokens().primary.withAlpha(alpha));
}

// ── Clip colour presets (DAW Professional track palette) ─────────────────────
inline std::vector<juce::Colour> getClipColourPresets()
{
    return {
        juce::Colour(0xffff6b6b), // Vocals
        juce::Colour(0xff4ecdc4), // Guitar
        juce::Colour(0xff45b7d1), // Bass
        juce::Colour(0xfff9ca24), // Drums
        juce::Colour(0xffa29bfe), // Keys
        juce::Colour(0xffff9ff3), // Synth
        juce::Colour(0xff4caf50), // Green
        juce::Colour(0xff3b82f6), // Blue
        juce::Colour(0xff26c6da), // Cyan
        juce::Colour(0xfff59e0b), // Orange
    };
}

// ── Version ─────────────────────────────────────────────────────────────────
namespace version {
    constexpr const char* name = "PatternFlow";
    constexpr const char* desc = "A MIDI composition tool for creative producers. "
                                   "Arrange, layer, and reshape MIDI clips with scale quantisation, "
                                   "humanisation, and flexible routing.";
    constexpr const char* license =
        "Commercial License\n\n"
        "Copyright (c) 2024-2026 PatternFlow. All rights reserved.\n\n"
        "This software is licensed, not sold. You are granted a non-exclusive, "
        "non-transferable license to use this software for personal and commercial "
        "music production. Redistribution, reverse engineering, or modification of "
        "the software is prohibited without prior written consent from the author.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND.";
}

// ── Metrics (DAW Professional Style Guide) ───────────────────────────────────
namespace metrics {
    // Slightly narrower default browser column (~10% vs the original w-64 baseline)
    constexpr int browserWidth      = 230;
    constexpr int browserMinWidth   = 160;
    constexpr int browserMaxWidth   = 420;
    constexpr int controlPanelH     = 40;   // Transport (reduced by 2/3)
    constexpr int pianoRollH        = 420;  // 50% taller than 280
    constexpr int laneHeaderW       = 120;  // Lane header (compact: name + M/S)
    constexpr int laneHeight        = 96;   // Default track h
    constexpr int laneHeightMin     = 40;   // Min when many lanes
    constexpr int compLaneHeight    = 36;
    // Global UI scale (requested +0.5x = 1.5x)
    constexpr float uiScale         = 1.5f;

    constexpr int knobSize          = (int)(52 * uiScale);
    constexpr int knobLabelH        = (int)(16 * uiScale);
    constexpr int knobSpacing       = (int)(68 * uiScale);
    // FigmaExample buttons/inputs use a tighter radius than the older M3 preset.
    constexpr float cornerRadius    = 6.0f;
    constexpr float clipCorner      = 8.0f;
    constexpr int scrollbarW        = 12;   // Custom scrollbar width
    constexpr int buttonH           = (int)(32 * uiScale);   // h-8 scaled
    constexpr int padding           = (int)(12 * uiScale);   // p-3 scaled
    constexpr float browserFontSize = 13.0f; // +1pt vs previous 12pt
    // Header + control strip heights (+1/8 vs prior 0.75× baseline)
    constexpr int titleBarH         = (int)(48 * uiScale * 0.75f * 1.125f);
    constexpr int controlStripH     = (int)(64 * uiScale * 0.75f * 1.125f);
    constexpr int titleBarPadding   = 4;
    constexpr int comboTextPadding  = 6;
    constexpr int rulerH            = 32;   // Timeline ruler h-8
}

// ── Custom LookAndFeel ───────────────────────────────────────────────────────
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
};

} // namespace pflow
