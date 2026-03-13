#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <map>

namespace pflow {

// ── Single note event ────────────────────────────────────────────────────────
struct NoteEvent
{
    int    noteNumber   = 60;
    int    velocity     = 100;
    double startBeat    = 0.0;
    double lengthBeats  = 1.0;
    int    channel      = 1;
};

// ── A loaded MIDI clip ───────────────────────────────────────────────────────
struct MidiClip
{
    juce::String       name;
    juce::String       filePath;
    double             lengthBeats = 4.0;
    std::vector<NoteEvent> notes;
    juce::Colour       colour { 0xff3a7bd5 };

    // Scale / transpose helpers
    int  rootNoteOffset = 0;  // semitones from original

    juce::MidiMessageSequence toMidiSequence(double bpm) const;

    // Get notes filtered by note number (for per-note comping)
    std::vector<NoteEvent> getNotesForPitch(int noteNumber) const;
    std::vector<int> getDistinctPitches() const;
};

// ── Parse a standard MIDI file ───────────────────────────────────────────────
MidiClip parseMidiFile(const juce::File& file);

// ── Scale definitions ────────────────────────────────────────────────────────
enum class ScaleType
{
    Chromatic, Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    PentatonicMajor, PentatonicMinor, Blues, WholeTone,
    Count
};

juce::String scaleTypeName(ScaleType s);
std::vector<int> scaleIntervals(ScaleType s);

// Map an incoming note to the nearest note within a given scale + root
int quantiseToScale(int inNote, int scaleRoot, ScaleType scale);

// ── Comp lane model ──────────────────────────────────────────────────────────
struct CompRegion
{
    double startBeat  = 0.0;
    double endBeat    = 4.0;
    int    clipIndex  = -1;       // index into the lane's clip list
    int    noteFilter = -1;       // -1 = all notes, >=0 = specific pitch only
    bool   muted      = false;
};

struct CompLane
{
    juce::String            name;
    juce::Colour            colour { 0xff3a7bd5 };
    std::vector<MidiClip>   clips;
    std::vector<CompRegion> regions;
    bool                    expanded = false;   // show sub-comp lanes?
    bool                    muted    = false;
    bool                    solo     = false;

    void addClipAtPosition(const MidiClip& clip, double beatPos);
    void removeRegion(int index);
};

// ── MIDI split routing ───────────────────────────────────────────────────────
struct MidiSplitRule
{
    int noteMin     = 0;
    int noteMax     = 127;
    int outputIndex = 0;   // virtual output bus 0..15
};

} // namespace pflow
