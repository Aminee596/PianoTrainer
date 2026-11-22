# Notices and credits

Piano Trainer is copyright (c) 2026 Aminee596 and is released under the
GNU Affero General Public License v3.0 (see `LICENSE`).

It includes the following third-party work. Each part keeps its own licence.

## Libraries

| Component | Author | Licence |
| --- | --- | --- |
| [JUCE](https://juce.com) | Raw Material Software | AGPLv3 (used under its open-source option) |
| [ONNX Runtime](https://github.com/microsoft/onnxruntime) 1.20.1 | Microsoft Corporation | MIT |
| [WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2) | Microsoft Corporation | BSD 3-Clause |
| [React](https://react.dev), React DOM | Meta Platforms, Inc. and affiliates | MIT |
| [Lucide](https://lucide.dev) icons (lucide-react) | Lucide Contributors | ISC |
| [Tailwind CSS](https://tailwindcss.com) | Tailwind Labs, Inc. | MIT |

## Model

**Transkun 2.0** piano transcription model by Yujia Yan, MIT licence
(`assets/models/transkun/LICENSE.transkun`),
<https://github.com/Yujia-Yan/Skipping-The-Frame-Level>.
ONNX export by TuesdayCrowd (public domain): <https://huggingface.co/TuesdayCrowd/transkun-onnx>.

## Piano sound

**Salamander Grand Piano V3** by Alexander Holm, licensed under
[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/)
(`assets/audio/SalamanderGrandPiano/LICENSE.txt`).
Source: <https://archive.org/details/SalamanderGrandPianoV3>.
Changed for Piano Trainer: 9 of 16 velocity layers kept, notes cut to 12 s with a fade,
converted to 16-bit FLAC, louder per-layer volume.

## Fonts

- **Manrope** by Mikhail Sharanda, SIL Open Font License 1.1 (`assets/fonts/OFL-Manrope.txt`)
- **JetBrains Mono** by JetBrains, SIL Open Font License 1.1 (`assets/fonts/OFL-JetBrainsMono.txt`)

## MIDI pieces

These pieces are by **Bernd Krueger, [piano-midi.de](http://www.piano-midi.de)**, licensed under
[CC BY-SA 3.0 DE](https://creativecommons.org/licenses/by-sa/3.0/de/deed.en).
Changes for Piano Trainer: renamed files and tracks, hands split into "Right hand" and
"Left hand" tracks, and repeated same-key note strikes removed. The changed files are
shared under the same licence:

- Albéniz: Asturias
- Bach: Prelude in C
- Beethoven: Moonlight Sonata, Pathétique (Adagio)
- Chopin: Fantaisie-Impromptu, Heroic Polonaise, Prelude No. 4, Prelude No. 7,
  Raindrop Prelude, Revolutionary Étude
- Grieg: Wedding Day at Troldhaugen
- Liszt: Hungarian Rhapsody No. 2, La Campanella, Liebestraum No. 3
- Mozart: Sonata Facile, Turkish March
- Mussorgsky: Promenade and Gnomus
- Schumann: The Happy Farmer, Träumerei
- Tchaikovsky: June

The starter songs, exercises and scales, Ode to Joy and Liszt's Friska were written
for Piano Trainer from public-domain music and are covered by this project's licence.
