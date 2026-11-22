#include "app/SamplePiano.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <random>

namespace app
{
namespace
{
    constexpr double maxSeconds = 14.0;

    int parseNoteName (juce::String s, bool allowNumbers = true)
    {
        s = s.trim().toLowerCase();
        if (s.isEmpty())
            return -1;
        if (s.containsOnly ("0123456789"))
            return allowNumbers ? juce::jlimit (0, 127, s.getIntValue()) : -1;

        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };
        const auto letter = s[0];
        if (letter < 'a' || letter > 'g')
            return -1;
        int note = base[letter - 'a'];
        int i = 1;
        if (s[i] == '#' || s[i] == 's') { ++note; ++i; }
        else if (s[i] == 'b' && s.length() > 2) { --note; ++i; }
        const auto octave = s.substring (i);
        if (octave.isEmpty() || ! octave.containsOnly ("-0123456789"))
            return -1;
        const int midi = (octave.getIntValue() + 1) * 12 + note;
        return juce::isPositiveAndBelow (midi, 128) ? midi : -1;
    }

    juce::String joinPath (const juce::String& dir, const juce::String& relative)
    {
        juce::StringArray parts, out;
        parts.addTokens ((dir.isEmpty() ? relative : dir + "/" + relative).replaceCharacter ('\\', '/'), "/", "");
        for (const auto& part : parts)
        {
            if (part.isEmpty() || part == ".")
                continue;
            if (part == "..")
            {
                if (! out.isEmpty())
                    out.remove (out.size() - 1);
                continue;
            }
            out.add (part);
        }
        return out.joinIntoString ("/");
    }

    juce::String parentOf (const juce::String& path)        { return path.containsChar ('/') ? path.upToLastOccurrenceOf ("/", false, false) : juce::String(); }
    juce::String fileNameOf (const juce::String& path)      { return path.fromLastOccurrenceOf ("/", false, false); }
    juce::String withoutExtension (const juce::String& name) { return name.containsChar ('.') ? name.upToLastOccurrenceOf (".", false, false) : name; }

    struct Source
    {
        virtual ~Source() = default;
        virtual juce::StringArray files() const = 0;
        virtual std::unique_ptr<juce::InputStream> open (const juce::String& path) const = 0;

        juce::String readText (const juce::String& path) const
        {
            if (auto stream = open (path))
                return stream->readEntireStreamAsString();
            return {};
        }

        juce::String name, location;
    };

    struct FolderSource final : Source
    {
        explicit FolderSource (const juce::File& folder) : root (folder)
        {
            name = root.getFileName();
            location = root.getFullPathName();
        }

        juce::StringArray files() const override
        {
            juce::StringArray out;
            for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*", juce::File::findFiles))
                out.add (entry.getFile().getRelativePathFrom (root).replaceCharacter ('\\', '/'));
            return out;
        }

        std::unique_ptr<juce::InputStream> open (const juce::String& path) const override
        {
            const auto file = root.getChildFile (path);
            if (! file.existsAsFile())
                return nullptr;
            return file.createInputStream();
        }

        juce::File root;
    };

    struct ZipSource final : Source
    {
        ZipSource (juce::InputStream* stream, const juce::String& sourceName, const juce::String& sourceLocation)
            : zip (stream, true)
        {
            name = sourceName;
            location = sourceLocation;
        }

        juce::StringArray files() const override
        {
            juce::StringArray out;
            for (int i = 0; i < zip.getNumEntries(); ++i)
                if (const auto* entry = zip.getEntry (i); entry != nullptr && ! entry->filename.endsWithChar ('/'))
                    out.add (entry->filename.replaceCharacter ('\\', '/'));
            return out;
        }

        std::unique_ptr<juce::InputStream> open (const juce::String& path) const override
        {
            const int index = zip.getIndexOfFileName (path, true);
            if (index < 0)
                return nullptr;
            return std::unique_ptr<juce::InputStream> (zip.createStreamForEntry (index));
        }

        mutable juce::ZipFile zip;
    };

    struct Decoder
    {
        Decoder() { formats.registerBasicFormats(); }

        bool load (const Source& source, const juce::String& path, int64_t offset, SampleRegion& r, size_t& bytes)
        {
            const auto key = path + ":" + juce::String (offset);
            if (auto it = cache.find (key); it != cache.end())
            {
                r.data = it->second.data;
                r.frames = it->second.frames;
                r.sampleRate = it->second.rate;
                return true;
            }

            auto stream = source.open (path);
            if (stream == nullptr)
                return false;
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
            if (reader == nullptr || reader->lengthInSamples <= offset)
                return false;

            const double rate = reader->sampleRate;
            const auto total = (int) std::min<int64_t> (reader->lengthInSamples - offset, (int64_t) (maxSeconds * rate));
            juce::AudioBuffer<float> buffer ((int) std::max (1u, reader->numChannels), total);
            reader->read (&buffer, 0, total, offset, true, true);

            const int fade = std::min (total, (int) (0.3 * rate));
            if (reader->lengthInSamples - offset > total)
                buffer.applyGainRamp (total - fade, fade, 1.0f, 0.0f);

            auto data = std::make_shared<std::vector<std::int16_t>> ((size_t) total * 2);
            const auto* left = buffer.getReadPointer (0);
            const auto* right = buffer.getReadPointer (std::min (1, buffer.getNumChannels() - 1));
            for (int i = 0; i < total; ++i)
            {
                (*data)[(size_t) i * 2]     = (std::int16_t) juce::jlimit (-32767.0f, 32767.0f, left[i] * 32767.0f);
                (*data)[(size_t) i * 2 + 1] = (std::int16_t) juce::jlimit (-32767.0f, 32767.0f, right[i] * 32767.0f);
            }
            bytes += data->size() * sizeof (std::int16_t);

            cache[key] = { data, total, rate };
            r.data = data;
            r.frames = total;
            r.sampleRate = rate;
            return true;
        }

        struct Entry { std::shared_ptr<const std::vector<std::int16_t>> data; int frames; double rate; };
        juce::AudioFormatManager formats;
        std::map<juce::String, Entry> cache;
    };

    juce::String stripComments (const juce::String& text)
    {
        juce::String out;
        auto p = text.getCharPointer();
        bool block = false;
        while (! p.isEmpty())
        {
            const auto c = *p;
            if (block)
            {
                if (c == '*' && *(p + 1) == '/') { block = false; ++p; }
            }
            else if (c == '/' && *(p + 1) == '/')
            {
                while (! p.isEmpty() && *p != '\n') ++p;
                continue;
            }
            else if (c == '/' && *(p + 1) == '*')
            {
                block = true;
                ++p;
            }
            else
            {
                out << juce::String::charToString (c);
            }
            ++p;
        }
        return out;
    }

    juce::String expand (juce::String text, const std::map<juce::String, juce::String>& defines)
    {
        if (! text.containsChar ('$'))
            return text;
        std::vector<std::pair<juce::String, juce::String>> sorted (defines.begin(), defines.end());
        std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
        for (const auto& [name, value] : sorted)
            text = text.replace (name, value);
        return text;
    }

    juce::String preprocess (const Source& source, const juce::String& path, std::map<juce::String, juce::String>& defines, int depth = 0)
    {
        if (depth > 8)
            return {};
        const auto text = source.readText (path);
        if (text.isEmpty())
            return {};
        juce::String result;
        juce::StringArray lines;
        lines.addLines (stripComments (text));
        for (auto line : lines)
        {
            const auto trimmed = line.trim();
            if (trimmed.startsWith ("#define"))
            {
                const auto rest = trimmed.substring (7).trim();
                defines[rest.upToFirstOccurrenceOf (" ", false, false)] = rest.fromFirstOccurrenceOf (" ", false, false).trim();
                continue;
            }
            if (trimmed.startsWith ("#include"))
            {
                auto name = trimmed.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
                name = expand (name, defines);
                result << preprocess (source, joinPath (parentOf (path), name), defines, depth + 1) << "\n";
                continue;
            }
            result << expand (line, defines) << "\n";
        }
        return result;
    }

    using Opcodes = std::map<juce::String, juce::String>;

    template <typename Callback>
    void parseSfz (const juce::String& text, Callback onHeader)
    {
        juce::String header;
        Opcodes opcodes;
        auto flush = [&] { if (header.isNotEmpty()) onHeader (header, opcodes); opcodes.clear(); };

        const auto tokens = juce::StringArray::fromTokens (text, " \t\r\n", "");
        juce::String key, value;
        auto finishOpcode = [&] { if (key.isNotEmpty()) opcodes[key] = value.trim(); key.clear(); value.clear(); };

        for (const auto& token : tokens)
        {
            if (token.startsWith ("<") && token.contains (">"))
            {
                finishOpcode();
                flush();
                header = token.fromFirstOccurrenceOf ("<", false, false).upToFirstOccurrenceOf (">", false, false);
                const auto after = token.fromFirstOccurrenceOf (">", false, false);
                if (after.containsChar ('='))
                {
                    key = after.upToFirstOccurrenceOf ("=", false, false);
                    value = after.fromFirstOccurrenceOf ("=", false, false);
                }
            }
            else if (token.containsChar ('=') && ! token.startsWith ("="))
            {
                finishOpcode();
                key = token.upToFirstOccurrenceOf ("=", false, false);
                value = token.fromFirstOccurrenceOf ("=", false, false);
            }
            else if (key.isNotEmpty())
            {
                value << " " << token;
            }
        }
        finishOpcode();
        flush();
    }

    std::shared_ptr<SampleSet> loadSfz (const Source& source, const juce::String& sfz, juce::String& error)
    {
        std::map<juce::String, juce::String> defines;
        const auto text = preprocess (source, sfz, defines);

        auto set = std::make_shared<SampleSet>();
        set->name = withoutExtension (fileNameOf (sfz));
        Decoder decoder;
        Opcodes control, global, master, group;
        int missing = 0;

        parseSfz (text, [&] (const juce::String& header, const Opcodes& ops)
        {
            if (header == "control") { control = ops; return; }
            if (header == "global")  { global = ops; master.clear(); group.clear(); return; }
            if (header == "master")  { master = ops; group.clear(); return; }
            if (header == "group")   { group = ops; return; }
            if (header != "region")  return;

            Opcodes o = global;
            for (const Opcodes* layer : std::initializer_list<const Opcodes*> { &master, &group, &ops })
                for (const auto& [k, v] : *layer)
                    o[k] = v;
            auto get = [&] (const char* k, const juce::String& fallback = {}) { auto it = o.find (k); return it != o.end() ? it->second : fallback; };

            if (get ("trigger", "attack") != "attack" || get ("sample").isEmpty() || get ("sample").startsWith ("*"))
                return;

            for (const auto& [k, v] : o)
                if (k.startsWith ("locc") || k.startsWith ("hicc"))
                {
                    const auto cc = k.substring (4);
                    if (! cc.containsOnly ("0123456789"))
                        continue;
                    double value = 0.0;
                    if (auto it = control.find ("set_cc" + cc); it != control.end())        value = it->second.getDoubleValue();
                    else if (auto hd = control.find ("set_hdcc" + cc); hd != control.end()) value = hd->second.getDoubleValue() * 127.0;
                    const double limit = v.getDoubleValue();
                    if (k.startsWith ("locc") ? value < limit : value > limit)
                        return;
                }

            SampleRegion r;
            const int key = parseNoteName (get ("key"));
            r.loKey = key >= 0 ? key : std::max (0, parseNoteName (get ("lokey", "0")));
            r.hiKey = key >= 0 ? key : parseNoteName (get ("hikey", "127"));
            if (r.hiKey < 0) r.hiKey = 127;
            const int centre = parseNoteName (get ("pitch_keycenter"));
            r.keyCenter = centre >= 0 ? centre : (key >= 0 ? key : r.loKey);
            r.loVel = juce::jlimit (1, 127, get ("lovel", "1").getIntValue());
            r.hiVel = juce::jlimit (1, 127, get ("hivel", "127").getIntValue());
            r.tuneCents = get ("tune", "0").getDoubleValue() + 100.0 * get ("transpose", "0").getIntValue();
            r.gain = juce::Decibels::decibelsToGain (get ("volume", "0").getFloatValue() + get ("group_volume", "0").getFloatValue());
            r.release = juce::jlimit (0.05f, 3.0f, get ("ampeg_release", "0.35").getFloatValue());
            r.loRand = get ("lorand", "0").getFloatValue();
            r.hiRand = get ("hirand", "1").getFloatValue();

            const auto path = (control.count ("default_path") ? control["default_path"] : juce::String())
                            + get ("sample");
            if (decoder.load (source, joinPath (parentOf (sfz), path), get ("offset", "0").getLargeIntValue(), r, set->bytes))
                set->regions.push_back (std::move (r));
            else
                ++missing;
        });

        if (set->regions.empty())
        {
            error = fileNameOf (sfz) + ": no playable samples" + (missing > 0 ? " (" + juce::String (missing) + " files missing or unreadable)" : "");
            return nullptr;
        }
        return set;
    }

    std::shared_ptr<SampleSet> loadNamedFiles (const Source& source, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        juce::StringArray extensions;
        extensions.addTokens (formats.getWildcardForAllFormats().toLowerCase().removeCharacters ("*"), ";", "");

        struct Found { juce::String path; int note; int layer; };
        std::vector<Found> found;
        for (const auto& path : source.files())
        {
            const auto file = fileNameOf (path);
            if (! file.containsChar ('.') || ! extensions.contains (file.fromLastOccurrenceOf (".", true, false).toLowerCase()))
                continue;
            const auto name = withoutExtension (file);
            const auto tokens = juce::StringArray::fromTokens (name, "_- .()", "");
            auto noteAt = [&] (int i, bool numbers)
            {
                const int n = parseNoteName (tokens[i], numbers);
                return numbers && (n < 21 || n > 108) ? -1 : n;
            };
            int note = -1, at = -1, layer = 0;
            for (int pass = 0; pass < 2 && at < 0; ++pass)
                for (int i = tokens.size(); --i >= 0;)
                    if (const int n = noteAt (i, pass == 1); n >= 0)
                    {
                        note = n;
                        at = i;
                        break;
                    }
            static const juce::StringArray dynamics { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };
            static const juce::StringArray words { "soft", "medium", "hard" };
            for (int j = at + 1; at >= 0 && j < tokens.size(); ++j)
            {
                const auto t = tokens[j].toLowerCase();
                if (const auto digits = t.retainCharacters ("0123456789"); digits.isNotEmpty()) { layer = digits.getIntValue(); break; }
                if (dynamics.contains (t)) { layer = dynamics.indexOf (t) + 1; break; }
                if (words.contains (t))    { layer = 3 * words.indexOf (t) + 2; break; }
            }
            if (note >= 0)
                found.push_back ({ path, note, layer });
        }

        if (found.empty())
        {
            error = "No .sfz file or note-named samples (like C4.wav) in " + source.location;
            return nullptr;
        }

        std::vector<int> notes;
        for (const auto& f : found) notes.push_back (f.note);
        std::sort (notes.begin(), notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());

        auto set = std::make_shared<SampleSet>();
        set->name = source.name;
        Decoder decoder;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int lo = i == 0 ? 0 : (notes[i - 1] + notes[i]) / 2 + 1;
            const int hi = i + 1 == notes.size() ? 127 : (notes[i] + notes[i + 1]) / 2;

            std::vector<Found> layers;
            for (const auto& f : found)
                if (f.note == notes[i]) layers.push_back (f);
            std::sort (layers.begin(), layers.end(), [] (const Found& a, const Found& b) { return a.layer < b.layer; });

            for (size_t l = 0; l < layers.size(); ++l)
            {
                SampleRegion r;
                r.loKey = lo;
                r.hiKey = hi;
                r.keyCenter = notes[i];
                r.loVel = 1 + (int) (127 * l / layers.size());
                r.hiVel = (int) (127 * (l + 1) / layers.size());
                if (decoder.load (source, layers[l].path, 0, r, set->bytes))
                    set->regions.push_back (std::move (r));
            }
        }
        if (set->regions.empty())
        {
            error = "The samples in " + source.location + " couldn't be read";
            return nullptr;
        }
        return set;
    }

    struct SampleSound final : juce::SynthesiserSound
    {
        explicit SampleSound (std::shared_ptr<const SampleSet> s) : set (std::move (s)) {}
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
        std::shared_ptr<const SampleSet> set;
    };

    class SampleVoice final : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SampleSound*> (s) != nullptr; }

        void startNote (int key, float velocity, juce::SynthesiserSound* s, int) override
        {
            auto* sound = static_cast<SampleSound*> (s);
            const int vel = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
            region = sound->set->find (key, vel, random (rng));
            if (region == nullptr || region->frames < 4)
            {
                clearCurrentNote();
                return;
            }
            set = sound->set;

            const double semitones = key - region->keyCenter + region->tuneCents / 100.0;
            step = std::pow (2.0, semitones / 12.0) * region->sampleRate / getSampleRate();
            position = 0.0;

            const float v = velocity;
            gain = 0.45f * region->gain * (sound->set->velocityLayers ? 0.55f + 0.45f * v : std::pow (v, 1.6f));
            release = 1.0f;
            releasing = false;
            releaseDecay = (float) std::exp (-1.0 / (region->release * 0.35 * getSampleRate()));
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff && region != nullptr) releasing = true;
            else                                   finish();
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}
        using SynthesiserVoice::renderNextBlock;

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (region == nullptr) return;
            const auto& d = *region->data;
            const int last = region->frames - 2;
            auto* left  = out.getWritePointer (0);
            auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
            constexpr float scale = 1.0f / 32767.0f;

            for (int i = start; i < start + num; ++i)
            {
                const int idx = (int) position;
                if (idx >= last)
                {
                    finish();
                    return;
                }
                const float t = (float) (position - idx);
                auto sampleAt = [&] (int frame, int ch) { return (float) d[(size_t) (std::max (0, frame) * 2 + ch)] * scale; };
                auto hermite = [&] (int ch)
                {
                    const float y0 = sampleAt (idx - 1, ch), y1 = sampleAt (idx, ch), y2 = sampleAt (idx + 1, ch), y3 = sampleAt (idx + 2, ch);
                    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                    return ((c3 * t + c2) * t + c1) * t + y1;
                };

                if (releasing)
                {
                    release *= releaseDecay;
                    if (release < 1.0e-4f)
                    {
                        finish();
                        return;
                    }
                }
                const float g = gain * release;
                left[i] += hermite (0) * g;
                if (right != nullptr) right[i] += hermite (1) * g;
                position += step;
            }
        }

    private:
        void finish()
        {
            region = nullptr;
            set.reset();
            clearCurrentNote();
        }

        const SampleRegion* region = nullptr;
        std::shared_ptr<const SampleSet> set;
        double position = 0.0, step = 1.0;
        float gain = 1.0f, release = 1.0f, releaseDecay = 1.0f;
        bool releasing = false;
        std::minstd_rand rng { 1234u };
        std::uniform_real_distribution<float> random { 0.0f, 1.0f };
    };
}

const SampleRegion* SampleSet::find (int key, int velocity, float r) const
{
    const SampleRegion* best = nullptr;
    int bestDistance = 1000;
    for (const auto& reg : regions)
    {
        if (velocity < reg.loVel || velocity > reg.hiVel || r < reg.loRand || r >= reg.hiRand + 1.0e-6f)
            continue;
        if (key >= reg.loKey && key <= reg.hiKey)
            return &reg;
        const int distance = key < reg.loKey ? reg.loKey - key : key - reg.hiKey;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = &reg;
        }
    }
    return best;
}

juce::Range<int> SampleSet::keyRange() const
{
    int lo = 127, hi = 0;
    for (const auto& r : regions)
    {
        lo = std::min (lo, r.loKey);
        hi = std::max (hi, r.hiKey);
    }
    return { lo, std::max (lo, hi) };
}

namespace
{
    std::shared_ptr<const SampleSet> loadFrom (const Source& source, juce::String& error)
    {
        std::shared_ptr<SampleSet> set;
        juce::StringArray sfzFiles, included;
        for (const auto& path : source.files())
            if (path.endsWithIgnoreCase (".sfz"))
                sfzFiles.add (path);
        for (const auto& f : sfzFiles)
        {
            juce::StringArray lines;
            lines.addLines (source.readText (f));
            for (const auto& line : lines)
                if (line.trim().startsWith ("#include"))
                    included.add (joinPath (parentOf (f), line.fromFirstOccurrenceOf ("\"", false, false)
                                                              .upToLastOccurrenceOf ("\"", false, false)));
        }
        for (int i = sfzFiles.size(); --i >= 0;)
            if (included.contains (sfzFiles[i], true))
                sfzFiles.remove (i);
        std::sort (sfzFiles.begin(), sfzFiles.end(), [] (const juce::String& a, const juce::String& b)
        {
            const int da = a.length() - a.replace ("/", "").length();
            const int db = b.length() - b.replace ("/", "").length();
            return da != db ? da < db : fileNameOf (a) < fileNameOf (b);
        });

        set = sfzFiles.isEmpty() ? loadNamedFiles (source, error) : loadSfz (source, sfzFiles[0], error);

        if (set != nullptr)
        {
            std::map<int, std::vector<int>> levels;
            for (const auto& r : set->regions)
                levels[r.keyCenter].push_back (r.loVel);
            for (auto& [key, v] : levels)
            {
                std::sort (v.begin(), v.end());
                if (std::unique (v.begin(), v.end()) - v.begin() > 1)
                    set->velocityLayers = true;
            }
        }
        return set;
    }
}

std::shared_ptr<const SampleSet> loadSampleSet (const juce::File& location, juce::String& error)
{
    if (location.isDirectory())
        return loadFrom (FolderSource (location), error);

    if (location.existsAsFile() && location.hasFileExtension ("zip"))
    {
        auto stream = location.createInputStream();
        if (stream == nullptr)
        {
            error = "Can't open " + location.getFullPathName();
            return nullptr;
        }
        return loadFrom (ZipSource (stream.release(), location.getFileNameWithoutExtension(), location.getFullPathName()), error);
    }

    error = "No folder or .zip at " + location.getFullPathName();
    return nullptr;
}

bool hasBuiltInPiano()
{
    const void* data = nullptr;
    size_t size = 0;
    return builtInPianoData (data, size);
}

std::shared_ptr<const SampleSet> loadBuiltInPiano (juce::String& error)
{
    const void* data = nullptr;
    size_t size = 0;
    if (! builtInPianoData (data, size))
    {
        error = "This build has no built-in piano";
        return nullptr;
    }
    return loadFrom (ZipSource (new juce::MemoryInputStream (data, size, false), "Built-in piano", "the built-in piano"), error);
}

juce::Array<juce::File> soundFolders (const juce::File& appDataFolder, bool includeProject)
{
    juce::Array<juce::File> folders;
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    folders.add (appDataFolder.getChildFile ("Sounds"));
    folders.add (exeDir.getChildFile ("Sounds"));

    if (includeProject)
        for (auto dir = exeDir; dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
            if (dir.getChildFile ("CMakeLists.txt").existsAsFile() && dir.getChildFile ("assets").isDirectory())
            {
                folders.add (dir.getChildFile ("assets").getChildFile ("audio"));
                break;
            }
    return folders;
}

juce::Array<juce::File> findSounds (const juce::Array<juce::File>& folders)
{
    juce::Array<juce::File> sounds;
    for (const auto& folder : folders)
        for (const auto& f : folder.findChildFiles (juce::File::findFilesAndDirectories, false, "*"))
        {
            const auto name = f.getFileName();
            if (name.startsWithChar ('.') || name.startsWithChar ('_'))
                continue;
            if (f.isDirectory() || f.hasFileExtension ("zip"))
                sounds.addIfNotAlreadyThere (f);
        }
    std::sort (sounds.begin(), sounds.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
    return sounds;
}

juce::SynthesiserSound* createSampleSound (std::shared_ptr<const SampleSet> set) { return new SampleSound (std::move (set)); }
juce::SynthesiserVoice* createSampleVoice()                                     { return new SampleVoice(); }

}
