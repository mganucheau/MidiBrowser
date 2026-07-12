#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "BuildInfo.h"

namespace pflow {

// ── Cupertino design tokens ──────────────────────────────────────────────────
// Single light native-macOS palette. Density + content size remain as Tweaks.

enum class Density { Compact, Comfortable };
enum class ContentSize { Small, Medium, Large };
enum class Appearance { System = 0, Light = 1, Dark = 2 };
enum class ScalePlacement { Top, Bottom };   // legacy; Pitch & Scale lives in inspector

constexpr int kNumContentSizes = 3;
constexpr int kNumAppearances = 3;
constexpr int kNumScalePlacements = 2;

struct ThemeTokens
{
    juce::Colour bg, panel, panel2, elev;
    juce::Colour line, lineStrong, lineSoft;
    juce::Colour text, text2, text3;
    juce::Colour rollBg, rollShade, rollRowline, rollNoteEdge, rollGhost;
    juce::Colour kbWhite, kbBlack;
    juce::Colour toolbarTop, toolbarBot, sidebarTop, sidebarBot;
    juce::Colour tableAlt, desktopTop, desktopBot;
    juce::Colour windowBorder;
};

struct AccentTokens
{
    juce::Colour accent;
    juce::Colour ink;
    juce::Colour soft;
    juce::Colour line;
    juce::Colour bright;
};

/** Flat effects-inspector palette (light / dark). */
struct InspectorTokens
{
    juce::Colour panelBg;
    juce::Colour accent;
    juce::Colour headerText;
    juce::Colour rowLabel;
    juce::Colour valueText;
    juce::Colour divider;
    juce::Colour sliderTrack;
    juce::Colour sliderKnob;
    juce::Colour controlSurface;
    juce::Colour controlSurfaceHi;
    juce::Colour controlSeparator;
    juce::Colour switchOffTrack;
    juce::Colour chevron;
    juce::Colour segmentText;
    juce::Colour controlHairline;
    bool dark = false;
};

const ThemeTokens& themeTokens();
const AccentTokens& accentTokens();
const InspectorTokens& inspectorTokens();

/** Resolves System → macOS setting; Light/Dark force appearance. */
bool usesDarkAppearance();

/** @deprecated Use usesDarkAppearance() */
inline bool inspectorUsesDarkPalette() { return usesDarkAppearance(); }

void drawInspectorControlSurface(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 bool over, bool down);
void drawInspectorDivider(juce::Graphics& g, juce::Rectangle<int> bounds);

/** App-level runtime Tweaks (density / content size / appearance). */
struct Tweaks
{
    std::atomic<int> density { (int) Density::Compact };
    std::atomic<int> size    { (int) ContentSize::Medium };
    std::atomic<int> appearance { (int) Appearance::System };
};

Tweaks& tweaks();

inline Density currentDensity()
{
    return (Density) juce::jlimit(0, 1, tweaks().density.load());
}

inline Appearance currentAppearance()
{
    return (Appearance) juce::jlimit(0, kNumAppearances - 1, tweaks().appearance.load());
}

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

// ── Fonts (system stack) ─────────────────────────────────────────────────────

juce::Font uiFont(float pt, bool semibold = false);
juce::Font monoFont(float pt, bool semibold = false);   // tabular-nums system font

enum class TextStyle
{
    LargeTitle,     // 28pt Semibold
    Title2,         // 17pt Semibold
    Headline,       // 14pt Semibold
    Body,           // 12pt Regular (Cupertino base)
    Callout,        // 13pt Regular
    Subheadline,    // 13pt Regular
    Footnote,       // 11pt Regular
    Caption         // 10.5pt Regular secondary
};

juce::Font fontFor(TextStyle s);

inline juce::Font systemFont(float pt, bool semibold = false) { return uiFont(pt, semibold); }

// ── Colour accessors ─────────────────────────────────────────────────────────

namespace colours {
    inline const ThemeTokens& th()  { return themeTokens(); }
    inline const AccentTokens& ac() { return accentTokens(); }

    inline juce::Colour bg()            { return th().bg; }
    inline juce::Colour panel()         { return th().panel; }
    inline juce::Colour panel2()        { return th().panel2; }
    inline juce::Colour elev()          { return th().elev; }
    inline juce::Colour line()          { return th().line; }
    inline juce::Colour lineStrong()    { return th().lineStrong; }
    inline juce::Colour lineSoft()      { return th().lineSoft; }

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

    inline juce::Colour toolbarTop()    { return th().toolbarTop; }
    inline juce::Colour toolbarBot()    { return th().toolbarBot; }
    inline juce::Colour sidebarTop()    { return th().sidebarTop; }
    inline juce::Colour sidebarBot()    { return th().sidebarBot; }
    inline juce::Colour tableAlt()      { return th().tableAlt; }
    inline juce::Colour desktopTop()    { return th().desktopTop; }
    inline juce::Colour desktopBot()    { return th().desktopBot; }
    inline juce::Colour windowBorder()  { return th().windowBorder; }

    inline juce::Colour playhead()      { return juce::Colour(0xffff5a52); }

    // Legacy aliases
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
    inline juce::Colour knobTrack()     { return usesDarkAppearance() ? juce::Colour(0xff48484a)
                                                                      : juce::Colour(0xffd5d2cc); }
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

    // Kept for compile compatibility; Cupertino uses a single light grid.
    inline juce::Colour bpBg()          { return rollBg(); }
    inline juce::Colour bpBar()         { return lineStrong(); }
    inline juce::Colour bpBeat()        { return lineSoft(); }
    inline juce::Colour bpRow()         { return rollShade(); }
    inline juce::Colour bpNote()        { return accent(); }
    inline juce::Colour bpEdge()        { return accentBright().withAlpha(0.55f); }
}

// ── Metrics ──────────────────────────────────────────────────────────────────

namespace metrics {
    constexpr int browserWidth      = 356;   // file table when other panes open
    constexpr int browserMinWidth   = 280;
    constexpr int browserMaxWidth   = 520;
    constexpr float uiScale         = 1.0f;

    constexpr float cornerRadius    = 10.0f;
    constexpr float windowRadius    = 12.0f;
    constexpr float groupedRadius   = 8.0f;
    constexpr float chipRadius      = 6.0f;
    constexpr float controlRadius   = 5.0f;
    constexpr float browserFontSize = 12.0f;

    constexpr int grid              = 8;
    constexpr int pluginPad         = 16;
    constexpr int sectionHeaderH    = 24;
    constexpr int toolbarH          = 46;
    constexpr int toolbarGap        = 8;
    constexpr int previewH          = 168;
    constexpr int previewControlsH  = 32;
    constexpr int editorHeaderH     = 48;
    constexpr int iconButtonSize    = 28;
    constexpr int comboTextPadding  = 6;

    constexpr int sidebarW          = 176;
    constexpr int sidebarRailW      = 48;
    constexpr int sidebarExpandedW  = 176;
    constexpr int fileTableW        = 356;
    constexpr int editorPaneW       = 470;
    constexpr int effectsPaneW      = 244;
    constexpr int openRollW         = editorPaneW;
    constexpr int openRollMinW      = 360;
    constexpr int foldedWindowW     = sidebarW + fileTableW;
    constexpr int openWindowW       = sidebarW + fileTableW + editorPaneW;

    inline int transportH()  { return toolbarH; }
    inline int listRowH()    { return currentDensity() == Density::Comfortable ? 28 : 24; }
    inline int listHeaderH() { return 28; }
    inline int padS()        { return currentDensity() == Density::Comfortable ? 12 : 8; }
    inline int miniRollH()   { return 112; }   // unused after mini-preview removal
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
