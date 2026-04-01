import type { Control } from "../types";
import { SearchableSelect } from "./SearchableSelect";

interface Props {
  controlIds: number[];
  onControlIdsChange: (ids: number[]) => void;
  controls: Control[];
}

export function ControlSequenceBuilder({ controlIds, onControlIdsChange, controls }: Props) {
  const options = controls.map((c) => ({
    value: String(c.id),
    label: `${c.code}${c.description ? ` – ${c.description}` : ""}`,
  }));

  function addControl(idStr: string) {
    if (!idStr) return;
    onControlIdsChange([...controlIds, Number(idStr)]);
  }

  function removeControl(index: number) {
    onControlIdsChange(controlIds.filter((_, i) => i !== index));
  }

  function moveUp(index: number) {
    if (index === 0) return;
    const next = [...controlIds];
    [next[index - 1], next[index]] = [next[index], next[index - 1]];
    onControlIdsChange(next);
  }

  function moveDown(index: number) {
    if (index === controlIds.length - 1) return;
    const next = [...controlIds];
    [next[index], next[index + 1]] = [next[index + 1], next[index]];
    onControlIdsChange(next);
  }

  function getControl(id: number) {
    return controls.find((c) => c.id === id);
  }

  return (
    <div>
      <div className="border rounded divide-y mb-2 min-h-[48px]" aria-label="Control sequence">
        {controlIds.length === 0 ? (
          <p className="px-3 py-3 text-sm text-gray-400">No controls added yet</p>
        ) : (
          controlIds.map((id, idx) => {
            const ctrl = getControl(id);
            const label = ctrl
              ? `${ctrl.code}${ctrl.description ? ` – ${ctrl.description}` : ""}`
              : `Control ${id}`;
            return (
              <div key={`${id}-${idx}`} className="flex items-center gap-2 px-3 py-2">
                <span className="text-xs text-gray-400 w-5 text-right">{idx + 1}.</span>
                <span className="flex-1 text-sm">{label}</span>
                <button
                  type="button"
                  onClick={() => moveUp(idx)}
                  disabled={idx === 0}
                  aria-label={`Move ${label} up`}
                  className="px-1 py-0.5 text-xs text-gray-500 hover:text-gray-700 disabled:opacity-30"
                >
                  ↑
                </button>
                <button
                  type="button"
                  onClick={() => moveDown(idx)}
                  disabled={idx === controlIds.length - 1}
                  aria-label={`Move ${label} down`}
                  className="px-1 py-0.5 text-xs text-gray-500 hover:text-gray-700 disabled:opacity-30"
                >
                  ↓
                </button>
                <button
                  type="button"
                  onClick={() => removeControl(idx)}
                  aria-label={`Remove ${label}`}
                  className="px-1 py-0.5 text-xs text-red-500 hover:text-red-700"
                >
                  ×
                </button>
              </div>
            );
          })
        )}
      </div>
      <SearchableSelect
        options={options}
        value=""
        onChange={addControl}
        placeholder="Add a control..."
      />
    </div>
  );
}
