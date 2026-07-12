#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PluginProcessor.h"
#include "Theme.h"
#include "TestHelpers.h"

using namespace pflow;
using Catch::Approx;

namespace
{

class ProcessorTestHarness
{
public:
    ProcessorTestHarness()
    {
        processor.prepareToPlay(44100.0, 512);
        processor.hostPlaying.store(true);
        processor.hostBpm.store(120.0);
        processor.hostBeatPos.store(0.0);
        processor.syncSessionBars.store(4);
        processor.previewArmed.store(true);   // transport play pressed
    }

    juce::MidiBuffer runBlock(double beatPos = 0.0)
    {
        processor.hostBeatPos.store(beatPos);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        return midi;
    }

    MidiBrowserProcessor processor;
};

MidiClip previewClipWithNoteAt(double startBeat)
{
    return test::makeClipWithNotes({ { 60, 100, startBeat, 0.5, 1 } }, 4.0);
}

} // namespace

TEST_CASE("Processor advertises MIDI capability", "[Processor][qa]")
{
    MidiBrowserProcessor processor;
    REQUIRE(processor.acceptsMidi());
    REQUIRE(processor.producesMidi());
    REQUIRE(processor.getName() == "Midi Browser");
    REQUIRE(processor.getTailLengthSeconds() == Approx(0.0));
}

TEST_CASE("Processor bus layout supports stereo or mono output", "[Processor][qa]")
{
    MidiBrowserProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.isBusesLayoutSupported(layout));
    layout.outputBuses.clear();
    layout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(processor.isBusesLayoutSupported(layout));
    layout.outputBuses.clear();
    layout.outputBuses.add(juce::AudioChannelSet::disabled());
    REQUIRE_FALSE(processor.isBusesLayoutSupported(layout));
}

TEST_CASE("Processor emits note-ons for active unmuted preview", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);

    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) >= 1);
}

TEST_CASE("Processor stays silent when preview is muted", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, true, false);

    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) == 0);
}

TEST_CASE("Processor sends all-notes-off once when transport stops", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.runBlock(0.0);   // sounding block first

    harness.processor.hostPlaying.store(false);
    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) == 0);
    REQUIRE(test::hasAllNotesOff(midi));

    // Subsequent stopped blocks stay silent (no all-notes-off spam).
    const auto next = harness.runBlock(0.0);
    REQUIRE(next.getNumEvents() == 0);
}

TEST_CASE("Processor loops preview across session length", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.processor.syncSessionBars.store(1);

    const auto firstPass = harness.runBlock(0.0);
    const auto wrapped = harness.runBlock(4.0);

    REQUIRE(test::countNoteOns(firstPass) >= 1);
    REQUIRE(test::countNoteOns(wrapped) >= 1);
}

TEST_CASE("Processor stays silent when preview is not armed", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.runBlock(0.0);   // sounding block first

    harness.processor.previewArmed.store(false);
    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) == 0);
    REQUIRE(test::hasAllNotesOff(midi));   // released once on disarm
}

TEST_CASE("Free-run loop wrap does not flush the last note", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    // Long note that starts near the end and should sustain across the wrap
    // until its natural note-off — not get all-notes-off'd by the discontinuity guard.
    auto clip = test::makeClipWithNotes({ { 60, 100, 3.5, 1.0, 1 } }, 4.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.processor.syncToHost.store(false);
    harness.processor.hostPlaying.store(false);
    harness.processor.freeBpm.store(120.0);
    harness.processor.previewLoopStartBeat.store(0.0);
    harness.processor.previewLoopEndBeat.store(4.0);
    harness.processor.freerunBeat.store(3.5);

    bool sawWrap = false;
    for (int i = 0; i < 2000; ++i)
    {
        const double before = harness.processor.freerunBeat.load();
        const auto midi = harness.runBlock(0.0);
        const double after = harness.processor.freerunBeat.load();
        if (after + 1.0e-9 < before)
        {
            REQUIRE_FALSE(test::hasAllNotesOff(midi));
            sawWrap = true;
            break;
        }
    }
    REQUIRE(sawWrap);
}

TEST_CASE("Synced preview auto-arms when host transport starts", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.processor.syncToHost.store(true);
    harness.processor.previewArmed.store(false);
    harness.processor.hostPlaying.store(false);
    harness.runBlock(0.0);   // establish stopped state

    harness.processor.hostPlaying.store(true);
    const auto midi = harness.runBlock(0.0);
    REQUIRE(harness.processor.previewArmed.load());
    REQUIRE(test::countNoteOns(midi) >= 1);
}

TEST_CASE("Free-run preview plays and advances without host transport", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.processor.syncToHost.store(false);
    harness.processor.hostPlaying.store(false);   // host stopped; we play anyway
    harness.processor.freeBpm.store(120.0);
    harness.processor.freerunBeat.store(0.0);

    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) >= 1);
    REQUIRE(harness.processor.freerunBeat.load() > 0.0);   // internal clock advanced

    // The internal clock loops over the clip length.
    harness.processor.freerunBeat.store(3.999);
    const auto wrapped = harness.runBlock(0.0);
    juce::ignoreUnused(wrapped);
    REQUIRE(harness.processor.freerunBeat.load() < 3.999);
}

TEST_CASE("Changing the preview clip releases held notes", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    // Contiguous block positions so the discontinuity detector stays quiet.
    const double blockBeats = 512.0 / 44100.0 * 2.0;   // 512 samples at 120bpm

    auto clipA = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clipA, true, false, false);
    harness.runBlock(0.0);   // note-on from clip A sounding

    // Browse to a different clip: the next block must flush the old notes.
    auto clipB = test::makeClipWithNotes({ { 72, 100, 0.0, 0.5, 1 } }, 4.0);
    harness.processor.setPreviewState(clipB, true, false, false);
    const auto midi = harness.runBlock(blockBeats);
    REQUIRE(test::hasAllNotesOff(midi));

    // Re-pushing the same clip does not flush again.
    harness.processor.setPreviewState(clipB, true, false, false);
    const auto next = harness.runBlock(blockBeats * 2.0);
    REQUIRE_FALSE(test::hasAllNotesOff(next));
}

TEST_CASE("Per-clip edits and grooves round-trip through plugin state", "[Processor][qa]")
{
    MidiBrowserProcessor original;
    ClipEdit e;
    e.octave = 1;
    e.fitScale = true;
    e.root = 5;
    e.mode = Mode::Aeolian;
    e.moves[3] = { 7, -4 };
    e.removedBars = { 0, 1 };
    original.clipEdits["/tmp/a.mid"] = e;

    GrooveParams k;
    k.swing = 40;
    k.pocket = -25;
    original.clipGrooves["/tmp/a.mid"] = k;

    juce::MemoryBlock state;
    original.getStateInformation(state);

    MidiBrowserProcessor restored;
    restored.setStateInformation(state.getData(), (int) state.getSize());

    REQUIRE(restored.clipEdits.count("/tmp/a.mid") == 1);
    const auto& re = restored.clipEdits.at("/tmp/a.mid");
    REQUIRE(re.octave == 1);
    REQUIRE(re.fitScale);
    REQUIRE(re.root == 5);
    REQUIRE(re.mode == Mode::Aeolian);
    REQUIRE(re.removedBars == std::vector<int>({ 0, 1 }));
    REQUIRE(re.moves.at(3).dPitch == 7);
    REQUIRE(re.moves.at(3).dStep == -4);

    REQUIRE(restored.clipGrooves.count("/tmp/a.mid") == 1);
    REQUIRE(restored.clipGrooves.at("/tmp/a.mid").swing == 40);
    REQUIRE(restored.clipGrooves.at("/tmp/a.mid").pocket == -25);
}

TEST_CASE("Saved folder helpers dedupe and cap list size", "[Processor][qa]")
{
    MidiBrowserProcessor processor;
    const auto tempRoot = test::tempMidiFile("bookmark_root");
    tempRoot.createDirectory();

    processor.addSavedBrowserDir(tempRoot.getFullPathName());
    processor.addSavedBrowserDir(tempRoot.getFullPathName());
    REQUIRE(processor.savedBrowserDirs.size() == 1);

    processor.addSavedBrowserDir("/path/that/does/not/exist");
    REQUIRE(processor.savedBrowserDirs.size() == 1);

    for (int i = 0; i < 30; ++i)
    {
        const auto dir = tempRoot.getChildFile("slot_" + juce::String(i));
        dir.createDirectory();
        processor.addSavedBrowserDir(dir.getFullPathName());
    }
    REQUIRE(processor.savedBrowserDirs.size() <= 24);

    const auto first = processor.savedBrowserDirs[0];
    processor.removeSavedBrowserDir(first);
    REQUIRE_FALSE(processor.savedBrowserDirs.contains(first));

    tempRoot.deleteRecursively();
}

TEST_CASE("Processor state round-trips browser settings", "[Processor][qa]")
{
    MidiBrowserProcessor original;
    original.lastBrowserDir = "/tmp/MidiBrowserSaved";
    original.trimEmptyMeasuresPreview = true;
    original.syncSessionBars.store(8);
    original.effectsOpen = true;
    original.effectsLock = true;
    original.lockedGroove.swing = 42;
    original.lockedGroove.pocket = -20;
    tweaks().density.store((int) Density::Comfortable);
    tweaks().size.store((int) ContentSize::Large);
    original.addSavedBrowserDir(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName());

    juce::MemoryBlock state;
    original.getStateInformation(state);

    tweaks().density.store((int) Density::Compact);
    tweaks().size.store((int) ContentSize::Medium);

    MidiBrowserProcessor restored;
    restored.setStateInformation(state.getData(), (int) state.getSize());

    REQUIRE(restored.lastBrowserDir == original.lastBrowserDir);
    REQUIRE(restored.trimEmptyMeasuresPreview == original.trimEmptyMeasuresPreview);
    REQUIRE(restored.syncSessionBars.load() == 8);
    REQUIRE(restored.effectsOpen);
    REQUIRE(restored.effectsLock);
    REQUIRE(restored.lockedGroove.swing == 42);
    REQUIRE(restored.lockedGroove.pocket == -20);
    REQUIRE(tweaks().density.load() == (int) Density::Comfortable);
    REQUIRE(tweaks().size.load() == (int) ContentSize::Large);
    REQUIRE(restored.savedBrowserDirs.size() == original.savedBrowserDirs.size());

    tweaks().density.store((int) Density::Compact);
    tweaks().size.store((int) ContentSize::Medium);
}

TEST_CASE("Processor state ignores empty and legacy-safe payloads", "[Processor][qa]")
{
    MidiBrowserProcessor processor;
    processor.lastBrowserDir = "before";
    processor.setStateInformation(nullptr, 0);
    REQUIRE(processor.lastBrowserDir.isEmpty());

    juce::XmlElement legacy("PatternFlowState");
    legacy.setAttribute("lastBrowserDir", "/legacy/path");
    legacy.setAttribute("arrangementBars", 16);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(legacy, block);

    processor.setStateInformation(block.getData(), (int) block.getSize());
    REQUIRE(processor.lastBrowserDir == "/legacy/path");
    REQUIRE(processor.syncSessionBars.load() == 16);
}
