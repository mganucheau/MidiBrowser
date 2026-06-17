#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PluginProcessor.h"
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

TEST_CASE("Processor sends all-notes-off when transport is stopped", "[Processor][qa]")
{
    ProcessorTestHarness harness;
    auto clip = previewClipWithNoteAt(0.0);
    harness.processor.setPreviewState(clip, true, false, false);
    harness.processor.hostPlaying.store(false);

    const auto midi = harness.runBlock(0.0);
    REQUIRE(test::countNoteOns(midi) == 0);
    REQUIRE(test::hasAllNotesOff(midi));
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
    original.appThemeId.store(11);
    original.addSavedBrowserDir(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName());

    juce::MemoryBlock state;
    original.getStateInformation(state);

    MidiBrowserProcessor restored;
    restored.setStateInformation(state.getData(), (int) state.getSize());

    REQUIRE(restored.lastBrowserDir == original.lastBrowserDir);
    REQUIRE(restored.trimEmptyMeasuresPreview == original.trimEmptyMeasuresPreview);
    REQUIRE(restored.syncSessionBars.load() == 8);
    REQUIRE(restored.appThemeId.load() == 11);
    REQUIRE(restored.savedBrowserDirs.size() == original.savedBrowserDirs.size());
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
