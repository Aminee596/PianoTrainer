// Sample data for working on the UI in a browser (npm run dev), without the app.

import type { Reply, Settings, State } from "./types";

const day = 24 * 3600 * 1000;
const now = Date.now();

const defaults: Settings = {
  inputShift: 0, monitor: false, outputDevice: "", listenVolume: 70, listenHands: "both", anyOctave: false, chordWindowMs: 150, buildChords: true, holdNotes: true, rhythmGrid: 8, detectionLog: true, gateDb: -50,
  chords: "all", noteNames: true, fallingSpeed: 160, showOtherHand: true, keyboardKeys: 61,
  colours: { toPlay: "#46C2FF", played: "#37D483", right: "#6E8BFF", left: "#FFA94D" },
};

function piece(title: string, favourite: boolean, file: string, hands: number, length: number, bars: number, best: number, ago: number | null) {
  return { id: "C:/Music/" + file, title, favourite, file, hands, length, bars, best, last: best,
           lastPlayed: ago === null ? 0 : now - ago, added: now - 30 * day, duplicate: false, missing: false };
}

function initialState(): State {
  return {
    page: "home",
    section: "",
    current: {
      id: "C:/Music/ode_to_joy.mid", title: "Ode to Joy", hand: "Right hand", step: 6, steps: 62, progress: 5 / 62,
      bar: 2, bars: 16, accuracy: 83, streak: 3, finished: false,
      preview: [
        { pitch: 0.1, start: 0.55, length: 0.08, hand: "other" },
        { pitch: 0.25, start: 0.1, length: 0.1, hand: "left" },
        { pitch: 0.35, start: 0.62, length: 0.3, hand: "right" },
        { pitch: 0.5, start: 0.37, length: 0.2, hand: "right" },
        { pitch: 0.62, start: 0.0, length: 0.17, hand: "right" },
        { pitch: 0.75, start: 0.5, length: 0.2, hand: "right" },
        { pitch: 0.88, start: 0.27, length: 0.15, hand: "left" },
      ],
    },
    library: [
      piece("Ode to Joy", false, "ode_to_joy.mid", 2, 38, 16, 83, 2 * 3600 * 1000),
      piece("Für Elise", false, "fur_elise.mid", 2, 175, 105, 64, day),
      piece("Minuet in G", false, "minuet_in_g.mid", 2, 92, 32, 91, 3 * day),
      piece("Prelude in C", false, "prelude_c_bwv846.mid", 2, 140, 35, 47, 8 * day),
      piece("Twinkle Twinkle", false, "twinkle.mid", 1, 30, 12, 100, 9 * day),
      piece("Gymnopédie No. 1", false, "gymnopedie_1.mid", 2, 190, 78, -1, null),
      piece("Canon in D", false, "canon_in_d.mid", 2, 285, 57, -1, null),
    ],
    settings: structuredClone(defaults),
    audio: {
      drivers: ["Windows Audio", "Windows Audio (Low Latency Mode)", "Windows Audio (Exclusive Mode)", "DirectSound"],
      driver: "Windows Audio (Low Latency Mode)",
      inputs: ["Line In (Realtek Audio)", "Microphone (Realtek Audio)", "Microphone (USB Webcam)"],
      input: "Line In (Realtek Audio)",
      output: "Headphones (Realtek Audio)",
      outputs: ["Headphones (Realtek Audio)", "Speakers (Realtek Audio)", "HDMI (NVIDIA High Definition Audio)"],
      defaultInput: "Microphone (Realtek Audio)",
      defaultOutput: "Headphones (Realtek Audio)",
      open: true, sampleRate: 48000, analysisMs: 33, platform: "Windows 11 (browser preview)",
      piano: { state: "samples", name: "Salamander Grand Piano", detail: "270 samples, A0 to C8, several loudness levels, 510 MB",
               choice: "", options: [{ value: "", label: "Salamander Grand Piano (built in)" }], folder: "C:\\Users\\you\\AppData\\Roaming\\PianoTrainer\\Sounds" },
    },
    calibration: {
      running: false, index: 0, total: 61, key: 36, keyName: "C2", saved: null,
      learning: { available: true, active: true, known: 21, total: 61, error: "",
        missing: ["C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2", "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G#3", "A#3", "C#4", "D#4", "A#4", "D#5", "F5", "G#5", "A#5", "C6", "C#6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6", "C7"] },
    },
  };
}

type Listener = (payload: any) => void;

export function createMock() {
  const state = initialState();
  const listeners = new Map<string, Set<Listener>>();
  const emit = (event: string, payload: unknown) => listeners.get(event)?.forEach((fn) => fn(payload));
  const reply = (error = ""): Reply => ({ ok: !error, error, state: structuredClone(state) });

  let calibrating: ReturnType<typeof setInterval> | undefined;

  // A level meter that moves like someone playing.
  let t = 0;
  setInterval(() => {
    t += 1;
    const hit = t % 12 === 0;
    const level = hit ? -12 : -18 - (t % 12) * 2.5;
    emit("meter", { levelDb: level, note: t % 12 < 7 ? ["E4", "F4", "G4", "D4"][Math.floor(t / 12) % 4] : "" });
  }, 1000 / 15);

  return {
    on(event: string, fn: Listener) {
      if (!listeners.has(event)) listeners.set(event, new Set());
      listeners.get(event)!.add(fn);
      return () => listeners.get(event)!.delete(fn);
    },

    async call(name: string, args: unknown[]): Promise<Reply> {
      await new Promise((r) => setTimeout(r, 30));
      switch (name) {
        case "openPiece":
        case "continuePractice":
          console.info("[mock] the app would show the practice screen now", args[0] ?? "");
          return reply();
        case "addPieces":
          return reply("The file dialog only opens in the app");
        case "importFiles": {
          for (const f of args[0] as { name: string }[])
            state.library.push(piece(f.name.replace(/\.midi?$/i, ""), false, f.name, 2, 60, 24, -1, null));
          return reply();
        }
        case "toggleFavourite": {
           state.library = state.library.map((p) => p.id === args[0] ? { ...p, favourite: !p.favourite } : p);
          return reply();
        }
        case "removeDuplicates":
          state.library = state.library.filter((p) => !p.duplicate);
          return reply();
        case "removeMissing":
          state.library = state.library.filter((p) => !p.missing);
          return reply();
        case "removePiece":
          state.library = state.library.filter((p) => p.id !== args[0]);
          return reply();
        case "updateSettings": {
          const c = args[0] as Partial<Settings>;
          state.settings = { ...state.settings, ...c, colours: { ...state.settings.colours, ...(c.colours ?? {}) } };
          return reply();
        }
        case "resetSettings":
          state.settings = structuredClone(defaults);
          return reply();
        case "setDriver":
          state.audio.driver = String(args[0]);
          return reply();
        case "setInput":
          state.audio.input = String(args[0]);
          return reply();
        case "startCalibration": {
          // Pretends a key is pressed every 0.7 s.
          const total = state.settings.keyboardKeys === 88 ? 88 : 61, low = total === 88 ? 21 : 36;
          const names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
          const at = (i: number) => ({ key: low + i, keyName: names[(low + i) % 12] + (Math.floor((low + i) / 12) - 1) });
          state.calibration = { ...state.calibration, running: true, index: 0, total, ...at(0) };
          clearInterval(calibrating);
          calibrating = setInterval(() => {
            const c = state.calibration;
            if (!c.running) return clearInterval(calibrating);
            const index = c.index + 1;
            state.calibration = index < total ? { ...c, index, ...at(index) }
              : { ...c, running: false, index, saved: { keys: total, low: at(0).keyName, high: at(total - 1).keyName, date: "7 Nov 2026", suits: true, rate: 48000 } };
            emit("state", structuredClone(state));
          }, 700);
          return reply();
        }
        case "calibrationBack":
          if (state.calibration.index > 0) state.calibration = { ...state.calibration, index: state.calibration.index - 1, key: state.calibration.key - 1 };
          return reply();
        case "stopCalibration":
          state.calibration = { ...state.calibration, running: false };
          return reply();
        case "clearCalibration":
          state.calibration = { ...state.calibration, saved: null };
          return reply();
        default:
          return reply();
      }
    },
  };
}
