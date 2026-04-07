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

    // Clip start offset: draggable start point within the clip (beats)
    double clipStartOffset = 0.0;

    juce::MidiMessageSequence toMidiSequence(double bpm) const;

    // Get notes filtered by note number (for per-note comping)
    std::vector<NoteEvent> getNotesForPitch(int noteNumber) const;
    std::vector<int> getDistinctPitches() const;
};

// ── Parse a standard MIDI file ───────────────────────────────────────────────
MidiClip parseMidiFile(const juce::File& file);

// ── Write a MidiClip to a standard MIDI file ────────────────────────────────
bool writeMidiFile(const MidiClip& clip, const juce::File& file, double bpm = 120.0);

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

// ── Comp system (Quick Swipe Comping) ────────────────────────────────────────
//
// Each lane holds clips placed at beat positions.  The comp system lets the user
// select which lane is active at each point in time by swiping across take lanes.
//
// CompSegment: a half-open interval [startBeat, endBeat) on a specific lane
// that is "selected" (active) in a comp.  Segments are non-overlapping across
// lanes — at any beat position, at most ONE lane is active.
//
// Comp: a named collection of CompSegments forming one composite take.
// Multiple comps can exist (Comp A, Comp B, ...) with independent selections.

struct CompSegment
{
    int    laneIndex  = 0;
    double startBeat  = 0.0;
    double endBeat    = 4.0;     // half-open: [startBeat, endBeat)
};

struct Comp
{
    juce::String               name { "Comp A" };
    std::vector<CompSegment>   segments;   // sorted by startBeat, non-overlapping

    // Apply a swipe: select laneIndex for [startBeat, endBeat).
    // Removes/trims any existing segments that overlap this range,
    // adds the new segment, and merges adjacent segments on the same lane.
    void swipe(int laneIndex, double startBeat, double endBeat);

    // Get the active lane index at a given beat, or -1 if no lane is active (gap)
    int activeLaneAtBeat(double beat) const;

    // Sort segments and merge adjacent ones on the same lane
    void sortAndMerge();
};

struct CompLane
{
    juce::String            name;
    juce::Colour            colour { 0xff3a7bd5 };
    std::vector<MidiClip>   clips;      // clips placed on this lane
    std::vector<double>     clipStarts; // beat position where each clip starts
    bool                    muted    = false;
    bool                    solo     = false;

    // Add a clip at a beat position
    void addClip(const MidiClip& clip, double beatPos);
    // Remove clip by index
    void removeClip(int index);
    // Get the clip and its start position that covers a given beat, or nullptr
    const MidiClip* clipAtBeat(double beat, double& clipStart) const;
    int clipIndexAtBeat(double beat) const;
};

// ── MIDI split routing ───────────────────────────────────────────────────────
struct MidiSplitRule
{
    int noteMin     = 0;
    int noteMax     = 127;
    int outputIndex = 0;   // virtual output bus 0..15
};

} // namespace pflow
