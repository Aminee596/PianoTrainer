// Full colour picker: shade square (white to black), hue (red to purple),
// opacity, and a hex field. Colours are "#RRGGBB" or "#RRGGBBAA".

import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from "react";

interface Hsva { h: number; s: number; v: number; a: number }  // h 0..360, s/v/a 0..1

export function parseHex(hex: string): { r: number; g: number; b: number; a: number } | null {
  const m = /^#?([0-9a-f]{6})([0-9a-f]{2})?$/i.exec(hex.trim());
  if (!m) return null;
  const n = parseInt(m[1], 16);
  return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255, a: m[2] ? parseInt(m[2], 16) / 255 : 1 };
}

function toHex({ h, s, v, a }: Hsva): string {
  const f = (k: number) => {
    const x = (k + h / 60) % 6;
    return Math.round(255 * (v - v * s * Math.max(0, Math.min(x, 4 - x, 1))));
  };
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  const alpha = Math.round(a * 255);
  return ("#" + hex(f(5)) + hex(f(3)) + hex(f(1)) + (alpha < 255 ? hex(alpha) : "")).toUpperCase();
}

function toHsva(hex: string, keepHue = 0): Hsva {
  const c = parseHex(hex) ?? { r: 70, g: 194, b: 255, a: 1 };
  const r = c.r / 255, g = c.g / 255, b = c.b / 255;
  const max = Math.max(r, g, b), d = max - Math.min(r, g, b);
  let h = keepHue;
  if (d > 0) {
    if (max === r) h = 60 * (((g - b) / d + 6) % 6);
    else if (max === g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
  }
  return { h, s: max === 0 ? 0 : d / max, v: max, a: c.a };
}

/** CSS colour for any of our hex strings. */
export function css(hex: string): string {
  const c = parseHex(hex);
  return c ? `rgba(${c.r}, ${c.g}, ${c.b}, ${c.a})` : hex;
}

const checker = "repeating-conic-gradient(#8a8f9c 0% 25%, #c9ccd4 0% 50%) 50% / 10px 10px";

export function ColourPicker({ value, onChange, onClose, above = false }: {
  value: string; onChange: (hex: string) => void; onClose: () => void; above?: boolean;
}) {
  const [hsva, setHsva] = useState<Hsva>(() => toHsva(value));
  const [text, setText] = useState(value);
  const box = useRef<HTMLDivElement>(null);
  const lastSent = useRef(0);

  // Close on a click outside, or Esc (without also leaving the Settings page).
  useEffect(() => {
    const onDown = (e: MouseEvent) => { if (box.current && !box.current.contains(e.target as Node)) onClose(); };
    const onKey = (e: KeyboardEvent) => { if (e.key === "Escape") { e.stopPropagation(); onClose(); } };
    window.addEventListener("pointerdown", onDown);
    window.addEventListener("keydown", onKey, true);
    return () => {
      window.removeEventListener("pointerdown", onDown);
      window.removeEventListener("keydown", onKey, true);
    };
  }, [onClose]);

  // Live while dragging (at most ~20 updates a second), and always the final value.
  const update = (next: Hsva, final = false) => {
    setHsva(next);
    const hex = toHex(next);
    setText(hex);
    const now = performance.now();
    if (final || now - lastSent.current > 50) {
      lastSent.current = now;
      onChange(hex);
    }
  };

  const drag = (e: ReactPointerEvent<HTMLDivElement>, apply: (x: number, y: number) => Hsva) => {
    const el = e.currentTarget;
    el.setPointerCapture(e.pointerId);
    const at = (ev: { clientX: number; clientY: number }, final = false) => {
      const r = el.getBoundingClientRect();
      const x = Math.min(1, Math.max(0, (ev.clientX - r.left) / r.width));
      const y = Math.min(1, Math.max(0, (ev.clientY - r.top) / r.height));
      update(apply(x, y), final);
    };
    at(e);
    const move = (ev: PointerEvent) => at(ev);
    const up = (ev: PointerEvent) => {
      at(ev, true);
      el.removeEventListener("pointermove", move);
      el.removeEventListener("pointerup", up);
    };
    el.addEventListener("pointermove", move);
    el.addEventListener("pointerup", up);
  };

  const solid = toHex({ ...hsva, a: 1 });
  const hue = toHex({ h: hsva.h, s: 1, v: 1, a: 1 });

  return (
    <div ref={box} className={`absolute right-0 z-40 w-[252px] rounded-lg border border-line-strong bg-raised p-4 shadow-2xl shadow-black/60 ${above ? "bottom-full mb-2" : "top-full mt-2"}`}>
      {/* Saturation (left to right) and brightness (top to bottom) */}
      <div
        className="relative h-[150px] cursor-crosshair rounded-md"
        style={{ background: `linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, ${hue})` }}
        onPointerDown={(e) => drag(e, (x, y) => ({ ...hsva, s: x, v: 1 - y }))}
      >
        <div
          className="pointer-events-none absolute h-3.5 w-3.5 -translate-x-1/2 -translate-y-1/2 rounded-full border-2 border-white shadow"
          style={{ left: `${hsva.s * 100}%`, top: `${(1 - hsva.v) * 100}%`, background: solid }}
        />
      </div>

      <Strip
        label="Hue"
        background="linear-gradient(to right, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00)"
        position={hsva.h / 360}
        thumb={hue}
        onPointerDown={(e) => drag(e, (x) => ({ ...hsva, h: Math.min(359.9, x * 360) }))}
      />
      <Strip
        label="Opacity"
        background={`linear-gradient(to right, transparent, ${solid}), ${checker}`}
        position={hsva.a}
        thumb={css(toHex(hsva))}
        onPointerDown={(e) => drag(e, (x) => ({ ...hsva, a: Math.round(x * 100) / 100 }))}
      />

      <div className="mt-4 flex items-center gap-3">
        <div className="h-8 w-8 shrink-0 overflow-hidden rounded-md" style={{ background: checker }}>
          <div className="h-full w-full" style={{ background: css(toHex(hsva)) }} />
        </div>
        <input
          value={text}
          spellCheck={false}
          onChange={(e) => {
            setText(e.target.value);
            const c = parseHex(e.target.value);
            if (c) update(toHsva(e.target.value, hsva.h), true);
          }}
          className="h-8 w-full rounded-md border border-line bg-panel px-2 font-mono text-[12px] text-ink uppercase outline-none focus:border-accent"
          aria-label="Hex colour"
        />
        <span className="w-10 shrink-0 text-right font-mono text-[11px] text-ink-3">{Math.round(hsva.a * 100)}%</span>
      </div>
    </div>
  );
}

function Strip({ label, background, position, thumb, onPointerDown }: {
  label: string; background: string; position: number; thumb: string;
  onPointerDown: (e: ReactPointerEvent<HTMLDivElement>) => void;
}) {
  return (
    <div className="mt-4">
      <div className="caps mb-1.5 text-[10px] text-ink-3">{label}</div>
      <div className="relative h-3 cursor-pointer rounded-full" style={{ background }} onPointerDown={onPointerDown}>
        <div
          className="pointer-events-none absolute top-1/2 h-4 w-4 -translate-x-1/2 -translate-y-1/2 rounded-full border-2 border-white shadow"
          style={{ left: `${position * 100}%`, background: thumb }}
        />
      </div>
    </div>
  );
}
