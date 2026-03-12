#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pflow {

PatternFlowProcessor::PatternFlowProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    CompLane defaultLane;
    defaultLane.name   = "Lane 1";
    defaultLane.colour = juce::Colour(0xff3a7bd5);
    lanes.push_back(defaultLane);
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

    if (!hostPlaying.load())
    {
        for (auto& an : activeNotes_)
            midi.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
        activeNotes_.clear();
        lastBeatPos_ = -1.0;
        return;
    }

    double bpm     = hostBpm.load();
    double beatPos = hostBeatPos.load();

    double secPerBeat     = 60.0 / bpm;
    double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    double endBeat        = beatPos + buffer.getNumSamples() * beatsPerSample;

    // Detect transport jump - send all-notes-off
    if (lastBeatPos_ >= 0.0 && std::abs(beatPos - lastBeatPos_) > beatsPerSample * 2.0)
    {
        for (auto& an : activeNotes_)
            generated.addEvent(juce::MidiMessage::noteOff(an.channel, an.pitch), 0);
        activeNotes_.clear();
    }

    generateMidiForBeatRange(beatPos, endBeat, generated, buffer.getNumSamples());
    lastBeatPos_ = endBeat;

    for (const auto metadata : generated)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);
}

void PatternFlowProcessor::generateMidiForBeatRange(double startBeat,
                                                     double endBeat,
                                                     juce::MidiBuffer& output,
                                                     int numSamples)
{
    juce::ScopedLock sl(laneLock);
    juce::ScopedLock sl2(splitLock);

    for (auto& lane : lanes)
    {
        for (auto& region : lane.regions)
        {
            if (region.muted || region.clipIndex < 0 ||
                region.clipIndex >= (int)lane.clips.size())
                continue;

            auto& clip = lane.clips[region.clipIndex];

            for (auto& note : clip.notes)
            {
                if (region.noteFilter >= 0 && note.noteNumber != region.noteFilter)
                    continue;

                double noteGlobalStart = region.startBeat + note.startBeat;
                double noteGlobalEnd   = noteGlobalStart + note.lengthBeats;

                if (noteGlobalStart >= startBeat && noteGlobalStart < endBeat)
                {
                    double fraction = (noteGlobalStart - startBeat) / (endBeat - startBeat);
                    int sampleOffset = juce::jlimit(0, numSamples - 1,
                                                    (int)(fraction * numSamples));

                    int pitch = note.noteNumber + clip.rootNoteOffset;
                    if (scaleEnabled.load())
                        pitch = quantiseToScale(pitch, scaleRoot.load(),
                                               (ScaleType)scaleType.load());
                    pitch = juce::jlimit(0, 127, pitch);

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
                    int sampleOffset = juce::jlimit(0, numSamples - 1,
                                                    (int)(fraction * numSamples));

                    int pitch = note.noteNumber + clip.rootNoteOffset;
                    if (scaleEnabled.load())
                        pitch = quantiseToScale(pitch, scaleRoot.load(),
                                               (ScaleType)scaleType.load());
                    pitch = juce::jlimit(0, 127, pitch);

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

// ── State persistence ────────────────────────────────────────────────────────

void PatternFlowProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("PatternFlowState");
    xml.setAttribute("version", 2);
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

        // Restore lanes and clips
        if (auto* lanesXml = xml->getChildByName("Lanes"))
        {
            juce::ScopedLock sl(laneLock);
            lanes.clear();

            for (auto* laneXml : lanesXml->getChildIterator())
            {
                CompLane lane;
                lane.name     = laneXml->getStringAttribute("name", "Lane");
                lane.colour   = juce::Colour((juce::uint32)laneXml->getIntAttribute("colour", (int)0xff3a7bd5));
                lane.expanded = laneXml->getBoolAttribute("expanded", false);

                if (auto* clipsXml = laneXml->getChildByName("Clips"))
                {
                    for (auto* clipXml : clipsXml->getChildIterator())
                    {
                        MidiClip clip;
                        clip.name           = clipXml->getStringAttribute("name");
                        clip.filePath       = clipXml->getStringAttribute("filePath");
                        clip.lengthBeats    = clipXml->getDoubleAttribute("lengthBeats", 4.0);
                        clip.colour         = juce::Colour((juce::uint32)clipXml->getIntAttribute("colour", (int)0xff3a7bd5));
                        clip.rootNoteOffset = clipXml->getIntAttribute("rootNoteOffset", 0);

                        if (auto* notesXml = clipXml->getChildByName("Notes"))
                        {
                            for (auto* noteXml : notesXml->getChildIterator())
                            {
                                NoteEvent note;
                                note.noteNumber  = noteXml->getIntAttribute("p", 60);
                                note.velocity    = noteXml->getIntAttribute("v", 100);
                                note.startBeat   = noteXml->getDoubleAttribute("s", 0.0);
                                note.lengthBeats = noteXml->getDoubleAttribute("l", 1.0);
                                note.channel     = noteXml->getIntAttribute("c", 1);
                                clip.notes.push_back(note);
                            }
                        }
                        lane.clips.push_back(clip);
                    }
                }

                if (auto* regionsXml = laneXml->getChildByName("Regions"))
                {
                    for (auto* regXml : regionsXml->getChildIterator())
                    {
                        CompRegion region;
                        region.startBeat  = regXml->getDoubleAttribute("start", 0.0);
                        region.endBeat    = regXml->getDoubleAttribute("end", 4.0);
                        region.clipIndex  = regXml->getIntAttribute("clip", -1);
                        region.noteFilter = regXml->getIntAttribute("filter", -1);
                        region.muted      = regXml->getBoolAttribute("muted", false);
                        lane.regions.push_back(region);
                    }
                }

                lanes.push_back(lane);
            }
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
