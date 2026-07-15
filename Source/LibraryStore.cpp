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
        && a.barsMin == b.barsMin
        && a.barsMax == b.barsMax
        && a.complexityMin == b.complexityMin
        && a.complexityMax == b.complexityMax
        && a.subdirs == b.subdirs
        && a.removeDuplicates == b.removeDuplicates;
}

namespace {

BrowserSearch readBrowserSearch(const juce::XmlElement& el)
{
    BrowserSearch s;
    s.query = el.getStringAttribute("query");
    s.keyRoot = el.getIntAttribute("key", -1);
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

void writeBrowserSearch(juce::XmlElement& el, const BrowserSearch& s)
{
    el.setAttribute("query", s.query);
    el.setAttribute("bpmMin", s.bpmMin);
    el.setAttribute("bpmMax", s.bpmMax);
    el.setAttribute("key", s.keyRoot);
    el.setAttribute("barsMin", s.barsMin);
    el.setAttribute("barsMax", s.barsMax);
    el.setAttribute("complexityMin", s.complexityMin);
    el.setAttribute("complexityMax", s.complexityMax);
    el.setAttribute("subdirs", s.subdirs ? 1 : 0);
    el.setAttribute("removeDuplicates", s.removeDuplicates ? 1 : 0);
}

} // namespace

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
                entry.search = readBrowserSearch(*child);
                entry.rootPath = child->getStringAttribute("root");
                if (entry.name.isNotEmpty())
                    savedSearches.push_back(entry);
            }
            else if (child->hasTagName("SearchCache"))
            {
                CachedSearch c;
                c.rootPath = child->getStringAttribute("root");
                c.criteria = readBrowserSearch(*child);
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

void LibraryStore::save() const
{
    juce::XmlElement xml("MidiBrowserLibrary");
    xml.setAttribute("version", 1);

    for (const auto& star : starredFiles)
    {
        if (star.isEmpty()) continue;
        auto* child = xml.createNewChildElement("StarredFile");
        child->setAttribute("path", star);
    }

    for (const auto& ss : savedSearches)
    {
        if (ss.name.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SavedSearch");
        child->setAttribute("name", ss.name);
        if (ss.rootPath.isNotEmpty())
            child->setAttribute("root", ss.rootPath);
        writeBrowserSearch(*child, ss.search);
    }

    for (const auto& c : searchCache)
    {
        if (c.rootPath.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SearchCache");
        child->setAttribute("root", c.rootPath);
        writeBrowserSearch(*child, c.criteria);
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
    xml.writeTo(file);
}

void LibraryStore::toggleStarred(const juce::String& path)
{
    if (path.isEmpty()) return;
    if (!starredFiles.contains(path))
        starredFiles.add(path);
    else
        starredFiles.removeString(path);
    save();
}

void LibraryStore::setStarredFiles(const juce::StringArray& paths)
{
    starredFiles = paths;
    save();
}

void LibraryStore::addSavedSearch(const SavedSearchEntry& entry)
{
    if (entry.name.isEmpty()) return;
    savedSearches.push_back(entry);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    save();
}

void LibraryStore::removeSavedSearch(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) savedSearches.size()))
        return;
    savedSearches.erase(savedSearches.begin() + index);
    save();
}

void LibraryStore::setSavedSearches(std::vector<SavedSearchEntry> entries)
{
    savedSearches = std::move(entries);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    save();
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
        for (const auto& have : savedSearches)
            if (have.name == ss.name && searchesEqual(have.search, ss.search))
            {
                exists = true;
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
        save();
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

    save();
}

} // namespace pflow
