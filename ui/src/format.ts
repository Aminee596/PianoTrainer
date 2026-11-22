export function duration(seconds: number): string {
  const s = Math.round(seconds);
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
}

export function playedAgo(ms: number, now = Date.now()): string {
  if (!ms) return "Never";
  const startOfToday = new Date(now).setHours(0, 0, 0, 0);
  const days = Math.ceil((startOfToday - ms) / 86_400_000);
  if (ms >= startOfToday) return "Today";
  if (days <= 1) return "Yesterday";
  if (days < 7) return `${days} days ago`;
  if (days < 14) return "Last week";
  if (days < 31) return `${Math.floor(days / 7)} weeks ago`;
  return new Date(ms).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" });
}

/** "Line In (Realtek Audio)" -> "Line In" */
export function shortDevice(name: string): string {
  const cut = name.indexOf(" (");
  return cut > 0 ? name.slice(0, cut) : name;
}

export function kHz(rate: number): string {
  const k = rate / 1000;
  return Math.abs(k - Math.round(k)) < 0.05 ? String(Math.round(k)) : k.toFixed(1);
}

/** WASAPI type names as the Settings screen shows them. */
export function driverLabel(type: string): string {
  switch (type) {
    case "Windows Audio": return "Shared";
    case "Windows Audio (Low Latency Mode)": return "Low latency";
    case "Windows Audio (Exclusive Mode)": return "Exclusive";
    default: return type;
  }
}

export function octaveLabel(shift: number): string {
  return shift === 0 ? "As heard" : `${shift > 0 ? "+" : ""}${shift} oct`;
}
