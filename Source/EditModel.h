#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <map>
#include <set>
#include <vector>

#include "MidiFileData.h"

namespace pflow {

// ── Step-domain clip model (16th-note grid, 4/4) ─────────────────────────────
// Port of Prototype/mb/data.jsx. All edits are non-destructive: the source
// clip is never mutated; every change lives in a ClipEdit and is applied as a
// display/playback transform by resolveClip().

constexpr int kStepsPerBar = 16;

struct RollNote
{
    int    id    = 0;
    int    pitch = 60;      // MIDI note number (60 = C4)
    double start = 0.0;     // 16th steps from clip start
    double len   = 1.0;     // 16th steps
    int    fileVelocity = 100;  // velocity as loaded from disk (0–127)
    int    channel = 1;
    bool   moved = false;   // set by resolveClip when a manual move applies
};

enum class ClipKind { Bass, Keys, Drums };

struct StepClip
{
    juce::String name;
    juce::String filePath;
    ClipKind kind = ClipKind::Keys;
    int    root = -1;       // pitch-class 0..11, -1 = unknown
    double bpm  = 120.0;
    int    bars = 1;
    std::vector<RollNote> notes;
};

// ── Music theory ─────────────────────────────────────────────────────────────

extern const std::array<const char*, 12> kNoteNames;   // 'C'..'B' (sharps)

enum class Mode { Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian };
constexpr int kNumModes = 7;

const char* modeName(Mode m);
const std::array<int, 7>& modeIntervals(Mode m);

juce::String pitchName(int midi);           // MIDI 60 = "C4"
bool isBlackKeyPitch(int midi);

/** True when midi pitch-class belongs to scale(rootPc, mode). */
bool pitchInScale(int midi, int rootPc, Mode mode);
int  noteNameIndex(const juce::String& name);   // "C"..."B" -> 0..11, else -1

/** Snap a pitch to the nearest tone of scale(root, mode), preserving register.
    Searches ±6 semitones; on distance ties the lower candidate wins. */
int fitToScale(int midi, int rootPc, Mode mode);

/** Scale degree label for a pitch class in key/mode, e.g. "3rd". Empty if out of scale. */
juce::String scaleDegreeLabel(int midi, int rootPc, Mode mode);

/** Note + degree annotation for the inspector, e.g. "B4 (3rd)". */
juce::String pitchScaleAnnotation(int midi, int rootPc, Mode mode);

// ── Edit record (one per clip; the clip itself is never touched) ─────────────

struct NoteMove { int dPitch = 0; int dStep = 0; };

struct ClipEdit
{
    int  octave    = 0;
    int  pitchShift = 0;   // whole-file semitone transpose, -12..12
    bool fitScale  = false;
    bool mapToRoot = false;
    int  root      = -1;            // target root pitch-class, -1 = unset
    Mode mode      = Mode::Ionian;  // displayed as "Major"
    std::map<int, NoteMove> moves;  // per-note manual moves keyed by note id
    /** Empty bars removed by Trim (any position, including middle gaps).
        Sorted ascending. Legacy trimLead/trimTail are migrated on load. */
    std::vector<int> removedBars;
    /** Session-load only: old trimLead/trimTail until resolve has clip.bars. */
    int legacyTrimLead = 0;
    int legacyTrimTail = 0;
    std::map<int, int> velocities;  // per-note velocity overrides (1-127)
    std::set<int> deleted;          // per-note non-destructive deletions

    bool hasTrim() const
    {
        return !removedBars.empty() || legacyTrimLead > 0 || legacyTrimTail > 0;
    }

    void clearTrim()
    {
        removedBars.clear();
        legacyTrimLead = 0;
        legacyTrimTail = 0;
    }
};

bool editIsClean(const ClipEdit& e);

/** Copy only the pitch-shaping fields (octave, fit-to-scale, map-to-root,
    root, mode) from `locked` into `target`, leaving the target's per-note
    moves and trim untouched. Drives the browse-lock feature. */
void applyPitchLock(const ClipEdit& locked, ClipEdit& target);

// ── Transform pipeline ───────────────────────────────────────────────────────

struct ResolvedClip
{
    std::vector<RollNote> notes;
    int bars = 1;
};

/** Effective notes + bar count for display and playback. Order:
    per-note move + octave → map-to-root → fit-to-scale → step move → trim. */
ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& edit);

struct EdgeBars { int lead = 0; int tail = 0; };

/** Fully-empty leading/trailing bars of a note set. */
EdgeBars emptyEdgeBars(const std::vector<RollNote>& notes, int bars);

/** Indices of every fully-empty bar (including middle gaps). Sorted ascending. */
std::vector<int> emptyBars(const std::vector<RollNote>& notes, int bars);

/** Remap a step position after removing the given bar indices. */
double remapStepAfterRemovingBars(double step, const std::vector<int>& removedBars);

struct EditBadge
{
    juce::String key;    // "oct" | "scale" | "map" | "moves" | "trim"
    juce::String label;
};

/** Toolbar chips for the non-default transforms in an edit. */
std::vector<EditBadge> editBadges(const StepClip& clip, const ClipEdit& edit);

// ── Bridging from the beats-based file model ─────────────────────────────────

/** Beats→steps view of a parsed MIDI file (1 beat = 4 steps). Assigns stable
    sequential note ids in file order; estimates root and kind from content. */
StepClip makeStepClip(const MidiClip& clip);

} // namespace pflow
