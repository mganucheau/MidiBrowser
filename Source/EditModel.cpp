#include "EditModel.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace pflow {

const std::array<const char*, 12> kNoteNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* clipKindName(ClipKind k)
{
    switch (k)
    {
        case ClipKind::Bass:   return "Bass";
        case ClipKind::Piano:  return "Piano";
        case ClipKind::Lead:   return "Lead";
        case ClipKind::Drums:  return "Drums";
        case ClipKind::Single: return "Single";
    }
    return "Lead";
}

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

// Friendly display names: Ionian/Aeolian read as Major/Minor.
const std::array<const char*, kNumModes> kModeNames {
    "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian"
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

bool pitchInScale(int midi, int rootPc, Mode mode)
{
    bool inScale[12] = {};
    for (int iv : modeIntervals(mode))
        inScale[wrapPc(rootPc + iv)] = true;
    return inScale[wrapPc(midi)];
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

juce::String scaleDegreeLabel(int midi, int rootPc, Mode mode)
{
    if (rootPc < 0) return {};
    static const char* kOrd[] { "Root", "2nd", "3rd", "4th", "5th", "6th", "7th" };
    const int pc = wrapPc(midi);
    const auto& iv = modeIntervals(mode);
    for (int deg = 0; deg < 7; ++deg)
        if (wrapPc(rootPc + iv[(size_t) deg]) == pc)
            return kOrd[deg];
    return {};
}

juce::String pitchScaleAnnotation(int midi, int rootPc, Mode mode)
{
    const auto note = pitchName(midi);
    if (rootPc < 0) return note;
    const auto deg = scaleDegreeLabel(midi, rootPc, mode);
    return deg.isNotEmpty() ? note + " (" + deg + ")" : note;
}

void applyPitchLock(const ClipEdit& locked, ClipEdit& target)
{
    target.octave = locked.octave;
    target.pitchShift = locked.pitchShift;
    target.fitScale = locked.fitScale;
    target.mapToRoot = locked.mapToRoot;
    target.root = locked.root;
    target.mode = locked.mode;
}

bool editIsClean(const ClipEdit& e)
{
    if (e.octave != 0 || e.pitchShift != 0 || e.fitScale || e.mapToRoot || e.hasTrim()
        || e.legacyTrimLead != 0 || e.legacyTrimTail != 0)
        return false;
    if (!e.velocities.empty() || !e.deleted.empty())
        return false;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0)
            return false;
    return true;
}

static std::vector<int> effectiveRemovedBars(const StepClip& clip, const ClipEdit& e)
{
    if (!e.removedBars.empty())
        return e.removedBars;

    std::vector<int> removed;
    if (e.legacyTrimLead <= 0 && e.legacyTrimTail <= 0)
        return removed;

    for (int i = 0; i < e.legacyTrimLead && i < clip.bars; ++i)
        removed.push_back(i);
    const int firstTail = clip.bars - e.legacyTrimTail;
    for (int i = std::max(firstTail, e.legacyTrimLead); i < clip.bars; ++i)
        removed.push_back(i);
    return removed;
}

ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& e)
{
    ResolvedClip out;
    out.notes.reserve(clip.notes.size());

    for (const auto& n : clip.notes)
    {
        if (e.deleted.count(n.id) > 0)
            continue;

        NoteMove mv;
        if (auto it = e.moves.find(n.id); it != e.moves.end())
            mv = it->second;

        int pitch = n.pitch + mv.dPitch + e.octave * 12 + e.pitchShift;

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
        if (auto vit = e.velocities.find(n.id); vit != e.velocities.end())
            r.fileVelocity = std::clamp(vit->second, 1, 127);
        out.notes.push_back(r);
    }

    const auto removed = effectiveRemovedBars(clip, e);
    int bars = clip.bars;
    if (!removed.empty())
    {
        for (auto& r : out.notes)
            r.start = remapStepAfterRemovingBars(r.start, removed);
        bars = clip.bars - (int) removed.size();
    }
    out.bars = std::max(1, bars);
    return out;
}

double remapStepAfterRemovingBars(double step, const std::vector<int>& removedBars)
{
    if (removedBars.empty())
        return step;

    const double barF = step / (double) kStepsPerBar;
    int removedBefore = 0;
    for (int b : removedBars)
    {
        if ((double) b < barF)
            ++removedBefore;
        else
            break;
    }
    return step - (double) removedBefore * (double) kStepsPerBar;
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

std::vector<int> emptyBars(const std::vector<RollNote>& notes, int bars)
{
    std::vector<int> out;
    if (bars <= 0)
        return out;

    for (int b = 0; b < bars; ++b)
    {
        const double ms = (double) b * (double) kStepsPerBar;
        const double me = (double) (b + 1) * (double) kStepsPerBar;
        bool hasNote = false;
        for (const auto& n : notes)
        {
            if (n.start < me && n.start + n.len > ms)
            {
                hasNote = true;
                break;
            }
        }
        if (!hasNote)
            out.push_back(b);
    }
    return out;
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

    const int trimBars = (int) effectiveRemovedBars(clip, e).size();
    if (trimBars > 0)
        out.push_back({ "trim", juce::String::fromUTF8("Trim −") + juce::String(trimBars)
                                    + (trimBars > 1 ? " bars" : " bar") });

    if (!e.velocities.empty())
        out.push_back({ "vel", juce::String((int) e.velocities.size()) + " vel" });

    if (!e.deleted.empty())
        out.push_back({ "del", juce::String((int) e.deleted.size()) + " deleted" });

    return out;
}

StepClip makeStepClip(const MidiClip& clip)
{
    StepClip s;
    s.name = clip.name;
    s.filePath = clip.filePath;
    s.bpm = clip.bpm;
    s.timeSigNum = juce::jmax(1, clip.timeSigNum);
    s.timeSigDen = juce::jmax(1, clip.timeSigDen);

    constexpr double stepsPerBeat = 4.0;

    double maxEnd = 0.0;
    bool anyDrumChannel = false;
    int pitchSum = 0;
    std::set<int> distinctPitches;

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
        distinctPitches.insert(n.noteNumber);
    }

    const double lengthSteps = std::max(clip.lengthBeats * stepsPerBeat, maxEnd);
    s.bars = std::max(1, (int) std::ceil(lengthSteps / kStepsPerBar));
    s.noteCount = (int) s.notes.size();
    s.difNotes = (int) distinctPitches.size();

    const double notesPerBar = (double) s.noteCount / (double) juce::jmax(1, s.bars);
    s.complexity = juce::jlimit(1, 99,
        (int) std::lround((double) s.difNotes * notesPerBar));

    if (!clip.notes.empty())
    {
        s.root = estimatePitchClassFromNotes(clip.notes);
        const int meanPitch = pitchSum / (int) clip.notes.size();

        int maxPoly = 1;
        double polySum = 0.0;
        int polySamples = 0;
        int chord3Hits = 0;
        for (const auto& n : clip.notes)
        {
            int active = 0;
            for (const auto& o : clip.notes)
                if (o.startBeat <= n.startBeat + 1.0e-9
                    && o.startBeat + o.lengthBeats > n.startBeat + 1.0e-9)
                    ++active;
            maxPoly = std::max(maxPoly, active);
            polySum += (double) active;
            ++polySamples;
            if (active >= 3)
                ++chord3Hits;
        }
        const double meanPoly = polySamples > 0 ? polySum / (double) polySamples : 1.0;
        const bool frequentChords = polySamples > 0 && chord3Hits * 4 >= polySamples;

        if (anyDrumChannel)
            s.kind = ClipKind::Drums;
        else if (s.difNotes <= 2 && maxPoly <= 1)
            s.kind = ClipKind::Single;
        else if (meanPitch < 48)
            s.kind = ClipKind::Bass;
        else if (meanPoly >= 2.5 || frequentChords)
            s.kind = ClipKind::Piano;
        else
            s.kind = ClipKind::Lead;
    }
    return s;
}

} // namespace pflow
