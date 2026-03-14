#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace pflow {

// ── Dark mode toggle (true = dark mode default, matching DAW convention) ─────
inline std::atomic<bool>& darkModeEnabled()
{
    static std::atomic<bool> enabled { true };
    return enabled;
}

// ── Colour palette ───────────────────────────────────────────────────────────
// Light mode: clean white/gray. Dark mode: deep navy DAW palette.
// ── Core colour theme ────────────────────────────────────────────────────────
//
//  LIGHT MODE                          DARK MODE
//  ──────────────────────────          ──────────────────────────
//  Background:                         Background:
//    bg           #EAEEF2                bg           #181B24
//    bgLight      #F5F7FA                bgLight      #212636
//    bgLighter    #DDE3E9                bgLighter    #2C3244
//    panel        #F0F2F5                panel        #1C2030
//    panelBorder  #BFC6CE                panelBorder  #333A4D
//
//  Accent (blue):                      Accent (blue):
//    accent       #2563EB                accent       #5B9BD5
//    accentDim    #1D4ED8                accentDim    #3D7AB8
//    accentBright #3B82F6                accentBright #7EC8FF
//
//  Text:                               Text:
//    text         #1E293B                text         #D8DDE6
//    textDim      #4B5563                textDim      #8890A0
//    textBright   #0F172A                textBright   #FFFFFF
//
//  Knob:                               Knob:
//    knobTrack    #C3CAD2                knobTrack    #3A4058
//
//  Piano roll:                         Piano roll:
//    pianoWhite   #EDF0F3                pianoWhite   #2A2F40
//    pianoBlack   #D0D6DD                pianoBlack   #1E2234
//    pianoGrid    #A8B0B9                pianoGrid    #404860
//    noteBlock    #2563EB                noteBlock    #5B9BD5
//    selection    #2563EB 40%            selection    #5B9BD5 40%
//    playhead     #DC2626                playhead     #FFFFFF
//
namespace colours {
    // Backgrounds – deep navy with blue undertones (matching DAW reference)
    inline juce::Colour bg()            { return darkModeEnabled() ? juce::Colour(0xff181b24) : juce::Colour(0xffeaeef2); }
    inline juce::Colour bgLight()       { return darkModeEnabled() ? juce::Colour(0xff212636) : juce::Colour(0xfff5f7fa); }
    inline juce::Colour bgLighter()     { return darkModeEnabled() ? juce::Colour(0xff2c3244) : juce::Colour(0xffdde3e9); }
    inline juce::Colour panel()         { return darkModeEnabled() ? juce::Colour(0xff1c2030) : juce::Colour(0xfff0f2f5); }
    inline juce::Colour panelBorder()   { return darkModeEnabled() ? juce::Colour(0xff333a4d) : juce::Colour(0xffbfc6ce); }

    // Accent (blue)
    inline juce::Colour accent()        { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2563eb); }
    inline juce::Colour accentDim()     { return darkModeEnabled() ? juce::Colour(0xff3d7ab8) : juce::Colour(0xff1d4ed8); }
    inline juce::Colour accentBright()  { return darkModeEnabled() ? juce::Colour(0xff7ec8ff) : juce::Colour(0xff3b82f6); }

    // Text
    inline juce::Colour text()          { return darkModeEnabled() ? juce::Colour(0xffd8dde6) : juce::Colour(0xff1e293b); }
    inline juce::Colour textDim()       { return darkModeEnabled() ? juce::Colour(0xff8890a0) : juce::Colour(0xff4b5563); }
    inline juce::Colour textBright()    { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xff0f172a); }

    // Knob / slider track
    inline juce::Colour knobTrack()     { return darkModeEnabled() ? juce::Colour(0xff3a4058) : juce::Colour(0xffc3cad2); }

    // Lane colours – DAW mixer-strip palette (pink, green, blue, cyan, amber, purple)
    inline juce::Colour laneA()         { return juce::Colour(0xffe85577); } // Coral-pink
    inline juce::Colour laneB()         { return juce::Colour(0xff4caf50); } // Green
    inline juce::Colour laneC()         { return juce::Colour(0xff4a90d9); } // Blue
    inline juce::Colour laneD()         { return juce::Colour(0xff26c6da); } // Cyan
    inline juce::Colour laneE()         { return juce::Colour(0xfffdd835); } // Amber

    // Piano roll
    inline juce::Colour pianoWhiteKey() { return darkModeEnabled() ? juce::Colour(0xff2a2f40) : juce::Colour(0xffedf0f3); }
    inline juce::Colour pianoBlackKey() { return darkModeEnabled() ? juce::Colour(0xff1e2234) : juce::Colour(0xffd0d6dd); }
    inline juce::Colour pianoGrid()     { return darkModeEnabled() ? juce::Colour(0xff404860) : juce::Colour(0xffa8b0b9); }
    inline juce::Colour noteBlock()     { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2563eb); }
    inline juce::Colour selection()     { return darkModeEnabled() ? juce::Colour(0x665b9bd5) : juce::Colour(0x662563eb); }
    inline juce::Colour playhead()      { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xffdc2626); }
}

// ── Clip colour presets (DAW mixer-strip palette) ────────────────────────────
inline std::vector<juce::Colour> getClipColourPresets()
{
    return {
        juce::Colour(0xffe85577), // Coral-pink
        juce::Colour(0xff4caf50), // Green
        juce::Colour(0xff4a90d9), // Blue
        juce::Colour(0xff26c6da), // Cyan
        juce::Colour(0xfffdd835), // Amber
        juce::Colour(0xffab47bc), // Purple
        juce::Colour(0xffff7043), // Orange
        juce::Colour(0xff26a69a), // Teal
        juce::Colour(0xffec407a), // Rose
        juce::Colour(0xff5c6bc0), // Indigo
    };
}

// ── Version ─────────────────────────────────────────────────────────────────
namespace version {
    constexpr const char* number = "0.4.0";
    constexpr const char* name   = "PatternFlow";
    constexpr const char* desc   = "A MIDI composition and comping tool for creative producers. "
                                   "Arrange, layer, and reshape MIDI clips with per-note comping, "
                                   "scale quantisation, humanisation, and flexible routing.";
    constexpr const char* license =
        "Commercial License\n\n"
        "Copyright (c) 2024-2026 PatternFlow. All rights reserved.\n\n"
        "This software is licensed, not sold. You are granted a non-exclusive, "
        "non-transferable license to use this software for personal and commercial "
        "music production. Redistribution, reverse engineering, or modification of "
        "the software is prohibited without prior written consent from the author.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND.";
}

// ── Metrics ──────────────────────────────────────────────────────────────────
namespace metrics {
    constexpr int browserWidth      = 240;
    constexpr int controlPanelH     = 90;
    constexpr int pianoRollH        = 280;
    constexpr int laneHeaderW       = 130;
    constexpr int laneHeight        = 60;
    constexpr int compLaneHeight    = 36;
    constexpr int knobSize          = 52;
    constexpr int knobLabelH        = 16;
    constexpr int knobSpacing       = 68;
    constexpr float cornerRadius    = 5.0f;
    constexpr float clipCorner      = 4.0f;
    constexpr int scrollbarW        = 7;
    constexpr int buttonH           = 26;
    constexpr int padding           = 10;
    constexpr float browserFontSize = 11.5f;
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

    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont(juce::Label&) override;
};

} // namespace pflow
