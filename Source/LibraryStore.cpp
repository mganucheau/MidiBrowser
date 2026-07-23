#include "LibraryStore.h"
#include <algorithm>

namespace pflow {

juce::File LibraryStore::libraryFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Midi Toolkit");
    dir.createDirectory();
    return dir.getChildFile("library.xml");
}

bool LibraryStore::searchesEqual(const BrowserSearch& a, const BrowserSearch& b)
{
    return a.query == b.query
        && std::abs(a.bpmMin - b.bpmMin) < 1.0e-6
        && std::abs(a.bpmMax - b.bpmMax) < 1.0e-6
        && a.keyRoot == b.keyRoot
        && a.keyMask == b.keyMask
        && a.barsMin == b.barsMin
        && a.barsMax == b.barsMax
        && a.complexityMin == b.complexityMin
        && a.complexityMax == b.complexityMax
        && a.subdirs == b.subdirs
        && a.removeDuplicates == b.removeDuplicates;
}

BrowserSearch LibraryStore::browserSearchFromXml(const juce::XmlElement& el)
{
    BrowserSearch s;
    s.query = el.getStringAttribute("query");
    s.keyRoot = el.getIntAttribute("key", -1);
    s.keyMask = (uint16_t) juce::jlimit(0, 0x0FFF, el.getIntAttribute("keyMask", 0));
    if (s.keyMask == 0 && s.keyRoot >= 0 && s.keyRoot < 12)
        s.keyMask = (uint16_t) (1u << s.keyRoot);
    s.subdirs = el.getIntAttribute("subdirs", 1) != 0;
    s.removeDuplicates = el.getIntAttribute("removeDuplicates", 1) != 0;
    s.complexityMin = juce::jmax(0, el.getIntAttribute("complexityMin", 0));
    s.complexityMax = juce::jmax(0, el.getIntAttribute("complexityMax", 0));

    if (el.hasAttribute("bpmMin") || el.hasAttribute("bpmMax"))
    {
        s.bpmMin = el.getDoubleAttribute("bpmMin", 0.0);
        s.bpmMax = el.getDoubleAttribute("bpmMax", 0.0);
    }
    else
    {
        // Legacy: single bpm → both min and max.
        const double bpm = el.getDoubleAttribute("bpm", 0.0);
        s.bpmMin = bpm;
        s.bpmMax = bpm;
    }

    if (el.hasAttribute("barsMin") || el.hasAttribute("barsMax"))
    {
        s.barsMin = juce::jmax(0, el.getIntAttribute("barsMin", 0));
        s.barsMax = juce::jmax(0, el.getIntAttribute("barsMax", 0));
    }
    else
    {
        // Legacy: single bars → barsMax (exact match via max only was wrong;
        // old behaviour was exact bars, so set both min and max).
        const int bars = juce::jmax(0, el.getIntAttribute("bars", 0));
        s.barsMin = bars;
        s.barsMax = bars;
    }
    return s;
}

void LibraryStore::browserSearchToXml(juce::XmlElement& el, const BrowserSearch& s)
{
    el.setAttribute("query", s.query);
    el.setAttribute("bpmMin", s.bpmMin);
    el.setAttribute("bpmMax", s.bpmMax);
    el.setAttribute("key", s.keyRoot);
    el.setAttribute("keyMask", (int) s.keyMask);
    el.setAttribute("barsMin", s.barsMin);
    el.setAttribute("barsMax", s.barsMax);
    el.setAttribute("complexityMin", s.complexityMin);
    el.setAttribute("complexityMax", s.complexityMax);
    el.setAttribute("subdirs", s.subdirs ? 1 : 0);
    el.setAttribute("removeDuplicates", s.removeDuplicates ? 1 : 0);
}

void LibraryStore::load()
{
    starredFiles.clear();
    savedSearches.clear();
    searchCache.clear();

    const auto file = libraryFile();
    if (!file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        if (!xml->hasTagName("MidiBrowserLibrary"))
            return;

        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName("StarredFile"))
            {
                const auto path = child->getStringAttribute("path");
                if (path.isNotEmpty() && !starredFiles.contains(path))
                    starredFiles.add(path);
            }
            else if (child->hasTagName("SavedSearch"))
            {
                SavedSearchEntry entry;
                entry.name = child->getStringAttribute("name");
                entry.search = browserSearchFromXml(*child);
                entry.rootPath = child->getStringAttribute("root");
                for (auto* pathEl : child->getChildIterator())
                    if (pathEl->hasTagName("Path") || pathEl->hasTagName("Result"))
                    {
                        const auto p = pathEl->hasAttribute("value")
                            ? pathEl->getStringAttribute("value")
                            : pathEl->getStringAttribute("path");
                        if (p.isNotEmpty())
                            entry.resultPaths.add(p);
                    }
                if (entry.name.isNotEmpty())
                    savedSearches.push_back(std::move(entry));
            }
            else if (child->hasTagName("SearchCache"))
            {
                CachedSearch c;
                c.rootPath = child->getStringAttribute("root");
                c.criteria = browserSearchFromXml(*child);
                c.scannedAtMs = (juce::int64) child->getStringAttribute("scannedAt").getLargeIntValue();
                for (auto* pathEl : child->getChildIterator())
                    if (pathEl->hasTagName("Path"))
                    {
                        const auto p = pathEl->getStringAttribute("value");
                        if (p.isNotEmpty())
                            c.resultPaths.add(p);
                    }
                if (c.rootPath.isNotEmpty())
                    searchCache.push_back(std::move(c));
            }
        }
    }
}

juce::StringArray LibraryStore::readStarsFromDisk()
{
    juce::StringArray stars;
    const auto file = libraryFile();
    if (!file.existsAsFile())
        return stars;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        if (!xml->hasTagName("MidiBrowserLibrary"))
            return stars;
        for (auto* child : xml->getChildIterator())
        {
            if (!child->hasTagName("StarredFile"))
                continue;
            const auto path = child->getStringAttribute("path");
            if (path.isNotEmpty() && !stars.contains(path))
                stars.add(path);
        }
    }
    return stars;
}

void LibraryStore::writeLibraryFile(juce::StringArray stars,
                                    std::vector<SavedSearchEntry> searches,
                                    std::vector<CachedSearch> cache)
{
    juce::XmlElement xml("MidiBrowserLibrary");
    xml.setAttribute("version", 1);

    for (const auto& star : stars)
    {
        if (star.isEmpty()) continue;
        auto* child = xml.createNewChildElement("StarredFile");
        child->setAttribute("path", star);
    }

    for (const auto& ss : searches)
    {
        if (ss.name.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SavedSearch");
        child->setAttribute("name", ss.name);
        if (ss.rootPath.isNotEmpty())
            child->setAttribute("root", ss.rootPath);
        browserSearchToXml(*child, ss.search);
        for (const auto& p : ss.resultPaths)
        {
            if (p.isEmpty()) continue;
            auto* pathEl = child->createNewChildElement("Path");
            pathEl->setAttribute("value", p);
        }
    }

    for (const auto& c : cache)
    {
        if (c.rootPath.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SearchCache");
        child->setAttribute("root", c.rootPath);
        browserSearchToXml(*child, c.criteria);
        child->setAttribute("scannedAt", juce::String(c.scannedAtMs));
        for (const auto& p : c.resultPaths)
        {
            if (p.isEmpty()) continue;
            auto* pathEl = child->createNewChildElement("Path");
            pathEl->setAttribute("value", p);
        }
    }

    const auto file = libraryFile();
    file.getParentDirectory().createDirectory();
    // Atomic-ish replace so a crash mid-write doesn't wipe the library.
    const auto tmp = file.getSiblingFile("library.xml.tmp");
    tmp.deleteFile();
    if (xml.writeTo(tmp))
        tmp.moveFileTo(file);
}

void LibraryStore::save() const
{
    const auto gen = ++saveGeneration_;
    writeLibraryFile(starredFiles, savedSearches, searchCache);
    saveCompleted_.store(gen);
}

void LibraryStore::saveAsync() const
{
    const auto gen = ++saveGeneration_;
    // Copy state on the caller thread, then write off the message thread.
    // Generation atomics are read only to drop superseded snapshots — never
    // touch `this` after the store may have been destroyed on shutdown.
    auto* genAtom = &saveGeneration_;
    auto* doneAtom = &saveCompleted_;
    juce::Thread::launch([gen, genAtom, doneAtom,
                          stars = starredFiles,
                          searches = savedSearches,
                          cache = searchCache]
    {
        // A newer save superseded this snapshot — skip the stale write.
        if (gen != genAtom->load())
            return;
        writeLibraryFile(std::move(stars), std::move(searches), std::move(cache));
        if (gen == genAtom->load())
            doneAtom->store(gen);
    });
}

void LibraryStore::flush() const
{
    // Always persist current memory — favorites must not depend on in-flight jobs.
    save();
}

void LibraryStore::mergeStarsFromDisk()
{
    for (const auto& s : readStarsFromDisk())
        if (s.isNotEmpty() && !starredFiles.contains(s))
            starredFiles.add(s);
}

void LibraryStore::toggleStarred(const juce::String& path)
{
    if (path.isEmpty()) return;
    // Accrue against disk so other plugin instances / prior sessions stick.
    mergeStarsFromDisk();
    if (!starredFiles.contains(path))
        starredFiles.add(path);
    else
        starredFiles.removeString(path);
    // Sync write — favorites must survive host quit / crash.
    save();
}

void LibraryStore::setStarredFiles(const juce::StringArray& paths)
{
    starredFiles.clear();
    for (const auto& p : paths)
        if (p.isNotEmpty() && !starredFiles.contains(p))
            starredFiles.add(p);
    save();
}

void LibraryStore::addSavedSearch(const SavedSearchEntry& entry)
{
    if (entry.name.isEmpty()) return;
    savedSearches.push_back(entry);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    saveAsync();
}

void LibraryStore::removeSavedSearch(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) savedSearches.size()))
        return;
    savedSearches.erase(savedSearches.begin() + index);
    saveAsync();
}

void LibraryStore::setSavedSearches(std::vector<SavedSearchEntry> entries)
{
    savedSearches = std::move(entries);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    saveAsync();
}

void LibraryStore::mergeFromPluginState(const juce::StringArray& stars,
                                        const std::vector<SavedSearchEntry>& searches)
{
    bool dirty = false;
    for (const auto& s : stars)
        if (s.isNotEmpty() && !starredFiles.contains(s))
        {
            starredFiles.add(s);
            dirty = true;
        }

    for (const auto& ss : searches)
    {
        if (ss.name.isEmpty()) continue;
        bool exists = false;
        for (auto& have : savedSearches)
            if (have.name == ss.name && searchesEqual(have.search, ss.search))
            {
                exists = true;
                // Prefer the copy that still carries result paths.
                if (have.resultPaths.isEmpty() && ss.resultPaths.size() > 0)
                {
                    have.resultPaths = ss.resultPaths;
                    if (have.rootPath.isEmpty())
                        have.rootPath = ss.rootPath;
                    dirty = true;
                }
                break;
            }
        if (!exists)
        {
            savedSearches.push_back(ss);
            dirty = true;
        }
    }
    while ((int) savedSearches.size() > kMaxSavedSearches)
    {
        savedSearches.erase(savedSearches.begin());
        dirty = true;
    }

    if (dirty)
        save(); // sync — keep accrued favorites durable across host state loads
}

const CachedSearch* LibraryStore::findSearchCache(const juce::File& root,
                                                  const BrowserSearch& s) const
{
    const auto rootPath = root.getFullPathName();
    for (const auto& c : searchCache)
        if (c.rootPath == rootPath && searchesEqual(c.criteria, s))
            return &c;
    return nullptr;
}

void LibraryStore::putSearchCache(const juce::File& root, const BrowserSearch& s,
                                  const juce::StringArray& resultPaths)
{
    const auto rootPath = root.getFullPathName();
    // Replace existing entry for this root+criteria.
    searchCache.erase(std::remove_if(searchCache.begin(), searchCache.end(),
                                     [&](const CachedSearch& c)
                                     {
                                         return c.rootPath == rootPath && searchesEqual(c.criteria, s);
                                     }),
                      searchCache.end());

    CachedSearch c;
    c.rootPath = rootPath;
    c.criteria = s;
    c.resultPaths = resultPaths;
    c.scannedAtMs = juce::Time::currentTimeMillis();
    searchCache.push_back(std::move(c));

    while ((int) searchCache.size() > kMaxSearchCache)
        searchCache.erase(searchCache.begin());

    saveAsync();
}

} // namespace pflow
