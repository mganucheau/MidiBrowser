#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CompingModel.h"
#include "Theme.h"
#include <algorithm>
#include <vector>

namespace pflow {

namespace {

constexpr double kCompSegMinBeats = 0.125;

juce::AudioProcessorValueTreeState::ParameterLayout createCompParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using R = juce::NormalisableRange<float>;
    const R range(0.f, 1.f, 0.f, 1.f);
    for (int i = 0; i < PatternFlowProcessor::kNumCompBoundaryAutomationParams; ++i)
    {
        const juce::String id = "compBnd" + juce::String(i);
        const auto name = "Comp slice " + juce::String(i + 1);
        auto attr = juce::AudioParameterFloatAttributes()
                        .withLabel("session")
                        .withCategory(juce::AudioProcessorParameter::genericParameter)
                        .withStringFromValueFunction(
                            [](float v, int)
                            {
                                return juce::String(juce::roundToInt(v * 1000.f) / 10.0, 1) + "%";
                            });
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, range, 0.5f, attr));
    }
    return layout;
}

/** Internal edges on the combined comp lane (0 < beat < sessionLen), sorted unique. */
std::vector<double> collectSortedInternalCompBoundaries(const std::vector<TakeCompSegment>& segs,
                                                        double sessionLen)
{
    std::vector<double> boundaries;
    constexpr double kEps = 1e-4;
    for (const auto& s : segs)
    {
        if (s.startBeat > kEps && s.startBeat < sessionLen - kEps)
            boundaries.push_back(s.startBeat);
        if (s.endBeat > kEps && s.endBeat < sessionLen - kEps)
            boundaries.push_back(s.endBeat);
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end(),
                                 [](double a, double b) { return std::abs(a - b) < 1e-4; }),
                     boundaries.end());
    return boundaries;
}

void normalizeCompSegmentsAfterDrag(std::vector<TakeCompSegment>& segs, double sessionLen)
{
    if (segs.empty()) return;
    std::sort(segs.begin(), segs.end(),
              [](const TakeCompSegment& a, const TakeCompSegment& b)
              { return a.startBeat < b.startBeat; });
    for (auto& s : segs)
    {
        s.startBeat = juce::jlimit(0.0, sessionLen, s.startBeat);
        s.endBeat = juce::jlimit(0.0, sessionLen, s.endBeat);
        if (s.endBeat - s.startBeat < kCompSegMinBeats)
            s.endBeat = juce::jmin(sessionLen, s.startBeat + kCompSegMinBeats);
    }
    for (size_t i = 1; i < segs.size(); ++i)
    {
        if (segs[i].startBeat < segs[i - 1].endBeat)
            segs[i].startBeat = segs[i - 1].endBeat;
        if (segs[i].endBeat - segs[i].startBeat < kCompSegMinBeats)
            segs[i].endBeat = juce::jmin(sessionLen, segs[i].startBeat + kCompSegMinBeats);
    }
}

} // namespace

PatternFlowProcessor::PatternFlowProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PFParams", createCompParameterLayout())
{
    resetToDefaultSession();
}

PatternFlowProcessor::~PatternFlowProcessor()
{
    compBoundaryParamSync.cancelPendingUpdate();
}

double PatternFlowProcessor::getGridDivision(GridSize gs)
{
    switch (gs)
    {
        case GridSize::Bar:             return 4.0;
        case GridSize::Beat:            return 1.0;
        case GridSize::HalfBeat:        return 0.5;
        case GridSize::QuarterBeat:     return 0.25;
        case GridSize::Eighth:          return 0.125;
        case GridSize::Sixteenth:       return 0.0625;
        case GridSize::ThirtySecond:    return 0.03125;
        case GridSize::EighthTriplet:   return 1.0 / 3.0;
        case GridSize::SixteenthTriplet: return 1.0 / 6.0;
        case GridSize::Off:             return 1.0;  // unused when Off
    }
    return 1.0;
}

double PatternFlowProcessor::snapBeat(double beat) const
{
    int g = gridSnap.load();
    // "Off" still snaps to a reasonable musical grid for editing (matches PianoRollEditor default visuals).
    const double div = (g == (int)GridSize::Off) ? 0.25 : getGridDivision((GridSize)g);
    return std::round(beat / div) * div;
}

bool PatternFlowProcessor::isValidLane(int laneIdx) const
{
    return laneIdx >= 0 && laneIdx < (int)lanes.size();
}

bool PatternFlowProcessor::isValidRegion(int laneIdx, int regionIdx) const
{
    if (!isValidLane(laneIdx)) return false;
    const auto& lane = lanes[laneIdx];
    return regionIdx >= 0 && regionIdx < (int)lane.regions.size();
}

bool PatternFlowProcessor::isValidClipIndex(int laneIdx, int clipIdx) const
{
    if (!isValidLane(laneIdx)) return false;
    const auto& lane = lanes[laneIdx];
    return clipIdx >= 0 && clipIdx < (int)lane.clips.size();
}

void PatternFlowProcessor::prepareToPlay(double sr, int /*samplesPerBlock*/)
{
    sampleRate_ = sr;
}

void PatternFlowProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    buffer.clear();

    juce::MidiBuffer generated;

    if (auto* playHead = getPlayHead())
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue())
        {
            if (auto bpm = posInfo->getBpm())
                hostBpm.store(*bpm);
            if (auto ppq = posInfo->getPpqPosition())
                hostBeatPos.store(*ppq);
            hostPlaying.store(posInfo->getIsPlaying());
        }
    }

    // ── Recording: capture incoming MIDI before clearing ────────────────────
    if (recording.load() && hostPlaying.load())
    {
        const double mul = playheadTempoMul.load();
        double bpmRec   = hostBpm.load() * mul;
        double beatRec  = hostBeatPos.load() * mul;
        if (bpmRec <= 0.0) bpmRec = 120.0;
        double secPerBeatRec = 60.0 / bpmRec;
        double beatsPerSampleRec = 1.0 / (sampleRate_ * secPerBeatRec);
        double sessionLenBeats = (double)(arrangementBars.load() * 4);
        double localBeat = beatRec - recordingStartBeat;

        if (localBeat >= sessionLenBeats)
        {
            finaliseRecordingClip();
            recordingStartBeat = beatRec;
            recordingClipCount++;
            juce::ScopedLock sl(laneLock);
            CompLane newLane;
            auto presets = getClipColourPresets();
            int idx = (int)lanes.size();
            newLane.name   = "Rec " + juce::String(recordingClipCount + 1);
            newLane.colour = presets[idx % presets.size()];
            MidiClip recClip;
            recClip.name = "Recording";
            recClip.colour = newLane.colour;
            recClip.lengthBeats = sessionLenBeats;
            newLane.clips.push_back(recClip);
            CompRegion recRegion;
            recRegion.startBeat = 0.0;
            recRegion.endBeat   = sessionLenBeats;
            recRegion.clipIndex = 0;
            newLane.regions.push_back(recRegion);
            lanes.push_back(newLane);
            recordingLaneIndex = idx;
        }

        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();
            double noteBeat = beatRec + metadata.samplePosition * beatsPerSampleRec;

            if (msg.isNoteOn())
            {
                juce::ScopedLock sl(laneLock);
                recordingActiveNotes.push_back({
                    msg.getNoteNumber(), msg.getVelocity(),
                    msg.getChannel(), noteBeat
                });
            }
            else if (msg.isNoteOff())
            {
                juce::ScopedLock sl(laneLock);
                int pitch = msg.getNoteNumber();
                int chan  = msg.getChannel();
                for (int i = (int)recordingActiveNotes.size() - 1; i >= 0; --i)
                {
                    auto& rn = recordingActiveNotes[i];
                    if (rn.noteNumber == pitch && rn.channel == chan)
                    {
                        // Write completed note to the recording lane's clip
                        if (recordingLaneIndex >= 0 && recordingLaneIndex < (int)lanes.size())
                        {
                            auto& lane = lanes[recordingLaneIndex];
                            if (!lane.clips.empty())
                            {
                                NoteEvent ne;
                                ne.noteNumber  = rn.noteNumber;
                                ne.velocity    = rn.velocity;
                                ne.channel     = rn.channel;
                                ne.startBeat   = rn.startBeat - recordingStartBeat;
                                ne.lengthBeats = std::max(0.01, noteBeat - rn.startBeat);
                                lane.clips.back().notes.push_back(ne);

                                // Update clip length if needed
                                double noteEnd = ne.startBeat + ne.lengthBeats;
                                if (noteEnd > lane.clips.back().lengthBeats)
                                {
                                    double quantLen = std::ceil(noteEnd / 4.0) * 4.0;
                                    lane.clips.back().lengthBeats = quantLen;
                                    // Also update the region end
                                    if (!lane.regions.empty())
                                        lane.regions.back().endBeat = quantLen;
                                }
                            }
                        }
                        recordingActiveNotes.erase(recordingActiveNotes.begin() + i);
                        break;
                    }
                }
            }
        }
    }

    applyCompMovesFromMidiAndHostParameters(midi);

    // Clear incoming MIDI - we generate our own output
    midi.clear();

    if (!hostPlaying.load())
    {
        // Recording is toggled only from the UI (record button), not when transport stops.

        for (auto& an : activeNotes_)
            midi.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
        activeNotes_.clear();
        lastBeatPos_ = -1.0;
        return;
    }

    const double mul = playheadTempoMul.load();
    double bpm     = hostBpm.load() * mul;
    double beatPos = hostBeatPos.load() * mul;

    if (bpm <= 0.0) bpm = 120.0;

    double secPerBeat     = 60.0 / bpm;
    double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    double blockBeats     = buffer.getNumSamples() * beatsPerSample;
    double endBeat        = beatPos + blockBeats;

    // Detect transport jump - send all-notes-off
    if (lastBeatPos_ >= 0.0 && std::abs(beatPos - lastBeatPos_) > beatsPerSample * 2.0)
    {
        for (auto& an : activeNotes_)
            generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
        activeNotes_.clear();
    }

    // Preview state (copy under lock)
    bool previewMuted = true, previewSoloed = false, previewHasClip = false;
    MidiClip previewClip;
    {
        juce::ScopedLock pl(previewLock_);
        previewMuted = previewMuted_;
        previewSoloed = previewSoloed_;
        previewHasClip = previewHasClip_;
        previewClip = previewClip_;
    }

    // Session has content if any lane has unmuted regions
    bool sessionHasContent = false;
    {
        juce::ScopedLock sl(laneLock);
        for (auto& l : lanes)
        {
            if (l.muted) continue;
            for (auto& r : l.regions)
            {
                if (!r.muted && r.clipIndex >= 0 && r.clipIndex < (int)l.clips.size())
                {
                    sessionHasContent = true;
                    break;
                }
            }
            if (sessionHasContent) break;
        }
    }

    bool playPreviewOnly = !previewMuted && previewHasClip && (previewSoloed || !sessionHasContent);

    const double sessionLenBeats = (double)(arrangementBars.load() * 4);

    auto addAllNotesOff = [&](int samplePos)
    {
        // Safety: stop any lingering synth voices, including preview notes (preview isn't tracked in activeNotes_).
        for (int ch = 1; ch <= 16; ++ch)
        {
            generated.addEvent(juce::MidiMessage::allNotesOff(ch), samplePos);
            generated.addEvent(juce::MidiMessage::controllerEvent(ch, 123 /*All Notes Off*/, 0), samplePos);
        }
    };

    auto playbackSessionWrap = [&]()
    {
        if (sessionLenBeats <= 1.0e-9)
        {
            mappedBeatPos.store(beatPos);
            if (!playPreviewOnly)
                generateMidiForBeatRange(beatPos, endBeat, generated, buffer.getNumSamples());
            return;
        }
        double mapped = std::fmod(beatPos, sessionLenBeats);
        if (mapped < 0.0) mapped += sessionLenBeats;
        mappedBeatPos.store(mapped);
        double mappedEnd = mapped + blockBeats;
        if (mappedEnd > sessionLenBeats + 1.0e-12)
        {
            double firstLen  = sessionLenBeats - mapped;
            int firstSamples = std::max(1, (int)(firstLen / blockBeats * buffer.getNumSamples()));
            int secondSamples = buffer.getNumSamples() - firstSamples;

            if (!playPreviewOnly)
                generateMidiForBeatRange(mapped, sessionLenBeats, generated, firstSamples);

            for (auto& an : activeNotes_)
                generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), firstSamples);
            activeNotes_.clear();
            addAllNotesOff(firstSamples);

            double secondLen = blockBeats - firstLen;
            if (!playPreviewOnly)
                generateMidiForBeatRange(0.0, secondLen, generated, secondSamples, firstSamples);
            mappedBeatPos.store(secondLen);
        }
        else
        {
            if (!playPreviewOnly)
                generateMidiForBeatRange(mapped, mappedEnd, generated, buffer.getNumSamples());
        }
    };

    // Loop wrapping: map host beat position into the loop range (clamped to session length)
    if (loopEnabled.load())
    {
        double loopStart = loopStartBeat.load();
        double loopEnd   = loopEndBeat.load();
        if (sessionLenBeats > 1.0e-9)
        {
            loopStart = juce::jlimit(0.0, sessionLenBeats, loopStart);
            loopEnd   = juce::jlimit(0.0, sessionLenBeats, loopEnd);
            if (loopEnd <= loopStart)
                loopEnd = juce::jmin(sessionLenBeats, loopStart + 0.25);
        }
        double loopLen = loopEnd - loopStart;

        if (loopLen > 0.0 && beatPos >= loopStart)
        {
            double mapped = loopStart + std::fmod(beatPos - loopStart, loopLen);
            mappedBeatPos.store(mapped);

            double mappedEnd = mapped + blockBeats;
            if (mappedEnd > loopEnd)
            {
                double firstLen  = loopEnd - mapped;
                int firstSamples = std::max(1, (int)(firstLen / blockBeats * buffer.getNumSamples()));
                int secondSamples = buffer.getNumSamples() - firstSamples;

                if (!playPreviewOnly)
                    generateMidiForBeatRange(mapped, loopEnd, generated, firstSamples);

                for (auto& an : activeNotes_)
                    generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), firstSamples);
                activeNotes_.clear();
                addAllNotesOff(firstSamples);

                double secondLen = blockBeats - firstLen;
                if (!playPreviewOnly)
                    generateMidiForBeatRange(loopStart, loopStart + secondLen, generated, secondSamples, firstSamples);
                mappedBeatPos.store(loopStart + secondLen);
            }
            else
            {
                if (!playPreviewOnly)
                    generateMidiForBeatRange(mapped, mappedEnd, generated, buffer.getNumSamples());
            }
        }
        else
            playbackSessionWrap();
    }
    else
        playbackSessionWrap();

    // Preview MIDI: when unmuted, add preview (by itself if solo/empty session, else in addition)
    if (!previewMuted && previewHasClip && previewClip.lengthBeats > 0)
    {
        if (playPreviewOnly)
        {
            for (auto& an : activeNotes_)
                generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
            activeNotes_.clear();
        }
        double pStart = beatPos;
        double pEnd = endBeat;
        if (loopEnabled.load())
        {
            double ls = loopStartBeat.load(), le = loopEndBeat.load();
            if (sessionLenBeats > 1.0e-9)
            {
                ls = juce::jlimit(0.0, sessionLenBeats, ls);
                le = juce::jlimit(0.0, sessionLenBeats, le);
            }
            if (le > ls && beatPos >= ls)
            {
                double loopLen = le - ls;
                pStart = ls + std::fmod(beatPos - ls, loopLen);
                pEnd = pStart + blockBeats;
            }
        }
        else if (sessionLenBeats > 1.0e-9)
        {
            pStart = std::fmod(beatPos, sessionLenBeats);
            if (pStart < 0.0) pStart += sessionLenBeats;
            pEnd = pStart + blockBeats;
        }
        generatePreviewMidi(previewClip, pStart, pEnd, generated, buffer.getNumSamples());
    }

    lastBeatPos_ = endBeat;

    for (const auto metadata : generated)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);
}

void PatternFlowProcessor::clearAllComps()
{
    juce::ScopedLock sl(laneLock);
    if (!takeComps.empty())
        takeComps[0].segments.clear();
    compsEnabled.store(false);
    rebuildCombinedClip();
}

void PatternFlowProcessor::generateMidiForBeatRange(double startBeat,
                                                     double endBeat,
                                                     juce::MidiBuffer& output,
                                                     int numSamples,
                                                     int sampleOffsetBase)
{
    juce::ScopedLock sl(laneLock);
    juce::ScopedLock sl2(splitLock);

    // Play exclusively from the pre-merged combinedClip.
    // If it's empty (no clips, or all muted), no MIDI output.
    if (combinedClip.notes.empty()) return;

    const int octaveShift = octaveShiftSemitones.load();
    for (auto& note : combinedClip.notes)
    {
        double noteGlobalStart = note.startBeat;
        double noteGlobalEnd   = noteGlobalStart + note.lengthBeats;

        if (noteGlobalStart >= startBeat && noteGlobalStart < endBeat)
        {
            double fraction = (noteGlobalStart - startBeat) / (endBeat - startBeat);
            int sampleOffset = sampleOffsetBase + juce::jlimit(0, numSamples - 1,
                                                               (int)(fraction * numSamples));

            int pitch = note.noteNumber;
            if (scaleEnabled.load())
                pitch = quantiseToScale(pitch, scaleRoot.load(),
                                       (ScaleType)scaleType.load());
            pitch = juce::jlimit(0, 127, pitch + octaveShift);

            int vel = applyHumanVelocity(note.velocity);
            int channel = note.channel;

            if (splitEnabled.load())
            {
                for (auto& rule : splitRules)
                {
                    if (pitch >= rule.noteMin && pitch <= rule.noteMax)
                    {
                        channel = juce::jlimit(1, 16, rule.outputIndex + 1);
                        break;
                    }
                }
            }

            output.addEvent(
                juce::MidiMessage::noteOn(channel, pitch, (juce::uint8)vel),
                sampleOffset);
            activeNotes_.push_back({ pitch, channel });
        }

        if (noteGlobalEnd >= startBeat && noteGlobalEnd < endBeat)
        {
            double fraction = (noteGlobalEnd - startBeat) / (endBeat - startBeat);
            int sampleOffset = sampleOffsetBase + juce::jlimit(0, numSamples - 1,
                                                               (int)(fraction * numSamples));

            int pitch = note.noteNumber;
            if (scaleEnabled.load())
                pitch = quantiseToScale(pitch, scaleRoot.load(),
                                       (ScaleType)scaleType.load());
            pitch = juce::jlimit(0, 127, pitch + octaveShift);

            int channel = note.channel;
            if (splitEnabled.load())
            {
                for (auto& rule : splitRules)
                {
                    if (pitch >= rule.noteMin && pitch <= rule.noteMax)
                    {
                        channel = juce::jlimit(1, 16, rule.outputIndex + 1);
                        break;
                    }
                }
            }

            output.addEvent(
                juce::MidiMessage::noteOff(channel, pitch),
                sampleOffset);

            activeNotes_.erase(
                std::remove_if(activeNotes_.begin(), activeNotes_.end(),
                    [pitch, channel](const ActiveNote& a) {
                        return a.pitch == pitch && a.channel == channel;
                    }),
                activeNotes_.end());
        }
    }
}

// generateMidiMergedLanes removed — playback now reads exclusively from combinedClip.

// generateMidiFromActiveComp removed — playback now reads exclusively from combinedClip.

int PatternFlowProcessor::applyHumanVelocity(int vel)
{
    float h = humanVelocity.load();
    if (h <= 0.0f) return vel;
    auto& rng = juce::Random::getSystemRandom();
    int deviation = (int)(h * 20.0f * (rng.nextFloat() * 2.0f - 1.0f));
    return juce::jlimit(1, 127, vel + deviation);
}

double PatternFlowProcessor::applyHumanTiming(double beatPos)
{
    float h = humanTiming.load();
    if (h <= 0.0f) return beatPos;
    auto& rng = juce::Random::getSystemRandom();
    double deviation = h * 0.03 * (rng.nextFloat() * 2.0f - 1.0f);
    return beatPos + deviation;
}

PatternFlowProcessor::TransportInfo PatternFlowProcessor::getTransport() const
{
    return { hostBpm.load(), hostBeatPos.load(), hostPlaying.load() };
}

// ── Preview panel state ──────────────────────────────────────────────────────

void PatternFlowProcessor::setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed)
{
    juce::ScopedLock sl(previewLock_);
    previewClip_ = clip;
    previewHasClip_ = hasClip;
    previewMuted_ = muted;
    previewSoloed_ = soloed;
}

void PatternFlowProcessor::generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                                               juce::MidiBuffer& output, int numSamples,
                                               int sampleOffsetBase)
{
    double clipLen = std::max(0.25, clip.lengthBeats);
    double blockLen = endBeat - startBeat;
    if (blockLen <= 0.0) return;

    const int octaveShift = octaveShiftSemitones.load();
    for (auto& note : clip.notes)
    {
        double noteStart = note.startBeat - clip.clipStartOffset;
        if (noteStart < 0.0) noteStart += clipLen;
        double noteEnd = noteStart + note.lengthBeats;

        // Find loop instances k where [k*clipLen + noteStart, k*clipLen + noteEnd] overlaps [startBeat, endBeat]
        int kMin = std::max(0, (int)std::floor((startBeat - noteEnd) / clipLen) + 1);
        int kMax = (int)std::ceil((endBeat - noteStart) / clipLen);
        for (int k = kMin; k <= kMax; ++k)
        {
            double globalStart = k * clipLen + noteStart;
            double globalEnd = k * clipLen + noteEnd;
            if (globalEnd <= startBeat || globalStart >= endBeat) continue;

            if (globalStart >= startBeat && globalStart < endBeat)
            {
                double fraction = (globalStart - startBeat) / blockLen;
                int sampleOffset = sampleOffsetBase + juce::jlimit(0, numSamples - 1, (int)(fraction * numSamples));
                int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset + octaveShift);
                int vel = note.velocity;
                output.addEvent(juce::MidiMessage::noteOn(note.channel, pitch, (juce::uint8)vel), sampleOffset);
            }
            if (globalEnd >= startBeat && globalEnd < endBeat)
            {
                double fraction = (globalEnd - startBeat) / blockLen;
                int sampleOffset = sampleOffsetBase + juce::jlimit(0, numSamples - 1, (int)(fraction * numSamples));
                int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset + octaveShift);
                output.addEvent(juce::MidiMessage::noteOff(note.channel, pitch), sampleOffset);
            }
        }
    }
}

// ── Recording ────────────────────────────────────────────────────────────────

void PatternFlowProcessor::startRecording()
{
    juce::ScopedLock sl(laneLock);

    recordingActiveNotes.clear();
    recordingClipCount = 0;

    // Create a new lane for recording
    CompLane recLane;
    auto presets = getClipColourPresets();
    int idx = (int)lanes.size();
    recLane.name   = "Rec 1";
    recLane.colour = presets[idx % presets.size()];

    // Create an empty clip in the lane
    MidiClip recClip;
    recClip.name = "Recording";
    recClip.colour = recLane.colour;
    recClip.lengthBeats = (double)(arrangementBars.load() * 4);
    recLane.clips.push_back(recClip);

    // Create a region spanning session length
    CompRegion recRegion;
    recRegion.startBeat = 0.0;
    recRegion.endBeat   = recClip.lengthBeats;
    recRegion.clipIndex = 0;
    recLane.regions.push_back(recRegion);

    lanes.push_back(recLane);
    recordingLaneIndex = idx;

    double beatPos = hostBeatPos.load();
    recordingStartBeat = hostPlaying.load() ? beatPos : 0.0;

    recording.store(true);
}

void PatternFlowProcessor::stopRecording()
{
    if (!recording.load()) return;

    recording.store(false);

    juce::ScopedLock sl(laneLock);

    // Finalise any remaining held notes at the current beat position
    double currentBeat = hostBeatPos.load();
    if (recordingLaneIndex >= 0 && recordingLaneIndex < (int)lanes.size())
    {
        auto& lane = lanes[recordingLaneIndex];
        if (!lane.clips.empty())
        {
            for (auto& rn : recordingActiveNotes)
            {
                NoteEvent ne;
                ne.noteNumber  = rn.noteNumber;
                ne.velocity    = rn.velocity;
                ne.channel     = rn.channel;
                ne.startBeat   = rn.startBeat - recordingStartBeat;
                ne.lengthBeats = std::max(0.01, currentBeat - rn.startBeat);
                lane.clips.back().notes.push_back(ne);
            }

            // Trim clip length to actual content
            double maxEnd = 0.0;
            for (auto& n : lane.clips.back().notes)
                maxEnd = std::max(maxEnd, n.startBeat + n.lengthBeats);
            if (maxEnd > 0.0)
            {
                double quantLen = std::ceil(maxEnd / 4.0) * 4.0;
                lane.clips.back().lengthBeats = quantLen;
                if (!lane.regions.empty())
                    lane.regions.back().endBeat = quantLen;
            }
        }
    }

    recordingActiveNotes.clear();

    // Delete empty lanes above the recording lane (including the first lane)
    int recIdx = recordingLaneIndex;
    recordingLaneIndex = -1;
    recordingClipCount = 0;

    if (recIdx >= 0 && recIdx < (int)lanes.size())
    {
        // If recording lane has no notes, delete it too
        auto& recLane = lanes[recIdx];
        bool recLaneEmpty = recLane.clips.empty() || (recLane.clips.size() == 1 && recLane.clips[0].notes.empty());
        if (recLaneEmpty)
        {
            lanes.erase(lanes.begin() + recIdx);
            recIdx = -1;
        }

        // Delete empty lanes above (from recIdx-1 down to 0; if recLane was deleted, check all)
        int checkUpTo = (recIdx >= 0) ? recIdx - 1 : (int)lanes.size() - 1;
        for (int i = checkUpTo; i >= 0; --i)
        {
            if (lanes[i].regions.empty())
            {
                lanes.erase(lanes.begin() + i);
                if (recIdx >= 0 && i < recIdx) recIdx--;
            }
        }
    }
}

void PatternFlowProcessor::finaliseRecordingClip()
{
    // Called when recording overflows past session length - finalise current clip
    juce::ScopedLock sl(laneLock);

    if (recordingLaneIndex < 0 || recordingLaneIndex >= (int)lanes.size())
        return;

    auto& lane = lanes[recordingLaneIndex];
    if (lane.clips.empty()) return;

    // Close any still-held notes at the session boundary
    double sessionLen = (double)(arrangementBars.load() * 4);
    for (auto& rn : recordingActiveNotes)
    {
        NoteEvent ne;
        ne.noteNumber  = rn.noteNumber;
        ne.velocity    = rn.velocity;
        ne.channel     = rn.channel;
        ne.startBeat   = rn.startBeat - recordingStartBeat;
        ne.lengthBeats = std::max(0.01, sessionLen - ne.startBeat);
        lane.clips.back().notes.push_back(ne);
    }

    // Trim clip
    double maxEnd = 0.0;
    for (auto& n : lane.clips.back().notes)
        maxEnd = std::max(maxEnd, n.startBeat + n.lengthBeats);
    if (maxEnd > 0.0)
    {
        double quantLen = std::ceil(maxEnd / 4.0) * 4.0;
        lane.clips.back().lengthBeats = quantLen;
        if (!lane.regions.empty())
            lane.regions.back().endBeat = quantLen;
    }

    recordingActiveNotes.clear();
}

// ── Combined clip rebuild ────────────────────────────────────────────────────

void PatternFlowProcessor::rebuildCombinedClip()
{
    juce::ScopedLock sl(laneLock);
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::rebuildCombinedClipImpl()
{
    combinedClip.notes.clear();
    combinedClip.name = "Combined";
    combinedClip.colour = colours::textMuted();

    bool anySolo = false;
    for (auto& lane : lanes)
        if (lane.solo) { anySolo = true; break; }

    double maxEndBeat = 0.0;
    for (int li = 0; li < (int)lanes.size(); ++li)
        for (int ri = 0; ri < (int)lanes[li].regions.size(); ++ri)
            maxEndBeat = std::max(maxEndBeat, lanes[li].regions[ri].endBeat);
    combinedClip.lengthBeats = maxEndBeat;

    if (activeTakeCompIndex >= 0 && activeTakeCompIndex < (int)takeComps.size())
    {
        const auto& activeComp = takeComps[(size_t)activeTakeCompIndex];
        if (!activeComp.segments.empty())
        {
        const auto segs = comping::sortedSegments(activeComp);
        for (const auto& seg : segs)
        {
            const int li = seg.laneIndex;
            if (li < 0 || li >= (int)lanes.size()) continue;
            auto& lane = lanes[li];
            if (lane.muted || (anySolo && !lane.solo)) continue;

            const double g0 = seg.startBeat;
            const double g1 = seg.endBeat;

            for (int ri = 0; ri < (int)lane.regions.size(); ++ri)
            {
                auto& region = lane.regions[ri];
                if (region.muted || region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size())
                    continue;
                auto& clip = lane.clips[region.clipIndex];
                double regionLen = region.endBeat - region.startBeat;
                if (regionLen <= 0.0 || clip.lengthBeats <= 0.0) continue;

                double clen = clip.lengthBeats;
                int loopCount = 1;
                if (clen > 1.0e-9 && regionLen > clen + 1.0e-6)
                    loopCount = juce::jmax(1, (int)std::ceil(regionLen / clen - 1.0e-9));

                for (int loop = 0; loop < loopCount; ++loop)
                {
                    double loopOffset = loop * clip.lengthBeats;
                    for (auto& note : clip.notes)
                    {
                        if (region.noteFilter >= 0 && note.noteNumber != region.noteFilter)
                            continue;

                        double adjustedStart = note.startBeat - clip.clipStartOffset;
                        if (adjustedStart < 0.0) adjustedStart += clip.lengthBeats;

                        double globalStart = region.startBeat + loopOffset + adjustedStart;
                        if (globalStart >= region.endBeat) continue;
                        double globalEnd = std::min(globalStart + note.lengthBeats, region.endBeat);

                        double gs = std::max(g0, globalStart);
                        double ge = std::min(g1, globalEnd);
                        if (ge <= gs + 1.0e-12) continue;

                        NoteEvent merged;
                        merged.noteNumber  = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                        merged.velocity    = note.velocity;
                        merged.startBeat   = gs;
                        merged.lengthBeats = ge - gs;
                        merged.channel     = note.channel;
                        combinedClip.notes.push_back(merged);
                    }
                }
            }
        }
        return;
        }
    }

    for (int li = 0; li < (int)lanes.size(); ++li)
    {
        auto& lane = lanes[li];
        if (lane.muted || (anySolo && !lane.solo)) continue;
        for (int ri = 0; ri < (int)lane.regions.size(); ++ri)
        {
            auto& region = lane.regions[ri];
            if (region.muted || region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size()) continue;
            auto& clip = lane.clips[region.clipIndex];
            double regionLen = region.endBeat - region.startBeat;
            if (regionLen <= 0.0 || clip.lengthBeats <= 0.0) continue;
            double clen = clip.lengthBeats;
            int loopCount = 1;
            if (clen > 1.0e-9 && regionLen > clen + 1.0e-6)
                loopCount = juce::jmax(1, (int)std::ceil(regionLen / clen - 1.0e-9));

            for (int loop = 0; loop < loopCount; ++loop)
            {
                double loopOffset = loop * clip.lengthBeats;
                for (auto& note : clip.notes)
                {
                    if (region.noteFilter >= 0 && note.noteNumber != region.noteFilter)
                        continue;

                    double adjustedStart = note.startBeat - clip.clipStartOffset;
                    if (adjustedStart < 0.0) adjustedStart += clip.lengthBeats;

                    double globalStart = region.startBeat + loopOffset + adjustedStart;
                    if (globalStart >= region.endBeat) continue;
                    double globalEnd = std::min(globalStart + note.lengthBeats, region.endBeat);

                    NoteEvent merged;
                    merged.noteNumber  = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                    merged.velocity    = note.velocity;
                    merged.startBeat   = globalStart;
                    merged.lengthBeats = globalEnd - globalStart;
                    merged.channel     = note.channel;
                    combinedClip.notes.push_back(merged);
                }
            }
        }
    }
}

void PatternFlowProcessor::removeEmptyLanesExceptFirst()
{
    juce::ScopedLock sl(laneLock);
    for (int i = (int)lanes.size() - 1; i >= 0; --i)
    {
        if (lanes[i].regions.empty())
            lanes.erase(lanes.begin() + i);
    }
}

void PatternFlowProcessor::clampTakeCompsToSessionLengthLocked(double sessionLen)
{
    if (sessionLen <= 1.0e-9) return;
    for (auto& tc : takeComps)
    {
        auto& segs = tc.segments;
        for (size_t i = 0; i < segs.size();)
        {
            auto& s = segs[i];
            s.startBeat = juce::jlimit(0.0, sessionLen, s.startBeat);
            s.endBeat   = juce::jlimit(0.0, sessionLen, s.endBeat);
            if (s.endBeat <= s.startBeat + 1.0e-9)
            {
                segs.erase(segs.begin() + (ptrdiff_t)i);
                continue;
            }
            ++i;
        }
        normalizeCompSegmentsAfterDrag(segs, sessionLen);
        comping::mergeAdjacentSameLane(segs);
    }
}

void PatternFlowProcessor::clampArrangementToSessionLength()
{
    const double sessionLen = (double)(arrangementBars.load() * 4);
    {
        juce::ScopedLock sl(laneLock);

    for (auto& lane : lanes)
    {
        for (size_t ri = 0; ri < lane.regions.size();)
        {
            auto& region = lane.regions[ri];
            if (region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size())
            {
                ++ri;
                continue;
            }
            auto& clip = lane.clips[region.clipIndex];

            if (region.endBeat <= 0.0 || region.startBeat >= sessionLen)
            {
                lane.regions.erase(lane.regions.begin() + (ptrdiff_t)ri);
                continue;
            }

            if (region.endBeat > sessionLen)
            {
                region.endBeat = sessionLen;
                const double span = region.endBeat - region.startBeat;
                if (span > 1.0e-9 && clip.lengthBeats > span)
                {
                    clip.lengthBeats = span;
                    clip.notes.erase(
                        std::remove_if(clip.notes.begin(), clip.notes.end(),
                            [&clip](const NoteEvent& n) { return n.startBeat >= clip.lengthBeats; }),
                        clip.notes.end());
                    for (auto& n : clip.notes)
                    {
                        if (n.startBeat + n.lengthBeats > clip.lengthBeats)
                            n.lengthBeats = std::max(0.01, clip.lengthBeats - n.startBeat);
                    }
                }
            }
            region.ensureSelectionInBounds();
            ++ri;
        }
    }

    clampTakeCompsToSessionLengthLocked(sessionLen);

    double le = loopEndBeat.load();
    if (le > sessionLen)
        loopEndBeat.store(sessionLen);
    double ls = loopStartBeat.load();
    if (ls >= sessionLen)
        loopStartBeat.store(0.0);
    if (loopStartBeat.load() >= loopEndBeat.load())
        loopStartBeat.store(std::max(0.0, loopEndBeat.load() - 0.25));

    rebuildCombinedClipImpl();
    }

    {
        const double eph = editPlayheadBeat.load();
        if (eph > sessionLen)
            editPlayheadBeat.store(juce::jmax(0.0, sessionLen - 1.0e-9));
    }

    scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::trimEmptyMeasuresInSelectedClips(const std::vector<std::pair<int, int>>& selectedRegions)
{
    if (selectedRegions.empty()) return;
    juce::ScopedLock sl(laneLock);
    for (const auto& pr : selectedRegions)
    {
        int li = pr.first, ri = pr.second;
        if (!isValidRegion(li, ri)) continue;
        auto& lane = lanes[li];
        auto& region = lane.regions[ri];
        if (region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size()) continue;
        auto& clip = lane.clips[region.clipIndex];
        trimEmptyMeasuresInClip(clip);
        region.endBeat = region.startBeat + clip.lengthBeats;
        region.ensureSelectionInBounds();
    }
    clampArrangementToSessionLength();
}

void PatternFlowProcessor::resetToDefaultSession()
{
    {
        juce::ScopedLock spl(splitLock);
        splitRules.clear();
    }
    splitEnabled.store(false);
    arrangementBars.store(4);
    gridSnap.store((int)GridSize::Beat);
    loopEnabled.store(false);
    loopStartBeat.store(0.0);
    loopEndBeat.store(16.0);
    loopSyncMoveTogether.store(false);
    scaleRoot.store(0);
    scaleType.store(0);
    scaleEnabled.store(false);
    rootNoteRemap.store(24);
    octaveShiftSemitones.store(0);
    humanVelocity.store(0.0f);
    humanTiming.store(0.0f);
    humanFeel.store(0.0f);
    intonation.store(0.0f);
    lastBrowserDir.clear();
    recording.store(false);
    appThemeId.store(11);
    applyAppTheme(11);

    juce::ScopedLock sl(laneLock);
    lanes.clear();
    takeComps.clear();
    activeTakeCompIndex = 0;
    compsEnabled.store(false);
    compRandomRegionCount.store(8);
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::transposeAllClipsToSelectedScale()
{
    const int targetRoot = juce::jlimit(0, 11, scaleRoot.load());
    const int stIdx = juce::jlimit(0, (int)ScaleType::Count - 1, scaleType.load());
    const auto st = (ScaleType)stIdx;

    juce::ScopedLock sl(laneLock);
    for (auto& lane : lanes)
    {
        for (auto& clip : lane.clips)
        {
            if (clip.notes.empty()) continue;
            const int estPc = estimatePitchClassFromNotes(clip.notes);
            const int delta = (targetRoot - estPc + 12) % 12;
            for (auto& n : clip.notes)
            {
                const int nn = juce::jlimit(0, 127, n.noteNumber + delta);
                n.noteNumber = quantiseToScale(nn, targetRoot, st);
            }
            clip.rootNoteOffset = 0;
        }
    }
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::enableLiveScaleSnapshotsAndApply()
{
    {
        juce::ScopedLock sl(laneLock);
        liveScaleBaselines_.clear();
        liveScaleBaselines_.reserve(lanes.size());
        for (const auto& lane : lanes)
            liveScaleBaselines_.push_back(lane.clips);
        liveScaleBaselinesValid_ = true;
    }
    applyLiveScaleMappingFromBaselines();
}

void PatternFlowProcessor::disableLiveScaleRevert()
{
    juce::ScopedLock sl(laneLock);
    if (!liveScaleBaselinesValid_)
        return;
    if (liveScaleBaselines_.size() != lanes.size())
    {
        liveScaleBaselinesValid_ = false;
        liveScaleBaselines_.clear();
        return;
    }
    for (size_t i = 0; i < lanes.size(); ++i)
        lanes[i].clips = liveScaleBaselines_[i];
    liveScaleBaselinesValid_ = false;
    liveScaleBaselines_.clear();
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::applyLiveScaleMappingFromBaselines()
{
    const int targetRoot = juce::jlimit(0, 11, scaleRoot.load());
    const int stIdx = juce::jlimit(0, (int)ScaleType::Count - 1, scaleType.load());
    const auto st = (ScaleType)stIdx;

    juce::ScopedLock sl(laneLock);
    if (!liveScaleBaselinesValid_)
        return;
    if (liveScaleBaselines_.size() != lanes.size())
        return;

    for (size_t li = 0; li < lanes.size(); ++li)
    {
        auto& lane = lanes[li];
        const auto& baseClips = liveScaleBaselines_[li];
        if (lane.clips.size() != baseClips.size())
            continue;
        for (size_t ci = 0; ci < lane.clips.size(); ++ci)
        {
            lane.clips[ci] = baseClips[ci];
            auto& clip = lane.clips[ci];
            if (clip.notes.empty()) continue;
            const int estPc = estimatePitchClassFromNotes(clip.notes);
            const int delta = (targetRoot - estPc + 12) % 12;
            for (auto& n : clip.notes)
            {
                const int nn = juce::jlimit(0, 127, n.noteNumber + delta);
                n.noteNumber = quantiseToScale(nn, targetRoot, st);
            }
            clip.rootNoteOffset = 0;
        }
    }
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::ensureDefaultTakeComp()
{
    if (takeComps.empty())
    {
        TakeComp c;
        c.name = "Comp";
        takeComps.push_back(std::move(c));
    }
    else if (takeComps.size() > 1)
    {
        TakeComp keep = std::move(takeComps[0]);
        takeComps.clear();
        takeComps.push_back(std::move(keep));
    }
    activeTakeCompIndex = 0;
}

std::vector<TakeCompSegment> PatternFlowProcessor::getActiveTakeCompSegmentsSnapshot() const
{
    juce::ScopedLock sl(laneLock);
    if (takeComps.empty())
        return {};
    return takeComps[0].segments;
}

void PatternFlowProcessor::applyCompSwipe(int laneIndex, double startBeat, double endBeat)
{
    {
        juce::ScopedLock sl(laneLock);
        ensureDefaultTakeComp();
        if (takeComps.empty()) return;
        if (!isValidLane(laneIndex)) return;
        const double sessionLen = (double)(arrangementBars.load() * 4);
        double s = juce::jlimit(0.0, sessionLen, startBeat);
        double e = juce::jlimit(0.0, sessionLen, endBeat);
        if (s > e) std::swap(s, e);
        comping::applySwipe(takeComps[0], laneIndex, s, e);
        rebuildCombinedClipImpl();
    }
    scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::randomizeActiveComp()
{
    {
        juce::ScopedLock sl(laneLock);
        ensureDefaultTakeComp();
        TakeComp& comp = takeComps[0];
        const int n = juce::jlimit(2, 16, compRandomRegionCount.load());
        const int numLanes = (int)lanes.size();
        const double sessionLen = (double)(arrangementBars.load() * 4);
        comp.segments.clear();

        if (numLanes < 1 || sessionLen <= 1.0e-9)
        {
            rebuildCombinedClipImpl();
            scheduleCompBoundaryParamSync();
            return;
        }

        juce::Random rng(juce::Time::getMillisecondCounterHiRes());
        std::vector<double> pts;
        pts.reserve((size_t)n + 1);
        pts.push_back(0.0);
        for (int i = 1; i < n; ++i)
            pts.push_back(rng.nextDouble() * sessionLen);
        std::sort(pts.begin(), pts.end());
        pts.push_back(sessionLen);
        // Snap to current grid division (Off still uses a sensible musical snap for editing).
        const int gs = gridSnap.load();
        const double div = (gs == (int)GridSize::Off) ? 0.25 : getGridDivision((GridSize)gs);
        for (auto& p : pts)
            p = juce::jlimit(0.0, sessionLen, std::round(p / div) * div);

        std::sort(pts.begin(), pts.end());
        pts.front() = 0.0;
        pts.back() = sessionLen;

        // Enforce strict increase on-grid (minimum = one grid division).
        for (size_t i = 1; i < pts.size(); ++i)
        {
            if (pts[i] <= pts[i - 1])
                pts[i] = std::min(sessionLen, pts[i - 1] + div);
        }
        pts.back() = sessionLen;

        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            double a = juce::jlimit(0.0, sessionLen, pts[i]);
            double b = juce::jlimit(0.0, sessionLen, pts[i + 1]);
            if (b > sessionLen) b = sessionLen;
            if (b <= a + 1.0e-9) continue;
            const int lane = rng.nextInt(numLanes);
            comp.segments.push_back({ lane, a, b });
        }
        clampTakeCompsToSessionLengthLocked(sessionLen);
        rebuildCombinedClipImpl();
    }
    scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::cycleCompSegmentsToNextLane()
{
    juce::ScopedLock sl(laneLock);
    ensureDefaultTakeComp();
    if (takeComps.empty()) return;
    const int numLanes = (int)lanes.size();
    if (numLanes < 1) return;
    auto& segs = takeComps[0].segments;
    if (segs.empty()) return;
    const int maxLi = numLanes - 1;
    for (auto& seg : segs)
    {
        const int li = juce::jlimit(0, maxLi, seg.laneIndex);
        seg.laneIndex = (li + 1) % numLanes;
    }
    comping::mergeAdjacentSameLane(segs);
    rebuildCombinedClipImpl();
    scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::scheduleCompBoundaryParamSync()
{
    compBoundaryParamSync.triggerAsyncUpdate();
}

double PatternFlowProcessor::applyCompBoundaryDrag(double fromBeat, double toBeat)
{
    double anchor;
    {
        juce::ScopedLock sl(laneLock);
        anchor = applyCompBoundaryDragLocked(fromBeat, toBeat, true, true);
    }
    scheduleCompBoundaryParamSync();
    return anchor;
}

double PatternFlowProcessor::applyCompBoundaryDragLocked(double fromBeat, double toBeat, bool snapToGrid,
                                                        bool rebuildAfter)
{
    ensureDefaultTakeComp();
    if (takeComps.empty()) return fromBeat;
    auto& segs = takeComps[0].segments;
    if (segs.empty()) return fromBeat;
    const double sessionLen = (double)(arrangementBars.load() * 4);

    auto collectEdges = [&segs]() {
        std::vector<double> edges;
        edges.reserve(segs.size() * 2);
        for (const auto& s : segs)
        {
            edges.push_back(s.startBeat);
            edges.push_back(s.endBeat);
        }
        std::sort(edges.begin(), edges.end());
        return edges;
    };

    auto nearestBoundary = [&](double beat) -> double {
        const auto edges = collectEdges();
        if (edges.empty()) return beat;
        double best = beat;
        double bestD = 0.12;
        for (double e : edges)
        {
            const double d = std::abs(e - beat);
            if (d < bestD)
            {
                bestD = d;
                best = e;
            }
        }
        return best;
    };

    fromBeat = nearestBoundary(fromBeat);

    constexpr double kEps = 3e-2;
    std::vector<std::pair<int, bool>> touches;
    touches.reserve(segs.size() * 2);
    for (int i = 0; i < (int)segs.size(); ++i)
    {
        if (std::abs(segs[(size_t)i].startBeat - fromBeat) < kEps)
            touches.push_back({ i, true });
        if (std::abs(segs[(size_t)i].endBeat - fromBeat) < kEps)
            touches.push_back({ i, false });
    }
    if (touches.empty()) return fromBeat;

    double tb = juce::jlimit(0.0, sessionLen, toBeat);
    if (snapToGrid) tb = snapBeat(tb);
    tb = juce::jlimit(0.0, sessionLen, tb);

    for (const auto& touch : touches)
    {
        const int idx = touch.first;
        const bool isStart = touch.second;
        if (idx < 0 || idx >= (int)segs.size()) continue;
        if (isStart)
            segs[(size_t)idx].startBeat = tb;
        else
            segs[(size_t)idx].endBeat = tb;
    }

    normalizeCompSegmentsAfterDrag(segs, sessionLen);

    const double refBeat = snapToGrid ? juce::jlimit(0.0, sessionLen, snapBeat(tb)) : tb;
    double outAnchor = refBeat;
    double bestD = 1e100;
    for (const auto& s : segs)
    {
        for (double e : { s.startBeat, s.endBeat })
        {
            const double d = std::abs(e - refBeat);
            if (d < bestD)
            {
                bestD = d;
                outAnchor = e;
            }
        }
    }
    if (rebuildAfter)
        rebuildCombinedClipImpl();
    return outAnchor;
}

void PatternFlowProcessor::applyCompBoundaryMovesFromNorms(const float* norms, int numNorms)
{
    const double sessionLen = (double)(arrangementBars.load() * 4);
    if (sessionLen <= 1.0e-9) return;

    juce::ScopedLock sl(laneLock);
    ensureDefaultTakeComp();
    if (takeComps.empty()) return;
    auto& segs = takeComps[0].segments;
    if (segs.empty()) return;

    const int nApply = juce::jmin(numNorms, kNumCompBoundaryAutomationParams);
    for (int i = 0; i < nApply; ++i)
    {
        const std::vector<double> bounds = collectSortedInternalCompBoundaries(segs, sessionLen);
        if (i >= (int)bounds.size()) break;

        const double targetBeat = juce::jlimit(0.0, sessionLen, (double)norms[i] * sessionLen);
        const double fromBeat = bounds[(size_t)i];

        if (std::abs(fromBeat - targetBeat) < 1.0e-7) continue;

        applyCompBoundaryDragLocked(fromBeat, targetBeat, false, false);
    }
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::applyCompMovesFromMidiAndHostParameters(juce::MidiBuffer& midi)
{
    float norms[kNumCompBoundaryAutomationParams];
    for (int i = 0; i < kNumCompBoundaryAutomationParams; ++i)
    {
        if (auto* raw = apvts.getRawParameterValue("compBnd" + juce::String(i)))
            norms[i] = raw->load();
        else
            norms[i] = 0.5f;
    }

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (!msg.isController()) continue;
        const int cc = msg.getControllerNumber();
        if (cc < kCompSliceMidiCcStart
            || cc >= kCompSliceMidiCcStart + kNumCompBoundaryAutomationParams)
            continue;
        const int idx = cc - kCompSliceMidiCcStart;
        norms[idx] = msg.getControllerValue() / 127.0f;
    }

    applyCompBoundaryMovesFromNorms(norms, kNumCompBoundaryAutomationParams);
}

void PatternFlowProcessor::deleteCompSegmentCoveringBeat(int laneIndex, double beat)
{
    bool changed = false;
    {
        juce::ScopedLock sl(laneLock);
        ensureDefaultTakeComp();
        if (takeComps.empty()) return;
        auto& segs = takeComps[0].segments;
        constexpr double kEps = 1e-5;
        for (size_t i = 0; i < segs.size(); ++i)
        {
            const auto& s = segs[i];
            if (s.laneIndex != laneIndex) continue;
            if (beat >= s.startBeat - kEps && beat < s.endBeat - kEps)
            {
                segs.erase(segs.begin() + (ptrdiff_t)i);
                rebuildCombinedClipImpl();
                changed = true;
                break;
            }
        }
    }
    if (changed) scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::syncCompBoundaryAutomationParamsFromModel()
{
    const double sessionLen = (double)(arrangementBars.load() * 4);
    std::vector<double> bounds;

    {
        juce::ScopedLock sl(laneLock);
        ensureDefaultTakeComp();
        const auto& segs = takeComps[0].segments;
        if (sessionLen > 1.0e-9 && !segs.empty())
            bounds = collectSortedInternalCompBoundaries(segs, sessionLen);
    }

    // Only push model → host for indices that map to a real boundary. Skipping the rest
    // avoids resetting Live LFO/modulation on unused "Comp slice N" slots (previously forced to 0.5).
    // Compare using getRawParameterValue() so we match what the audio thread sees; getValue() can
    // lag the modulated effective value and caused spurious setValueNotifyingHost that killed LFOs.
    for (int i = 0; i < (int)bounds.size() && i < kNumCompBoundaryAutomationParams; ++i)
    {
        const juce::String pid = "compBnd" + juce::String(i);
        const float nv = (float)(bounds[(size_t)i] / sessionLen);
        if (auto* raw = apvts.getRawParameterValue(pid))
        {
            if (std::abs(raw->load() - nv) > 1.5e-5f)
                if (auto* p = apvts.getParameter(pid))
                    p->setValueNotifyingHost(nv);
        }
    }
}

void PatternFlowProcessor::remapLanesAfterDeleteLocked(int removedIndex)
{
    comping::remapLanesAfterDelete(takeComps, removedIndex);
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::remapLanesAfterInsertLocked(int insertLaneIndex)
{
    comping::remapLanesAfterInsert(takeComps, insertLaneIndex);
    rebuildCombinedClipImpl();
}

void PatternFlowProcessor::remapLanesAfterDelete(int removedIndex)
{
    {
        juce::ScopedLock sl(laneLock);
        remapLanesAfterDeleteLocked(removedIndex);
    }
    scheduleCompBoundaryParamSync();
}

void PatternFlowProcessor::remapLanesAfterInsert(int insertLaneIndex)
{
    {
        juce::ScopedLock sl(laneLock);
        remapLanesAfterInsertLocked(insertLaneIndex);
    }
    scheduleCompBoundaryParamSync();
}

// ── State persistence ─────────────────────────────────────────────────────────

void PatternFlowProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("PatternFlowState");
    xml.setAttribute("version", 4);
    xml.setAttribute("appThemeId", appThemeId.load());
    xml.setAttribute("scaleEnabled", scaleEnabled.load());
    xml.setAttribute("scaleRoot",    scaleRoot.load());
    xml.setAttribute("scaleType",    scaleType.load());
    xml.setAttribute("splitEnabled", splitEnabled.load());
    xml.setAttribute("humanVelocity", (double)humanVelocity.load());
    xml.setAttribute("humanTiming",   (double)humanTiming.load());
    xml.setAttribute("humanFeel",     (double)humanFeel.load());
    xml.setAttribute("humanIntonation", (double)intonation.load());
    xml.setAttribute("arrangementBars", arrangementBars.load());
    xml.setAttribute("gridSnap",     gridSnap.load());
    xml.setAttribute("loopEnabled",  loopEnabled.load());
    xml.setAttribute("loopStartBeat", loopStartBeat.load());
    xml.setAttribute("loopEndBeat",  loopEndBeat.load());
    xml.setAttribute("lastBrowserDir", lastBrowserDir);

    // Serialize lanes and clips
    {
        juce::ScopedLock sl(laneLock);
        auto* lanesXml = xml.createNewChildElement("Lanes");
        for (auto& lane : lanes)
        {
            auto* laneXml = lanesXml->createNewChildElement("Lane");
            laneXml->setAttribute("name", lane.name);
            laneXml->setAttribute("colour", (int)lane.colour.getARGB());
            laneXml->setAttribute("expanded", lane.expanded);

            auto* clipsXml = laneXml->createNewChildElement("Clips");
            for (auto& clip : lane.clips)
            {
                auto* clipXml = clipsXml->createNewChildElement("Clip");
                clipXml->setAttribute("name", clip.name);
                clipXml->setAttribute("filePath", clip.filePath);
                clipXml->setAttribute("lengthBeats", clip.lengthBeats);
                clipXml->setAttribute("colour", (int)clip.colour.getARGB());
                clipXml->setAttribute("rootNoteOffset", clip.rootNoteOffset);
                clipXml->setAttribute("clipStartOffset", clip.clipStartOffset);

                auto* notesXml = clipXml->createNewChildElement("Notes");
                for (auto& note : clip.notes)
                {
                    auto* noteXml = notesXml->createNewChildElement("N");
                    noteXml->setAttribute("p", note.noteNumber);
                    noteXml->setAttribute("v", note.velocity);
                    noteXml->setAttribute("s", note.startBeat);
                    noteXml->setAttribute("l", note.lengthBeats);
                    noteXml->setAttribute("c", note.channel);
                }
            }

            auto* regionsXml = laneXml->createNewChildElement("Regions");
            for (auto& region : lane.regions)
            {
                auto* regXml = regionsXml->createNewChildElement("R");
                regXml->setAttribute("start", region.startBeat);
                regXml->setAttribute("end", region.endBeat);
                regXml->setAttribute("clip", region.clipIndex);
                regXml->setAttribute("filter", region.noteFilter);
                regXml->setAttribute("muted", region.muted);
            }
        }

        auto* compsRoot = xml.createNewChildElement("TakeComps");
        compsRoot->setAttribute("enabled", compsEnabled.load());
        compsRoot->setAttribute("randomN", compRandomRegionCount.load());
        if (!takeComps.empty())
        {
            const auto& tc = takeComps[0];
            auto* cx = compsRoot->createNewChildElement("Comp");
            cx->setAttribute("name", tc.name);
            for (const auto& seg : tc.segments)
            {
                auto* sx = cx->createNewChildElement("Seg");
                sx->setAttribute("lane", seg.laneIndex);
                sx->setAttribute("s", seg.startBeat);
                sx->setAttribute("e", seg.endBeat);
            }
        }
    }

    // Serialize split rules
    {
        auto* splitsXml = xml.createNewChildElement("SplitRules");
        juce::ScopedLock sl(splitLock);
        for (auto& rule : splitRules)
        {
            auto* ruleXml = splitsXml->createNewChildElement("Rule");
            ruleXml->setAttribute("min", rule.noteMin);
            ruleXml->setAttribute("max", rule.noteMax);
            ruleXml->setAttribute("out", rule.outputIndex);
        }
    }

    if (auto paramsState = apvts.copyState().createXml())
        xml.addChildElement(paramsState.release());

    copyXmlToBinary(xml, dest);
}

void PatternFlowProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    resetToDefaultSession();
    if (data != nullptr && sizeInBytes > 0)
    {
        if (auto xml = getXmlFromBinary(data, sizeInBytes))
        {
            if (auto* pNode = xml->getChildByName(apvts.state.getType().toString()))
                apvts.replaceState(juce::ValueTree::fromXml(*pNode));
        }
    }
}

juce::AudioProcessorEditor* PatternFlowProcessor::createEditor()
{
    return new PatternFlowEditor(*this);
}

} // namespace pflow

// ── Entry point ──────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pflow::PatternFlowProcessor();
}
