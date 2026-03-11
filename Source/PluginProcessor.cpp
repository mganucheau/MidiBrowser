#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pflow {

PatternFlowProcessor::PatternFlowProcessor()
    : AudioProcessor(BusesProperties())
{
    // Start with a single empty lane
    CompLane defaultLane;
    defaultLane.name   = "Lane 1";
    defaultLane.colour = juce::Colour(0xff3a7bd5);
    lanes.push_back(defaultLane);
}

void PatternFlowProcessor::prepareToPlay(double sr, int /*samplesPerBlock*/)
{
    sampleRate_ = sr;
}

void PatternFlowProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();

    // Read host transport
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

    if (!hostPlaying.load()) return;

    double bpm     = hostBpm.load();
    double beatPos = hostBeatPos.load();

    double secPerBeat    = 60.0 / bpm;
    double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    double endBeat       = beatPos + buffer.getNumSamples() * beatsPerSample;

    generateMidiForBeatRange(beatPos, endBeat, midi, buffer.getNumSamples());

    lastBeatPos_ = endBeat;
}

void PatternFlowProcessor::generateMidiForBeatRange(double startBeat,
                                                     double endBeat,
                                                     juce::MidiBuffer& output,
                                                     int numSamples)
{
    juce::ScopedLock sl(laneLock);

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
                // Filter by note if per-note comping
                if (region.noteFilter >= 0 && note.noteNumber != region.noteFilter)
                    continue;

                double noteGlobalStart = region.startBeat + note.startBeat;
                double noteGlobalEnd   = noteGlobalStart + note.lengthBeats;

                // Note-on in this block?
                if (noteGlobalStart >= startBeat && noteGlobalStart < endBeat)
                {
                    double fraction = (noteGlobalStart - startBeat) / (endBeat - startBeat);
                    int sampleOffset = juce::jlimit(0, numSamples - 1,
                                                    (int)(fraction * numSamples));

                    int pitch = note.noteNumber + clip.rootNoteOffset;

                    // Apply scale quantisation
                    if (scaleEnabled.load())
                        pitch = quantiseToScale(pitch, scaleRoot.load(),
                                               (ScaleType)scaleType.load());

                    pitch = juce::jlimit(0, 127, pitch);

                    int vel = applyHumanVelocity(note.velocity);

                    int channel = note.channel;

                    // Apply MIDI split routing
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
                }

                // Note-off in this block?
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
    // TODO: serialise lanes, split rules, scale settings
    juce::ignoreUnused(dest);
}

void PatternFlowProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
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
