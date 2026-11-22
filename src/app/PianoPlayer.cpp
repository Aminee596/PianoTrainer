#include "app/PianoPlayer.h"

#include <algorithm>
#include <cmath>

namespace app
{
namespace
{
    struct PianoSound final : juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class PianoVoice final : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PianoSound*> (s) != nullptr; }

        void startNote (int pitch, float velocity, juce::SynthesiserSound*, int) override
        {
            const double sr = getSampleRate();
            const double f0 = 440.0 * std::pow (2.0, (pitch - 69) / 12.0);
            const float v = juce::jlimit (0.05f, 1.0f, velocity);

            const double t0 = juce::jlimit (0.8, 9.0, 9.0 * std::pow (2.0, -(pitch - 36) / 18.0));
            const double slope = 1.9 - 0.9 * v;

            count = 0;
            for (int k = 1; k <= maxPartials; ++k)
            {
                const double f = k * f0 * std::sqrt (1.0 + 0.0004 * k * k);
                if (f > sr * 0.45 || f > 12000.0) break;
                auto& p = partials[(size_t) count++];
                const double w = juce::MathConstants<double>::twoPi * f / sr;
                p.rotRe = std::cos (w);
                p.rotIm = std::sin (w);
                p.re = 1.0;
                p.im = 0.0;
                const double amp = std::pow ((double) k, -slope) * (f > 4000.0 ? 4000.0 / f : 1.0);
                const double tk = t0 / (1.0 + 0.5 * (k - 1));
                p.fast = amp * 0.6;
                p.slow = amp * 0.4;
                p.fastDecay = std::exp (-1.0 / (tk * 0.22 * sr));
                p.slowDecay = std::exp (-1.0 / (tk * sr));
            }

            gain = 0.3f * std::pow (v, 1.4f);
            attack = 0.0f;
            attackStep = (float) (1.0 / (0.002 * sr));
            release = 1.0f;
            releasing = false;
            releaseDecay = (float) std::exp (-1.0 / (0.09 * sr));
            thump = 0.05f * v;
            thumpDecay = (float) std::exp (-1.0 / (0.004 * sr));
            noiseState = 0.0f;

            const float pan = juce::jlimit (-0.6f, 0.6f, (pitch - 60) / 48.0f);
            const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            panL = std::cos (angle);
            panR = std::sin (angle);
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff) releasing = true;
            else              clearCurrentNote();
        }

        using SynthesiserVoice::renderNextBlock;
        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive()) return;
            auto* left  = out.getWritePointer (0);
            auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

            for (int i = start; i < start + num; ++i)
            {
                double s = 0.0;
                for (int k = 0; k < count; ++k)
                {
                    auto& p = partials[(size_t) k];
                    const double re = p.re * p.rotRe - p.im * p.rotIm;
                    p.im = p.re * p.rotIm + p.im * p.rotRe;
                    p.re = re;
                    s += p.im * (p.fast + p.slow);
                    p.fast *= p.fastDecay;
                    p.slow *= p.slowDecay;
                }

                random = random * 1664525u + 1013904223u;
                const float white = (float) (random >> 8) / 8388608.0f - 1.0f;
                noiseState += 0.25f * (white - noiseState);
                const float hammer = noiseState * thump;
                thump *= thumpDecay;

                attack = std::min (1.0f, attack + attackStep);
                if (releasing) release *= releaseDecay;

                const float sample = ((float) s * gain + hammer) * attack * release;
                left[i] += sample * panL;
                if (right != nullptr) right[i] += sample * panR;
            }

            double loudest = 0.0;
            for (int k = 0; k < count; ++k)
            {
                auto& p = partials[(size_t) k];
                const double m = std::sqrt (p.re * p.re + p.im * p.im);
                p.re /= m;
                p.im /= m;
                loudest = std::max (loudest, p.fast + p.slow);
            }
            if (release < 1.0e-4f || loudest * gain < 1.0e-5)
                clearCurrentNote();
        }

    private:
        static constexpr int maxPartials = 14;
        struct Partial { double re, im, rotRe, rotIm, fast, slow, fastDecay, slowDecay; };
        std::array<Partial, maxPartials> partials {};
        int count = 0;
        float gain = 0.0f, attack = 0.0f, attackStep = 0.0f, release = 1.0f, releaseDecay = 1.0f;
        bool releasing = false;
        float thump = 0.0f, thumpDecay = 0.0f, noiseState = 0.0f, panL = 0.7f, panR = 0.7f;
        std::uint32_t random = 12345u;
    };
}

PianoPlayer::PianoPlayer()
{
    for (int i = 0; i < 48; ++i)
    {
        synth.addVoice (new PianoVoice());
        synth.addVoice (createSampleVoice());
    }
    synth.addSound (new PianoSound());
    synth.setNoteStealingEnabled (true);
    offs.reserve (1024);
    midi.ensureSize (8192);
}

PianoPlayer::~PianoPlayer() = default;

void PianoPlayer::setSamples (std::shared_ptr<const SampleSet> samples)
{
    synth.clearSounds();
    if (samples != nullptr)
        synth.addSound (createSampleSound (std::move (samples)));
    else
        synth.addSound (new PianoSound());
}

void PianoPlayer::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    synth.setCurrentPlaybackSampleRate (sr);
    scratch.setSize (2, std::max (maxBlockSize, 4096), false, true, true);
}

void PianoPlayer::start (std::vector<piano::Note> notesToPlay, std::vector<std::uint8_t> litNotes, double fromSeconds)
{
    litNotes.resize (notesToPlay.size(), 1);

    Command c;
    c.notes = std::move (notesToPlay);
    c.lit = std::move (litNotes);
    c.from = std::max (0.0, fromSeconds);
    c.play = true;
    c.generation = requested.load() + 1;

    pos = c.from;
    wanted = true;
    requested = c.generation;

    const juce::SpinLock::ScopedLockType sl (lock);
    std::swap (pending, c);
    hasPending = true;
}

void PianoPlayer::stop()
{
    wanted = false;
    const juce::SpinLock::ScopedLockType sl (lock);
    pending.play = false;
    pending.generation = requested.load();
    hasPending = true;
}

bool PianoPlayer::isPlaying() const noexcept
{
    return wanted.load() && finished.load() != requested.load();
}

void PianoPlayer::takeStrikes (std::array<std::uint32_t, 128>& seen, std::vector<int>& struck) const
{
    struck.clear();
    for (size_t p = 0; p < 128; ++p)
        if (const auto n = strikes[p].load (std::memory_order_relaxed); n != seen[p])
        {
            seen[p] = n;
            struck.push_back ((int) p);
        }
}

std::vector<int> PianoPlayer::soundingPitches() const
{
    std::vector<int> pitches;
    for (int p = 0; p < 128; ++p)
        if ((sounding[(size_t) (p >> 6)].load (std::memory_order_relaxed) >> (p & 63)) & 1u)
            pitches.push_back (p);
    return pitches;
}

void PianoPlayer::takeCommand()
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || ! hasPending)
        return;
    hasPending = false;

    for (const auto& off : offs)
        midi.addEvent (juce::MidiMessage::noteOff (1, off.pitch), 0);
    offs.clear();
    counts.fill (0);

    generation = pending.generation;
    playing = pending.play;
    if (! playing)
        return;

    std::swap (notes, pending.notes);
    std::swap (lit, pending.lit);
    time = pending.from;
    next = (size_t) (std::lower_bound (notes.begin(), notes.end(), time - 1.0e-6,
                                       [] (const piano::Note& n, double t) { return n.start < t; })
                     - notes.begin());
    endTime = 0.0;
    for (const auto& n : notes)
        endTime = std::max (endTime, n.start + n.duration);
}

void PianoPlayer::render (float* const* outputs, int numOutputs, int numSamples)
{
    midi.clear();
    takeCommand();

    if (playing)
    {
        const double rate = juce::jlimit (0.1, 4.0, (double) speed.load (std::memory_order_relaxed));
        const double blockEnd = time + numSamples / sampleRate * rate;
        auto offset = [&] (double t) { return juce::jlimit (0, numSamples - 1, (int) ((t - time) * sampleRate / rate)); };

        for (size_t i = 0; i < offs.size();)
        {
            if (offs[i].end < blockEnd)
            {
                const int p = offs[i].pitch;
                midi.addEvent (juce::MidiMessage::noteOff (1, p), offset (offs[i].end));
                if (offs[i].lit && counts[(size_t) p] > 0) --counts[(size_t) p];
                offs[i] = offs.back();
                offs.pop_back();
            }
            else
            {
                ++i;
            }
        }

        while (next < notes.size() && notes[next].start < blockEnd)
        {
            const bool shown = next < lit.size() && lit[next] != 0;
            const auto& n = notes[next++];
            midi.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) juce::jlimit (1, 127, n.velocity)), offset (n.start));
            if (offs.size() < offs.capacity())
            {
                offs.push_back ({ n.start + std::max (0.05, n.duration), n.pitch, shown });
                if (shown)
                {
                    ++counts[(size_t) n.pitch];
                    strikes[(size_t) n.pitch].fetch_add (1, std::memory_order_relaxed);
                }
            }
        }

        time = blockEnd;
        pos.store (time);
        if (time > endTime + 0.5)
        {
            playing = false;
            finished.store (generation);
        }
    }

    std::uint64_t bits[2] = {};
    for (int p = 0; p < 128; ++p)
        if (counts[(size_t) p] > 0)
            bits[p >> 6] |= std::uint64_t (1) << (p & 63);
    sounding[0].store (bits[0], std::memory_order_relaxed);
    sounding[1].store (bits[1], std::memory_order_relaxed);

    if (numOutputs == 0 || numSamples > scratch.getNumSamples())
        return;

    scratch.clear (0, numSamples);
    synth.renderNextBlock (scratch, midi, 0, numSamples);

    const float g = volume.load (std::memory_order_relaxed);
    if (numOutputs == 1)
        scratch.addFrom (0, 0, scratch, 1, 0, numSamples);
    for (int ch = 0; ch < numOutputs; ++ch)
        if (auto* out = outputs[ch])
        {
            const auto* src = scratch.getReadPointer (std::min (ch, 1));
            const float chGain = numOutputs == 1 ? g * 0.5f : g;
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = out[i] + src[i] * chGain, a = std::abs (x);
                out[i] = a <= 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
            }
        }
}

}
