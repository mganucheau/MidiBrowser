#pragma once
#include <juce_core/juce_core.h>
#include "BrowserPanels.h"
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
 */
class LibraryStore
{
public:
    static juce::File libraryFile();

    void load();
    void save() const;

    juce::StringArray starredFiles;
    std::vector<SavedSearchEntry> savedSearches;
    std::vector<CachedSearch> searchCache;

    bool isStarred(const juce::String& path) const { return starredFiles.contains(path); }

    void toggleStarred(const juce::String& path);
    void setStarredFiles(const juce::StringArray& paths);
    void addSavedSearch(const SavedSearchEntry& entry);
    void removeSavedSearch(int index);
    void setSavedSearches(std::vector<SavedSearchEntry> entries);

    /** Merge stars/searches from host plugin state (union; library remains source of truth). */
    void mergeFromPluginState(const juce::StringArray& stars,
                              const std::vector<SavedSearchEntry>& searches);

    const CachedSearch* findSearchCache(const juce::File& root, const BrowserSearch& s) const;
    void putSearchCache(const juce::File& root, const BrowserSearch& s,
                        const juce::StringArray& resultPaths);

    static bool searchesEqual(const BrowserSearch& a, const BrowserSearch& b);

private:
    static constexpr int kMaxSavedSearches = 48;
    static constexpr int kMaxSearchCache = 32;
};

} // namespace pflow
