#include "app/Library.h"
#include "core/MidiReader.h"
#include "core/PracticeSession.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string_view>

namespace app
{
namespace
{
    juce::String contentHash (const void* data, size_t size)
    {
        const std::string_view bytes (static_cast<const char*> (data), size);
        return juce::String::toHexString ((juce::int64) std::hash<std::string_view> {} (bytes)) + ":" + juce::String ((juce::int64) size);
    }

    juce::String contentHash (const juce::File& file)
    {
        juce::MemoryBlock bytes;
        if (! file.loadFileAsData (bytes) || bytes.isEmpty())
            return {};
        return contentHash (bytes.getData(), bytes.getSize());
    }
}

Library::Library (juce::File storageFile)
    : storage (std::move (storageFile))
{
    load();
}

const Piece* Library::find (const juce::String& path) const
{
    for (const auto& p : items)
        if (p.path == path)
            return &p;
    return nullptr;
}

Piece* Library::findMutable (const juce::String& path)
{
    return const_cast<Piece*> (find (path));
}

juce::String Library::add (const juce::File& file)
{
    juce::MemoryBlock bytes;
    piano::Score score;
    std::string error;
    if (! file.loadFileAsData (bytes))
        return "Couldn't read " + file.getFileName();
    if (! piano::parseMidi ((const std::uint8_t*) bytes.getData(), bytes.getSize(), score, error))
        return file.getFileName() + ": " + juce::String (error.empty() ? "not a MIDI file" : error);
    if (score.empty())
        return file.getFileName() + " has no notes";

    piano::PracticeSession session;
    const auto title = score.title.empty() ? file.getFileNameWithoutExtension() : juce::String (score.title);
    const auto length = score.lengthSeconds();
    session.load (std::move (score));

    auto* p = findMutable (file.getFullPathName());
    if (p == nullptr)
    {
        items.push_back ({});
        p = &items.back();
        p->path  = file.getFullPathName();
        p->added = juce::Time::currentTimeMillis();
    }
    p->title  = title;
    p->hands  = session.canChooseHand() ? 2 : 1;
    p->length = length;
    p->bars   = session.barCount();
    p->hash   = contentHash (bytes.getData(), bytes.getSize());
    save();
    return {};
}

void Library::remove (const juce::String& path)
{
    items.erase (std::remove_if (items.begin(), items.end(), [&] (const Piece& p) { return p.path == path; }), items.end());
    save();
}

int Library::removeDuplicates()
{
    std::map<juce::String, size_t> kept;
    std::vector<Piece> result;
    for (auto& p : items)
    {
        if (p.hash.isEmpty())
        {
            result.push_back (p);
            continue;
        }
        const auto found = kept.find (p.hash);
        if (found == kept.end())
        {
            kept.emplace (p.hash, result.size());
            result.push_back (p);
            continue;
        }
        auto& keep = result[found->second];
        keep.favourite = keep.favourite || p.favourite;
        keep.best = std::max (keep.best, p.best);
        if (p.lastPlayed > keep.lastPlayed)
        {
            keep.lastPlayed = p.lastPlayed;
            keep.last = p.last;
        }
    }
    const auto removed = (int) (items.size() - result.size());
    if (removed > 0)
    {
        items = std::move (result);
        save();
    }
    return removed;
}

int Library::removeMissing()
{
    const auto before = items.size();
    items.erase (std::remove_if (items.begin(), items.end(), [] (const Piece& p) { return ! p.file().existsAsFile(); }), items.end());
    const auto removed = (int) (before - items.size());
    if (removed > 0)
        save();
    return removed;
}

void Library::markPlayed (const juce::String& path)
{
    if (auto* p = findMutable (path))
    {
        p->lastPlayed = juce::Time::currentTimeMillis();
        save();
    }
}

void Library::recordRun (const juce::String& path, int accuracy, bool finished)
{
    if (auto* p = findMutable (path); p != nullptr && accuracy >= 0)
    {
        p->last = accuracy;
        if (finished)
            p->best = std::max (p->best, accuracy);
        save();
    }
}

void Library::toggleFavourite (const juce::String& path)
{
    if (auto* p = findMutable (path))
    {
        p->favourite = ! p->favourite;
        save();
    }
}

juce::File Library::importFolder() const
{
    return storage.getSiblingFile ("Pieces");
}

juce::var Library::toVar() const
{
    juce::Array<juce::var> list;
    juce::StringArray seen;
    for (const auto& p : items)
    {
        const bool duplicate = p.hash.isNotEmpty() && seen.contains (p.hash);
        if (p.hash.isNotEmpty())
            seen.add (p.hash);
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", p.path);
        o->setProperty ("duplicate", duplicate);
        o->setProperty ("favourite", p.favourite);
        o->setProperty ("title", p.title);
        o->setProperty ("file", p.file().getFileName());
        o->setProperty ("hands", p.hands);
        o->setProperty ("length", p.length);
        o->setProperty ("bars", p.bars);
        o->setProperty ("best", p.best);
        o->setProperty ("last", p.last);
        o->setProperty ("lastPlayed", (double) p.lastPlayed);
        o->setProperty ("added", (double) p.added);
        o->setProperty ("missing", ! p.file().existsAsFile());
        list.add (juce::var (o));
    }
    return list;
}

void Library::load()
{
    const auto json = juce::JSON::parse (storage);
    if (const auto* list = json["pieces"].getArray())
        for (const auto& v : *list)
        {
            Piece p;
            p.path       = v["path"].toString();
            p.favourite  = (bool) v.getProperty ("favourite", false);
            p.title      = v["title"].toString();
            p.hands      = (int) v.getProperty ("hands", 1);
            p.length     = (double) v.getProperty ("length", 0.0);
            p.bars       = (int) v.getProperty ("bars", 0);
            p.best       = (int) v.getProperty ("best", -1);
            p.last       = (int) v.getProperty ("last", -1);
            p.lastPlayed = (juce::int64) v.getProperty ("lastPlayed", 0);
            p.added      = (juce::int64) v.getProperty ("added", 0);
            p.hash       = contentHash (p.file());
            if (p.path.isNotEmpty())
                items.push_back (p);
        }
}

void Library::save() const
{
    juce::Array<juce::var> list;
    for (const auto& p : items)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("path", p.path);
        o->setProperty ("favourite", p.favourite);
        o->setProperty ("title", p.title);
        o->setProperty ("hands", p.hands);
        o->setProperty ("length", p.length);
        o->setProperty ("bars", p.bars);
        o->setProperty ("best", p.best);
        o->setProperty ("last", p.last);
        o->setProperty ("lastPlayed", p.lastPlayed);
        o->setProperty ("added", p.added);
        list.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("pieces", list);

    storage.getParentDirectory().createDirectory();
    storage.replaceWithText (juce::JSON::toString (juce::var (root)));
}

}
