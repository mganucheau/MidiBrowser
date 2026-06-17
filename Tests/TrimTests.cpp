#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "MidiFileData.h"
#include "TestHelpers.h"

using namespace pflow;
using Catch::Approx;

namespace
{

MidiClip copyClip(const MidiClip& source)
{
    MidiClip copy = source;
    copy.notes = source.notes;
    return copy;
}

bool sameNoteLayout(const MidiClip& a, const MidiClip& b)
{
    if (a.notes.size() != b.notes.size()) return false;
    for (size_t i = 0; i < a.notes.size(); ++i)
    {
        if (a.notes[i].noteNumber != b.notes[i].noteNumber) return false;
        if (a.notes[i].startBeat != Approx(b.notes[i].startBeat).margin(1e-9)) return false;
        if (a.notes[i].lengthBeats != Approx(b.notes[i].lengthBeats).margin(1e-9)) return false;
    }
    return a.lengthBeats == Approx(b.lengthBeats).margin(1e-9);
}

} // namespace

TEST_CASE("trimEmptyMeasuresInClip removes a single middle empty bar", "[Trim][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },
        { 62, 90, 8.0, 1.0, 1 },
    }, 16.0);

    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes.size() == 2);
    REQUIRE(clip.notes[0].startBeat == Approx(0.0));
    REQUIRE(clip.notes[1].startBeat == Approx(4.0));
    REQUIRE(clip.lengthBeats == Approx(8.0));
}

TEST_CASE("trimEmptyMeasuresInClip removes multiple consecutive empty bars", "[Trim][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 36, 110, 0.0, 0.5, 10 },
        { 42, 100, 12.0, 0.5, 10 },
    }, 16.0);

    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes[0].startBeat == Approx(0.0));
    REQUIRE(clip.notes[1].startBeat == Approx(4.0));
    REQUIRE(clip.lengthBeats == Approx(8.0));
}

TEST_CASE("trimEmptyMeasuresInClip removes leading empty bars", "[Trim][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 38, 100, 8.0, 0.25, 1 },
    }, 12.0);

    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes[0].startBeat == Approx(0.0));
    REQUIRE(clip.lengthBeats == Approx(4.0));
}

TEST_CASE("trimEmptyMeasuresInClip removes trailing empty bars", "[Trim][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 38, 100, 0.0, 0.25, 1 },
    }, 12.0);

    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes[0].startBeat == Approx(0.0));
    REQUIRE(clip.lengthBeats == Approx(4.0));
}

TEST_CASE("trimEmptyMeasuresInClip is a no-op when every bar has notes", "[Trim][qa]")
{
    auto original = test::makeClipWithNotes({
        { 36, 100, 0.0, 0.5, 1 },
        { 38, 100, 4.0, 0.5, 1 },
        { 42, 100, 8.0, 0.5, 1 },
    }, 12.0);

    auto clip = copyClip(original);
    trimEmptyMeasuresInClip(clip, 4.0);
    REQUIRE(sameNoteLayout(original, clip));
}

TEST_CASE("trimEmptyMeasuresInClip does not delete notes", "[Trim][qa]")
{
    auto original = test::makeClipWithNotes({
        { 36, 100, 0.0, 0.5, 1 },
        { 42, 100, 12.0, 0.5, 1 },
    }, 16.0);

    auto clip = copyClip(original);
    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes.size() == original.notes.size());
    REQUIRE(clip.notes[0].noteNumber == original.notes[0].noteNumber);
    REQUIRE(clip.notes[1].noteNumber == original.notes[1].noteNumber);
}

TEST_CASE("trim toggle workflow preserves original when reloaded from disk", "[Trim][qa]")
{
    auto source = test::makeClipWithNotes({
        { 36, 110, 0.0, 0.5, 10 },
        { 42, 100, 12.0, 0.5, 10 },
    }, 16.0);

    const auto tempFile = test::tempMidiFile("trim_toggle.mid");
    tempFile.getParentDirectory().createDirectory();
    tempFile.deleteFile();
    REQUIRE(writeMidiFile(source, tempFile, 120.0));

    auto trimmed = parseMidiFile(tempFile);
    trimEmptyMeasuresInClip(trimmed, 4.0);
    REQUIRE(trimmed.notes[1].startBeat == Approx(4.0));

    const auto restored = parseMidiFile(tempFile);
    test::removeTempMidiFile(tempFile);

    REQUIRE(restored.notes[1].startBeat == Approx(12.0));
    REQUIRE(restored.lengthBeats == Approx(12.5).margin(0.1));
}

TEST_CASE("trimEmptyMeasuresInClip ignores invalid beatsPerBar", "[Trim][qa]")
{
    auto original = test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0);
    auto clip = copyClip(original);
    trimEmptyMeasuresInClip(clip, 0.0);
    REQUIRE(sameNoteLayout(original, clip));
}
