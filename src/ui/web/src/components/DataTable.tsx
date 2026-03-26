import { useState, useMemo } from "react";

export interface ColumnDef<T> {
  key: string;
  header: string;
  accessor: (row: T) => string | number | boolean | null | undefined;
  sortable?: boolean;
}

export interface DataTableProps<T> {
  columns: ColumnDef<T>[];
  data: T[];
  isLoading?: boolean;
  pageSize?: number;
  enableSelection?: boolean;
  getItemId?: (item: T) => string | number;
  onSelectionChange?: (selected: Set<string | number>) => void;
  renderRowActions?: (item: T) => React.ReactNode;
}

type SortDir = "asc" | "desc" | null;

export function DataTable<T>({
  columns,
  data,
  isLoading = false,
  pageSize = 20,
  enableSelection = false,
  getItemId,
  onSelectionChange,
  renderRowActions,
}: DataTableProps<T>) {
  const getId = getItemId ?? ((item: T) => (item as { id: string | number }).id);

  const [filter, setFilter] = useState("");
  const [sortKey, setSortKey] = useState<string | null>(null);
  const [sortDir, setSortDir] = useState<SortDir>(null);
  const [page, setPage] = useState(0);
  const [selected, setSelected] = useState<Set<string | number>>(new Set());

  const filtered = useMemo(() => {
    if (!filter) return data;
    const lower = filter.toLowerCase();
    return data.filter((row) =>
      columns.some((col) => {
        const val = col.accessor(row);
        return typeof val === "string" && val.toLowerCase().includes(lower);
      })
    );
  }, [data, filter, columns]);

  const sorted = useMemo(() => {
    if (!sortKey || !sortDir) return filtered;
    const col = columns.find((c) => c.key === sortKey);
    if (!col) return filtered;
    return [...filtered].sort((a, b) => {
      const av = col.accessor(a);
      const bv = col.accessor(b);
      if (av == null && bv == null) return 0;
      if (av == null) return 1;
      if (bv == null) return -1;
      if (typeof av === "string" && typeof bv === "string") {
        return sortDir === "asc" ? av.localeCompare(bv) : bv.localeCompare(av);
      }
      if (av < bv) return sortDir === "asc" ? -1 : 1;
      if (av > bv) return sortDir === "asc" ? 1 : -1;
      return 0;
    });
  }, [filtered, sortKey, sortDir, columns]);

  const pageCount = Math.max(1, Math.ceil(sorted.length / pageSize));
  const currentPage = Math.min(page, pageCount - 1);
  const paginated = sorted.slice(currentPage * pageSize, (currentPage + 1) * pageSize);

  const handleSort = (key: string) => {
    if (sortKey !== key) {
      setSortKey(key);
      setSortDir("asc");
    } else if (sortDir === "asc") {
      setSortDir("desc");
    } else if (sortDir === "desc") {
      setSortDir(null);
      setSortKey(null);
    } else {
      setSortDir("asc");
    }
    setPage(0);
  };

  const handleFilterChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setFilter(e.target.value);
    setPage(0);
  };

  const updateSelection = (next: Set<string | number>) => {
    setSelected(next);
    onSelectionChange?.(next);
  };

  const toggleAll = () => {
    const allIds = sorted.map((row) => getId(row));
    const allSelected = allIds.length > 0 && allIds.every((id) => selected.has(id));
    updateSelection(allSelected ? new Set() : new Set(allIds));
  };

  const toggleRow = (id: string | number) => {
    const next = new Set(selected);
    if (next.has(id)) {
      next.delete(id);
    } else {
      next.add(id);
    }
    updateSelection(next);
  };

  if (isLoading) {
    return (
      <div aria-label="Loading" className="space-y-2">
        {Array.from({ length: 5 }).map((_, i) => (
          <div key={i} className="h-10 bg-gray-200 rounded animate-pulse" />
        ))}
      </div>
    );
  }

  const colSpan = columns.length + (enableSelection ? 1 : 0) + (renderRowActions ? 1 : 0);
  const allOnPageSelected =
    paginated.length > 0 && paginated.every((row) => selected.has(getId(row)));

  return (
    <div>
      <input
        type="text"
        placeholder="Search..."
        value={filter}
        onChange={handleFilterChange}
        className="mb-4 border rounded px-3 py-2 w-full"
        aria-label="Search"
      />
      <table className="w-full border-collapse">
        <thead>
          <tr>
            {enableSelection && (
              <th className="p-2 w-8">
                <input
                  type="checkbox"
                  checked={allOnPageSelected}
                  onChange={toggleAll}
                  aria-label="Select all"
                />
              </th>
            )}
            {columns.map((col) => (
              <th
                key={col.key}
                className="p-2 text-left border-b cursor-pointer select-none"
                onClick={() => col.sortable !== false && handleSort(col.key)}
                aria-sort={
                  sortKey === col.key
                    ? sortDir === "asc"
                      ? "ascending"
                      : "descending"
                    : undefined
                }
              >
                {col.header}
                {sortKey === col.key && sortDir === "asc" && " ↑"}
                {sortKey === col.key && sortDir === "desc" && " ↓"}
              </th>
            ))}
            {renderRowActions && <th className="p-2 w-32 border-b" />}
          </tr>
        </thead>
        <tbody>
          {paginated.length === 0 ? (
            <tr>
              <td
                colSpan={colSpan}
                className="p-4 text-center text-gray-500"
              >
                No data
              </td>
            </tr>
          ) : (
            paginated.map((row) => {
              const id = getId(row);
              return (
                <tr key={id} className="group hover:bg-gray-50">
                  {enableSelection && (
                    <td className="p-2">
                      <input
                        type="checkbox"
                        checked={selected.has(id)}
                        onChange={() => toggleRow(id)}
                        aria-label={`Select row ${id}`}
                      />
                    </td>
                  )}
                  {columns.map((col) => (
                    <td key={col.key} className="p-2 border-b">
                      {String(col.accessor(row) ?? "")}
                    </td>
                  ))}
                  {renderRowActions && (
                    <td className="p-2 border-b text-right">
                      {renderRowActions(row)}
                    </td>
                  )}
                </tr>
              );
            })
          )}
        </tbody>
      </table>
      {pageCount > 1 && (
        <div className="flex gap-2 mt-4 items-center justify-end">
          <button
            onClick={() => setPage((p) => Math.max(0, p - 1))}
            disabled={currentPage === 0}
            className="px-3 py-1 border rounded disabled:opacity-50"
            aria-label="Previous page"
          >
            Previous
          </button>
          <span aria-label="Page info">
            {currentPage + 1} / {pageCount}
          </span>
          <button
            onClick={() => setPage((p) => Math.min(pageCount - 1, p + 1))}
            disabled={currentPage === pageCount - 1}
            className="px-3 py-1 border rounded disabled:opacity-50"
            aria-label="Next page"
          >
            Next
          </button>
        </div>
      )}
    </div>
  );
}
