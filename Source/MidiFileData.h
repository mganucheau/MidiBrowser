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
