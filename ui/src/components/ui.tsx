// Building blocks matching the Figma components page (Button, Toggle, Slider,
// Segment, Select, Level meter...).

import { ChevronDown, ChevronLeft, type LucideIcon } from "lucide-react";
import { useEffect, useState, type ReactNode } from "react";

export function cx(...names: (string | false | null | undefined)[]) {
  return names.filter(Boolean).join(" ");
}

// ---- Button ------------------------------------------------------------------

type Variant = "primary" | "secondary" | "ghost";

export function Button({ variant = "secondary", icon: Icon, children, onClick, disabled, className }: {
  variant?: Variant; icon?: LucideIcon; children?: ReactNode; onClick?: () => void; disabled?: boolean; className?: string;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      disabled={disabled}
      className={cx(
        "inline-flex h-[38px] items-center gap-2 rounded-md px-4 text-[14px] font-semibold whitespace-nowrap transition-colors disabled:opacity-40",
        variant === "primary" && "bg-accent text-on-accent hover:brightness-110",
        variant === "secondary" && "border border-line bg-raised text-ink hover:bg-hover",
        variant === "ghost" && "text-ink-2 hover:bg-raised hover:text-ink",
        className,
      )}
    >
      {Icon && <Icon size={18} strokeWidth={2} />}
      {children}
    </button>
  );
}

/** "< Home" back link and the page title, left side of the menu headers. */
export function BackTitle({ title, onBack, label = "Home" }: { title: string; onBack: () => void; label?: string }) {
  // Esc goes back too.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape" && !(e.target instanceof HTMLInputElement && e.target.value)) onBack();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onBack]);

  return (
    <div className="flex items-center">
      <Button variant="ghost" icon={ChevronLeft} onClick={onBack} className="-ml-2 px-2">{label}</Button>
      <div className="mx-3 h-6 w-px bg-line" />
      <h1 className="text-[18px] font-bold">{title}</h1>
    </div>
  );
}

export function Header({ children }: { children: ReactNode }) {
  return (
    <header className="flex h-16 shrink-0 items-center justify-between gap-6 border-b border-line bg-panel px-6">
      {children}
    </header>
  );
}

// ---- Cards and setting rows ------------------------------------------------------

export function Card({ title, description, children, id }: { title: string; description: string; children: ReactNode; id?: string }) {
  return (
    <section id={id} className="rounded-lg border border-line bg-panel">
      <div className="border-b border-line px-6 pt-4 pb-3">
        <h2 className="caps text-accent">{title}</h2>
        <p className="mt-1 text-ink-3">{description}</p>
      </div>
      <div className="divide-y divide-line">{children}</div>
    </section>
  );
}

export function Row({ label, description, children }: { label: string; description?: string; children: ReactNode }) {
  return (
    <div className="flex min-h-[54px] items-center justify-between gap-6 px-6 py-3.5">
      <div className="min-w-0 max-w-[290px]">
        <div className="text-ink">{label}</div>
        {description && <div className="mt-0.5 leading-5 text-ink-3">{description}</div>}
      </div>
      <div className="flex shrink-0 items-center gap-3">{children}</div>
    </div>
  );
}

// ---- Toggle ------------------------------------------------------------------

export function Toggle({ value, onChange, label }: { value: boolean; onChange: (v: boolean) => void; label: string }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={value}
      aria-label={label}
      onClick={() => onChange(!value)}
      className="flex items-center gap-2.5 text-ink-2"
    >
      <span className={cx("relative h-[18px] w-8 rounded-full transition-colors", value ? "bg-accent" : "bg-hover")}>
        <span
          className={cx(
            "absolute top-[3px] h-3 w-3 rounded-full transition-all",
            value ? "left-[17px] bg-on-accent" : "left-[3px] bg-ink-2",
          )}
        />
      </span>
      <span className="w-6 text-left">{value ? "On" : "Off"}</span>
    </button>
  );
}

// ---- Slider ------------------------------------------------------------------

export function Slider({ value, min, max, step, onChange, format, label }: {
  value: number; min: number; max: number; step: number; onChange: (v: number) => void; format: (v: number) => string; label: string;
}) {
  // While dragging, show the local value so replies from the app can't make the thumb jump.
  const [dragged, setDragged] = useState<number | null>(null);
  const shown = dragged ?? value;
  const fill = ((shown - min) / (max - min)) * 100;
  return (
    <div className="flex items-center gap-3">
      <input
        type="range"
        aria-label={label}
        min={min}
        max={max}
        step={step}
        value={shown}
        onChange={(e) => {
          const v = Number(e.target.value);
          setDragged(v);
          onChange(v);
        }}
        onPointerUp={() => setDragged(null)}
        onKeyUp={() => setDragged(null)}
        onBlur={() => setDragged(null)}
        className="slider h-4 w-[200px] cursor-pointer appearance-none bg-transparent"
        style={{ "--fill": `${fill}%` } as React.CSSProperties}
      />
      <span className="w-[60px] text-right font-mono text-[11px] text-ink-2">{format(shown)}</span>
    </div>
  );
}

// ---- Segmented ----------------------------------------------------------------

export function Segmented<T extends string>({ options, value, onChange, label }: {
  options: { value: T; label: string }[]; value: T | null; onChange: (v: T) => void; label: string;
}) {
  return (
    <div role="radiogroup" aria-label={label} className="flex rounded-md border border-line bg-raised p-1">
      {options.map((o) => (
        <button
          key={o.value}
          type="button"
          role="radio"
          aria-checked={o.value === value}
          onClick={() => onChange(o.value)}
          className={cx(
            "h-[30px] rounded-[7px] px-4 whitespace-nowrap transition-colors",
            o.value === value ? "bg-hover text-ink" : "text-ink-2 hover:text-ink",
          )}
        >
          {o.label}
        </button>
      ))}
    </div>
  );
}

// ---- Select ------------------------------------------------------------------

export function Select({ prefix, value, options, onChange, className }: {
  prefix?: string; value: string; options: { value: string; label: string }[]; onChange: (v: string) => void; className?: string;
}) {
  const current = options.find((o) => o.value === value)?.label ?? value;
  return (
    <label
      className={cx(
        "relative flex h-[38px] cursor-pointer items-center gap-2 rounded-md border border-line bg-raised pr-3 pl-3 hover:bg-hover",
        className,
      )}
    >
      {prefix && <span className="caps text-[10px] text-ink-3">{prefix}</span>}
      <span className="truncate text-ink">{current || "None"}</span>
      <ChevronDown size={16} className="ml-auto shrink-0 text-ink-2" />
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        className="absolute inset-0 cursor-pointer opacity-0"
        aria-label={prefix}
      >
        {options.map((o) => (
          <option key={o.value} value={o.value}>{o.label}</option>
        ))}
      </select>
    </label>
  );
}

// ---- Level meter --------------------------------------------------------------

/** Input level from -70 to 0 dBFS; red above -6 dB. */
export function LevelBar({ db, className }: { db: number; className?: string }) {
  const amount = Math.min(1, Math.max(0, (db + 70) / 70));
  return (
    <div className={cx("h-1.5 overflow-hidden rounded-full bg-hover", className)}>
      <div
        className={cx("h-full rounded-full transition-[width] duration-75", db > -6 ? "bg-bad" : "bg-heard")}
        style={{ width: `${amount * 100}%` }}
      />
    </div>
  );
}
