#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"

namespace pflow {

PatternFlowProcessor::PatternFlowProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    auto presets = getClipColourPresets();
    {
        CompLane lane;
        lane.name   = "Lane 1";
        lane.colour = presets[0];
        lanes.push_back(lane);
    }
}

double PatternFlowProcessor::snapBeat(double beat) const
{
    int g = gridSnap.load();
    if (g == (int)GridSize::Off) return beat;

    double div = 1.0;
    switch ((GridSize)g)
    {
        case GridSize::Bar:         div = 4.0;   break;
        case GridSize::Beat:        div = 1.0;   break;
        case GridSize::HalfBeat:    div = 0.5;   break;
        case GridSize::QuarterBeat: div = 0.25;  break;
        case GridSize::Eighth:      div = 0.5;   break;
        case GridSize::Sixteenth:   div = 0.25;  break;
        case GridSize::Off:         return beat;
    }
    return std::round(beat / div) * div;
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
        double bpmRec   = hostBpm.load();
        double beatRec  = hostBeatPos.load();
        if (bpmRec <= 0.0) bpmRec = 120.0;
        double secPerBeatRec = 60.0 / bpmRec;
        double beatsPerSampleRec = 1.0 / (sampleRate_ * secPerBeatRec);

        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();
            double noteBeat = beatRec + metadata.samplePosition * beatsPerSampleRec;

            if (msg.isNoteOn())
            {
                double sessionLenBeats = (double)(arrangementBars.load() * 4);
                double localBeat = noteBeat - recordingStartBeat;

                // If we've gone past session length, finalise clip and start new lane
                if (localBeat >= sessionLenBeats)
                {
                    finaliseRecordingClip();
                    // Start a new recording lane
                    recordingStartBeat = noteBeat;
                    recordingClipCount++;
                    {
                        juce::ScopedLock sl(laneLock);
                        CompLane newLane;
                        auto presets = getClipColourPresets();
                        int idx = (int)lanes.size();
                        newLane.name   = "Rec " + juce::String(recordingClipCount + 1);
                        newLane.colour = presets[idx % presets.size()];

                        // Create empty clip for the new lane
                        MidiClip recClip;
                        recClip.name = "Recording";
                        recClip.colour = newLane.colour;
                        recClip.lengthBeats = sessionLenBeats;
                        newLane.addClip(recClip, 0.0);

                        lanes.push_back(newLane);
                        recordingLaneIndex = idx;
                    }
                }

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

    // Clear incoming MIDI - we generate our own
    midi.clear();

    if (!hostPlaying.load())
    {
        // If recording was active, stop it when transport stops
        if (recording.load())
            stopRecording();

        for (auto& an : activeNotes_)
            midi.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
        activeNotes_.clear();
        lastBeatPos_ = -1.0;
        return;
    }

    double bpm     = hostBpm.load();
    double beatPos = hostBeatPos.load();

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

    // Loop wrapping: map host beat position into the loop range
    if (loopEnabled.load())
    {
        double loopStart = loopStartBeat.load();
        double loopEnd   = loopEndBeat.load();
        double loopLen   = loopEnd - loopStart;

        if (loopLen > 0.0 && beatPos >= loopStart)
        {
            double mapped = loopStart + std::fmod(beatPos - loopStart, loopLen);
            mappedBeatPos.store(mapped);

            // Check if this block crosses the loop boundary
            double mappedEnd = mapped + blockBeats;
            if (mappedEnd > loopEnd)
            {
                // Split into two ranges: before and after the loop wrap
                double firstLen  = loopEnd - mapped;
                int firstSamples = std::max(1, (int)(firstLen / blockBeats * buffer.getNumSamples()));
                int secondSamples = buffer.getNumSamples() - firstSamples;

                generateMidiForBeatRange(mapped, loopEnd, generated, firstSamples);

                // Send note-offs at wrap point
                for (auto& an : activeNotes_)
                    generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), firstSamples);
                activeNotes_.clear();

                double secondLen = blockBeats - firstLen;
                generateMidiForBeatRange(loopStart, loopStart + secondLen, generated, secondSamples, firstSamples);
                mappedBeatPos.store(loopStart + secondLen);
            }
            else
            {
                generateMidiForBeatRange(mapped, mappedEnd, generated, buffer.getNumSamples());
            }
        }
        else
        {
            mappedBeatPos.store(beatPos);
            generateMidiForBeatRange(beatPos, endBeat, generated, buffer.getNumSamples());
        }
    }
    else
    {
        mappedBeatPos.store(beatPos);
        generateMidiForBeatRange(beatPos, endBeat, generated, buffer.getNumSamples());
    }
    lastBeatPos_ = endBeat;

    for (const auto metadata : generated)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);
}

void PatternFlowProcessor::generateMidiForBeatRange(double startBeat,
                                                     double endBeat,
                                                     juce::MidiBuffer& output,
                                                     int numSamples,
                                                     int sampleOffsetBase)
{
    juce::ScopedLock sl(laneLock);
    juce::ScopedLock sl2(splitLock);

    bool anySolo = false;
    for (auto& l : lanes)
        if (l.solo) { anySolo = true; break; }

    for (int li = 0; li < (int)lanes.size(); ++li)
    {
        auto& lane = lanes[static_cast<size_t>(li)];
        if (lane.muted) continue;
        if (anySolo && !lane.solo) continue;

        for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
        {
            auto& clip = lane.clips[static_cast<size_t>(ci)];
            double cs = lane.clipStarts[static_cast<size_t>(ci)];
            double ce = cs + clip.lengthBeats;
            if (ce <= startBeat || cs >= endBeat) continue;

            double loopLen = clip.lengthBeats;
            if (loopLen <= 0.0) continue;

            for (auto& note : clip.notes)
            {
                double adjustedStart = note.startBeat - clip.clipStartOffset;
                if (adjustedStart < 0.0) adjustedStart += loopLen;

                double noteGlobalStart = cs + adjustedStart;
                while (noteGlobalStart < ce)
                {
                    if (noteGlobalStart >= startBeat && noteGlobalStart < endBeat)
                    {
                        double noteGlobalEnd = noteGlobalStart + note.lengthBeats;

                        double fraction = (noteGlobalStart - startBeat) / (endBeat - startBeat);
                        int sampleOffset = sampleOffsetBase + juce::jlimit(0, numSamples - 1,
                                                                           (int)(fraction * numSamples));
                        int pitch = note.noteNumber + clip.rootNoteOffset;
                        if (scaleEnabled.load())
                            pitch = quantiseToScale(pitch, scaleRoot.load(), (ScaleType)scaleType.load());
                        pitch = juce::jlimit(0, 127, pitch);
                        int vel = applyHumanVelocity(note.velocity);
                        int channel = note.channel;
                        if (splitEnabled.load())
                            for (auto& rule : splitRules)
                                if (pitch >= rule.noteMin && pitch <= rule.noteMax)
                                    { channel = juce::jlimit(1, 16, rule.outputIndex + 1); break; }
                        output.addEvent(juce::MidiMessage::noteOn(channel, pitch, (juce::uint8)vel), sampleOffset);
                        activeNotes_.push_back({ pitch, channel });

                        if (noteGlobalEnd >= startBeat && noteGlobalEnd < endBeat)
                        {
                            double frac2 = (noteGlobalEnd - startBeat) / (endBeat - startBeat);
                            int sOff2 = sampleOffsetBase + juce::jlimit(0, numSamples - 1,
                                                                        (int)(frac2 * numSamples));
                            output.addEvent(juce::MidiMessage::noteOff(channel, pitch), sOff2);
                            activeNotes_.erase(
                                std::remove_if(activeNotes_.begin(), activeNotes_.end(),
                                    [pitch, channel](const ActiveNote& a) {
                                        return a.pitch == pitch && a.channel == channel; }),
                                activeNotes_.end());
                        }
                    }
                    noteGlobalStart += loopLen;
                }
            }
        }
    }
}

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

    // Create an empty clip in the lane at beat 0
    MidiClip recClip;
    recClip.name = "Recording";
    recClip.colour = recLane.colour;
    recClip.lengthBeats = (double)(arrangementBars.load() * 4);
    recLane.addClip(recClip, 0.0);

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
            }
        }
    }

    recordingActiveNotes.clear();
    recordingLaneIndex = -1;
    recordingClipCount = 0;
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
    }

    // Keep the active notes list - they'll be re-opened in the new clip
    // But update their start beats to 0 (relative to new clip start)
    // (this happens in the caller which sets recordingStartBeat)
}

// ── Master clip rebuild ──────────────────────────────────────────────────────

void PatternFlowProcessor::rebuildMasterClip()
{
    juce::ScopedLock sl(laneLock);

    masterClip.notes.clear();
    masterClip.name = "Master";
    masterClip.colour = juce::Colour(0xff888888);

    int totalBeats = arrangementBars.load() * 4;
    masterClip.lengthBeats = (double)totalBeats;

    bool anySolo = false;
    for (auto& lane : lanes)
        if (lane.solo) { anySolo = true; break; }

    for (auto& lane : lanes)
    {
        if (lane.muted) continue;
        if (anySolo && !lane.solo) continue;

        for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
        {
            auto& clip = lane.clips[static_cast<size_t>(ci)];
            double clipStart = lane.clipStarts[static_cast<size_t>(ci)];
            if (clip.lengthBeats <= 0.0) continue;

            for (auto& note : clip.notes)
            {
                double adjustedStart = note.startBeat - clip.clipStartOffset;
                if (adjustedStart < 0.0) adjustedStart += clip.lengthBeats;

                double globalStart = clipStart + adjustedStart;
                double globalEnd = globalStart + note.lengthBeats;
                if (globalStart >= totalBeats) continue;
                globalEnd = std::min(globalEnd, (double)totalBeats);

                NoteEvent merged;
                merged.noteNumber  = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                merged.velocity    = note.velocity;
                merged.startBeat   = globalStart;
                merged.lengthBeats = globalEnd - globalStart;
                merged.channel     = note.channel;
                masterClip.notes.push_back(merged);
            }
        }
    }
}

// ── State persistence ────────────────────────────────────────────────────────

void PatternFlowProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("PatternFlowState");
    xml.setAttribute("version", 3);
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
            laneXml->setAttribute("muted", lane.muted);
            laneXml->setAttribute("solo", lane.solo);

            auto* clipsXml = laneXml->createNewChildElement("Clips");
            for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
            {
                auto& clip = lane.clips[static_cast<size_t>(ci)];
                auto* clipXml = clipsXml->createNewChildElement("Clip");
                clipXml->setAttribute("name", clip.name);
                clipXml->setAttribute("filePath", clip.filePath);
                clipXml->setAttribute("lengthBeats", clip.lengthBeats);
                clipXml->setAttribute("colour", (int)clip.colour.getARGB());
                clipXml->setAttribute("rootNoteOffset", clip.rootNoteOffset);
                clipXml->setAttribute("clipStartOffset", clip.clipStartOffset);
                clipXml->setAttribute("beatPos", lane.clipStarts[static_cast<size_t>(ci)]);

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

    copyXmlToBinary(xml, dest);
}

void PatternFlowProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName("PatternFlowState"))
    {
        scaleEnabled .store(xml->getBoolAttribute("scaleEnabled", false));
        scaleRoot    .store(xml->getIntAttribute("scaleRoot", 0));
        scaleType    .store(xml->getIntAttribute("scaleType", 0));
        splitEnabled .store(xml->getBoolAttribute("splitEnabled", false));
        humanVelocity.store((float)xml->getDoubleAttribute("humanVelocity", 0.0));
        humanTiming  .store((float)xml->getDoubleAttribute("humanTiming", 0.0));
        humanFeel    .store((float)xml->getDoubleAttribute("humanFeel", 0.0));
        intonation   .store((float)xml->getDoubleAttribute("humanIntonation", 0.0));
        arrangementBars.store(xml->getIntAttribute("arrangementBars", 8));
        gridSnap     .store(xml->getIntAttribute("gridSnap", (int)GridSize::Beat));
        loopEnabled  .store(xml->getBoolAttribute("loopEnabled", false));
        loopStartBeat.store(xml->getDoubleAttribute("loopStartBeat", 0.0));
        loopEndBeat  .store(xml->getDoubleAttribute("loopEndBeat", 32.0));
        lastBrowserDir = xml->getStringAttribute("lastBrowserDir", "");

        // Default: start with an empty session (one blank lane).
        {
            juce::ScopedLock sl(laneLock);
            lanes.clear();

            CompLane lane;
            lane.name   = "Lane 1";
            lane.colour = getClipColourPresets()[0];
            lanes.push_back(lane);
        }

        // Restore split rules
        if (auto* splitsXml = xml->getChildByName("SplitRules"))
        {
            juce::ScopedLock sl(splitLock);
            splitRules.clear();
            for (auto* ruleXml : splitsXml->getChildIterator())
            {
                MidiSplitRule rule;
                rule.noteMin     = ruleXml->getIntAttribute("min", 0);
                rule.noteMax     = ruleXml->getIntAttribute("max", 127);
                rule.outputIndex = ruleXml->getIntAttribute("out", 0);
                splitRules.push_back(rule);
            }
        }
    }
    rebuildMasterClip();
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
