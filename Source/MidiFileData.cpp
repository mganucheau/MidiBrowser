#include "MidiFileData.h"

namespace pflow {

// ── MidiClip helpers ─────────────────────────────────────────────────────────

juce::MidiMessageSequence MidiClip::toMidiSequence(double bpm) const
{
    juce::MidiMessageSequence seq;
    double secPerBeat = 60.0 / bpm;

    for (auto& n : notes)
    {
        double onTime  = n.startBeat * secPerBeat;
        double offTime = (n.startBeat + n.lengthBeats) * secPerBeat;
        int    pitch   = juce::jlimit(0, 127, n.noteNumber + rootNoteOffset);

        seq.addEvent(juce::MidiMessage::noteOn(n.channel, pitch, (juce::uint8)n.velocity), onTime);
        seq.addEvent(juce::MidiMessage::noteOff(n.channel, pitch), offTime);
    }
    seq.updateMatchedPairs();
    return seq;
}

std::vector<NoteEvent> MidiClip::getNotesForPitch(int noteNumber) const
{
    std::vector<NoteEvent> result;
    for (auto& n : notes)
        if (n.noteNumber == noteNumber)
            result.push_back(n);
    return result;
}

std::vector<int> MidiClip::getDistinctPitches() const
{
    std::set<int> pitches;
    for (auto& n : notes) pitches.insert(n.noteNumber);
    return { pitches.begin(), pitches.end() };
}

// ── MIDI file parser ─────────────────────────────────────────────────────────

MidiClip parseMidiFile(const juce::File& file)
{
    MidiClip clip;
    clip.name     = file.getFileNameWithoutExtension();
    clip.filePath = file.getFullPathName();

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return clip;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) return clip;

    // Use ticks directly — do NOT call convertTimestampTicksToSeconds().
    // Ticks / ticksPerQuarterNote = beat positions directly, with no
    // tempo-dependent conversion errors that cause gaps or wrong lengths.
    short tpqn = midiFile.getTimeFormat();
    bool useTicks = (tpqn > 0);
    double ticksPerBeat = (double)tpqn;

    if (!useTicks)
    {
        // SMPTE format (rare) — fall back to seconds-based conversion
        midiFile.convertTimestampTicksToSeconds();
    }

    double maxBeat = 0.0;
    double trackEndBeat = 0.0;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* trackPtr = midiFile.getTrack(t);
        if (trackPtr == nullptr) continue;

        juce::MidiMessageSequence track(*trackPtr);
        track.updateMatchedPairs();

        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            auto* evHolder = track.getEventPointer(i);
            auto& msg = evHolder->message;

            // Track the last event of any kind (including end-of-track meta)
            // to determine the true MIDI file length
            if (useTicks)
                trackEndBeat = std::max(trackEndBeat, msg.getTimeStamp() / ticksPerBeat);
            else
                trackEndBeat = std::max(trackEndBeat, msg.getTimeStamp() / 0.5); // assume 120 BPM

            if (msg.isNoteOn())
            {
                NoteEvent ne;
                ne.noteNumber = msg.getNoteNumber();
                ne.velocity   = msg.getVelocity();
                ne.channel    = msg.getChannel();

                if (useTicks)
                {
                    ne.startBeat = msg.getTimeStamp() / ticksPerBeat;
                    if (evHolder->noteOffObject != nullptr)
                        ne.lengthBeats = (evHolder->noteOffObject->message.getTimeStamp()
                                          - msg.getTimeStamp()) / ticksPerBeat;
                    else
                        ne.lengthBeats = 0.25;
                }
                else
                {
                    double secPerBeat = 0.5; // 60/120
                    ne.startBeat = msg.getTimeStamp() / secPerBeat;
                    if (evHolder->noteOffObject != nullptr)
                        ne.lengthBeats = (evHolder->noteOffObject->message.getTimeStamp()
                                          - msg.getTimeStamp()) / secPerBeat;
                    else
                        ne.lengthBeats = 0.25;
                }

                clip.notes.push_back(ne);
                maxBeat = std::max(maxBeat, ne.startBeat + ne.lengthBeats);
            }
        }
    }

    // Use the end-of-track timestamp if it's on a clean bar boundary and
    // covers all notes. Otherwise fall back to the last note-off position.
    double barLen = 4.0;
    double effectiveEnd = maxBeat;

    if (trackEndBeat >= maxBeat)
    {
        double trackBars = trackEndBeat / barLen;
        double trackRounded = std::round(trackBars);
        if (trackRounded > 0.0 && std::abs(trackBars - trackRounded) < 0.01)
            effectiveEnd = trackRounded * barLen;
        else
            effectiveEnd = trackEndBeat;
    }

    // Quantise to whole bars, snapping near-integer values to avoid
    // an extra bar from floating-point overshoot (e.g. 8.001 -> 8)
    double bars = effectiveEnd / barLen;
    double rounded = std::round(bars);
    if (std::abs(bars - rounded) < 0.01 && rounded > 0.0)
        bars = rounded;
    else
        bars = std::ceil(bars);
    clip.lengthBeats = std::max(barLen, bars * barLen);
    return clip;
}

// ── Write MIDI file ─────────────────────────────────────────────────────────

bool writeMidiFile(const MidiClip& clip, const juce::File& file, double bpm)
{
    if (clip.notes.empty()) return false;

    juce::MidiFile midiFile;
    int ticksPerBeat = 480;
    midiFile.setTicksPerQuarterNote(ticksPerBeat);

    juce::MidiMessageSequence track;

    // Tempo meta event at tick 0
    track.addEvent(juce::MidiMessage::tempoMetaEvent(
        (int)(60000000.0 / bpm)), 0.0);

    // Track name
    track.addEvent(juce::MidiMessage::textMetaEvent(3, clip.name), 0.0);

    for (auto& n : clip.notes)
    {
        double onTick  = n.startBeat * ticksPerBeat;
        double offTick = (n.startBeat + n.lengthBeats) * ticksPerBeat;
        int pitch = juce::jlimit(0, 127, n.noteNumber);

        track.addEvent(juce::MidiMessage::noteOn(n.channel, pitch, (juce::uint8)n.velocity), onTick);
        track.addEvent(juce::MidiMessage::noteOff(n.channel, pitch), offTick);
    }

    track.updateMatchedPairs();
    midiFile.addTrack(track);

    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;

    return midiFile.writeTo(out, 0);
}

// ── Scale helpers ────────────────────────────────────────────────────────────

juce::String scaleTypeName(ScaleType s)
{
    switch (s)
    {
        case ScaleType::Chromatic:       return "Chromatic";
        case ScaleType::Major:           return "Major";
        case ScaleType::NaturalMinor:    return "Natural Minor";
        case ScaleType::HarmonicMinor:   return "Harmonic Minor";
        case ScaleType::MelodicMinor:    return "Melodic Minor";
        case ScaleType::Dorian:          return "Dorian";
        case ScaleType::Phrygian:        return "Phrygian";
        case ScaleType::Lydian:          return "Lydian";
        case ScaleType::Mixolydian:      return "Mixolydian";
        case ScaleType::Locrian:         return "Locrian";
        case ScaleType::PentatonicMajor: return "Pentatonic Major";
        case ScaleType::PentatonicMinor: return "Pentatonic Minor";
        case ScaleType::Blues:           return "Blues";
        case ScaleType::WholeTone:       return "Whole Tone";
        default: return "Chromatic";
    }
}

std::vector<int> scaleIntervals(ScaleType s)
{
    switch (s)
    {
        case ScaleType::Major:           return {0,2,4,5,7,9,11};
        case ScaleType::NaturalMinor:    return {0,2,3,5,7,8,10};
        case ScaleType::HarmonicMinor:   return {0,2,3,5,7,8,11};
        case ScaleType::MelodicMinor:    return {0,2,3,5,7,9,11};
        case ScaleType::Dorian:          return {0,2,3,5,7,9,10};
        case ScaleType::Phrygian:        return {0,1,3,5,7,8,10};
        case ScaleType::Lydian:          return {0,2,4,6,7,9,11};
        case ScaleType::Mixolydian:      return {0,2,4,5,7,9,10};
        case ScaleType::Locrian:         return {0,1,3,5,6,8,10};
        case ScaleType::PentatonicMajor: return {0,2,4,7,9};
        case ScaleType::PentatonicMinor: return {0,3,5,7,10};
        case ScaleType::Blues:           return {0,3,5,6,7,10};
        case ScaleType::WholeTone:       return {0,2,4,6,8,10};
        default:                         return {0,1,2,3,4,5,6,7,8,9,10,11};
    }
}

int quantiseToScale(int inNote, int scaleRoot, ScaleType scale)
{
    if (scale == ScaleType::Chromatic) return inNote;

    auto intervals = scaleIntervals(scale);
    int octave    = (inNote - scaleRoot) / 12;
    int degree    = (inNote - scaleRoot) % 12;
    if (degree < 0) { degree += 12; octave--; }

    // Find closest scale degree
    int bestDist = 999;
    int bestInterval = 0;
    for (int iv : intervals)
    {
        int dist = std::abs(degree - iv);
        if (dist < bestDist) { bestDist = dist; bestInterval = iv; }
    }
    return scaleRoot + octave * 12 + bestInterval;
}

// ── CompLane helpers ─────────────────────────────────────────────────────────

void CompLane::addClipAtPosition(const MidiClip& clip, double beatPos)
{
    int idx = (int)clips.size();
    clips.push_back(clip);

    CompRegion r;
    r.startBeat = beatPos;
    r.endBeat   = beatPos + clip.lengthBeats;
    r.clipIndex  = idx;
    regions.push_back(r);
}

void CompLane::removeRegion(int index)
{
    if (index >= 0 && index < (int)regions.size())
        regions.erase(regions.begin() + index);
}

} // namespace pflow
