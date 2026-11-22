import { Library as LibraryIcon, Music, Play, SlidersHorizontal, type LucideIcon } from "lucide-react";
import { InputChip } from "../components/InputChip";
import { Button, Header } from "../components/ui";
import { useStore } from "../store";
import type { Colours, Current } from "../types";
import wallpaper from "../assets/wallpaper.webp";

/** Figma frame "Home — 1440". */
export function Home() {
  const { state, go } = useStore();
  const { current, library, settings } = state;

  return (
    <div className="relative isolate flex min-h-0 flex-1 flex-col">
      <div
        aria-hidden
        className="pointer-events-none absolute inset-0 -z-10 bg-cover bg-center opacity-20"
        style={{ backgroundImage: `url(${wallpaper})` }}
      />
      <Header>
        <div className="flex items-center gap-3">
          <div className="grid h-9 w-9 place-items-center rounded-md bg-accent text-on-accent">
            <Music size={20} strokeWidth={2.2} />
          </div>
          <div>
            <div className="text-[18px] leading-6 font-semibold">Piano Trainer</div>
            <div className="text-ink-3">Wait-mode practice for real pianos</div>
          </div>
        </div>
        <InputChip />
      </Header>

      <main className="flex-1 overflow-y-auto">
        <div className="mx-auto max-w-[1248px] px-6 pt-14 pb-10">
          <div className="caps text-ink-3">{current ? "Welcome back" : "Welcome"}</div>
          <h1 className="mt-1 text-[28px] font-extrabold tracking-[-0.02em]">What would you like to practice?</h1>

          {current ? <ContinueCard current={current} colours={settings.colours} /> : <StartCard />}

          <div className="mt-6 grid grid-cols-2 gap-6">
            <Tile
              icon={LibraryIcon}
              title="Library"
              text="Your pieces: open MIDI files, see what you played last and how well."
              footer={`${library.length} ${library.length === 1 ? "piece" : "pieces"}`}
              accentFooter
              onClick={() => go("library")}
            />
            <Tile
              icon={SlidersHorizontal}
              title="Settings"
              text="Detection, chords, octave shift, falling speed, colours and display options."
              footer="Detection · Display · Colours"
              onClick={() => go("settings")}
            />
          </div>

          <p className="mt-8 text-ink-3">Tip: plug the keyboard's headphone jack into your PC's line-in for the cleanest detection.</p>
        </div>
      </main>
    </div>
  );
}

function ContinueCard({ current, colours }: { current: Current; colours: Colours }) {
  const { act } = useStore();
  const meta = [current.hand, `step ${current.step} of ${current.steps}`, current.bars > 0 && `bar ${current.bar} of ${current.bars}`]
    .filter(Boolean)
    .join(" · ");

  return (
    <section className="mt-6 flex items-center gap-6 rounded-lg border border-line bg-panel p-6">
      <Thumbnail current={current} colours={colours} />
      <div className="min-w-0 flex-1">
        <div className="caps text-accent">{current.finished ? "Finished · play it again" : "Continue practicing"}</div>
        <div className="mt-1.5 truncate text-[20px] font-semibold">{current.title}</div>
        <div className="mt-1 text-ink-2">{meta}</div>
        <div className="mt-3 h-1 w-[400px] max-w-full overflow-hidden rounded-full bg-hover">
          <div className="h-full rounded-full bg-accent" style={{ width: `${current.progress * 100}%` }} />
        </div>
      </div>
      <Stat label="Accuracy" value={current.accuracy >= 0 ? `${current.accuracy}%` : "-"} />
      <Stat label="Streak" value={String(current.streak)} />
      <Button variant="primary" icon={Play} onClick={() => act("continuePractice")} className="ml-2">
        Continue
      </Button>
    </section>
  );
}

function StartCard() {
  const { go } = useStore();
  return (
    <section className="mt-6 flex items-center gap-6 rounded-lg border border-line bg-panel p-6">
      <div className="grid h-[120px] w-[168px] shrink-0 place-items-center rounded-md bg-stage text-ink-3">
        <Music size={36} />
      </div>
      <div className="flex-1">
        <div className="caps text-accent">Start here</div>
        <div className="mt-1.5 text-[20px] font-semibold">Pick a piece to practice</div>
        <div className="mt-1 text-ink-2">Open a MIDI file from your library, or add one.</div>
      </div>
      <Button variant="primary" icon={LibraryIcon} onClick={() => go("library")}>Open library</Button>
    </section>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <div className="w-[68px]">
      <div className="caps text-ink-3">{label}</div>
      <div className="mt-1 font-mono text-[22px] font-bold tracking-[-0.02em]">{value}</div>
    </div>
  );
}

/** Falling notes around the playhead, a tiny version of the practice screen. */
function Thumbnail({ current, colours }: { current: Current; colours: Colours }) {
  const W = 168, H = 120, hitY = 104, now = 0.5 / 4.5;
  const colourFor = (hand: string, start: number) => {
    if (hand === "other") return "#2B3140";
    if (start <= now + 0.02) return colours.toPlay;
    return hand === "left" ? colours.left : colours.right;
  };
  return (
    <div className="relative shrink-0 overflow-hidden rounded-md bg-stage" style={{ width: W, height: H }}>
      {current.preview.map((n, i) => {
        const x = 18 + n.pitch * (W - 46);
        const bottom = hitY - ((n.start - now) / (1 - now)) * hitY;
        const height = Math.max(8, (n.length / (1 - now)) * hitY);
        const top = Math.max(0, bottom - height);
        if (bottom <= 0) return null;
        return (
          <div
            key={i}
            className="absolute rounded-[3px]"
            style={{ left: x, width: 10, top, height: Math.min(bottom, hitY) - top, background: colourFor(n.hand, n.start) }}
          />
        );
      })}
      <div className="absolute right-0 left-0 h-0.5 bg-ink" style={{ top: hitY }} />
    </div>
  );
}

function Tile({ icon: Icon, title, text, footer, accentFooter, onClick }: {
  icon: LucideIcon; title: string; text: string; footer: string; accentFooter?: boolean; onClick: () => void;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="group flex h-[232px] flex-col rounded-lg border border-line bg-panel p-6 text-left transition hover:border-accent hover:bg-raised hover:shadow-[0_0_32px_-8px_rgb(70_194_255/0.35)]"
    >
      <div className="grid h-12 w-12 place-items-center rounded-md bg-accent text-on-accent">
        <Icon size={24} strokeWidth={2} />
      </div>
      <div className="mt-6 text-[20px] font-semibold">{title}</div>
      <div className="mt-1.5 leading-5 text-ink-2">{text}</div>
      <div className={`caps mt-auto ${accentFooter ? "text-accent" : "text-ink-3"}`}>{footer}</div>
    </button>
  );
}
