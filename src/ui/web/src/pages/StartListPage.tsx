import { useState } from "react";
import type { StartListEntry, Class, Runner, Club } from "../types";
import { useEntities } from "../api/hooks";

export function StartListPage() {
  const [selectedClassId, setSelectedClassId] = useState<string>("");

  const { data: startList = [], isLoading } = useEntities<StartListEntry>("startlist");
  const { data: classes = [] } = useEntities<Class>("classes");
  const { data: runners = [] } = useEntities<Runner>("runners");
  const { data: clubs = [] } = useEntities<Club>("clubs");

  const filtered = selectedClassId
    ? startList.filter((e) => e.classId === Number(selectedClassId))
    : startList;

  const sorted = [...filtered].sort((a, b) => a.startTime.localeCompare(b.startTime));

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-bold">Start List</h1>
        <button
          onClick={() => window.print()}
          className="px-4 py-2 bg-blue-600 text-white rounded text-sm font-medium hover:bg-blue-700 no-print"
        >
          Print
        </button>
      </div>

      <div className="mb-4 flex items-center gap-2 no-print">
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

      {isLoading ? (
        <div aria-label="Loading" className="p-8 text-center text-gray-500">
          Loading...
        </div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full border-collapse text-sm">
            <thead>
              <tr className="border-b bg-gray-50">
                <th className="text-left p-2 font-semibold text-gray-600 w-16">Bib</th>
                <th className="text-left p-2 font-semibold text-gray-600 w-28">Start Time</th>
                <th className="text-left p-2 font-semibold text-gray-600">Name</th>
                <th className="text-left p-2 font-semibold text-gray-600">Club</th>
                <th className="text-left p-2 font-semibold text-gray-600">Class</th>
              </tr>
            </thead>
            <tbody>
              {sorted.length === 0 ? (
                <tr>
                  <td colSpan={5} className="text-center p-8 text-gray-500">
                    No start list entries
                  </td>
                </tr>
              ) : (
                sorted.map((entry) => {
                  const runner = runners.find((r) => r.id === entry.runnerId);
                  const club =
                    runner?.clubId != null
                      ? clubs.find((c) => c.id === runner.clubId)
                      : undefined;
                  const cls = classes.find((c) => c.id === entry.classId);

                  return (
                    <tr key={entry.id} className="border-b hover:bg-gray-50">
                      <td className="p-2">{entry.bib ?? "—"}</td>
                      <td className="p-2">{entry.startTime}</td>
                      <td className="p-2 font-medium">{runner?.name ?? "—"}</td>
                      <td className="p-2 text-gray-600">{club?.name ?? "—"}</td>
                      <td className="p-2 text-gray-600">{cls?.name ?? "—"}</td>
                    </tr>
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
