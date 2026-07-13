#include "EditModel.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>

using namespace pflow;
using Catch::Approx;

namespace {

StepClip makeClip(std::initializer_list<RollNote> notes, int bars, int root = 0)
{
    StepClip c;
    c.name = "Test";
    c.root = root;
    c.bars = bars;
    c.notes = notes;
    return c;
}

} // namespace

TEST_CASE("pitchName maps MIDI 60 to C4", "[editmodel]")
{
    CHECK(pitchName(60) == "C4");
    CHECK(pitchName(61) == "C#4");
    CHECK(pitchName(59) == "B3");
    CHECK(pitchName(0) == "C-1");
    CHECK(pitchName(127) == "G9");
}

TEST_CASE("isBlackKeyPitch matches {1,3,6,8,10} pitch classes", "[editmodel]")
{
    CHECK_FALSE(isBlackKeyPitch(60));  // C
    CHECK(isBlackKeyPitch(61));        // C#
    CHECK(isBlackKeyPitch(63));        // D#
    CHECK_FALSE(isBlackKeyPitch(64));  // E
    CHECK(isBlackKeyPitch(66));        // F#
    CHECK(isBlackKeyPitch(68));        // G#
    CHECK(isBlackKeyPitch(70));        // A#
    CHECK_FALSE(isBlackKeyPitch(71));  // B
}

TEST_CASE("modeIntervals match the seven-mode table", "[editmodel]")
{
    CHECK(modeIntervals(Mode::Ionian)     == std::array<int, 7> { 0, 2, 4, 5, 7, 9, 11 });
    CHECK(modeIntervals(Mode::Dorian)     == std::array<int, 7> { 0, 2, 3, 5, 7, 9, 10 });
    CHECK(modeIntervals(Mode::Phrygian)   == std::array<int, 7> { 0, 1, 3, 5, 7, 8, 10 });
    CHECK(modeIntervals(Mode::Lydian)     == std::array<int, 7> { 0, 2, 4, 6, 7, 9, 11 });
    CHECK(modeIntervals(Mode::Mixolydian) == std::array<int, 7> { 0, 2, 4, 5, 7, 9, 10 });
    CHECK(modeIntervals(Mode::Aeolian)    == std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 });
    CHECK(modeIntervals(Mode::Locrian)    == std::array<int, 7> { 0, 1, 3, 5, 6, 8, 10 });
}

TEST_CASE("fitToScale matches prototype golden values", "[editmodel]")
{
    // Golden values computed by running the verbatim Prototype/mb/data.jsx
    // fitToScale in Node (ties at equal |d| resolve to the lower candidate).
    CHECK(fitToScale(61, 0, Mode::Ionian) == 60);    // C# -> C
    CHECK(fitToScale(63, 0, Mode::Ionian) == 62);    // D# -> D
    CHECK(fitToScale(66, 0, Mode::Ionian) == 65);    // F# -> F
    CHECK(fitToScale(68, 0, Mode::Ionian) == 67);    // G# -> G
    CHECK(fitToScale(70, 0, Mode::Ionian) == 69);    // A# -> A
    CHECK(fitToScale(64, 0, Mode::Aeolian) == 63);   // E  -> D# in C minor
    CHECK(fitToScale(61, 9, Mode::Dorian) == 60);    // C# -> C in A Dorian
    CHECK(fitToScale(37, 6, Mode::Locrian) == 36);
    CHECK(fitToScale(0, 0, Mode::Phrygian) == 0);    // in scale, unchanged
    CHECK(fitToScale(127, 11, Mode::Lydian) == 126);
}

TEST_CASE("editIsClean detects default edits", "[editmodel]")
{
    ClipEdit e;
    CHECK(editIsClean(e));

    SECTION("octave") { e.octave = 1; CHECK_FALSE(editIsClean(e)); }
    SECTION("fitScale") { e.fitScale = true; CHECK_FALSE(editIsClean(e)); }
    SECTION("mapToRoot") { e.mapToRoot = true; CHECK_FALSE(editIsClean(e)); }
    SECTION("moves") { e.moves[3] = { 1, 0 }; CHECK_FALSE(editIsClean(e)); }
    SECTION("zero-delta move stays clean") { e.moves[3] = { 0, 0 }; CHECK(editIsClean(e)); }
    SECTION("trim") { e.removedBars = { 0 }; CHECK_FALSE(editIsClean(e)); }
    SECTION("root/mode alone are not edits")
    {
        e.root = 4;
        e.mode = Mode::Lydian;
        CHECK(editIsClean(e));
    }
}

TEST_CASE("resolveClip applies octave and per-note moves", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 0.0, 4.0 }, { 1, 64, 4.0, 2.0 } }, 1);
    ClipEdit e;
    e.octave = -1;
    e.moves[1] = { 2, 3 };

    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 2);
    CHECK(r.notes[0].pitch == 48);
    CHECK(r.notes[0].start == Approx(0.0));
    CHECK_FALSE(r.notes[0].moved);
    CHECK(r.notes[1].pitch == 64 + 2 - 12);
    CHECK(r.notes[1].start == Approx(7.0));
    CHECK(r.notes[1].moved);

    // Source clip untouched (non-destructive contract).
    CHECK(clip.notes[1].pitch == 64);
    CHECK(clip.notes[1].start == Approx(4.0));
}

TEST_CASE("resolveClip map-to-root transposes by nearest direction", "[editmodel]")
{
    // Clip root G (7), target C (0): d = (0-7) = -7 -> wraps to +5.
    auto clip = makeClip({ { 0, 55, 0.0, 4.0 } }, 1, 7);
    ClipEdit e;
    e.mapToRoot = true;
    e.root = 0;
    CHECK(resolveClip(clip, e).notes[0].pitch == 60);

    // Clip root C (0), target G (7): d = 7 -> wraps to -5.
    clip.root = 0;
    e.root = 7;
    CHECK(resolveClip(clip, e).notes[0].pitch == 50);

    // No clip root -> no transpose.
    clip.root = -1;
    CHECK(resolveClip(clip, e).notes[0].pitch == 55);
}

TEST_CASE("resolveClip composes map-to-root then fit-to-scale", "[editmodel]")
{
    // Root D (2) clip: D and F#. Map to C, then snap into C Aeolian.
    auto clip = makeClip({ { 0, 62, 0.0, 2.0 }, { 1, 66, 2.0, 2.0 } }, 1, 2);
    ClipEdit e;
    e.mapToRoot = true;
    e.fitScale = true;
    e.root = 0;
    e.mode = Mode::Aeolian;

    const auto r = resolveClip(clip, e);
    CHECK(r.notes[0].pitch == 60);  // D -> C, in scale
    CHECK(r.notes[1].pitch == 63);  // F# -> E -> D# (C minor third)
}

TEST_CASE("resolveClip trim shifts notes and shrinks bars", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 16.0, 4.0 }, { 1, 62, 40.0, 2.0 } }, 4);
    ClipEdit e;
    e.removedBars = { 0, 3 };

    const auto r = resolveClip(clip, e);
    CHECK(r.bars == 2);
    CHECK(r.notes[0].start == Approx(0.0));
    CHECK(r.notes[1].start == Approx(24.0));

    SECTION("bars never drop below 1")
    {
        e.removedBars = { 0, 1, 2, 3 };
        CHECK(resolveClip(clip, e).bars == 1);
    }

    SECTION("middle empty bar is compacted")
    {
        auto mid = makeClip({ { 0, 60, 0.0, 4.0 }, { 1, 62, 32.0, 4.0 } }, 3);
        ClipEdit t;
        t.removedBars = { 1 };
        const auto out = resolveClip(mid, t);
        CHECK(out.bars == 2);
        CHECK(out.notes[0].start == Approx(0.0));
        CHECK(out.notes[1].start == Approx(16.0));
    }
}

TEST_CASE("emptyBars finds every fully-empty measure", "[editmodel]")
{
    std::vector<RollNote> notes { { 0, 60, 0.0, 4.0 }, { 1, 62, 32.0, 4.0 } };
    CHECK(emptyBars(notes, 3) == std::vector<int>{ 1 });

    std::vector<RollNote> edges { { 0, 60, 17.0, 2.0 }, { 1, 62, 36.0, 4.0 } };
    CHECK(emptyBars(edges, 4) == std::vector<int>({ 0, 3 }));
}

TEST_CASE("emptyEdgeBars counts fully-empty edge bars", "[editmodel]")
{
    std::vector<RollNote> notes { { 0, 60, 17.0, 2.0 }, { 1, 62, 36.0, 4.0 } };

    const auto e = emptyEdgeBars(notes, 4);
    CHECK(e.lead == 1);   // bar 0 empty (first note at step 17)
    CHECK(e.tail == 1);   // last note ends at step 40 -> bar 3 empty

    CHECK(emptyEdgeBars({}, 4).lead == 0);
    CHECK(emptyEdgeBars({}, 4).tail == 0);

    // Note ending exactly on a bar boundary leaves the next bar empty.
    std::vector<RollNote> exact { { 0, 60, 0.0, 16.0 } };
    CHECK(emptyEdgeBars(exact, 2).lead == 0);
    CHECK(emptyEdgeBars(exact, 2).tail == 1);
}

TEST_CASE("editBadges lists only non-default transforms", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 0.0, 4.0 } }, 2, 0);

    ClipEdit e;
    CHECK(editBadges(clip, e).empty());

    e.octave = 2;
    e.fitScale = true;
    e.mapToRoot = true;
    e.root = 3;               // D#
    e.mode = Mode::Aeolian;
    e.moves[0] = { 0, 4 };
    e.removedBars = { 0 };

    const auto badges = editBadges(clip, e);
    REQUIRE(badges.size() == 5);
    CHECK(badges[0].label == "Oct +2");
    CHECK(badges[1].label == "D# Minor");
    CHECK(badges[2].label == "-> D# root");
    CHECK(badges[3].label == "1 note moved");
    CHECK(badges[4].label == "Trim -1 bar");

    SECTION("plurals")
    {
        e.moves[1] = { 1, 0 };
        e.removedBars = { 0, 1 };
        const auto b = editBadges(clip, e);
        CHECK(b[3].label == "2 notes moved");
        CHECK(b[4].label == "Trim -2 bars");
    }

    SECTION("negative octave")
    {
        ClipEdit neg;
        neg.octave = -1;
        CHECK(editBadges(clip, neg)[0].label == "Oct -1");
    }
}

TEST_CASE("applyPitchLock copies pitch fields, preserves moves and trim", "[editmodel]")
{
    ClipEdit locked;
    locked.octave = 1;
    locked.fitScale = true;
    locked.mapToRoot = true;
    locked.root = 0;              // C
    locked.mode = Mode::Aeolian;

    ClipEdit target;
    target.moves[7] = { 2, -4 };
    target.removedBars = { 0 };
    target.octave = -2;
    target.root = 9;

    applyPitchLock(locked, target);
    CHECK(target.octave == 1);
    CHECK(target.fitScale);
    CHECK(target.mapToRoot);
    CHECK(target.root == 0);
    CHECK(target.mode == Mode::Aeolian);
    CHECK(target.moves.at(7).dPitch == 2);    // per-note moves untouched
    CHECK(target.moves.at(7).dStep == -4);
    CHECK(target.removedBars == std::vector<int>{ 0 });  // trim untouched
}

TEST_CASE("parseMidiFile honours end-of-track length for trailing empty bars", "[editmodel]")
{
    // One beat of notes but an end-of-track marker at 16 beats: the clip
    // should keep its intended 4-bar length so Trim has something to do.
    juce::MidiFile mf;
    constexpr int tpq = 480;
    mf.setTicksPerQuarterNote(tpq);
    juce::MidiMessageSequence seq;
    seq.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0.0);
    seq.addEvent(juce::MidiMessage::noteOff(1, 60), tpq * 1.0);
    seq.addEvent(juce::MidiMessage::endOfTrack(), tpq * 16.0);
    seq.updateMatchedPairs();
    mf.addTrack(seq);

    const auto file = test::tempMidiFile("eot-length.mid");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    {
        juce::FileOutputStream out(file);
        REQUIRE(out.openedOk());
        REQUIRE(mf.writeTo(out, 0));
    }

    const auto clip = parseMidiFile(file);
    REQUIRE(clip.notes.size() == 1);
    CHECK(clip.lengthBeats == Approx(16.0));

    const auto step = makeStepClip(clip);
    CHECK(step.bars == 4);
    const auto edges = emptyEdgeBars(step.notes, step.bars);
    CHECK(edges.lead == 0);
    CHECK(edges.tail == 3);   // bars 2-4 are empty and trimmable

    test::removeTempMidiFile(file);
}

TEST_CASE("makeStepClip converts beats to 16th steps with stable ids", "[editmodel]")
{
    MidiClip mc = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },     // C4, beat 0, 1 beat
        { 43, 90, 2.5, 0.5, 1 },      // G2
    }, 8.0);

    const auto s = makeStepClip(mc);
    REQUIRE(s.notes.size() == 2);
    CHECK(s.notes[0].id == 0);
    CHECK(s.notes[1].id == 1);
    CHECK(s.notes[0].start == Approx(0.0));
    CHECK(s.notes[0].len == Approx(4.0));
    CHECK(s.notes[1].start == Approx(10.0));
    CHECK(s.notes[1].len == Approx(2.0));
    CHECK(s.notes[0].fileVelocity == 100);
    CHECK(s.bars == 2);   // 8 beats = 2 bars
    CHECK(s.root >= 0);
}
