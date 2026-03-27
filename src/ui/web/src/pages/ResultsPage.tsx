import { useState, Fragment, useEffect, useRef } from "react";
import type { Result, Class, Runner, Club, Control } from "../types";
import { useEntities } from "../api/hooks";

export function formatTime(seconds: number): string {
  if (seconds < 3600) {
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    return `${m}:${String(s).padStart(2, "0")}`;
  }
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  return `${h}:${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
}

export function formatTimeBehind(totalTime: number, leaderTime: number): string {
  const diff = totalTime - leaderTime;
  if (diff <= 0) return "";
  const m = Math.floor(diff / 60);
  const s = diff % 60;
  return `+${m}:${String(s).padStart(2, "0")}`;
}

const STATUS_LABELS: Record<string, string> = {
  dns: "DNS",
  dnf: "DNF",
  dsq: "DSQ",
  mp: "MP",
};

const POLL_INTERVALS = [
  { label: "5s", value: 5000 },
  { label: "10s", value: 10000 },
  { label: "30s", value: 30000 },
  { label: "60s", value: 60000 },
] as const;

export function ResultsPage() {
  const [selectedClassId, setSelectedClassId] = useState<string>("");
  const [expandedIds, setExpandedIds] = useState<Set<number>>(new Set());
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [pollInterval, setPollInterval] = useState(10000);
  const [lastRefreshedAt, setLastRefreshedAt] = useState<Date | null>(null);
  const [highlightedIds, setHighlightedIds] = useState<Set<number>>(new Set());
  const prevResultsRef = useRef<Map<number, string>>(new Map());
  const wasFetchingRef = useRef(false);

  const {
    data: results = [],
    isLoading: resultsLoading,
    isFetching: resultsFetching,
  } = useEntities<Result>("results", {
    refetchInterval: autoRefresh ? pollInterval : false,
  });
  const { data: classes = [] } = useEntities<Class>("classes");
  const { data: runners = [] } = useEntities<Runner>("runners");
  const { data: clubs = [] } = useEntities<Club>("clubs");
  const { data: controls = [] } = useEntities<Control>("controls");

  // Update last refreshed time when a fetch completes
  useEffect(() => {
    if (!resultsFetching && wasFetchingRef.current) {
      setLastRefreshedAt(new Date());
    }
    wasFetchingRef.current = resultsFetching;
  }, [resultsFetching]);

  // Detect changed rows and briefly highlight them
  useEffect(() => {
    if (!results.length) return;
    const changed = new Set<number>();
    for (const r of results) {
      const key = JSON.stringify({ totalTime: r.totalTime, status: r.status, position: r.position });
      const prev = prevResultsRef.current.get(r.id);
      if (prev !== undefined && prev !== key) {
        changed.add(r.id);
      }
      prevResultsRef.current.set(r.id, key);
    }
    if (changed.size === 0) return;
    setHighlightedIds((prev) => new Set([...prev, ...changed]));
    const timer = setTimeout(() => {
      setHighlightedIds((prev) => {
        const next = new Set(prev);
        for (const id of changed) next.delete(id);
        return next;
      });
    }, 2000);
    return () => clearTimeout(timer);
  }, [results]);

  const filtered = selectedClassId
    ? results.filter((r) => r.classId === Number(selectedClassId))
    : results;

  const sorted = [...filtered].sort((a, b) => {
    if (a.status === "ok" && b.status === "ok") {
      return (a.position ?? 999) - (b.position ?? 999);
    }
    if (a.status === "ok") return -1;
    if (b.status === "ok") return 1;
    const nameA = runners.find((r) => r.id === a.runnerId)?.name ?? "";
    const nameB = runners.find((r) => r.id === b.runnerId)?.name ?? "";
    return nameA.localeCompare(nameB);
  });

  const leaderTimes = new Map<number, number>();
  for (const result of sorted) {
    if (result.status === "ok" && result.totalTime != null && !leaderTimes.has(result.classId)) {
      leaderTimes.set(result.classId, result.totalTime);
    }
  }

  function toggleExpand(id: number) {
    setExpandedIds((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  }

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold mb-6">Results</h1>

      <div className="mb-4 flex items-center gap-4 flex-wrap">
        <div className="flex items-center gap-2">
          <label htmlFor="class-selector" className="text-sm font-medium text-gray-700">
            Class:
          </label>
          <select
            id="class-selector"
            value={selectedClassId}
            onChange={(e) => setSelectedClassId(e.target.value)}
            className="border rounded px-2 py-1 text-sm"
          >
            <option value="">All classes</option>
            {classes.map((c) => (
              <option key={c.id} value={String(c.id)}>
                {c.name}
              </option>
            ))}
          </select>
        </div>

        <label className="flex items-center gap-1.5 text-sm font-medium text-gray-700 cursor-pointer">
          <input
            type="checkbox"
            checked={autoRefresh}
            onChange={(e) => setAutoRefresh(e.target.checked)}
            aria-label="Auto-refresh"
          />
          Auto-refresh
        </label>

        {autoRefresh && (
          <select
            value={pollInterval}
            onChange={(e) => setPollInterval(Number(e.target.value))}
            className="border rounded px-2 py-1 text-sm"
            aria-label="Refresh interval"
          >
            {POLL_INTERVALS.map(({ label, value }) => (
              <option key={value} value={value}>
                {label}
              </option>
            ))}
          </select>
        )}

        <div className="flex items-center gap-2 text-sm ml-auto">
          <span aria-label="Auto-refresh status">
            {autoRefresh ? (
              <span className="flex items-center gap-1 text-green-600">
                <span className="inline-block w-2 h-2 rounded-full bg-green-500 animate-pulse" />
                Auto-refresh active
              </span>
            ) : (
              <span className="text-gray-400">Auto-refresh off</span>
            )}
          </span>
          {lastRefreshedAt && (
            <span aria-label="Last refreshed" className="text-gray-500">
              · Last: {lastRefreshedAt.toLocaleTimeString()}
            </span>
          )}
        </div>
      </div>

      {resultsLoading ? (
        <div aria-label="Loading" className="p-8 text-center text-gray-500">
          Loading...
        </div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full border-collapse text-sm">
            <thead>
              <tr className="border-b bg-gray-50">
                <th className="text-left p-2 font-semibold text-gray-600 w-12">Pos</th>
                <th className="text-left p-2 font-semibold text-gray-600">Name</th>
                <th className="text-left p-2 font-semibold text-gray-600">Club</th>
                <th className="text-right p-2 font-semibold text-gray-600">Time</th>
                <th className="text-right p-2 font-semibold text-gray-600">+Time</th>
                <th className="text-center p-2 font-semibold text-gray-600 w-16">Status</th>
              </tr>
            </thead>
            <tbody>
              {sorted.length === 0 ? (
                <tr>
                  <td colSpan={6} className="text-center p-8 text-gray-500">
                    No results
                  </td>
                </tr>
              ) : (
                sorted.map((result) => {
                  const runner = runners.find((r) => r.id === result.runnerId);
                  const club =
                    runner?.clubId != null
                      ? clubs.find((c) => c.id === runner.clubId)
                      : undefined;
                  const leaderTime = leaderTimes.get(result.classId);
                  const isExpanded = expandedIds.has(result.id);
                  const isHighlighted = highlightedIds.has(result.id);

                  return (
                    <Fragment key={result.id}>
                      <tr
                        onClick={() => toggleExpand(result.id)}
                        className={`border-b cursor-pointer transition-colors duration-1000 ${
                          isHighlighted
                            ? "bg-yellow-100 hover:bg-yellow-100"
                            : "hover:bg-gray-50"
                        }`}
                        aria-expanded={isExpanded}
                      >
                        <td className="p-2">
                          {result.status === "ok" ? (result.position ?? "—") : "—"}
                        </td>
                        <td className="p-2 font-medium">{runner?.name ?? "—"}</td>
                        <td className="p-2 text-gray-600">{club?.name ?? "—"}</td>
                        <td className="p-2 text-right">
                          {result.totalTime != null ? formatTime(result.totalTime) : "—"}
                        </td>
                        <td className="p-2 text-right text-gray-500">
                          {result.status === "ok" &&
                          result.totalTime != null &&
                          leaderTime != null
                            ? formatTimeBehind(result.totalTime, leaderTime)
                            : ""}
                        </td>
                        <td className="p-2 text-center">
                          {result.status !== "ok" ? (
                            <span className="px-1.5 py-0.5 bg-gray-100 text-gray-700 rounded text-xs">
                              {STATUS_LABELS[result.status] ?? result.status.toUpperCase()}
                            </span>
                          ) : null}
                        </td>
                      </tr>
                      {isExpanded && result.splits.length > 0 && (
                        <tr className="bg-blue-50">
                          <td colSpan={6} className="px-4 py-3">
                            <div
                              className="text-xs font-semibold text-gray-500 mb-1"
                              aria-label="Split times"
                            >
                              Split times
                            </div>
                            <div className="flex flex-wrap gap-4">
                              {result.splits.map((split, idx) => {
                                const control = controls.find((c) => c.id === split.controlId);
                                return (
                                  <span key={idx} className="text-sm">
                                    <span className="font-medium">
                                      {control?.code ?? split.controlId}
                                    </span>
                                    {": "}
                                    <span>{formatTime(split.time)}</span>
                                  </span>
                                );
                              })}
                            </div>
                          </td>
                        </tr>
                      )}
                    </Fragment>
                  );
                })
              )}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
