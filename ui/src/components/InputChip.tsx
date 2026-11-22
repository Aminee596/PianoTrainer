import { Mic } from "lucide-react";
import { kHz, shortDevice } from "../format";
import { useMeter, useStore } from "../store";
import { LevelBar } from "./ui";

/** Figma "Input chip": device, sample rate, what is heard, level. Opens the audio settings. */
export function InputChip() {
  const { state, go } = useStore();
  const meter = useMeter();
  const { audio } = state;

  const status = !audio.open ? "No input" : meter.note ? `Hearing ${meter.note}` : "Ready";
  return (
    <button
      type="button"
      onClick={() => go("settings", "audio")}
      title="Audio input. Click to choose the microphone or line-in."
      className="flex h-[52px] w-[264px] items-center gap-2.5 rounded-md border border-line bg-raised px-3 text-left hover:bg-hover"
    >
      <Mic size={20} className="shrink-0 text-heard" />
      <div className="min-w-0 flex-1">
        <div className="truncate font-semibold">
          {audio.open ? `${shortDevice(audio.input)} · ${kHz(audio.sampleRate)} kHz` : "No input"}
        </div>
        <div className={audio.open ? "text-[11px] font-semibold text-good" : "text-[11px] font-semibold text-bad"}>{status}</div>
      </div>
      <LevelBar db={audio.open ? meter.levelDb : -100} className="w-24 shrink-0" />
    </button>
  );
}
