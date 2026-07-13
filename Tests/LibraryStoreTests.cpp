#include <catch2/catch_test_macros.hpp>
#include "LibraryStore.h"
#include "PluginProcessor.h"

using namespace pflow;

TEST_CASE("LibraryStore persists stars and search cache to disk", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-test");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        LibraryStore store;
        store.toggleStarred("/tmp/a.mid");
        store.toggleStarred("/tmp/b.mid");

        BrowserSearch s;
        s.query = "groove";
        s.bpmMin = 120.0;
        s.bpmMax = 120.0;
        s.subdirs = true;
        store.putSearchCache(juce::File("/tmp/midi"), s, { "/tmp/midi/a.mid", "/tmp/midi/b.mid" });

        SavedSearchEntry entry;
        entry.name = "groove 120";
        entry.search = s;
        store.addSavedSearch(entry);
    }

    {
        LibraryStore loaded;
        loaded.load();
        REQUIRE(loaded.isStarred("/tmp/a.mid"));
        REQUIRE(loaded.isStarred("/tmp/b.mid"));
        REQUIRE(loaded.savedSearches.size() == 1);
        REQUIRE(loaded.savedSearches[0].name == "groove 120");

        BrowserSearch s;
        s.query = "groove";
        s.bpmMin = 120.0;
        s.bpmMax = 120.0;
        s.subdirs = true;
        const auto* cached = loaded.findSearchCache(juce::File("/tmp/midi"), s);
        REQUIRE(cached != nullptr);
        REQUIRE(cached->resultPaths.size() == 2);
    }

    file.deleteFile();
    if (backup.existsAsFile())
    {
        backup.moveFileTo(file);
    }
}

TEST_CASE("Processor loads library stars on construction", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-test2");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        LibraryStore store;
        store.toggleStarred("/tmp/starred-clip.mid");
    }

    MidiBrowserProcessor processor;
    REQUIRE(processor.isStarred("/tmp/starred-clip.mid"));

    file.deleteFile();
    if (backup.existsAsFile())
        backup.moveFileTo(file);
}
