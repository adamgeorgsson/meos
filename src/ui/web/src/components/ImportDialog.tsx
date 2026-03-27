import { useState, useRef } from "react";
import Papa from "papaparse";
import type { Runner, Club, Class } from "../types";
import { create } from "../api/client";

type ImportTab = "csv" | "xml";
type ImportStep = "upload" | "preview" | "importing";

interface ParsedRow {
  rowIndex: number;
  name: string;
  clubName: string;
  className: string;
  startTime: string;
  cardNumber: string;
  errors: string[];
  isConflict: boolean;
}

interface ImportDialogProps {
  open: boolean;
  onClose: () => void;
  onImported: () => void;
  runners: Runner[];
  clubs: Club[];
  classes: Class[];
}

// ── CSV parsing ───────────────────────────────────────────────────────────────

const NAME_KEYS = new Set(["name", "fullname", "runner", "runnername"]);
const CLUB_KEYS = new Set(["club", "clubname", "organisation", "org"]);
const CLASS_KEYS = new Set(["class", "category", "cat", "division"]);
const START_TIME_KEYS = new Set(["starttime", "start"]);
const CARD_KEYS = new Set(["card", "cardnumber", "chip", "si", "sipunch"]);

function normalizeKey(k: string): string {
  return k.toLowerCase().replace(/[\s_-]+/g, "");
}

function pickField(row: Record<string, string>, keys: Set<string>): string {
  for (const [k, v] of Object.entries(row)) {
    if (keys.has(normalizeKey(k))) return (v ?? "").trim();
  }
  return "";
}

function buildCsvRows(data: Record<string, string>[], runners: Runner[]): ParsedRow[] {
  const existingNames = new Set(runners.map((r) => r.name.toLowerCase()));
  return data
    .filter((row) => Object.values(row).some((v) => v.trim()))
    .map((rawRow, i) => {
      const name = pickField(rawRow, NAME_KEYS);
      const errors: string[] = [];
      if (!name) errors.push("Name is required");
      return {
        rowIndex: i + 1,
        name,
        clubName: pickField(rawRow, CLUB_KEYS),
        className: pickField(rawRow, CLASS_KEYS),
        startTime: pickField(rawRow, START_TIME_KEYS),
        cardNumber: pickField(rawRow, CARD_KEYS),
        errors,
        isConflict: name ? existingNames.has(name.toLowerCase()) : false,
      };
    });
}

// ── IOF XML 3.0 parsing ───────────────────────────────────────────────────────

function getFirstEl(parent: Element | Document, tag: string): Element | null {
  return parent.getElementsByTagName(tag)[0] ?? null;
}

function getElText(parent: Element, tag: string): string {
  return getFirstEl(parent, tag)?.textContent?.trim() ?? "";
}

export function parseIofXml(xmlText: string, runners: Runner[]): ParsedRow[] {
  const parser = new DOMParser();
  const doc = parser.parseFromString(xmlText, "application/xml");

  if (doc.getElementsByTagName("parsererror").length > 0) {
    throw new Error("Invalid XML format.");
  }

  const entries = Array.from(doc.getElementsByTagName("PersonEntry"));
  if (entries.length === 0) {
    throw new Error("No runner entries found in XML.");
  }

  const existingNames = new Set(runners.map((r) => r.name.toLowerCase()));

  return entries.map((entry, i) => {
    const person = getFirstEl(entry, "Person");
    const nameEl = person ? getFirstEl(person, "Name") : null;
    const family = nameEl ? getElText(nameEl, "Family") : "";
    const given = nameEl ? getElText(nameEl, "Given") : "";
    const name = [given, family].filter(Boolean).join(" ");

    const org = getFirstEl(entry, "Organisation");
    const clubName = org ? getElText(org, "Name") : "";

    const cls = getFirstEl(entry, "Class");
    const className = cls ? getElText(cls, "Name") : "";

    const cardNumber = getElText(entry, "ControlCard");

    const startTimeEl = getFirstEl(entry, "StartTime");
    const startTime = startTimeEl ? getElText(startTimeEl, "Time") : "";

    const errors: string[] = [];
    if (!name) errors.push("Name is required");

    return {
      rowIndex: i + 1,
      name,
      clubName,
      className,
      startTime,
      cardNumber,
      errors,
      isConflict: name ? existingNames.has(name.toLowerCase()) : false,
    };
  });
}

// ── Component ─────────────────────────────────────────────────────────────────

export function ImportDialog({
  open,
  onClose,
  onImported,
  runners,
  clubs,
  classes,
}: ImportDialogProps) {
  const [tab, setTab] = useState<ImportTab>("csv");
  const [step, setStep] = useState<ImportStep>("upload");
  const [rows, setRows] = useState<ParsedRow[]>([]);
  const [importError, setImportError] = useState<string | null>(null);
  const csvFileRef = useRef<HTMLInputElement>(null);
  const xmlFileRef = useRef<HTMLInputElement>(null);

  function resetDialog() {
    setStep("upload");
    setRows([]);
    setImportError(null);
    if (csvFileRef.current) csvFileRef.current.value = "";
    if (xmlFileRef.current) xmlFileRef.current.value = "";
  }

  function handleClose() {
    resetDialog();
    onClose();
  }

  function handleTabChange(newTab: ImportTab) {
    setTab(newTab);
    setStep("upload");
    setRows([]);
    setImportError(null);
  }

  function handleCsvFileChange(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    setImportError(null);
    Papa.parse<Record<string, string>>(file, {
      header: true,
      skipEmptyLines: true,
      complete: (result) => {
        const parsed = buildCsvRows(result.data, runners);
        setRows(parsed);
        setStep("preview");
      },
      error: () => {
        setImportError("Failed to parse CSV file.");
      },
    });
  }

  async function handleXmlFileChange(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    setImportError(null);
    try {
      const text = await file.text();
      const parsed = parseIofXml(text, runners);
      setRows(parsed);
      setStep("preview");
    } catch (err) {
      setImportError(err instanceof Error ? err.message : "Failed to parse XML file.");
    }
  }

  async function handleImport() {
    const validRows = rows.filter((r) => r.errors.length === 0);
    if (validRows.length === 0) return;
    setStep("importing");
    try {
      const runnerData = validRows.map((row) => {
        const club = clubs.find(
          (c) => c.name.toLowerCase() === row.clubName.toLowerCase()
        );
        const cls = classes.find(
          (c) => c.name.toLowerCase() === row.className.toLowerCase()
        );
        return {
          name: row.name,
          ...(club ? { clubId: club.id } : {}),
          ...(cls ? { classId: cls.id } : {}),
          ...(row.startTime ? { startTime: row.startTime } : {}),
          ...(row.cardNumber ? { cardNumber: Number(row.cardNumber) } : {}),
        };
      });
      await create<Runner[]>("runners/batch", runnerData);
      onImported();
      handleClose();
    } catch {
      setImportError("Import failed. Please try again.");
      setStep("preview");
    }
  }

  if (!open) return null;

  const validCount = rows.filter((r) => r.errors.length === 0).length;
  const errorCount = rows.filter((r) => r.errors.length > 0).length;
  const conflictCount = rows.filter(
    (r) => r.errors.length === 0 && r.isConflict
  ).length;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center"
      role="dialog"
      aria-modal="true"
      aria-label="Import Runners"
    >
      <div className="fixed inset-0 bg-black/50" onClick={handleClose} />
      <div className="relative z-10 bg-white rounded-lg shadow-xl w-full max-w-2xl max-h-[90vh] flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-4 border-b">
          <h2 className="text-lg font-semibold">Import Runners</h2>
          <button
            onClick={handleClose}
            className="text-gray-400 hover:text-gray-600"
            aria-label="Close"
          >
            ✕
          </button>
        </div>

        {/* Tabs */}
        <div className="flex border-b">
          <button
            role="tab"
            aria-selected={tab === "csv"}
            onClick={() => handleTabChange("csv")}
            className={`px-6 py-3 text-sm font-medium border-b-2 transition-colors ${
              tab === "csv"
                ? "border-blue-600 text-blue-600"
                : "border-transparent text-gray-500 hover:text-gray-700"
            }`}
          >
            CSV
          </button>
          <button
            role="tab"
            aria-selected={tab === "xml"}
            onClick={() => handleTabChange("xml")}
            className={`px-6 py-3 text-sm font-medium border-b-2 transition-colors ${
              tab === "xml"
                ? "border-blue-600 text-blue-600"
                : "border-transparent text-gray-500 hover:text-gray-700"
            }`}
          >
            IOF XML
          </button>
        </div>

        {/* Step indicator */}
        <div className="flex items-center gap-2 px-6 py-3 border-b bg-gray-50 text-sm">
          <span
            className={
              step === "upload" ? "font-semibold text-blue-600" : "text-gray-500"
            }
          >
            1. Upload
          </span>
          <span className="text-gray-300">→</span>
          <span
            className={
              step === "preview"
                ? "font-semibold text-blue-600"
                : "text-gray-500"
            }
          >
            2. Preview
          </span>
          <span className="text-gray-300">→</span>
          <span
            className={
              step === "importing"
                ? "font-semibold text-blue-600"
                : "text-gray-500"
            }
          >
            3. Import
          </span>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-auto px-6 py-4">
          {step === "upload" && tab === "csv" && (
            <div className="flex flex-col items-center justify-center py-8 gap-4">
              <p className="text-gray-600 text-sm text-center max-w-md">
                Upload a CSV file with runner data. Supported columns: Name,
                Club, Class, Start Time, Card Number.
              </p>
              <label className="cursor-pointer px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm">
                Choose CSV File
                <input
                  ref={csvFileRef}
                  type="file"
                  accept=".csv"
                  className="hidden"
                  onChange={handleCsvFileChange}
                  aria-label="Choose CSV file"
                />
              </label>
              {importError && (
                <p className="text-red-600 text-sm" role="alert">
                  {importError}
                </p>
              )}
            </div>
          )}

          {step === "upload" && tab === "xml" && (
            <div className="flex flex-col items-center justify-center py-8 gap-4">
              <p className="text-gray-600 text-sm text-center max-w-md">
                Upload an IOF XML 3.0 entry file (EntryList format). Runners
                will be extracted from PersonEntry elements.
              </p>
              <label className="cursor-pointer px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm">
                Choose IOF XML File
                <input
                  ref={xmlFileRef}
                  type="file"
                  accept=".xml"
                  className="hidden"
                  onChange={handleXmlFileChange}
                  aria-label="Choose IOF XML file"
                />
              </label>
              {importError && (
                <p className="text-red-600 text-sm" role="alert">
                  {importError}
                </p>
              )}
            </div>
          )}

          {step === "preview" && (
            <div className="flex flex-col gap-3">
              <div className="flex items-center gap-4 text-sm">
                <span className="text-green-700 font-medium">
                  {validCount} valid
                </span>
                {conflictCount > 0 && (
                  <span className="text-yellow-700 font-medium">
                    {conflictCount} duplicate{conflictCount !== 1 ? "s" : ""}
                  </span>
                )}
                {errorCount > 0 && (
                  <span className="text-red-700 font-medium">
                    {errorCount} error{errorCount !== 1 ? "s" : ""}
                  </span>
                )}
              </div>
              {importError && (
                <p className="text-red-600 text-sm" role="alert">
                  {importError}
                </p>
              )}
              <table className="w-full text-sm border-collapse">
                <thead>
                  <tr className="border-b text-left text-gray-500">
                    <th className="py-2 pr-3 font-medium">#</th>
                    <th className="py-2 pr-3 font-medium">Name</th>
                    <th className="py-2 pr-3 font-medium">Club</th>
                    <th className="py-2 pr-3 font-medium">Class</th>
                    <th className="py-2 font-medium">Status</th>
                  </tr>
                </thead>
                <tbody>
                  {rows.map((row) => (
                    <tr
                      key={row.rowIndex}
                      className={
                        row.errors.length > 0
                          ? "bg-red-50"
                          : row.isConflict
                          ? "bg-yellow-50"
                          : ""
                      }
                    >
                      <td className="py-1 pr-3 text-gray-400">{row.rowIndex}</td>
                      <td className="py-1 pr-3">{row.name || "—"}</td>
                      <td className="py-1 pr-3">{row.clubName || "—"}</td>
                      <td className="py-1 pr-3">{row.className || "—"}</td>
                      <td className="py-1">
                        {row.errors.length > 0 ? (
                          <span className="text-red-600">
                            Error: {row.errors[0]}
                          </span>
                        ) : row.isConflict ? (
                          <span className="text-yellow-700">Duplicate name</span>
                        ) : (
                          <span className="text-green-700">Valid</span>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}

          {step === "importing" && (
            <div className="flex flex-col items-center justify-center py-8 gap-3">
              <div
                className="w-8 h-8 border-4 border-blue-600 border-t-transparent rounded-full animate-spin"
                aria-label="Importing"
              />
              <p className="text-gray-600 text-sm">Importing runners...</p>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="flex justify-end gap-2 px-6 py-4 border-t">
          {step !== "importing" && (
            <button
              onClick={handleClose}
              className="px-4 py-2 text-sm border rounded hover:bg-gray-50"
            >
              Cancel
            </button>
          )}
          {step === "preview" && (
            <>
              <button
                onClick={() => {
                  setImportError(null);
                  setStep("upload");
                  if (csvFileRef.current) csvFileRef.current.value = "";
                  if (xmlFileRef.current) xmlFileRef.current.value = "";
                }}
                className="px-4 py-2 text-sm border rounded hover:bg-gray-50"
              >
                Back
              </button>
              <button
                onClick={() => void handleImport()}
                disabled={validCount === 0}
                className="px-4 py-2 text-sm bg-blue-600 text-white rounded hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed"
              >
                Import {validCount} runner{validCount !== 1 ? "s" : ""}
              </button>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
