"""Writes assets/ode_to_joy.mid: Beethoven's "Ode to Joy" (public domain),
right hand melody on track 1 and a simple left hand on track 2.

    pip install mido
    python tools/make_sample_midi.py
"""
from pathlib import Path

import mido

TPB = 480  # ticks per quarter note
Q, H, E, DQ = TPB, TPB * 2, TPB // 2, TPB * 3 // 2

# (MIDI note, length)
C4, D4, E4, F4, G4, G3 = 60, 62, 64, 65, 67, 55
phrase_a = [(E4, Q), (E4, Q), (F4, Q), (G4, Q), (G4, Q), (F4, Q), (E4, Q), (D4, Q),
            (C4, Q), (C4, Q), (D4, Q), (E4, Q)]
right = (phrase_a + [(E4, DQ), (D4, E), (D4, H)]
         + phrase_a + [(D4, DQ), (C4, E), (C4, H)]
         + [(D4, Q), (D4, Q), (E4, Q), (C4, Q),
            (D4, Q), (E4, E), (F4, E), (E4, Q), (C4, Q),
            (D4, Q), (E4, E), (F4, E), (E4, Q), (D4, Q),
            (C4, Q), (D4, Q), (G3, H)]
         + phrase_a + [(D4, DQ), (C4, E), (C4, H)])

# Left hand: one bass note per half bar (C = 48/55, G = 43/50).
C, G = (48, 55), (43, 50)
harmony = [C, G, C, G, C, G, C, (43, 48),
           G, (43, 48), (43, 48), (48, 43),
           C, G, C, (43, 48)]
left = []
for bar in harmony:
    left += [(bar[0], H), (bar[1], H)]


def track(name, notes, channel, velocity):
    t = mido.MidiTrack()
    t.append(mido.MetaMessage("track_name", name=name, time=0))
    for pitch, length in notes:
        t.append(mido.Message("note_on", note=pitch, velocity=velocity, channel=channel, time=0))
        t.append(mido.Message("note_off", note=pitch, velocity=0, channel=channel, time=length))
    return t


song = mido.MidiFile(ticks_per_beat=TPB)
conductor = mido.MidiTrack()
conductor.append(mido.MetaMessage("track_name", name="Ode to Joy", time=0))
conductor.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(100), time=0))
conductor.append(mido.MetaMessage("time_signature", numerator=4, denominator=4, time=0))
song.tracks += [conductor, track("Right Hand", right, 0, 90), track("Left Hand", left, 1, 70)]

out = Path(__file__).resolve().parent.parent / "assets" / "ode_to_joy.mid"
out.parent.mkdir(exist_ok=True)
song.save(out)
print(f"wrote {out}")
