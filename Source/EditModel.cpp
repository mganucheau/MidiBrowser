#include "EditModel.h"
#include <algorithm>
#include <cmath>

namespace pflow {

const std::array<const char*, 12> kNoteNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

namespace {

const std::array<std::array<int, 7>, kNumModes> kModeIntervals {{
    {{ 0, 2, 4, 5, 7, 9, 11 }},  // Ionian
    {{ 0, 2, 3, 5, 7, 9, 10 }},  // Dorian
    {{ 0, 1, 3, 5, 7, 8, 10 }},  // Phrygian
    {{ 0, 2, 4, 6, 7, 9, 11 }},  // Lydian
    {{ 0, 2, 4, 5, 7, 9, 10 }},  // Mixolydian
    {{ 0, 2, 3, 5, 7, 8, 10 }},  // Aeolian
    {{ 0, 1, 3, 5, 6, 8, 10 }},  // Locrian
}};

const std::array<const char*, kNumModes> kModeNames {
    "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian"
};

int wrapPc(int v) { return ((v % 12) + 12) % 12; }

} // namespace

const char* modeName(Mode m) { return kModeNames[(size_t) m]; }

const std::array<int, 7>& modeIntervals(Mode m) { return kModeIntervals[(size_t) m]; }

juce::String pitchName(int midi)
{
    const int oct = (int) std::floor(midi / 12.0) - 1;   // MIDI 60 = C4
    return juce::String(kNoteNames[(size_t) wrapPc(midi)]) + juce::String(oct);
}

bool isBlackKeyPitch(int midi)
{
    const int pc = wrapPc(midi);
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int noteNameIndex(const juce::String& name)
{
    for (int i = 0; i < 12; ++i)
        if (name == kNoteNames[(size_t) i])
            return i;
    return -1;
}

int fitToScale(int midi, int rootPc, Mode mode)
{
    bool inScale[12] = {};
    for (int iv : modeIntervals(mode))
        inScale[wrapPc(rootPc + iv)] = true;

    if (inScale[wrapPc(midi)])
        return midi;

    int best = midi, bestDist = 99;
    for (int d = -6; d <= 6; ++d)
    {
        if (inScale[wrapPc(midi + d)] && std::abs(d) < bestDist)
        {
            bestDist = std::abs(d);
            best = midi + d;
        }
    }
    return best;
}

void applyPitchLock(const ClipEdit& locked, ClipEdit& target)
{
    target.octave = locked.octave;
    target.fitScale = locked.fitScale;
    target.mapToRoot = locked.mapToRoot;
    target.root = locked.root;
    target.mode = locked.mode;
}

bool editIsClean(const ClipEdit& e)
{
    if (e.octave != 0 || e.fitScale || e.mapToRoot || e.trimLead != 0 || e.trimTail != 0)
        return false;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0)
            return false;
    return true;
}

ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& e)
{
    ResolvedClip out;
    out.notes.reserve(clip.notes.size());

    for (const auto& n : clip.notes)
    {
        NoteMove mv;
        if (auto it = e.moves.find(n.id); it != e.moves.end())
            mv = it->second;

        int pitch = n.pitch + mv.dPitch + e.octave * 12;

        if (e.mapToRoot && e.root >= 0 && clip.root >= 0)
        {
            // Move the clip's scale root to the target root: a transpose, not
            // a snap, wrapped into [-6, 6] so it goes the nearest direction.
            int d = (e.root - clip.root) % 12;
            if (d > 6)  d -= 12;
            if (d < -6) d += 12;
            pitch += d;
        }

        if (e.fitScale && e.root >= 0)
            pitch = fitToScale(pitch, e.root, e.mode);

        RollNote r = n;
        r.pitch = pitch;
        r.start = n.start + mv.dStep;
        r.moved = (mv.dPitch != 0 || mv.dStep != 0);
        out.notes.push_back(r);
    }

    int bars = clip.bars;
    if (e.trimLead != 0 || e.trimTail != 0)
    {
        for (auto& r : out.notes)
            r.start -= e.trimLead * kStepsPerBar;
        bars = clip.bars - e.trimLead - e.trimTail;
    }
    out.bars = std::max(1, bars);
    return out;
}

EdgeBars emptyEdgeBars(const std::vector<RollNote>& notes, int bars)
{
    if (notes.empty())
        return {};

    double minStep = std::numeric_limits<double>::infinity();
    double maxStep = -std::numeric_limits<double>::infinity();
    for (const auto& n : notes)
    {
        minStep = std::min(minStep, n.start);
        maxStep = std::max(maxStep, n.start + n.len);
    }

    EdgeBars e;
    e.lead = std::max(0, (int) std::floor(minStep / kStepsPerBar));
    e.tail = std::max(0, bars - (int) std::ceil(maxStep / kStepsPerBar));
    return e;
}

std::vector<EditBadge> editBadges(const StepClip& clip, const ClipEdit& e)
{
    std::vector<EditBadge> out;

    if (e.octave != 0)
        out.push_back({ "oct", juce::String("Oct ") + (e.octave > 0 ? "+" : "") + juce::String(e.octave) });

    if (e.fitScale && e.root >= 0)
        out.push_back({ "scale", juce::String(kNoteNames[(size_t) e.root]) + " " + modeName(e.mode) });

    if (e.mapToRoot && e.root >= 0)
        out.push_back({ "map", juce::String::fromUTF8("→ ") + kNoteNames[(size_t) e.root] + " root" });

    int moved = 0;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0)
            ++moved;
    if (moved > 0)
        out.push_back({ "moves", juce::String(moved) + (moved > 1 ? " notes moved" : " note moved") });

    const int trimBars = e.trimLead + e.trimTail;
    if (trimBars > 0)
        out.push_back({ "trim", juce::String::fromUTF8("Trim −") + juce::String(trimBars)
                                    + (trimBars > 1 ? " bars" : " bar") });

    juce::ignoreUnused(clip);
    return out;
}

StepClip makeStepClip(const MidiClip& clip)
{
    StepClip s;
    s.name = clip.name;
    s.filePath = clip.filePath;
    s.bpm = clip.bpm;

    constexpr double stepsPerBeat = 4.0;

    double maxEnd = 0.0;
    bool anyDrumChannel = false;
    int pitchSum = 0;

    int nextId = 0;
    s.notes.reserve(clip.notes.size());
    for (const auto& n : clip.notes)
    {
        RollNote r;
        r.id = nextId++;
        r.pitch = n.noteNumber;
        r.start = n.startBeat * stepsPerBeat;
        r.len = std::max(0.25, n.lengthBeats * stepsPerBeat);
        r.fileVelocity = n.velocity;
        r.channel = n.channel;
        s.notes.push_back(r);

        maxEnd = std::max(maxEnd, r.start + r.len);
        anyDrumChannel = anyDrumChannel || (n.channel == 10);
        pitchSum += n.noteNumber;
    }

    const double lengthSteps = std::max(clip.lengthBeats * stepsPerBeat, maxEnd);
    s.bars = std::max(1, (int) std::ceil(lengthSteps / kStepsPerBar));

    if (!clip.notes.empty())
    {
        s.root = estimatePitchClassFromNotes(clip.notes);
        const int meanPitch = pitchSum / (int) clip.notes.size();
        s.kind = anyDrumChannel ? ClipKind::Drums
               : meanPitch < 48 ? ClipKind::Bass
                                : ClipKind::Keys;
    }
    return s;
}

} // namespace pflow
