import { Check, Mic, Plus, RefreshCw, Undo2, X } from "lucide-react";
import { useEffect, useState } from "react";
import { ColourPicker, css } from "../components/ColourPicker";
import { BackTitle, Button, Card, Header, LevelBar, Row, Segmented, Select, Slider, Toggle, cx } from "../components/ui";
import { driverLabel, octaveLabel } from "../format";
import { useMeter, useStore } from "../store";
import type { Calibration, Colours, Settings as SettingsT } from "../types";

const swatches = ["#46C2FF", "#37D483", "#6E8BFF", "#FFA94D", "#FF6BB5", "#B07CFF", "#F5C451", "#2ED3C5"];

const presets: { value: string; label: string; colours: Colours }[] = [
  { value: "default", label: "Default", colours: { toPlay: "#46C2FF", played: "#37D483", right: "#6E8BFF", left: "#FFA94D" } },
  { value: "contrast", label: "High contrast", colours: { toPlay: "#F5C451", played: "#37D483", right: "#46C2FF", left: "#FF6BB5" } },
  { value: "colourblind", label: "Colour-blind", colours: { toPlay: "#46C2FF", played: "#F5C451", right: "#B07CFF", left: "#FFA94D" } },
];

/** Figma frame "Settings — 1440". */
export function Settings() {
  const { state, act, section, back, backLabel } = useStore();
  const s = state.settings;
  const set = (changes: Partial<SettingsT>) => act("updateSettings", changes);

  useEffect(() => {
    if (section) document.getElementById(section)?.scrollIntoView({ block: "start", behavior: "smooth" });
  }, [section]);

  return (
    <>
      <Header>
        <BackTitle title="Settings" onBack={back} label={backLabel} />
        <Button variant="ghost" onClick={() => act("resetSettings")}>Reset to defaults</Button>
      </Header>

      <main className="flex-1 overflow-y-auto">
        <div className="mx-auto grid max-w-[1248px] grid-cols-2 items-start gap-6 px-6 pt-8 pb-10">
          <div className="flex flex-col gap-6">
            <AudioCard />
            <ListenCard />
            <Card title="Display" description="What the practice screen shows.">
              <Row label="Keyboard" description="Match your instrument. A piece that goes beyond it adds an octave.">
                <Segmented label="Keyboard" value={String(s.keyboardKeys)}
                           onChange={(v) => set({ keyboardKeys: Number(v) as 61 | 88 })}
                           options={[{ value: "61", label: "61 keys" }, { value: "88", label: "88 keys" }]} />
              </Row>
              <Row label="Note names on keys">
                <Toggle label="Note names on keys" value={s.noteNames} onChange={(v) => set({ noteNames: v })} />
              </Row>
              <Row label="Falling speed" description="How fast the notes come down.">
                <Slider label="Falling speed" min={80} max={320} step={10} value={s.fallingSpeed}
                        onChange={(v) => set({ fallingSpeed: v })} format={(v) => `${v} px/s`} />
              </Row>
              <Row label="Show the other hand" description="In grey, while you practice one hand.">
                <Toggle label="Show the other hand" value={s.showOtherHand} onChange={(v) => set({ showOtherHand: v })} />
              </Row>
            </Card>
          </div>

          <div className="flex flex-col gap-6">
            <KeyboardSoundCard />
            <Card title="Detection" description="How notes are recognised and judged.">
              <Row label="Accept any octave" description="The right note in any octave counts.">
                <Toggle label="Accept any octave" value={s.anyOctave} onChange={(v) => set({ anyOctave: v })} />
              </Row>
              <Row label="Chords" description="Which notes of a chord you have to play.">
                <Segmented label="Chords" value={s.chords} onChange={(v) => set({ chords: v })}
                           options={[{ value: "all", label: "All notes" }, { value: "top", label: "Top" }, { value: "bottom", label: "Bottom" }]} />
              </Row>
              <Row label="Build chords key by key" description="Press a chord's keys one at a time: the keys you hold keep counting until the chord is complete.">
                <Toggle label="Build chords key by key" value={s.buildChords} onChange={(v) => set({ buildChords: v })} />
              </Row>
              {!s.buildChords && (
                <Row label="Chord window" description="Notes of a chord must all start within this time.">
                  <Slider label="Chord window" min={50} max={300} step={10} value={s.chordWindowMs}
                          onChange={(v) => set({ chordWindowMs: v })} format={(v) => `${v} ms`} />
                </Row>
              )}
              <Row label="Hold long notes" description="The piece pauses if you let go before a note ends.">
                <Toggle label="Hold long notes" value={s.holdNotes} onChange={(v) => set({ holdNotes: v })} />
              </Row>
              <Row label="Rhythm accuracy" description="A tap counts as this note value; longer notes must be held for their length, give or take half of it.">
                <Segmented label="Rhythm accuracy" value={String(s.rhythmGrid)}
                           onChange={(v) => set({ rhythmGrid: Number(v) as 4 | 8 | 16 })}
                           options={[{ value: "4", label: "1/4" }, { value: "8", label: "1/8" }, { value: "16", label: "1/16" }]} />
              </Row>
              <Row label="Detection log" description="Writes every key strike heard and what was decided to a spreadsheet file, and records the input as a FLAC sound file next to it (a few MB a minute) so detection can be tuned on your keyboard's real sound.">
                <Button variant="ghost" onClick={() => act("openLogFolder")}>Open folder</Button>
                <Toggle label="Detection log" value={s.detectionLog} onChange={(v) => set({ detectionLog: v })} />
              </Row>
              <Row label="Noise gate" description="Quieter sounds than this are ignored.">
                <Slider label="Noise gate" min={-80} max={-30} step={1} value={s.gateDb}
                        onChange={(v) => set({ gateDb: v })} format={(v) => `${Math.round(v)} dB`} />
              </Row>
            </Card>
            <ColoursCard colours={s.colours} />
          </div>
        </div>
      </main>
      {state.calibration.running && <CalibrationOverlay calibration={state.calibration} />}
    </>
  );
}

/** Calibration: the app records how each key of the keyboard sounds. */
function KeyboardSoundCard() {
  const { state, act } = useStore();
  const saved = state.calibration.saved;
  const learning = state.calibration.learning;
  const keys = state.settings.keyboardKeys;
  const description = saved
    ? `Learned ${saved.date}: all ${saved.keys} keys, ${saved.low} to ${saved.high}. Every key strike is matched against them. Record them again if you change the keyboard's voice.`
    : learning.available
      ? `The app is learning your keyboard from your playing: ${learning.known} of ${learning.total} keys so far. Until all are known it uses its standard detection. Or press each key once to record them all now (about two minutes).`
      : `Press each of the ${keys} keys once so the app learns how your keyboard sounds. It then tells chords, octaves and low notes apart far better. Takes about two minutes.`;
  const shown = 14;
  return (
    <Card id="keyboard" title="Your keyboard's sound" description="Detection learns from your instrument.">
      {!saved && learning.available && (
        <Row label="Learning from your playing" description={learning.missing.length > 0
          ? `Still to learn: ${learning.missing.slice(0, shown).join(", ")}${learning.missing.length > shown ? ` and ${learning.missing.length - shown} more` : ""}. Play each of these alone a couple of times.`
          : "Every key is known."}>
          <div className="w-[160px]">
            <div className="h-1.5 overflow-hidden rounded-full bg-hover">
              <div className="h-full rounded-full bg-good transition-[width]" style={{ width: `${(learning.known / Math.max(1, learning.total)) * 100}%` }} />
            </div>
            <div className="mt-1 text-right font-mono text-[11px] text-ink-3">{learning.known} / {learning.total}</div>
          </div>
        </Row>
      )}
      <Row label="Key fingerprints" description={description}>
        {saved && <Button variant="ghost" onClick={() => act("clearCalibration")}>Remove</Button>}
        <Button variant={saved && saved.suits ? "secondary" : "primary"} icon={Mic} onClick={() => act("startCalibration")}
                disabled={!state.audio.open}>
          {saved ? "Record again" : "Record keys"}
        </Button>
      </Row>
    </Card>
  );
}

/** Asks for one key after another, lowest to highest, until every key is recorded. */
function CalibrationOverlay({ calibration: c }: { calibration: Calibration }) {
  const { act } = useStore();
  const meter = useMeter();
  const low = c.key - c.index, high = low + c.total - 1;

  // Esc cancels (before the page's own Esc, which would leave Settings).
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== "Escape") return;
      e.stopImmediatePropagation();
      act("stopCalibration");
    };
    window.addEventListener("keydown", onKey, { capture: true });
    return () => window.removeEventListener("keydown", onKey, { capture: true });
  }, [act]);

  return (
    <div className="fixed inset-0 z-50 grid place-items-center bg-black/60 px-6" role="dialog" aria-modal="true" aria-label="Record your keyboard's keys">
      <div className="w-full max-w-[880px] rounded-lg border border-line bg-panel shadow-2xl">
        <div className="flex items-start justify-between gap-6 border-b border-line px-6 pt-5 pb-4">
          <div>
            <h2 className="text-[18px] font-bold">Press each key once</h2>
            <p className="mt-1 text-ink-3">Lowest to highest, black keys too. Press, hold about a second, let go. No sustain, same volume as when you practice.</p>
          </div>
          <Button variant="ghost" icon={X} onClick={() => act("stopCalibration")}>Cancel</Button>
        </div>
        <div className="flex items-center gap-8 px-6 pt-6 pb-2">
          <div className="w-[150px] shrink-0">
            <div className="caps text-ink-3">Press</div>
            <div className="text-[56px] leading-none font-bold text-accent">{c.keyName}</div>
          </div>
          <div className="min-w-0 flex-1">
            <div className="flex items-baseline justify-between text-ink-2">
              <span>Key {Math.min(c.index + 1, c.total)} of {c.total}</span>
              <span className="font-mono text-[11px] text-ink-3">{meter.levelDb > -99 ? `${Math.round(meter.levelDb)} dB` : "no sound"}</span>
            </div>
            <div className="mt-2 h-1.5 overflow-hidden rounded-full bg-hover">
              <div className="h-full rounded-full bg-good transition-[width]" style={{ width: `${(c.index / c.total) * 100}%` }} />
            </div>
            <LevelBar db={meter.levelDb} className="mt-2" />
          </div>
        </div>
        <div className="px-6 pt-4 pb-5">
          <KeyStrip low={low} high={high} current={c.key} />
        </div>
        <div className="flex items-center justify-between border-t border-line px-6 py-4">
          <span className="text-ink-3">Pressed a wrong key or two at once? Go back and press it again.</span>
          <Button icon={Undo2} onClick={() => act("calibrationBack")} disabled={c.index === 0}>Back one key</Button>
        </div>
      </div>
    </div>
  );
}

/** The keyboard, with the key to press lit and the keys done shaded. */
function KeyStrip({ low, high, current }: { low: number; high: number; current: number }) {
  const black = (p: number) => [1, 3, 6, 8, 10].includes(((p % 12) + 12) % 12);
  const whites: number[] = [];
  for (let p = low; p <= high; p++) if (!black(p)) whites.push(p);
  const w = 100 / whites.length;
  const shade = (p: number) => (p === current ? "bg-accent" : p < current ? "bg-good/70" : "");
  return (
    <div className="relative h-[84px] w-full select-none">
      <div className="flex h-full overflow-hidden rounded-b-[4px]">
        {whites.map((p) => (
          <div key={p} className={cx("flex-1 border-r border-[#C9CDD6] bg-[#F3F4F7]", shade(p))} />
        ))}
      </div>
      {Array.from({ length: high - low + 1 }, (_, i) => low + i).filter(black).map((p) => {
        const left = whites.indexOf(p - 1) + 1;
        return (
          <div key={p} className={cx("absolute top-0 h-[60%] rounded-b-[3px] bg-[#191C24]", shade(p))}
               style={{ left: `calc(${left * w}% - ${w * 0.3}%)`, width: `${w * 0.6}%` }} />
        );
      })}
    </div>
  );
}

function AudioCard() {
  const { state, act } = useStore();
  const { audio, settings: s } = state;
  const meter = useMeter();
  const level = audio.open ? meter.levelDb : -100;

  // With WASAPI available, its three modes are the useful choices (DirectSound is slower).
  const wasapi = audio.drivers.some((d) => d.startsWith("Windows Audio"));
  const drivers = audio.drivers.filter((d) => !(wasapi && d === "DirectSound"));
  const inputs = audio.inputs.map((i) => ({ value: i, label: i === audio.defaultInput ? `${i}  (Windows default)` : i }));
  if (!audio.open) inputs.unshift({ value: "", label: "None" });

  return (
    <Card id="audio" title="Audio input" description="Where the piano's sound comes from.">
      <Row label="Input device"
           description={!audio.input ? "No input is open: pick the line-in or microphone the cable is in."
                        : /micro/i.test(audio.input) && audio.inputs.length > 1 ? "With the cable, pick the line-in or the jack's input, not a built-in microphone." : undefined}>
        <Select prefix="Device" value={audio.input} options={inputs} onChange={(v) => act("setInput", v)} className="w-[256px]" />
      </Row>
      {drivers.length > 1 && (
        <Row label="Driver mode" description={wasapi ? "Low latency gives the fastest response." : undefined}>
          <Segmented label="Driver mode" value={audio.driver} onChange={(v) => act("setDriver", v)}
                     options={drivers.map((d) => ({ value: d, label: driverLabel(d) }))} />
        </Row>
      )}
      <Row label="Input level" description="Play a few notes: it should move but stay out of the red.">
        <LevelBar db={level} className="w-[200px]" />
        <span className="w-[60px] text-right font-mono text-[11px] text-ink-2">{level > -99 ? `${Math.round(level)} dB` : "-"}</span>
      </Row>
      <Row label="Input octave" description="If the keyboard's TRANSPOSE is set to another octave.">
        <Select prefix="Shift" value={String(s.inputShift)} onChange={(v) => act("updateSettings", { inputShift: Number(v) })}
                options={[-2, -1, 0, 1, 2].map((n) => ({ value: String(n), label: octaveLabel(n) }))} className="w-[156px]" />
      </Row>
      <Row label="Hear it through the PC" description="The jack cable mutes the keyboard's speakers.">
        <Toggle label="Hear it through the PC" value={s.monitor} onChange={(v) => act("updateSettings", { monitor: v })} />
      </Row>
    </Card>
  );
}

function ListenCard() {
  const { state, act } = useStore();
  const { piano } = state.audio;
  const dot = piano.state === "samples" ? "bg-good" : piano.state === "loading" ? "bg-heard" : "bg-ink-3";
  return (
    <Card title="Listen mode" description="The piano the app plays when you press Listen.">
      <div className="flex items-center justify-between gap-6 px-6 py-3.5">
        <div className="min-w-0">
          <div className="flex items-center gap-2 text-ink">
            <span className={cx("h-2 w-2 shrink-0 rounded-full", dot)} />
            <span className="truncate">{piano.state === "loading" ? `Loading ${piano.name}...` : piano.name}</span>
          </div>
          <div className="mt-0.5 leading-5 text-ink-3">{piano.detail}</div>
        </div>
        <Button variant="secondary" icon={RefreshCw} onClick={() => act("reloadPiano")} disabled={piano.state === "loading"}>
          Reload
        </Button>
      </div>
      {piano.options.length > 0 && (
        <Row label="Piano sound" description="To add your own, put an SFZ piano (a folder or a .zip) in the sounds folder, then press Reload.">
          <div className="flex items-center gap-2">
            <Select prefix="Sound" value={piano.choice} onChange={(v) => act("setPianoSound", v)}
                    options={piano.options} className="w-[256px]" />
            <Button variant="ghost" onClick={() => act("openSoundsFolder")}>Open folder</Button>
          </div>
        </Row>
      )}
      <Row label="Hands" description="Which hand plays. Only the hand you practice lights up.">
        <Segmented label="Listen hands" value={state.settings.listenHands}
                   onChange={(v) => act("updateSettings", { listenHands: v })}
                   options={[{ value: "both", label: "Both" }, { value: "right", label: "Right" }, { value: "left", label: "Left" }]} />
      </Row>
      <Row label="Plays through" description={state.settings.outputDevice ? "Always this device, when it's plugged in." : "Follows your PC's sound output: plug in headphones and it switches."}>
        <Select prefix="Output" value={state.settings.outputDevice}
                onChange={(v) => act("updateSettings", { outputDevice: v })}
                options={[{ value: "", label: `System default${state.audio.defaultOutput ? ` (${state.audio.defaultOutput})` : ""}` },
                          ...state.audio.outputs.map((o) => ({ value: o, label: o }))]}
                className="w-[256px]" />
      </Row>
      <Row label="Volume">
        <Slider label="Listen volume" min={0} max={100} step={1} value={state.settings.listenVolume}
                onChange={(v) => act("updateSettings", { listenVolume: v })} format={(v) => `${v}%`} />
      </Row>
    </Card>
  );
}

function ColoursCard({ colours }: { colours: Colours }) {
  const { act } = useStore();
  const setColours = (c: Partial<Colours>) => act("updateSettings", { colours: c });
  const same = (a: string, b: string) => a.toUpperCase() === b.toUpperCase();
  const preset = presets.find((p) => (Object.keys(p.colours) as (keyof Colours)[]).every((k) => same(p.colours[k], colours[k])));
  const [picking, setPicking] = useState<keyof Colours | null>(null);

  const rows: { key: keyof Colours; label: string; description: string }[] = [
    { key: "toPlay", label: "To play now", description: "The keys you should press. Falling notes glow in their hand's colour." },
    { key: "played", label: "Played", description: "The glow where a note you played meets the keyboard." },
    { key: "right", label: "Right hand", description: "Right-hand notes." },
    { key: "left", label: "Left hand", description: "Left-hand notes." },
  ];

  return (
    <Card title="Colours" description="Notes and keys on the practice screen.">
      <Row label="Preset" description="Start from a set, then adjust.">
        <Segmented label="Colour preset" value={preset?.value ?? null}
                   onChange={(v) => setColours(presets.find((p) => p.value === v)!.colours)}
                   options={presets.map(({ value, label }) => ({ value, label }))} />
      </Row>
      <div className="px-6 py-4">
        <Preview colours={colours} />
      </div>
      {rows.map((r, index) => {
        const custom = !swatches.some((c) => same(c, colours[r.key]));
        return (
          <Row key={r.key} label={r.label} description={r.description}>
            <div role="radiogroup" aria-label={r.label} className="relative flex gap-2">
              {swatches.map((c) => (
                <Swatch key={c} colour={c} selected={same(colours[r.key], c)} onClick={() => setColours({ [r.key]: c })} />
              ))}
              {/* Any colour and opacity: the current one if it's custom, else a rainbow to open the picker. */}
              <button
                type="button"
                aria-label="Custom colour"
                title="Custom colour and opacity"
                onClick={() => setPicking(picking === r.key ? null : r.key)}
                className={cx(
                  "grid h-[22px] w-[22px] place-items-center overflow-hidden rounded-full transition-transform hover:scale-110",
                  (custom || picking === r.key) && "ring-2 ring-ink ring-offset-2 ring-offset-panel",
                )}
                style={{ background: custom ? "repeating-conic-gradient(#8a8f9c 0% 25%, #c9ccd4 0% 50%) 50% / 8px 8px"
                                            : "conic-gradient(#f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00)" }}
              >
                {custom ? <span className="h-full w-full" style={{ background: css(colours[r.key]) }} />
                        : <Plus size={12} strokeWidth={3} className="text-white drop-shadow" />}
              </button>
              {picking === r.key && (
                <ColourPicker value={colours[r.key]} above={index >= 2}
                              onChange={(hex) => setColours({ [r.key]: hex })} onClose={() => setPicking(null)} />
              )}
            </div>
          </Row>
        );
      })}
    </Card>
  );
}

function Swatch({ colour, selected, onClick }: { colour: string; selected: boolean; onClick: () => void }) {
  return (
    <button
      type="button"
      role="radio"
      aria-checked={selected}
      aria-label={colour}
      onClick={onClick}
      className={cx(
        "grid h-[22px] w-[22px] place-items-center rounded-full transition-transform hover:scale-110",
        selected && "ring-2 ring-ink ring-offset-2 ring-offset-panel",
      )}
      style={{ background: colour }}
    >
      {selected && <Check size={12} strokeWidth={3} className="text-on-accent" />}
    </button>
  );
}

/** A slice of the practice screen in the chosen colours. */
function Preview({ colours }: { colours: Colours }) {
  const whiteNames = ["C", "D", "E", "F", "G", "A", "B"];
  const whites = 17;                       // C3 .. E5
  const keyW = 100 / whites;
  const blackAfter = new Set([0, 1, 3, 4, 5]);  // C D F G A have a sharp
  const toPlayKey = 7 + 2;                 // E4
  const playedKey = 7 + 3;                 // F4: being played, touching the keyboard

  // [white key index, top %, height %, colour]
  const notes: [number, number, number, string][] = [
    [1, 0, 28, colours.left], [3, 18, 58, colours.left],
    [toPlayKey, 46, 38, colours.right], [playedKey, 40, 60, colours.right],
    [11, 0, 22, colours.right], [13, 12, 36, colours.right], [15, 0, 30, colours.right],
  ];
  const played = css(colours.played);

  return (
    <div className="overflow-hidden rounded-md border border-line">
      <div className="relative h-[74px] bg-stage">
        {notes.map(([k, top, h, c], i) => (
          <div key={i} className="absolute rounded-[3px]"
               style={{ left: `calc(${k * keyW}% + 3px)`, width: `calc(${keyW}% - 6px)`, top: `${top}%`, height: `${h}%`, background: css(c),
                        // the played note touching the keyboard glows all over
                        boxShadow: k === playedKey ? `0 0 14px 2px ${played}, inset 0 0 0 2px ${played}`
                                 : k === toPlayKey ? `0 0 14px 2px ${css(c)}, inset 0 0 0 2px #ECEEF3` : undefined }} />
        ))}
        <div className="absolute right-0 bottom-0 left-0 h-0.5 bg-ink/85" />
        <div className="absolute bottom-[-2px] h-1.5 rounded-full"
             style={{ left: `calc(${playedKey * keyW}% + 1px)`, width: `calc(${keyW}% - 2px)`, background: played,
                      boxShadow: `0 0 12px 3px ${played}` }} />
      </div>
      <div className="relative flex h-[52px] bg-app">
        {Array.from({ length: whites }, (_, k) => (
          <div key={k}
               className="flex-1 rounded-b-[4px] border-r border-[#C9CDD6]"
               title={whiteNames[k % 7]}
               style={{ background: k === toPlayKey ? css(colours.toPlay) : k === playedKey ? played : "#F3F4F7" }} />
        ))}
        {Array.from({ length: whites - 1 }, (_, k) =>
          blackAfter.has(k % 7) ? (
            <div key={`b${k}`} className="absolute top-0 h-[60%] rounded-b-[3px] bg-[#191C24]"
                 style={{ left: `calc(${(k + 1) * keyW}% - ${keyW * 0.3}%)`, width: `${keyW * 0.6}%` }} />
          ) : null,
        )}
      </div>
    </div>
  );
}
