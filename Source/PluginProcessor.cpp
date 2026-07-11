#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"
#include <algorithm>
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
            flushActiveNotes(midi, 0);
        wasSounding_ = false;
        lastBeatPos_ = -1.0;
        return;
    }
    wasSounding_ = true;

    const double mult = juce::jlimit(0.25, 4.0, bpmMultiplier.load());
    double bpm = synced ? hostBpm.load() * mult : freeBpm.load();
    double beatPos = synced ? hostBeatPos.load() * mult : freerunBeat.load();
    if (bpm <= 0.0) bpm = 120.0;

    const double secPerBeat = 60.0 / bpm;
    const double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    const int numSamples = buffer.getNumSamples();
    const double blockBeats = (double) numSamples * beatsPerSample;
    const double endBeat = beatPos + blockBeats;

    juce::MidiBuffer generated;

    if (lastBeatPos_ >= 0.0 && std::abs(beatPos - lastBeatPos_) > beatsPerSample * 2.0)
        flushActiveNotes(generated, 0);

    bool previewMuted = true, previewHasClip = false, flushHeldNotes = false;
    MidiClip previewClip;
    {
        juce::ScopedLock pl(previewLock_);
        previewMuted = previewMuted_;
        previewHasClip = previewHasClip_;
        previewClip = previewClip_;
        flushHeldNotes = previewFlushPending_;
        previewFlushPending_ = false;
    }

    if (flushHeldNotes)
        flushActiveNotes(generated, 0);

    if (!previewMuted && previewHasClip && previewClip.lengthBeats > 0.0)
    {
        const double sessionLenBeats = (double) (juce::jmax(1, syncSessionBars.load()) * 4);

        double pStart = beatPos;
        double pEnd = endBeat;

        if (!synced)
        {
            // Internal clock: loop within the piano-roll loop region (or full clip).
            const double clipLen = juce::jmax(0.25, previewClip.lengthBeats);
            double loopStart = previewLoopStartBeat.load();
            double loopEnd = previewLoopEndBeat.load();
            if (loopEnd <= loopStart + 1.0e-9)
            {
                loopStart = 0.0;
                loopEnd = clipLen;
            }
            loopStart = juce::jlimit(0.0, clipLen, loopStart);
            loopEnd = juce::jlimit(loopStart + 0.25, clipLen, loopEnd);
            const double loopLen = loopEnd - loopStart;
            double local = beatPos;
            if (local < loopStart || local >= loopEnd)
                local = loopStart + std::fmod(std::max(0.0, local - loopStart), loopLen);
            else
                local = loopStart + std::fmod(local - loopStart, loopLen);
            if (local < loopStart) local += loopLen;
            pStart = local;
            pEnd = pStart + blockBeats;
            double next = pStart + blockBeats;
            if (next >= loopEnd)
                next = loopStart + std::fmod(next - loopStart, loopLen);
            freerunBeat.store(next);
        }
        else if (mult != 1.0)
        {
            // Speed-shifted sync: wrap the scaled position over the clip.
            const double clipLen = juce::jmax(0.25, previewClip.lengthBeats);
            pStart = std::fmod(beatPos, clipLen);
            if (pStart < 0.0) pStart += clipLen;
            pEnd = pStart + blockBeats;
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

namespace {

// Cheap content fingerprint so a changed preview (new file, transposed notes,
// new groove timing) releases anything still sounding from the old one.
juce::uint64 previewFingerprint(const MidiClip& clip)
{
    juce::uint64 h = (juce::uint64) clip.notes.size() * 1099511628211ULL;
    for (const auto& n : clip.notes)
    {
        h = h * 1099511628211ULL
            ^ ((juce::uint64) n.noteNumber << 48)
            ^ ((juce::uint64) n.velocity << 40)
            ^ ((juce::uint64) juce::jmax<juce::int64>(0, (juce::int64) std::llround(n.startBeat * 960.0)))
            ^ ((juce::uint64) juce::jmax<juce::int64>(0, (juce::int64) std::llround(n.lengthBeats * 960.0)) << 20);
    }
    return h;
}

} // namespace

void MidiBrowserProcessor::setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed)
{
    const auto fp = previewFingerprint(clip);
    juce::ScopedLock sl(previewLock_);
    if (fp != previewFingerprint_ && previewHasClip_)
        previewFlushPending_ = true;
    previewFingerprint_ = fp;
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
                activeNotes_[juce::jlimit(1, 16, note.channel) - 1][pitch] = true;
            }
            if (globalEnd >= startBeat && globalEnd < endBeat)
            {
                const double fraction = (globalEnd - startBeat) / blockLen;
                const int sampleOffset = sampleOffsetBase
                    + juce::jlimit(0, numSamples - 1, (int) (fraction * (double) numSamples));
                const int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                output.addEvent(juce::MidiMessage::noteOff(note.channel, pitch), sampleOffset);
                activeNotes_[juce::jlimit(1, 16, note.channel) - 1][pitch] = false;
            }
        }
    }
}

void MidiBrowserProcessor::flushActiveNotes(juce::MidiBuffer& output, int samplePosition)
{
    // Explicit note-offs for every held note (some instruments ignore CC123),
    // then all-notes-off + sustain-off as belt and braces on every channel.
    for (int ch = 0; ch < 16; ++ch)
    {
        for (int n = 0; n < 128; ++n)
            if (activeNotes_[ch][n])
            {
                output.addEvent(juce::MidiMessage::noteOff(ch + 1, n), samplePosition);
                activeNotes_[ch][n] = false;
            }
        output.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 64, 0), samplePosition);  // sustain off
        output.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 123, 0), samplePosition); // all notes off
        output.addEvent(juce::MidiMessage::allNotesOff(ch + 1), samplePosition);
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
    xml.setAttribute("tweakSize", tweaks().size.load());
    xml.setAttribute("tweakScalePlacement", tweaks().scalePlacement.load());
    xml.setAttribute("syncSessionBars", syncSessionBars.load());
    xml.setAttribute("lastBrowserDir", lastBrowserDir);
    xml.setAttribute("trimEmptyMeasuresPreview", trimEmptyMeasuresPreview ? 1 : 0);
    xml.setAttribute("syncToHost", syncToHost.load() ? 1 : 0);
    xml.setAttribute("freeBpm", freeBpm.load());
    xml.setAttribute("bpmMultiplier", bpmMultiplier.load());
    xml.setAttribute("editorOpen", editorOpen ? 1 : 0);
    xml.setAttribute("sidebarCollapsed", sidebarCollapsed ? 1 : 0);
    xml.setAttribute("miniOpen", miniOpen ? 1 : 0);
    xml.setAttribute("editLock", editLock ? 1 : 0);
    xml.setAttribute("lockAutoTrim", lockAutoTrim ? 1 : 0);
    xml.setAttribute("lockOctave", lockedEdit.octave);
    xml.setAttribute("lockFitScale", lockedEdit.fitScale ? 1 : 0);
    xml.setAttribute("lockMapToRoot", lockedEdit.mapToRoot ? 1 : 0);
    xml.setAttribute("lockRoot", lockedEdit.root);
    xml.setAttribute("lockMode", (int) lockedEdit.mode);
    for (const auto& folder : savedBrowserDirs)
    {
        if (folder.isNotEmpty())
        {
            auto* child = xml.createNewChildElement("SavedFolder");
            child->setAttribute("path", folder);
        }
    }
    for (const auto& star : starredFiles)
    {
        if (star.isNotEmpty())
        {
            auto* child = xml.createNewChildElement("StarredFile");
            child->setAttribute("path", star);
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
        if (!edit.removedBars.empty())
        {
            juce::StringArray parts;
            for (int b : edit.removedBars)
                parts.add(juce::String(b));
            e->setAttribute("removedBars", parts.joinIntoString(","));
        }
        else if (edit.legacyTrimLead > 0 || edit.legacyTrimTail > 0)
        {
            e->setAttribute("trimLead", edit.legacyTrimLead);
            e->setAttribute("trimTail", edit.legacyTrimTail);
        }
        for (const auto& [id, mv] : edit.moves)
        {
            if (mv.dPitch == 0 && mv.dStep == 0) continue;
            auto* m = e->createNewChildElement("Move");
            m->setAttribute("id", id);
            m->setAttribute("dPitch", mv.dPitch);
            m->setAttribute("dStep", mv.dStep);
        }
        for (const auto& [id, vel] : edit.velocities)
        {
            auto* v = e->createNewChildElement("Vel");
            v->setAttribute("id", id);
            v->setAttribute("v", vel);
        }
        for (int id : edit.deleted)
        {
            auto* d = e->createNewChildElement("Del");
            d->setAttribute("id", id);
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
        e->setAttribute("swingBase", (int) k.swingBase);
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
            tweaks().size.store(juce::jlimit(0, kNumContentSizes - 1,
                xml->getIntAttribute("tweakSize", (int) ContentSize::Medium)));
            tweaks().scalePlacement.store(juce::jlimit(0, kNumScalePlacements - 1,
                xml->getIntAttribute("tweakScalePlacement", (int) ScalePlacement::Top)));
            syncSessionBars.store(juce::jlimit(1, 256, xml->getIntAttribute("syncSessionBars",
                xml->getIntAttribute("arrangementBars", syncSessionBars.load()))));
            lastBrowserDir = xml->getStringAttribute("lastBrowserDir");
            trimEmptyMeasuresPreview = xml->getIntAttribute("trimEmptyMeasuresPreview", 0) != 0;
            syncToHost.store(xml->getIntAttribute("syncToHost", 1) != 0);
            freeBpm.store(juce::jlimit(20.0, 300.0, xml->getDoubleAttribute("freeBpm", 124.0)));
            bpmMultiplier.store(juce::jlimit(0.25, 4.0, xml->getDoubleAttribute("bpmMultiplier", 1.0)));
            editorOpen = xml->getIntAttribute("editorOpen", 0) != 0;
            sidebarCollapsed = xml->getIntAttribute("sidebarCollapsed", 1) != 0;
            miniOpen = xml->getIntAttribute("miniOpen", 1) != 0;
            editLock = xml->getIntAttribute("editLock", 0) != 0;
            lockAutoTrim = xml->getIntAttribute("lockAutoTrim", 0) != 0;
            lockedEdit = ClipEdit();
            lockedEdit.octave = juce::jlimit(-3, 3, xml->getIntAttribute("lockOctave", 0));
            lockedEdit.fitScale = xml->getIntAttribute("lockFitScale", 0) != 0;
            lockedEdit.mapToRoot = xml->getIntAttribute("lockMapToRoot", 0) != 0;
            lockedEdit.root = juce::jlimit(-1, 11, xml->getIntAttribute("lockRoot", -1));
            lockedEdit.mode = (Mode) juce::jlimit(0, kNumModes - 1,
                xml->getIntAttribute("lockMode", (int) Mode::Dorian));
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("SavedFolder"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty() && juce::File(path).isDirectory()
                        && !savedBrowserDirs.contains(path))
                        savedBrowserDirs.add(path);
                }
                else if (child->hasTagName("StarredFile"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty() && !starredFiles.contains(path))
                        starredFiles.add(path);
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
                    const auto removedAttr = child->getStringAttribute("removedBars");
                    if (removedAttr.isNotEmpty())
                    {
                        for (const auto& part : juce::StringArray::fromTokens(removedAttr, ",", ""))
                        {
                            const int b = part.getIntValue();
                            if (b >= 0)
                                e.removedBars.push_back(b);
                        }
                        std::sort(e.removedBars.begin(), e.removedBars.end());
                        e.removedBars.erase(std::unique(e.removedBars.begin(), e.removedBars.end()),
                                            e.removedBars.end());
                    }
                    else
                    {
                        e.legacyTrimLead = juce::jmax(0, child->getIntAttribute("trimLead", 0));
                        e.legacyTrimTail = juce::jmax(0, child->getIntAttribute("trimTail", 0));
                    }
                    for (auto* m : child->getChildIterator())
                    {
                        if (m->hasTagName("Move"))
                            e.moves[m->getIntAttribute("id")] = {
                                m->getIntAttribute("dPitch"), m->getIntAttribute("dStep") };
                        else if (m->hasTagName("Vel"))
                            e.velocities[m->getIntAttribute("id")] =
                                juce::jlimit(1, 127, m->getIntAttribute("v", 100));
                        else if (m->hasTagName("Del"))
                            e.deleted.insert(m->getIntAttribute("id"));
                    }
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
                    // Legacy sessions stored Intensity default as 80; treat missing as 100 (neutral).
                    k.set(5, child->getIntAttribute("intensity", 100));
                    k.swingBase = child->getIntAttribute("swingBase", 0) != 0
                                      ? SwingBase::Sixteenth : SwingBase::Eighth;
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
