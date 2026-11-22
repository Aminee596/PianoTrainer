import { useEffect, useState } from "react";
import wallpaper from "../assets/wallpaper.webp";

export function Start({ onContinue }: { onContinue: () => void }) {
  const [leaving, setLeaving] = useState(false);

  useEffect(() => {
    const go = (e: KeyboardEvent | PointerEvent) => {
      if (e instanceof KeyboardEvent && (e.repeat || ["Shift", "Control", "Alt", "Meta"].includes(e.key))) return;
      setLeaving(true);
      setTimeout(onContinue, 300);
    };
    window.addEventListener("keydown", go);
    window.addEventListener("pointerdown", go);
    return () => { window.removeEventListener("keydown", go); window.removeEventListener("pointerdown", go); };
  }, [onContinue]);

  return (
    <div
      className={`fixed inset-0 z-50 flex flex-col items-center justify-end bg-cover bg-center pb-24 transition-opacity duration-300 ${leaving ? "opacity-0" : "opacity-100"}`}
      style={{ backgroundImage: `url(${wallpaper})` }}
    >
      <h1 className="text-5xl font-semibold text-ink">Piano Trainer</h1>
      <p className="mt-4 animate-pulse text-ink-2">Press any key to continue</p>
    </div>
  );
}