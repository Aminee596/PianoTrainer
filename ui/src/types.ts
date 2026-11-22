// Shapes of the data the app sends (see AppShell::state() in src/app/AppShell.cpp).

export type Page = "home" | "library" | "settings";

export interface Piece {
  id: string;          // full path of the MIDI file
  title: string;
  favourite: boolean;  // user marked as favourite
  file: string;        // file name
  hands: number;       // 2 = left and right hand parts
  length: number;      // seconds
  bars: number;
  best: number;        // best finished run, %, -1 = none
  last: number;        // last run, %, -1 = none
  lastPlayed: number;  // ms since epoch, 0 = never
  added: number;
  duplicate: boolean;
  missing: boolean;    // the file was moved or deleted
}

export interface PreviewNote { pitch: number; start: number; length: number; hand: "left" | "right" | "other" }

export interface Current {
  id: string;
  title: string;
  hand: string;
  step: number;
  steps: number;
  progress: number;    // 0..1
  bar: number;
  bars: number;
  accuracy: number;    // %, -1 = no notes yet
  streak: number;
  finished: boolean;
  preview: PreviewNote[];
}

export interface Colours { toPlay: string; played: string; right: string; left: string }

export interface Settings {
  inputShift: number;
  monitor: boolean;
  outputDevice: string;  // "" = follow the system default
  listenVolume: number;  // 0..100
  listenHands: "both" | "right" | "left";
  anyOctave: boolean;
  chordWindowMs: number;
  buildChords: boolean;
  holdNotes: boolean;
  rhythmGrid: 4 | 8 | 16;
  detectionLog: boolean;
  gateDb: number;
  chords: "all" | "top" | "bottom";
  noteNames: boolean;
  fallingSpeed: number;
  showOtherHand: boolean;
  keyboardKeys: 61 | 88;
  colours: Colours;
}

export interface Audio {
  drivers: string[];
  driver: string;
  inputs: string[];
  input: string;
  output: string;      // the output in use
  outputs: string[];
  defaultInput: string;
  defaultOutput: string;
  open: boolean;
  sampleRate: number;
  analysisMs: number;
  platform: string;
  piano: { state: "loading" | "samples" | "builtin"; name: string; detail: string;
           choice: string; options: { value: string; label: string }[]; folder: string };
}

/** Keyboard calibration: each key's sound, recorded once (AppShell::calibrationState). */
export interface Calibration {
  running: boolean;
  /** Learning the keyboard's sound from playing (Transkun in the background). */
  learning: { available: boolean; active: boolean; known: number; total: number; missing: string[]; error: string };
  index: number;       // keys recorded so far
  total: number;
  key: number;         // MIDI pitch to press next
  keyName: string;
  saved: { keys: number; low: string; high: string; date: string; suits: boolean; rate: number } | null;
}

export interface State {
  page: Page;
  section: string;
  current: Current | null;
  library: Piece[];
  settings: Settings;
  audio: Audio;
  calibration: Calibration;
}

export interface Reply { ok: boolean; error: string; state: State }

export interface Meter { levelDb: number; note: string }
