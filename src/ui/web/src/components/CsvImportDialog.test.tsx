import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Papa from "papaparse";
import { CsvImportDialog } from "./CsvImportDialog";
import type { Runner, Club, Class } from "../types";

vi.mock("papaparse", () => ({
  default: { parse: vi.fn() },
}));

const mockRunners: Runner[] = [
  { id: 1, name: "Anna Lindström", clubId: 1, classId: 1 },
];

const mockClubs: Club[] = [
  { id: 1, name: "IF Berget" },
  { id: 2, name: "OK Älgen" },
];

const mockClasses: Class[] = [
  { id: 1, name: "H21E" },
  { id: 2, name: "D21E" },
];

type ParseOptions = {
  complete?: (result: { data: Record<string, string>[] }) => void;
  error?: () => void;
};

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const parseMock = () => (Papa as any).parse as { mockImplementationOnce: (fn: (f: unknown, o: ParseOptions) => void) => void };

function triggerParse(rows: Record<string, string>[] | "error") {
  parseMock().mockImplementationOnce((_: unknown, opts: ParseOptions) => {
    if (rows === "error") {
      opts.error?.();
    } else {
      opts.complete?.({ data: rows });
    }
  });
  const file = new File(["content"], "test.csv", { type: "text/csv" });
  fireEvent.change(screen.getByLabelText("Choose CSV file"), {
    target: { files: [file] },
  });
}

function renderDialog(overrides: Record<string, unknown> = {}) {
  const onClose = vi.fn();
  const onImported = vi.fn();
  render(
    <CsvImportDialog
      open={true}
      onClose={onClose}
      onImported={onImported}
      runners={mockRunners}
      clubs={mockClubs}
      classes={mockClasses}
      {...overrides}
    />
  );
  return { onClose, onImported };
}

describe("CsvImportDialog", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("renders nothing when closed", () => {
    render(
      <CsvImportDialog
        open={false}
        onClose={vi.fn()}
        onImported={vi.fn()}
        runners={mockRunners}
        clubs={mockClubs}
        classes={mockClasses}
      />
    );
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
  });

  it("shows upload step with file input when open", () => {
    renderDialog();
    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Import Runners from CSV")).toBeInTheDocument();
    expect(screen.getByText("Choose CSV File")).toBeInTheDocument();
    expect(screen.getByLabelText("Choose CSV file")).toBeInTheDocument();
  });

  it("transitions to preview after parsing CSV", () => {
    renderDialog();
    triggerParse([
      { Name: "John Doe", Club: "IF Berget", Class: "H21E" },
      { Name: "Jane Smith" },
    ]);
    expect(screen.getByText("John Doe")).toBeInTheDocument();
    expect(screen.getByText("Jane Smith")).toBeInTheDocument();
    expect(screen.getAllByText("Valid")).toHaveLength(2);
  });

  it("shows error status for rows with missing name", () => {
    renderDialog();
    triggerParse([{ Club: "IF Berget" }]);
    expect(screen.getByText(/Error: Name is required/)).toBeInTheDocument();
  });

  it("shows duplicate name status for conflicts with existing runners", () => {
    renderDialog();
    triggerParse([{ Name: "Anna Lindström" }]);
    expect(screen.getByText("Duplicate name")).toBeInTheDocument();
  });

  it("shows conflict count in summary when duplicates exist", () => {
    renderDialog();
    triggerParse([
      { Name: "Anna Lindström" },
      { Name: "New Runner" },
    ]);
    expect(screen.getByText("1 duplicate")).toBeInTheDocument();
    // both rows have no errors, so validCount=2 (duplicate is still "valid" to import)
    expect(screen.getByText(/2 valid/)).toBeInTheDocument();
  });

  it("shows parse error message when CSV parsing fails", () => {
    renderDialog();
    triggerParse("error");
    expect(screen.getByText("Failed to parse CSV file.")).toBeInTheDocument();
  });

  it("disables import button when all rows have errors", () => {
    renderDialog();
    triggerParse([{ Club: "IF Berget" }]); // no name
    expect(
      screen.getByRole("button", { name: /import 0 runners/i })
    ).toBeDisabled();
  });

  it("Back button returns to upload step", () => {
    renderDialog();
    triggerParse([{ Name: "John Doe" }]);
    expect(screen.getByText("John Doe")).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Back" }));
    expect(screen.getByText("Choose CSV File")).toBeInTheDocument();
  });

  it("Cancel button calls onClose", () => {
    const { onClose } = renderDialog();
    fireEvent.click(screen.getByRole("button", { name: "Cancel" }));
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("imports valid runners via batch endpoint and calls onImported", async () => {
    const { onImported, onClose } = renderDialog();
    triggerParse([{ Name: "New Runner", Club: "IF Berget", Class: "H21E" }]);
    fireEvent.click(screen.getByRole("button", { name: /import 1 runner$/i }));
    await waitFor(() => expect(onImported).toHaveBeenCalledOnce());
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("supports flexible CSV header names", () => {
    renderDialog();
    triggerParse([
      { "Full Name": "Test Runner", "Club Name": "IF Berget", category: "H21E" },
    ]);
    expect(screen.getByText("Test Runner")).toBeInTheDocument();
    expect(screen.getByText("IF Berget")).toBeInTheDocument();
    expect(screen.getByText("H21E")).toBeInTheDocument();
  });

  it("supports snake_case header names", () => {
    renderDialog();
    triggerParse([
      { name: "Alice", club: "OK Älgen", start_time: "10:00:00" },
    ]);
    expect(screen.getByText("Alice")).toBeInTheDocument();
    expect(screen.getByText("Valid")).toBeInTheDocument();
  });

  it("conflict duplicate runners are still included in import count", () => {
    renderDialog();
    triggerParse([
      { Name: "Anna Lindström" }, // duplicate
      { Name: "New Runner" },
    ]);
    // Both are valid (no errors), so validCount = 2
    expect(
      screen.getByRole("button", { name: /import 2 runners/i })
    ).toBeEnabled();
  });
});
