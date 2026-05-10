#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"
#include <cmath>

namespace pflow {

MidiBrowserProcessor::MidiBrowserProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    applyAppTheme(appThemeId.load());
}

void MidiBrowserProcessor::prepareToPlay(double sr, int)
{
    sampleRate_ = sr;
}

void MidiBrowserProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    buffer.clear();

    if (auto* playHead = getPlayHead())
    {
        if (const auto posInfo = playHead->getPosition())
        {
            if (auto bpm = posInfo->getBpm(); bpm.hasValue())
                hostBpm.store(*bpm);
            if (auto ppq = posInfo->getPpqPosition(); ppq.hasValue())
                hostBeatPos.store(*ppq);
            hostPlaying.store(posInfo->getIsPlaying());

            const bool looping = posInfo->getIsLooping();
            if (looping)
            {
                if (auto lp = posInfo->getLoopPoints(); lp.hasValue()
                    && lp->ppqEnd > lp->ppqStart + 1.0e-6)
                {
                    hostLoopActive.store(true);
                    hostLoopPpqStart.store(lp->ppqStart);
                    hostLoopPpqEnd.store(lp->ppqEnd);
                }
                else
                    hostLoopActive.store(false);
            }
            else
                hostLoopActive.store(false);
        }
    }

    midi.clear();

    if (!hostPlaying.load())
    {
        for (int ch = 1; ch <= 16; ++ch)
            midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        lastBeatPos_ = -1.0;
        return;
    }

    double bpm = hostBpm.load();
    double beatPos = hostBeatPos.load();
    if (bpm <= 0.0) bpm = 120.0;

    const double secPerBeat = 60.0 / bpm;
    const double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    const int numSamples = buffer.getNumSamples();
    const double blockBeats = (double) numSamples * beatsPerSample;
    const double endBeat = beatPos + blockBeats;

    juce::MidiBuffer generated;

    if (lastBeatPos_ >= 0.0 && std::abs(beatPos - lastBeatPos_) > beatsPerSample * 2.0)
    {
        for (int ch = 1; ch <= 16; ++ch)
        {
            generated.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
            generated.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
        }
    }

    bool previewMuted = true, previewHasClip = false;
    MidiClip previewClip;
    {
        juce::ScopedLock pl(previewLock_);
        previewMuted = previewMuted_;
        previewHasClip = previewHasClip_;
        previewClip = previewClip_;
    }

    if (!previewMuted && previewHasClip && previewClip.lengthBeats > 0.0)
    {
        const double sessionLenBeats = (double) (juce::jmax(1, syncSessionBars.load()) * 4);

        double pStart = beatPos;
        double pEnd = endBeat;

        if (hostLoopActive.load())
        {
            const double ls = hostLoopPpqStart.load();
            const double le = hostLoopPpqEnd.load();
            const double loopLen = le - ls;
            if (loopLen > 1.0e-6 && beatPos + 1.0e-9 >= ls)
            {
                pStart = ls + std::fmod(beatPos - ls, loopLen);
                pEnd = pStart + blockBeats;
            }
            else if (sessionLenBeats > 1.0e-9)
            {
                pStart = std::fmod(beatPos, sessionLenBeats);
                if (pStart < 0.0) pStart += sessionLenBeats;
                pEnd = pStart + blockBeats;
            }
        }
        else if (sessionLenBeats > 1.0e-9)
        {
            pStart = std::fmod(beatPos, sessionLenBeats);
            if (pStart < 0.0) pStart += sessionLenBeats;
            pEnd = pStart + blockBeats;
        }

        generatePreviewMidi(previewClip, pStart, pEnd, generated, numSamples);
    }

    lastBeatPos_ = endBeat;

    for (const auto metadata : generated)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);
}

void MidiBrowserProcessor::setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed)
{
    juce::ScopedLock sl(previewLock_);
    previewClip_ = clip;
    previewHasClip_ = hasClip;
    previewMuted_ = muted;
    juce::ignoreUnused(soloed);
}

void MidiBrowserProcessor::generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                                               juce::MidiBuffer& output, int numSamples,
                                               int sampleOffsetBase)
{
    const double clipLen = std::max(0.25, clip.lengthBeats);
    const double blockLen = endBeat - startBeat;
    if (blockLen <= 0.0) return;

    for (const auto& note : clip.notes)
    {
        double noteStart = note.startBeat - clip.clipStartOffset;
        if (noteStart < 0.0) noteStart += clipLen;
        const double noteEnd = noteStart + note.lengthBeats;

        const int kMin = juce::jmax(0, (int) std::floor((startBeat - noteEnd) / clipLen) + 1);
        const int kMax = (int) std::ceil((endBeat - noteStart) / clipLen);
        for (int k = kMin; k <= kMax; ++k)
        {
            const double globalStart = (double) k * clipLen + noteStart;
            const double globalEnd = (double) k * clipLen + noteEnd;
            if (globalEnd <= startBeat || globalStart >= endBeat) continue;

            if (globalStart >= startBeat && globalStart < endBeat)
            {
                const double fraction = (globalStart - startBeat) / blockLen;
                const int sampleOffset = sampleOffsetBase
                    + juce::jlimit(0, numSamples - 1, (int) (fraction * (double) numSamples));
                const int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                output.addEvent(juce::MidiMessage::noteOn(note.channel, pitch, (juce::uint8) note.velocity),
                                sampleOffset);
            }
            if (globalEnd >= startBeat && globalEnd < endBeat)
            {
                const double fraction = (globalEnd - startBeat) / blockLen;
                const int sampleOffset = sampleOffsetBase
                    + juce::jlimit(0, numSamples - 1, (int) (fraction * (double) numSamples));
                const int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                output.addEvent(juce::MidiMessage::noteOff(note.channel, pitch), sampleOffset);
            }
        }
    }
}

void MidiBrowserProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("MidiBrowserState");
    xml.setAttribute("version", 1);
    xml.setAttribute("appThemeId", appThemeId.load());
    xml.setAttribute("syncSessionBars", syncSessionBars.load());
    xml.setAttribute("lastBrowserDir", lastBrowserDir);
    copyXmlToBinary(xml, dest);
}

void MidiBrowserProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    lastBrowserDir.clear();
    syncSessionBars.store(4);
    if (data == nullptr || sizeInBytes <= 0)
        return;
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName("MidiBrowserState") || xml->hasTagName("PatternFlowState"))
        {
            appThemeId.store(xml->getIntAttribute("appThemeId", appThemeId.load()));
            syncSessionBars.store(juce::jlimit(1, 256, xml->getIntAttribute("syncSessionBars",
                xml->getIntAttribute("arrangementBars", syncSessionBars.load()))));
            lastBrowserDir = xml->getStringAttribute("lastBrowserDir");
            applyAppTheme(appThemeId.load());
        }
    }
}

juce::AudioProcessorEditor* MidiBrowserProcessor::createEditor()
{
    return new MidiBrowserEditor(*this);
}

} // namespace pflow

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pflow::MidiBrowserProcessor();
}
