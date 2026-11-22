# Piano Trainer

A desktop app that loads a MIDI piece, listens to your piano through a microphone
or a jack cable, and only moves on once you play the right note (wait-mode).

Built in C++20 with [JUCE](https://juce.com). Windows first; macOS and Linux later.

## How to use

Download `Piano Trainer.exe` from the latest release and run it. Everything is built in:
the piano sound, the practice pieces and the keyboard-learning model.

## Set up on Windows

JUCE needs **MSVC** on Windows (MinGW/MSYS2 GCC is not supported).

1. Install **Visual Studio Build Tools** (2022 or newer) with **Desktop development with C++**, plus **Git**.
2. In VS Code, install **C/C++** and **CMake Tools**, open this folder and pick the **Windows (MSVC + Ninja)**
   configure preset. CMake Tools loads the MSVC environment itself, so any CMake 3.22+ works,
   including MSYS2's. The first configure downloads JUCE (~1 min).
3. Pick the **Release** build preset, build (F7), run (Ctrl+F5).

From a plain terminal, use the Visual Studio solution preset instead:

```text
cmake --preset windows-vs
cmake --build --preset windows-vs-release
ctest --preset windows-vs
```

The app is written to `build/<preset>/PianoTrainer_artefacts/<Config>/Piano Trainer.exe`.

## Using it

1. **Audio input...**: choose the microphone, or the line-in where the keyboard's headphone jack is plugged.
   On Windows, *Windows Audio (Low Latency Mode)* gives the lowest delay.
   Check that the level meter (top right) moves when you play and stays out of the red.
2. **Open MIDI...** (or drop a `.mid` file on the window). Try `assets/ode_to_joy.mid`.
3. Pick **Left hand**, **Right hand** or **Both hands** at the top. Play the glowing key; the piece waits for it.

| Control | What it does |
| --- | --- |
| Chords | Which notes of a chord must be played: all, top (melody) or bottom (bass) |
| Input | Compensates if the keyboard's TRANSPOSE is set to another octave |
| Any octave | Accepts the right note in any octave |
| R / ← / → | Restart / previous step / next step |
| A W S E D F T G Y H U J K | Play C4 to C5 from the computer keyboard (testing without a piano); Z / X change octave |

If Windows hears nothing: *Settings → Privacy & security → Microphone →* allow desktop apps.

## How detection works

`src/core` is plain C++ (no JUCE) and fully unit tested with synthetic piano tones:

- **KeyAttacks**: finds key presses, not sounds. A press starts with a sudden burst of
  new energy across the spectrum (spectral flux); ringing, echoes, beating and fading
  tails never make one. At each strike, the spectrum just before is subtracted, and the
  new sound is explained key by key ("88 meters", one per key, reading its partials):
  strongest fundamental first, each key's overtones removed so they can't pass for other
  keys. A key counts only if its own sound jumped and its fundamental is really there.
  One strike = one press of each key found; keys struck within ~0.1 s form one chord.
- **NoteTracker**: the gatekeeper between the audio and the piece (uses KeyAttacks).
- **WaitModeFollower**: groups the score into steps (notes starting together), checks
  played notes against the current step, counts right and wrong notes.

A note-on comes about 90 ms after the strike: the decision waits until the long analysis
window holds only sound from after the strike (a real keyboard's attack flutters and thumps
for a few tens of milliseconds), or comes sooner when the next strike arrives first. Tuned
on recordings of a Yamaha PSR-F51 (see Detection log in Settings: it also saves the input
as a FLAC file) and tested on a keyboard-like test voice
(three detuned strings per partial, hammer noise, damper, room reverb, line noise):
melodies, the same key six times in 0.6 s, legato re-presses, 3–4 note chords, and a
wrong key pressed again and again while the expected note still rings (it never counts
as the expected one). Hard cases: an octave in a chord when only the lower key is played
is accepted; very close low notes (C2 with D2), and two keys a semitone apart below about
B4 struck together, may be missed (their sounds can't be told apart in time).

The audio callback only copies samples into a lock-free FIFO; detection runs on its own
high-priority thread, and results reach the UI on the message thread.

## Design

UI design in Figma: [Piano Trainer — UI](https://www.figma.com/design/3t2q8WcV1Q11cwaw4GpMmu).
Colours, spacing and radii are Figma variables (collection *Theme*) so they can be
copied into `src/app/Theme.h` one-to-one.

## Project layout

```text
src/core/    detection, wait-mode, MIDI reader, practice session (no dependencies)
src/design/  design tokens from Figma
src/app/     JUCE app: audio input, MIDI loading, and the UI built from the Figma design
             (Theme.h = Figma variables and text styles, assets/fonts = Manrope + JetBrains Mono)
ui/          Home, Library and Settings (React + Vite + Tailwind); ui/dist is embedded in the app
tests/       unit tests for src/core
assets/      sample MIDI
tools/       script that generates the sample MIDI
```

## Roadmap

- [x] UI from the Figma design
- [x] "Wait for me" off: play-along mode at the piece's tempo
- [x] Polyphonic detection (chords) from the keyboard's audio
- [ ] MIDI keyboard input (for keyboards with USB)
- [ ] Practice options: loop a section, tempo-follow mode, progress per piece
- [ ] macOS and Linux builds

## Licence note

JUCE is dual-licensed (AGPLv3 or the JUCE commercial licence). Read
[JUCE's licence](https://juce.com/get-juce/#licensing-faq) before publishing or selling builds.
