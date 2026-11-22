import { Copy, FolderOpen, Music, Play, Plus, Search, Trash2, Star } from "lucide-react";
import { useMemo, useState, type DragEvent } from "react";
import { BackTitle, Button, Header, Segmented, Select, cx } from "../components/ui";
import { duration, playedAgo } from "../format";
import { useStore } from "../store";
import type { Colours, Piece } from "../types";

type Sort = "Favourite" |"lastPlayed" | "title" | "length" | "best" | "added";
const sorts: { value: Sort; label: string }[] = [
  { value: "Favourite", label: "Favourite" },
  { value: "lastPlayed", label: "Last played" },
  { value: "title", label: "Title" },
  { value: "length", label: "Length" },
  { value: "best", label: "Best" },
  { value: "added", label: "Recently added" },
];

const columns = "grid-cols-[minmax(0,1fr)_184px_104px_104px_164px_176px]";

/** Figma frame "Library — 1440". */
export function Library() {
  const { state, act, back, backLabel } = useStore();
  const [query, setQuery] = useState("");
  const [tab, setTab] = useState<"all" | "recent">("all");
  const [sort, setSort] = useState<Sort>("lastPlayed");
  const [dragging, setDragging] = useState(false);

  const pieces = useMemo(() => {
    const q = query.trim().toLowerCase();
    let list = state.library.filter((p) => !q || p.title.toLowerCase().includes(q) || p.file.toLowerCase().includes(q));
    if (tab === "recent") list = list.filter((p) => p.lastPlayed > 0);
    const by: Record<Sort, (a: Piece, b: Piece) => number> = {
      Favourite: (a, b) => (b.favourite ? 1 : 0) - (a.favourite ? 1 : 0) || b.lastPlayed - a.lastPlayed || a.title.localeCompare(b.title),
      lastPlayed: (a, b) => b.lastPlayed - a.lastPlayed || a.title.localeCompare(b.title),
      title: (a, b) => a.title.localeCompare(b.title),
      length: (a, b) => a.length - b.length,
      best: (a, b) => b.best - a.best,
      added: (a, b) => b.added - a.added,
    };
    return [...list].sort(tab === "recent" ? by.lastPlayed : by[sort]);
  }, [state.library, query, tab, sort]);
  const missing = state.library.filter((p) => p.missing).length;
  const duplicates = state.library.filter((p) => p.duplicate).length;

  const onDrop = async (e: DragEvent) => {
    e.preventDefault();
    setDragging(false);
    const files = [...e.dataTransfer.files].filter((f) => /\.midi?$/i.test(f.name));
    if (files.length === 0) return;
    const payload = await Promise.all(files.map(async (f) => ({ name: f.name, data: await toBase64(f) })));
    act("importFiles", payload);
  };

  return (
    <div
      className="flex min-h-0 flex-1 flex-col"
      onDragOver={(e) => { e.preventDefault(); setDragging(true); }}
      onDragLeave={(e) => { if (e.currentTarget === e.target) setDragging(false); }}
      onDrop={onDrop}
    >
      <Header>
        <BackTitle title="Library" onBack={back} label={backLabel} />
        <label className="flex h-[38px] w-[360px] items-center gap-2.5 rounded-md border border-line bg-raised px-3 focus-within:border-accent">
          <Search size={18} className="text-ink-3" />
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search pieces"
            className="w-full bg-transparent text-ink outline-none placeholder:text-ink-3"
          />
        </label>
        <Button variant="primary" icon={Plus} onClick={() => act("addPieces")}>Add pieces</Button>
      </Header>

      <main className="flex-1 overflow-y-auto">
        <div className="mx-auto max-w-[1248px] px-6 pt-8 pb-10">
          <div className="flex items-center justify-between">
            <Segmented
              label="Show"
              value={tab}
              onChange={setTab}
              options={[{ value: "all", label: "All pieces" }, { value: "recent", label: "Recent" }]}
            />
            <div className="flex items-center gap-4">
              {duplicates > 0 && (
                <button
                  type="button"
                  title="Remove the extra copies of identical pieces, keeping the oldest one with the best scores and favourites (no files are deleted)"
                  onClick={() => act("removeDuplicates")}
                  className="flex h-[38px] items-center gap-2 rounded-md border border-line px-3 text-ink-2 hover:border-bad hover:text-bad"
                >
                  <Copy size={16} />
                  Remove {duplicates} {duplicates === 1 ? "duplicate" : "duplicates"}
                </button>
              )}
              {missing > 0 && (
                <button
                  type="button"
                  title="Remove pieces whose file was moved or deleted (no files are deleted)"
                  onClick={() => act("removeMissing")}
                  className="flex h-[38px] items-center gap-2 rounded-md border border-line px-3 text-ink-2 hover:border-bad hover:text-bad"
                >
                  <Trash2 size={16} />
                  Remove {missing} unavailable
                </button>
              )}
              <span className="caps text-ink-3">{pieces.length} {pieces.length === 1 ? "piece" : "pieces"}</span>
              {tab === "all" && (
                <Select prefix="Sort" value={sort} options={sorts} onChange={(v) => setSort(v as Sort)} className="w-[172px]" />
              )}
            </div>
          </div>

          <div className={cx("caps mt-7 grid px-4 text-ink-3", columns)}>
            <span className="pl-[64px]">Piece</span>
            <span>Hands</span>
            <span>Length</span>
            <span>Bars</span>
            <span>Best</span>
            <span>Last played</span>
          </div>

          <div className="mt-3 flex flex-col gap-2">
            {pieces.map((p) => (
              <PieceRow key={p.id} piece={p} colours={state.settings.colours} />
            ))}
            {pieces.length === 0 && (
              <div className="py-10 text-center text-ink-3">
                {state.library.length === 0 ? "No pieces yet." : tab === "recent" ? "Nothing played yet." : "No piece matches your search."}
              </div>
            )}
          </div>

          <button
            type="button"
            onClick={() => act("addPieces")}
            className={cx(
              "mt-6 flex h-[56px] w-full items-center justify-center gap-3 rounded-lg border border-dashed text-ink-2 transition-colors",
              dragging ? "border-accent bg-accent/10 text-ink" : "border-line-strong hover:border-ink-3",
            )}
          >
            <FolderOpen size={20} />
            Drop MIDI files here to add them to the library
          </button>
        </div>
      </main>
    </div>
  );
}

function PieceRow({ piece, colours }: { piece: Piece; colours: Colours }) {
  const { act } = useStore();
  const open = () => !piece.missing && act("openPiece", piece.id);

  return (
    <div
      role="button"
      tabIndex={0}
      onClick={open}
      onKeyDown={(e) => e.key === "Enter" && open()}
      className={cx(
        "group grid h-16 items-center rounded-md border border-line bg-panel px-4 transition-colors",
        columns,
        piece.missing ? "opacity-60" : "cursor-pointer hover:border-accent hover:bg-raised",
      )}
    >
      <div className="flex min-w-0 items-center gap-6">
        <button
          type="button"
          title={piece.favourite ? "Remove from favourites" : "Mark as favourite"}
          onClick={(e) => { e.stopPropagation(); act("toggleFavourite", piece.id); }}
          className={cx(
            "grid h-9 w-9 place-items-center rounded-full transition-colors",
            piece.favourite ? "text-accent" : "text-ink-3 opacity-0 group-hover:opacity-100",
          )}
        >
          <Star size={piece.favourite ? 18 : 16} fill={piece.favourite ? "currentColor" : "none"} strokeWidth={piece.favourite ? 0 : 2}/>
        </button>
        <div className="grid h-10 w-10 shrink-0 place-items-center rounded-[8px] bg-raised text-ink-2 group-hover:bg-hover">
          <Music size={18} />
        </div>
        <div className="min-w-0">
          <div className="flex min-w-0 items-center gap-2">
            <span className="truncate text-ink">{piece.title}</span>
            {piece.duplicate && (
              <span className="caps shrink-0 rounded-full bg-raised px-2 py-[2px] text-[10px] text-ink-3" title="Same MIDI data as another piece in the library">
                Duplicate
              </span>
            )}
          </div>
          <div className={cx("truncate font-mono text-[11px]", piece.missing ? "text-bad" : "text-ink-3")}>
            {piece.missing ? `${piece.file} · file not found` : piece.file}
          </div>
        </div>
      </div>

      <div className="flex gap-1">
        <HandChip label="Right" colour={colours.right} />
        {piece.hands > 1 && <HandChip label="Left" colour={colours.left} />}
      </div>
      <span className="font-mono text-[11px] text-ink-2">{duration(piece.length)}</span>
      <span className="font-mono text-[11px] text-ink-2">{piece.bars > 0 ? `${piece.bars} bars` : "-"}</span>
      <Best value={piece.best} />

      <div className="flex items-center justify-between">
        <span className="text-ink-2">{playedAgo(piece.lastPlayed)}</span>
        <div className="flex items-center gap-1 opacity-0 transition-opacity group-hover:opacity-100 group-focus-visible:opacity-100">
          <button
            type="button"
            title="Remove from the library (the file is kept)"
            onClick={(e) => { e.stopPropagation(); act("removePiece", piece.id); }}
            className="grid h-9 w-9 place-items-center rounded-full text-ink-3 hover:bg-hover hover:text-bad"
          >
            <Trash2 size={16} />
          </button>
          {!piece.missing && (
            <span className="grid h-9 w-9 place-items-center rounded-full bg-accent text-on-accent">
              <Play size={18} className="ml-0.5" />
            </span>
          )}
        </div>
      </div>
    </div>
  );
}

function HandChip({ label, colour }: { label: string; colour: string }) {
  return (
    <span className="caps rounded-full px-3 py-[3px] text-[10px] text-on-accent/80" style={{ background: colour }}>
      {label}
    </span>
  );
}

function Best({ value }: { value: number }) {
  return (
    <div className="flex items-center gap-2.5">
      <div className="h-1 w-16 overflow-hidden rounded-full bg-hover">
        {value >= 0 && (
          <div className={cx("h-full rounded-full", value >= 60 ? "bg-good" : "bg-accent")} style={{ width: `${value}%` }} />
        )}
      </div>
      <span className="font-mono text-[11px] text-ink">{value >= 0 ? `${value}%` : "-"}</span>
    </div>
  );
}

function toBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result).replace(/^data:[^,]*,/, ""));
    reader.onerror = () => reject(reader.error);
    reader.readAsDataURL(file);
  });
}
