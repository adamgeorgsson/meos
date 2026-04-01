import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Papa from "papaparse";
import { ImportDialog, parseIofXml } from "./ImportDialog";
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

function triggerCsvParse(rows: Record<string, string>[] | "error") {
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
    <ImportDialog
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

const VALID_XML = `<?xml version="1.0" encoding="UTF-8"?>
<EntryList>
  <PersonEntry>
    <Person><Name><Family>Doe</Family><Given>John</Given></Name></Person>
    <Organisation><Name>IF Berget</Name></Organisation>
    <Class><Name>H21E</Name></Class>
    <ControlCard>12345</ControlCard>
    <StartTime><Time>10:00:00</Time></StartTime>
  </PersonEntry>
</EntryList>`;

const MULTI_ENTRY_XML = `<?xml version="1.0" encoding="UTF-8"?>
<EntryList>
  <PersonEntry>
    <Person><Name><Family>Smith</Family><Given>Jane</Given></Name></Person>
  </PersonEntry>
  <PersonEntry>
    <Person><Name><Family>Jones</Family><Given>Bob</Given></Name></Person>
  </PersonEntry>
</EntryList>`;

const NO_NAME_XML = `<?xml version="1.0" encoding="UTF-8"?>
<EntryList>
  <PersonEntry>
    <Organisation><Name>IF Berget</Name></Organisation>
  </PersonEntry>
</EntryList>`;

const CONFLICT_XML = `<?xml version="1.0" encoding="UTF-8"?>
<EntryList>
  <PersonEntry>
    <Person><Name><Family>Lindström</Family><Given>Anna</Given></Name></Person>
  </PersonEntry>
</EntryList>`;

describe("ImportDialog", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("renders nothing when closed", () => {
    render(
      <ImportDialog
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

  it("shows both CSV and IOF XML tabs when open", () => {
    renderDialog();
    expect(screen.getByRole("tab", { name: "CSV" })).toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "IOF XML" })).toBeInTheDocument();
  });

  it("shows CSV upload step by default", () => {
    renderDialog();
    expect(screen.getByLabelText("Choose CSV file")).toBeInTheDocument();
    expect(screen.queryByLabelText("Choose IOF XML file")).not.toBeInTheDocument();
  });

  it("switches to IOF XML tab and shows XML file input", () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    expect(screen.getByLabelText("Choose IOF XML file")).toBeInTheDocument();
    expect(screen.queryByLabelText("Choose CSV file")).not.toBeInTheDocument();
  });

  it("CSV: transitions to preview after parsing", () => {
    renderDialog();
    triggerCsvParse([
      { Name: "John Doe", Club: "IF Berget" },
      { Name: "Jane Smith" },
    ]);
    expect(screen.getByText("John Doe")).toBeInTheDocument();
    expect(screen.getByText("Jane Smith")).toBeInTheDocument();
    expect(screen.getAllByText("Valid")).toHaveLength(2);
  });

  it("XML: transitions to preview after parsing valid XML", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([VALID_XML], "test.xml", { type: "application/xml" });
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByText("John Doe")).toBeInTheDocument();
    });
    expect(screen.getByText("IF Berget")).toBeInTheDocument();
    expect(screen.getByText("H21E")).toBeInTheDocument();
    expect(screen.getByText("Valid")).toBeInTheDocument();
  });

  it("XML: parses multiple entries", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([MULTI_ENTRY_XML], "test.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByText("Jane Smith")).toBeInTheDocument();
    });
    expect(screen.getByText("Bob Jones")).toBeInTheDocument();
    expect(screen.getAllByText("Valid")).toHaveLength(2);
  });

  it("XML: shows error for invalid XML", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File(["not valid xml <<<"], "bad.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
    expect(screen.getByRole("alert").textContent).toMatch(/invalid xml/i);
  });

  it("XML: shows error when no PersonEntry elements found", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File(
      [`<?xml version="1.0"?><EntryList></EntryList>`],
      "empty.xml"
    );
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
    expect(screen.getByRole("alert").textContent).toMatch(/no runner entries/i);
  });

  it("XML: shows error status for entry missing name", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([NO_NAME_XML], "test.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByText(/Error: Name is required/)).toBeInTheDocument();
    });
  });

  it("XML: shows duplicate name status for conflicts with existing runners", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([CONFLICT_XML], "test.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByText("Duplicate name")).toBeInTheDocument();
    });
  });

  it("XML: imports valid runners via batch endpoint and calls onImported", async () => {
    const { onImported, onClose } = renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([VALID_XML], "test.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByRole("button", { name: /import 1 runner/i })).toBeInTheDocument();
    });
    fireEvent.click(screen.getByRole("button", { name: /import 1 runner/i }));
    await waitFor(() => expect(onImported).toHaveBeenCalledOnce());
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("Back button returns to upload step", async () => {
    renderDialog();
    fireEvent.click(screen.getByRole("tab", { name: "IOF XML" }));
    const file = new File([VALID_XML], "test.xml");
    fireEvent.change(screen.getByLabelText("Choose IOF XML file"), {
      target: { files: [file] },
    });
    await waitFor(() => {
      expect(screen.getByRole("button", { name: "Back" })).toBeInTheDocument();
    });
    fireEvent.click(screen.getByRole("button", { name: "Back" }));
    expect(screen.getByLabelText("Choose IOF XML file")).toBeInTheDocument();
  });

  it("Cancel button calls onClose", () => {
    const { onClose } = renderDialog();
    fireEvent.click(screen.getByRole("button", { name: "Cancel" }));
    expect(onClose).toHaveBeenCalledOnce();
  });
});

// ── Unit tests for parseIofXml ─────────────────────────────────────────────

describe("parseIofXml", () => {
  it("parses family and given name into 'Given Family' order", () => {
    const xml = `<EntryList><PersonEntry>
      <Person><Name><Family>Lindström</Family><Given>Anna</Given></Name></Person>
    </PersonEntry></EntryList>`;
    const rows = parseIofXml(xml, []);
    expect(rows[0].name).toBe("Anna Lindström");
  });

  it("handles entry with only family name", () => {
    const xml = `<EntryList><PersonEntry>
      <Person><Name><Family>Solo</Family></Name></Person>
    </PersonEntry></EntryList>`;
    const rows = parseIofXml(xml, []);
    expect(rows[0].name).toBe("Solo");
  });

  it("extracts organisation, class, card number, and start time", () => {
    const xml = `<EntryList><PersonEntry>
      <Person><Name><Family>Doe</Family><Given>John</Given></Name></Person>
      <Organisation><Name>IF Berget</Name></Organisation>
      <Class><Name>H21E</Name></Class>
      <ControlCard>99999</ControlCard>
      <StartTime><Time>11:30:00</Time></StartTime>
    </PersonEntry></EntryList>`;
    const rows = parseIofXml(xml, []);
    expect(rows[0].clubName).toBe("IF Berget");
    expect(rows[0].className).toBe("H21E");
    expect(rows[0].cardNumber).toBe("99999");
    expect(rows[0].startTime).toBe("11:30:00");
  });

  it("throws on invalid XML", () => {
    expect(() => parseIofXml("<<< bad >>>", [])).toThrow(/invalid xml/i);
  });

  it("throws when no PersonEntry elements found", () => {
    expect(() =>
      parseIofXml(`<?xml version="1.0"?><EntryList></EntryList>`, [])
    ).toThrow(/no runner entries/i);
  });

  it("marks conflict when name matches existing runner (case-insensitive)", () => {
    const xml = `<EntryList><PersonEntry>
      <Person><Name><Family>Lindström</Family><Given>Anna</Given></Name></Person>
    </PersonEntry></EntryList>`;
    const existing: Runner[] = [{ id: 1, name: "Anna Lindström" }];
    const rows = parseIofXml(xml, existing);
    expect(rows[0].isConflict).toBe(true);
  });

  it("works with IOF XML 3.0 namespace", () => {
    const xml = `<?xml version="1.0" encoding="UTF-8"?>
<EntryList xmlns="http://www.orienteering.org/datastandard/3.0" iofVersion="3.0">
  <PersonEntry>
    <Person><Name><Family>Berg</Family><Given>Lars</Given></Name></Person>
  </PersonEntry>
</EntryList>`;
    const rows = parseIofXml(xml, []);
    expect(rows[0].name).toBe("Lars Berg");
  });
});
