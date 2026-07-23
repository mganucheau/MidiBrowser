#pragma once
#include <juce_core/juce_core.h>
#include "BrowserPanels.h"
#include <atomic>
#include <vector>

namespace pflow {

/** One cached search: criteria + root folder + matching file paths. */
struct CachedSearch
{
    BrowserSearch criteria;
    juce::String rootPath;
    juce::StringArray resultPaths;
    juce::int64 scannedAtMs = 0;
};

/**
 * App-owned library file (Application Support), independent of DAW project state.
 * Survives plugin upgrades and is shared by Standalone / AU / VST3.
 *
 * Stores: starred file paths, saved search terms, and search result path caches.
 * Favorites accrue across sessions and plugin instances — disk is the source of truth.
 */
class LibraryStore
{
public:
    static juce::File libraryFile();

    void load();
    void save() const;
    /** Snapshot + write on a background thread so the host UI never waits on disk. */
    void saveAsync() const;
    /** Block until the latest scheduled async write finishes (call on shutdown). */
    void flush() const;

    juce::StringArray starredFiles;
    std::vector<SavedSearchEntry> savedSearches;
    std::vector<CachedSearch> searchCache;

    bool isStarred(const juce::String& path) const { return starredFiles.contains(path); }

    void toggleStarred(const juce::String& path);
    void setStarredFiles(const juce::StringArray& paths);
    void addSavedSearch(const SavedSearchEntry& entry);
    void removeSavedSearch(int index);
    void setSavedSearches(std::vector<SavedSearchEntry> entries);

    /** Union stars/searches from host plugin state into the app library. */
    void mergeFromPluginState(const juce::StringArray& stars,
                              const std::vector<SavedSearchEntry>& searches);

    /** Pull any stars written by other instances into memory (no save). */
    void mergeStarsFromDisk();

    const CachedSearch* findSearchCache(const juce::File& root, const BrowserSearch& s) const;
    void putSearchCache(const juce::File& root, const BrowserSearch& s,
                        const juce::StringArray& resultPaths);

    static bool searchesEqual(const BrowserSearch& a, const BrowserSearch& b);

    /** Shared XML helpers for host state + library file. */
    static BrowserSearch browserSearchFromXml(const juce::XmlElement& el);
    static void browserSearchToXml(juce::XmlElement& el, const BrowserSearch& s);

private:
    static constexpr int kMaxSavedSearches = 48;
    static constexpr int kMaxSearchCache = 32;

    static juce::StringArray readStarsFromDisk();
    static void writeLibraryFile(juce::StringArray stars,
                                 std::vector<SavedSearchEntry> searches,
                                 std::vector<CachedSearch> cache);

    /** Bumped on every save request; writers bail if a newer request superseded them. */
    mutable std::atomic<uint64_t> saveGeneration_ { 0 };
    mutable std::atomic<uint64_t> saveCompleted_ { 0 };
};

} // namespace pflow
