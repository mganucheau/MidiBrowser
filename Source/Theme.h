#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include "BuildInfo.h"

namespace pflow {

// ── MidiBrowser design tokens ────────────────────────────────────────────────
// Dark themes only: three themes × three accents, plus density and roll-grid
// style. Default graphite / amber / compact / minimal. Values ported from
// Prototype/mbd/app-d.jsx (THEMES_D / ACCENTS_D).

enum class ThemeId { Charcoal, Graphite, Ink };
enum class AccentId { Blue, Amber, Mint };
enum class Density { Compact, Comfortable };
enum class GridStyle { Lanes, Minimal, Blueprint };
enum class ContentSize { Small, Medium, Large };

constexpr int kNumThemes = 3;
constexpr int kNumAccents = 3;
constexpr int kNumGridStyles = 3;
constexpr int kNumContentSizes = 3;

struct ThemeTokens
{
    const char* name;
    juce::Colour bg, panel, panel2, elev;
    juce::Colour line, lineStrong;
    juce::Colour text, text2, text3;
    juce::Colour rollBg, rollShade, rollRowline, rollNoteEdge, rollGhost;
    juce::Colour kbWhite, kbBlack;
};

struct AccentTokens
{
    const char* name;
    juce::Colour accent;   // main accent
    juce::Colour ink;      // text/icon on accent
    juce::Colour soft;     // translucent fill
    juce::Colour line;     // translucent border
    juce::Colour bright;   // selection rings / knob pointer
};

const ThemeTokens& themeTokens(ThemeId t);
const AccentTokens& accentTokens(AccentId a);

/** App-level runtime Tweaks (theme / accent / density / grid style). */
struct Tweaks
{
    std::atomic<int> theme   { (int) ThemeId::Graphite };
    std::atomic<int> accent  { (int) AccentId::Amber };
    std::atomic<int> density { (int) Density::Compact };
    std::atomic<int> grid    { (int) GridStyle::Minimal };
    std::atomic<int> size    { (int) ContentSize::Large };
};

Tweaks& tweaks();

inline ThemeId currentTheme()     { return (ThemeId) juce::jlimit(0, kNumThemes - 1, tweaks().theme.load()); }
inline AccentId currentAccent()   { return (AccentId) juce::jlimit(0, kNumAccents - 1, tweaks().accent.load()); }
inline Density currentDensity()   { return (Density) juce::jlimit(0, 1, tweaks().density.load()); }
inline GridStyle currentGrid()    { return (GridStyle) juce::jlimit(0, kNumGridStyles - 1, tweaks().grid.load()); }

/** UI scale for the Small / Medium / Large content-size tweak. */
inline float contentScale()
{
    switch ((ContentSize) juce::jlimit(0, kNumContentSizes - 1, tweaks().size.load()))
    {
        case ContentSize::Small:  return 0.85f;
        case ContentSize::Medium: return 1.0f;
        case ContentSize::Large:  return 1.2f;
    }
    return 1.0f;
}

// ── Fonts ────────────────────────────────────────────────────────────────────
// Schibsted Grotesk for UI, JetBrains Mono for numbers / paths (embedded).

juce::Font uiFont(float pt, bool semibold = false);
juce::Font monoFont(float pt, bool semibold = false);

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

juce::Font fontFor(TextStyle s);

inline juce::Font systemFont(float pt, bool semibold = false) { return uiFont(pt, semibold); }

// ── Colour accessors (always reflect the current tweaks) ────────────────────

namespace colours {
    inline const ThemeTokens& th()  { return themeTokens(currentTheme()); }
    inline const AccentTokens& ac() { return accentTokens(currentAccent()); }

    inline juce::Colour bg()            { return th().bg; }
    inline juce::Colour panel()         { return th().panel; }
    inline juce::Colour panel2()        { return th().panel2; }
    inline juce::Colour elev()          { return th().elev; }
    inline juce::Colour line()          { return th().line; }
    inline juce::Colour lineStrong()    { return th().lineStrong; }

    inline juce::Colour text()          { return th().text; }
    inline juce::Colour text2()         { return th().text2; }
    inline juce::Colour text3()         { return th().text3; }

    inline juce::Colour accent()        { return ac().accent; }
    inline juce::Colour accentInk()     { return ac().ink; }
    inline juce::Colour accentSoft()    { return ac().soft; }
    inline juce::Colour accentLine()    { return ac().line; }
    inline juce::Colour accentBright()  { return ac().bright; }

    inline juce::Colour rollBg()        { return th().rollBg; }
    inline juce::Colour rollShade()     { return th().rollShade; }
    inline juce::Colour rollRowline()   { return th().rollRowline; }
    inline juce::Colour rollNoteEdge()  { return th().rollNoteEdge; }
    inline juce::Colour rollGhost()     { return th().rollGhost; }
    inline juce::Colour kbWhite()       { return th().kbWhite; }
    inline juce::Colour kbBlack()       { return th().kbBlack; }

    inline juce::Colour playhead()      { return juce::Colour(0xffff5a52); }

    // Blueprint grid-style fixed tokens
    inline juce::Colour bpBg()          { return juce::Colour(0xff0c1a25); }
    inline juce::Colour bpBar()         { return juce::Colour(0xff6ec8ff).withAlpha(0.40f); }
    inline juce::Colour bpBeat()        { return juce::Colour(0xff6ec8ff).withAlpha(0.13f); }
    inline juce::Colour bpRow()         { return juce::Colour(0xff6ec8ff).withAlpha(0.06f); }
    inline juce::Colour bpNote()        { return juce::Colour(0xff46c2ff); }
    inline juce::Colour bpEdge()        { return juce::Colour(0xffbeebff).withAlpha(0.55f); }

    // Legacy aliases still used by pre-spec components.
    inline juce::Colour bgLight()       { return panel2(); }
    inline juce::Colour bgLighter()     { return elev(); }
    inline juce::Colour panelBorder()   { return line(); }
    inline juce::Colour borderHover()   { return lineStrong(); }
    inline juce::Colour separator()     { return line(); }
    inline juce::Colour accentDim()     { return accentSoft(); }
    inline juce::Colour onPrimary()     { return accentInk(); }
    inline juce::Colour textDim()       { return text2(); }
    inline juce::Colour textBright()    { return text(); }
    inline juce::Colour textMuted()     { return text3(); }
    inline juce::Colour controlFill()   { return elev(); }
    inline juce::Colour knobTrack()     { return lineStrong(); }
    inline juce::Colour compSelectionHighlight() { return accentSoft(); }
    inline juce::Colour pianoWhiteKey() { return kbWhite(); }
    inline juce::Colour pianoBlackKey() { return kbBlack(); }
    inline juce::Colour pianoGrid()     { return rollRowline(); }
    inline juce::Colour noteBlock()     { return accent(); }
    inline juce::Colour selection()     { return accentSoft(); }
    inline juce::Colour muteInactive()  { return controlFill(); }
    inline juce::Colour muteYellow()    { return juce::Colour(0xffff9f0a); }
    inline juce::Colour soloBlue()      { return accent(); }
    inline juce::Colour recordRed()     { return playhead(); }
    inline juce::Colour activeGreen()   { return juce::Colour(0xff30d158); }
    inline juce::Colour muteRed()       { return muteYellow(); }
    inline juce::Colour soloGreen()     { return activeGreen(); }
}

// ── Metrics ──────────────────────────────────────────────────────────────────

namespace metrics {
    constexpr int browserWidth      = 200;   // slim file list; meta sits tight to the name
    constexpr int browserMinWidth   = 150;
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
    constexpr int comboTextPadding  = 6;

    constexpr int sidebarRailW      = 48;
    constexpr int sidebarExpandedW  = 168;
    constexpr int foldedWindowW     = 300;
    constexpr int openWindowW       = 980;

    // Density-scaled values
    inline int transportH()  { return currentDensity() == Density::Comfortable ? 52 : 46; }
    inline int listRowH()    { return currentDensity() == Density::Comfortable ? 28 : 24; }
    inline int listHeaderH() { return currentDensity() == Density::Comfortable ? 34 : 30; }
    inline int padS()        { return currentDensity() == Density::Comfortable ? 10 : 8; }
    // Folded (editor closed) mini preview: 2× the previous lane height so notes read clearly.
    inline int miniRollH()   { return currentDensity() == Density::Comfortable ? 256 : 224; }
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
