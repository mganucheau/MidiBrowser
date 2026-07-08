#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"
#include <cmath>

namespace pflow {

MidiBrowserProcessor::MidiBrowserProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
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

    const bool synced = syncToHost.load();
    const bool sounding = previewArmed.load() && (!synced || hostPlaying.load());

    if (!sounding)
    {
        // Release held notes once on the playing → stopped transition; stay
        // silent afterwards so downstream instruments aren't spammed.
        if (wasSounding_)
            for (int ch = 1; ch <= 16; ++ch)
            {
                midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                midi.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
            }
        wasSounding_ = false;
        lastBeatPos_ = -1.0;
        return;
    }
    wasSounding_ = true;

    double bpm = synced ? hostBpm.load() : freeBpm.load();
    double beatPos = synced ? hostBeatPos.load() : freerunBeat.load();
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

        if (!synced)
        {
            // Internal clock: loop the armed clip at freeBpm.
            const double clipLen = juce::jmax(0.25, previewClip.lengthBeats);
            pStart = std::fmod(beatPos, clipLen);
            if (pStart < 0.0) pStart += clipLen;
            pEnd = pStart + blockBeats;
            freerunBeat.store(std::fmod(pStart + blockBeats, clipLen));
        }
        else if (hostLoopActive.load())
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

void MidiBrowserProcessor::addSavedBrowserDir(const juce::String& path)
{
    const juce::File dir(path);
    if (!dir.isDirectory()) return;
    const auto fullPath = dir.getFullPathName();
    savedBrowserDirs.removeString(fullPath);
    savedBrowserDirs.insert(0, fullPath);
    while (savedBrowserDirs.size() > 24)
        savedBrowserDirs.remove(savedBrowserDirs.size() - 1);
}

void MidiBrowserProcessor::removeSavedBrowserDir(const juce::String& path)
{
    savedBrowserDirs.removeString(path);
}

void MidiBrowserProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("MidiBrowserState");
    xml.setAttribute("version", 3);
    xml.setAttribute("tweakTheme", tweaks().theme.load());
    xml.setAttribute("tweakAccent", tweaks().accent.load());
    xml.setAttribute("tweakDensity", tweaks().density.load());
    xml.setAttribute("tweakGrid", tweaks().grid.load());
    xml.setAttribute("syncSessionBars", syncSessionBars.load());
    xml.setAttribute("lastBrowserDir", lastBrowserDir);
    xml.setAttribute("trimEmptyMeasuresPreview", trimEmptyMeasuresPreview ? 1 : 0);
    xml.setAttribute("syncToHost", syncToHost.load() ? 1 : 0);
    xml.setAttribute("freeBpm", freeBpm.load());
    xml.setAttribute("editorOpen", editorOpen ? 1 : 0);
    xml.setAttribute("sidebarCollapsed", sidebarCollapsed ? 1 : 0);
    xml.setAttribute("miniOpen", miniOpen ? 1 : 0);
    for (const auto& folder : savedBrowserDirs)
    {
        if (folder.isNotEmpty())
        {
            auto* child = xml.createNewChildElement("SavedFolder");
            child->setAttribute("path", folder);
        }
    }

    for (const auto& [path, edit] : clipEdits)
    {
        if (path.isEmpty() || editIsClean(edit))
            continue;
        auto* e = xml.createNewChildElement("ClipEdit");
        e->setAttribute("path", path);
        e->setAttribute("octave", edit.octave);
        e->setAttribute("fitScale", edit.fitScale ? 1 : 0);
        e->setAttribute("mapToRoot", edit.mapToRoot ? 1 : 0);
        e->setAttribute("root", edit.root);
        e->setAttribute("mode", (int) edit.mode);
        e->setAttribute("trimLead", edit.trimLead);
        e->setAttribute("trimTail", edit.trimTail);
        for (const auto& [id, mv] : edit.moves)
        {
            if (mv.dPitch == 0 && mv.dStep == 0) continue;
            auto* m = e->createNewChildElement("Move");
            m->setAttribute("id", id);
            m->setAttribute("dPitch", mv.dPitch);
            m->setAttribute("dStep", mv.dStep);
        }
    }
    for (const auto& [path, k] : clipGrooves)
    {
        if (path.isEmpty() || k.isDefault())
            continue;
        auto* e = xml.createNewChildElement("ClipGroove");
        e->setAttribute("path", path);
        e->setAttribute("swing", k.swing);
        e->setAttribute("pocket", k.pocket);
        e->setAttribute("humanize", k.humanize);
        e->setAttribute("dynamics", k.dynamics);
        e->setAttribute("length", k.length);
        e->setAttribute("intensity", k.intensity);
    }
    copyXmlToBinary(xml, dest);
}

void MidiBrowserProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    lastBrowserDir.clear();
    savedBrowserDirs.clear();
    trimEmptyMeasuresPreview = false;
    syncSessionBars.store(4);
    clipEdits.clear();
    clipGrooves.clear();
    if (data == nullptr || sizeInBytes <= 0)
        return;
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName("MidiBrowserState") || xml->hasTagName("PatternFlowState"))
        {
            tweaks().theme.store(juce::jlimit(0, kNumThemes - 1,
                xml->getIntAttribute("tweakTheme", (int) ThemeId::Graphite)));
            tweaks().accent.store(juce::jlimit(0, kNumAccents - 1,
                xml->getIntAttribute("tweakAccent", (int) AccentId::Amber)));
            tweaks().density.store(juce::jlimit(0, 1,
                xml->getIntAttribute("tweakDensity", (int) Density::Compact)));
            tweaks().grid.store(juce::jlimit(0, kNumGridStyles - 1,
                xml->getIntAttribute("tweakGrid", (int) GridStyle::Minimal)));
            syncSessionBars.store(juce::jlimit(1, 256, xml->getIntAttribute("syncSessionBars",
                xml->getIntAttribute("arrangementBars", syncSessionBars.load()))));
            lastBrowserDir = xml->getStringAttribute("lastBrowserDir");
            trimEmptyMeasuresPreview = xml->getIntAttribute("trimEmptyMeasuresPreview", 0) != 0;
            syncToHost.store(xml->getIntAttribute("syncToHost", 1) != 0);
            freeBpm.store(juce::jlimit(20.0, 300.0, xml->getDoubleAttribute("freeBpm", 124.0)));
            editorOpen = xml->getIntAttribute("editorOpen", 1) != 0;
            sidebarCollapsed = xml->getIntAttribute("sidebarCollapsed", 1) != 0;
            miniOpen = xml->getIntAttribute("miniOpen", 1) != 0;
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("SavedFolder"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty() && juce::File(path).isDirectory()
                        && !savedBrowserDirs.contains(path))
                        savedBrowserDirs.add(path);
                }
                else if (child->hasTagName("ClipEdit"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isEmpty()) continue;
                    ClipEdit e;
                    e.octave = juce::jlimit(-3, 3, child->getIntAttribute("octave", 0));
                    e.fitScale = child->getIntAttribute("fitScale", 0) != 0;
                    e.mapToRoot = child->getIntAttribute("mapToRoot", 0) != 0;
                    e.root = juce::jlimit(-1, 11, child->getIntAttribute("root", -1));
                    e.mode = (Mode) juce::jlimit(0, kNumModes - 1,
                                                 child->getIntAttribute("mode", (int) Mode::Dorian));
                    e.trimLead = juce::jmax(0, child->getIntAttribute("trimLead", 0));
                    e.trimTail = juce::jmax(0, child->getIntAttribute("trimTail", 0));
                    for (auto* m : child->getChildIterator())
                        if (m->hasTagName("Move"))
                            e.moves[m->getIntAttribute("id")] = {
                                m->getIntAttribute("dPitch"), m->getIntAttribute("dStep") };
                    clipEdits[path] = std::move(e);
                }
                else if (child->hasTagName("ClipGroove"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isEmpty()) continue;
                    GrooveParams k;
                    k.set(0, child->getIntAttribute("swing", 0));
                    k.set(1, child->getIntAttribute("pocket", 0));
                    k.set(2, child->getIntAttribute("humanize", 0));
                    k.set(3, child->getIntAttribute("dynamics", 0));
                    k.set(4, child->getIntAttribute("length", 100));
                    k.set(5, child->getIntAttribute("intensity", 80));
                    clipGrooves[path] = k;
                }
            }
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
