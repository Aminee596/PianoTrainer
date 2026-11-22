#include "core/MidiReader.h"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace piano
{
namespace
{
    struct Reader
    {
        const std::uint8_t* p;
        const std::uint8_t* end;

        bool has (std::size_t n) const { return (std::size_t) (end - p) >= n; }
        std::uint32_t u32() { std::uint32_t v = (std::uint32_t) p[0] << 24 | (std::uint32_t) p[1] << 16 | (std::uint32_t) p[2] << 8 | p[3]; p += 4; return v; }
        std::uint16_t u16() { std::uint16_t v = (std::uint16_t) (p[0] << 8 | p[1]); p += 2; return v; }

        bool vlq (std::uint32_t& v)
        {
            v = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (p >= end) return false;
                const auto b = *p++;
                v = (v << 7) | (b & 0x7f);
                if ((b & 0x80) == 0) return true;
            }
            return false;
        }
    };

    struct RawNote { int track, channel, pitch, velocity; std::uint64_t on, off; };
    struct TempoChange { std::uint64_t tick; double secondsPerTick; };
    struct TimeSig { std::uint64_t tick; int numerator, denominator; };

    struct TempoMap
    {
        std::vector<TempoChange> changes;
        std::vector<double> startSeconds;

        void build()
        {
            std::stable_sort (changes.begin(), changes.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
            startSeconds.assign (changes.size(), 0.0);
            for (std::size_t i = 1; i < changes.size(); ++i)
                startSeconds[i] = startSeconds[i - 1] + (double) (changes[i].tick - changes[i - 1].tick) * changes[i - 1].secondsPerTick;
        }

        double seconds (std::uint64_t tick) const
        {
            std::size_t i = 0;
            while (i + 1 < changes.size() && changes[i + 1].tick <= tick) ++i;
            return startSeconds[i] + (double) (tick - changes[i].tick) * changes[i].secondsPerTick;
        }
    };
}

bool parseMidi (const std::uint8_t* data, std::size_t size, Score& out, std::string& error)
{
    Reader r { data, data + size };

    if (! r.has (14) || std::string ((const char*) r.p, 4) != "MThd")
    {
        error = "not a MIDI file";
        return false;
    }
    r.p += 4;
    const auto headerLen = r.u32();
    if (headerLen < 6 || ! r.has (headerLen)) { error = "damaged MIDI header"; return false; }
    r.u16();
    const int numTracks = r.u16();
    const std::uint16_t division = r.u16();
    r.p += headerLen - 6;

    TempoMap tempo;
    double smpteSecondsPerTick = 0.0;
    int ticksPerQuarter = 480;
    bool smpte = false;
    if (division & 0x8000)
    {
        const int fps = -(int) (std::int8_t) (division >> 8);
        const int sub = division & 0xff;
        smpteSecondsPerTick = 1.0 / (double) (std::max (1, fps) * std::max (1, sub));
        tempo.changes.push_back ({ 0, smpteSecondsPerTick });
        smpte = true;
    }
    else
    {
        ticksPerQuarter = std::max (1, (int) division);
        tempo.changes.push_back ({ 0, 0.5 / ticksPerQuarter });
    }

    std::vector<RawNote> notes;
    std::vector<TimeSig> signatures;
    std::vector<std::string> trackNames ((std::size_t) numTracks);
    std::uint64_t endTick = 0;

    for (int t = 0; t < numTracks; ++t)
    {
        std::uint32_t len = 0;
        for (;;)
        {
            if (! r.has (8)) { error = "file ends before all tracks"; return false; }
            const std::string id ((const char*) r.p, 4);
            r.p += 4;
            len = r.u32();
            if (! r.has (len)) { error = "damaged track"; return false; }
            if (id == "MTrk") break;
            r.p += len;
        }

        Reader tr { r.p, r.p + len };
        r.p += len;

        std::uint64_t tick = 0;
        std::uint8_t running = 0;
        std::map<std::tuple<int, int>, std::deque<std::size_t>> open;

        while (tr.p < tr.end)
        {
            std::uint32_t delta;
            if (! tr.vlq (delta)) break;
            tick += delta;

            std::uint8_t status = *tr.p;
            if (status & 0x80) ++tr.p;
            else if (running) status = running;
            else break;

            if (status == 0xff)
            {
                if (! tr.has (1)) break;
                const std::uint8_t type = *tr.p++;
                std::uint32_t mlen;
                if (! tr.vlq (mlen) || ! tr.has (mlen)) break;
                const auto* m = tr.p;
                tr.p += mlen;

                if (type == 0x03 && trackNames[(std::size_t) t].empty())
                    trackNames[(std::size_t) t] = std::string ((const char*) m, mlen);
                else if (type == 0x51 && mlen == 3 && smpteSecondsPerTick == 0.0)
                {
                    const double usPerQuarter = (double) (m[0] << 16 | m[1] << 8 | m[2]);
                    tempo.changes.push_back ({ tick, usPerQuarter / 1.0e6 / ticksPerQuarter });
                }
                else if (type == 0x58 && mlen >= 2)
                    signatures.push_back ({ tick, std::max (1, (int) m[0]), 1 << std::min (6, (int) m[1]) });
                else if (type == 0x2f)
                    break;
                continue;
            }

            if (status == 0xf0 || status == 0xf7)
            {
                std::uint32_t slen;
                if (! tr.vlq (slen) || ! tr.has (slen)) break;
                tr.p += slen;
                continue;
            }

            if (status >= 0xf0) continue;

            running = status;
            const int kind = status & 0xf0;
            const int channel = (status & 0x0f) + 1;
            const int dataBytes = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
            if (! tr.has ((std::size_t) dataBytes)) break;
            const int d1 = tr.p[0] & 0x7f;
            const int d2 = dataBytes == 2 ? tr.p[1] & 0x7f : 0;
            tr.p += dataBytes;

            if (channel == 10) continue;

            if (kind == 0x90 && d2 > 0)
            {
                open[{ channel, d1 }].push_back (notes.size());
                notes.push_back ({ t, channel, d1, d2, tick, tick });
            }
            else if (kind == 0x80 || (kind == 0x90 && d2 == 0))
            {
                auto& queue = open[{ channel, d1 }];
                if (! queue.empty())
                {
                    notes[queue.front()].off = tick;
                    queue.pop_front();
                }
            }
        }
        endTick = std::max (endTick, tick);
    }

    tempo.build();

    Score result;
    if (! trackNames.empty())
        result.title = trackNames[0];

    if (smpteSecondsPerTick == 0.0)
    {
        std::stable_sort (signatures.begin(), signatures.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
        int num = 4, den = 4;
        std::size_t next = 0;
        for (double tick = 0.0; tick < (double) endTick - 1.0 && result.barStarts.size() < 10000;)
        {
            while (next < signatures.size() && (double) signatures[next].tick <= tick + 0.5)
            {
                num = signatures[next].numerator;
                den = signatures[next].denominator;
                ++next;
            }
            result.barStarts.push_back (tempo.seconds ((std::uint64_t) tick));
            tick += ticksPerQuarter * 4.0 * num / den;
        }
    }

    std::map<std::pair<int, int>, int> logical;
    std::map<int, std::set<int>> channelsPerTrack;
    for (const auto& n : notes)
        channelsPerTrack[n.track].insert (n.channel);

    std::vector<std::size_t> order (notes.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort (order.begin(), order.end(), [&] (std::size_t a, std::size_t b)
    {
        return notes[a].track != notes[b].track ? notes[a].track < notes[b].track : notes[a].on < notes[b].on;
    });

    for (const auto i : order)
    {
        const auto& n = notes[i];
        const auto key = std::make_pair (n.track, n.channel);
        auto it = logical.find (key);
        if (it == logical.end())
        {
            std::string name = trackNames[(std::size_t) n.track];
            if (name.empty()) name = "Track " + std::to_string (n.track + 1);
            if (channelsPerTrack[n.track].size() > 1) name += " (ch " + std::to_string (n.channel) + ")";

            it = logical.emplace (key, (int) result.tracks.size()).first;
            TrackInfo info;
            info.name = name;
            result.tracks.push_back (info);
        }

        const double start = tempo.seconds (n.on);
        double end = tempo.seconds (n.off);
        if (end <= start) end = start + 0.25;

        Note note { n.pitch, start, end - start, it->second, n.velocity };
        if (! smpte)
        {
            const auto tpq = (std::uint64_t) ticksPerQuarter;
            note.startBeat      = (double) n.on / (double) tpq;
            note.beats          = n.off > n.on ? (double) (n.off - n.on) / (double) tpq : -1.0;
            note.secondsPerBeat = tempo.seconds (n.on + tpq) - start;
        }
        result.notes.push_back (note);
    }

    if (result.notes.empty())
    {
        error = "no notes in the file";
        return false;
    }

    result.finalise();
    out = std::move (result);
    return true;
}

}
