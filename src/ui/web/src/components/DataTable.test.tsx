import { render, screen, fireEvent } from "@testing-library/react";
import { describe, test, expect, vi } from "vitest";
import { DataTable } from "./DataTable";
import type { ColumnDef } from "./DataTable";

interface Person {
  id: number;
  name: string;
  club: string;
}

const columns: ColumnDef<Person>[] = [
  { key: "name", header: "Name", accessor: (r) => r.name },
  { key: "club", header: "Club", accessor: (r) => r.club },
];

const data: Person[] = [
  { id: 1, name: "Alice", club: "Alpha" },
  { id: 2, name: "Bob", club: "Beta" },
  { id: 3, name: "Charlie", club: "Alpha" },
  { id: 4, name: "Dave", club: "Gamma" },
  { id: 5, name: "Eve", club: "Delta" },
];

describe("DataTable", () => {
  test("renders column headers", () => {
    render(<DataTable columns={columns} data={data} />);
    expect(screen.getByText("Name")).toBeDefined();
    expect(screen.getByText("Club")).toBeDefined();
  });

  test("renders all data rows by default", () => {
    render(<DataTable columns={columns} data={data} />);
    expect(screen.getByText("Alice")).toBeDefined();
    expect(screen.getByText("Bob")).toBeDefined();
    expect(screen.getByText("Charlie")).toBeDefined();
    expect(screen.getByText("Dave")).toBeDefined();
    expect(screen.getByText("Eve")).toBeDefined();
  });

  test("shows empty state when data is empty", () => {
    render(<DataTable columns={columns} data={[]} />);
    expect(screen.getByText("No data")).toBeDefined();
  });

  test("shows skeleton when isLoading=true", () => {
    render(<DataTable columns={columns} data={[]} isLoading />);
    expect(screen.getByLabelText("Loading")).toBeDefined();
    expect(screen.queryByText("No data")).toBeNull();
  });

  test("does not show table when isLoading=true", () => {
    render(<DataTable columns={columns} data={data} isLoading />);
    expect(screen.queryByText("Alice")).toBeNull();
    expect(screen.queryByText("Name")).toBeNull();
  });

  test("text filter searches string columns", () => {
    render(<DataTable columns={columns} data={data} />);
    const input = screen.getByLabelText("Search");
    fireEvent.change(input, { target: { value: "ali" } });
    expect(screen.getByText("Alice")).toBeDefined();
    expect(screen.queryByText("Bob")).toBeNull();
    expect(screen.queryByText("Charlie")).toBeNull();
  });

  test("text filter matches club column", () => {
    render(<DataTable columns={columns} data={data} />);
    const input = screen.getByLabelText("Search");
    fireEvent.change(input, { target: { value: "Alpha" } });
    expect(screen.getByText("Alice")).toBeDefined();
    expect(screen.getByText("Charlie")).toBeDefined();
    expect(screen.queryByText("Bob")).toBeNull();
  });

  test("filter shows empty state when no results", () => {
    render(<DataTable columns={columns} data={data} />);
    const input = screen.getByLabelText("Search");
    fireEvent.change(input, { target: { value: "zzz" } });
    expect(screen.getByText("No data")).toBeDefined();
  });

  test("sorting: click header sorts ascending then descending then resets", () => {
    render(<DataTable columns={columns} data={data} />);
    const nameHeader = screen.getByText("Name");

    // first click → ascending (A→E)
    fireEvent.click(nameHeader);
    const rows1 = screen.getAllByRole("row").slice(1); // skip header
    expect(rows1[0].textContent).toContain("Alice");
    expect(rows1[4].textContent).toContain("Eve");

    // second click → descending (E→A)
    fireEvent.click(nameHeader);
    const rows2 = screen.getAllByRole("row").slice(1);
    expect(rows2[0].textContent).toContain("Eve");
    expect(rows2[4].textContent).toContain("Alice");

    // third click → no sort (original order)
    fireEvent.click(nameHeader);
    const rows3 = screen.getAllByRole("row").slice(1);
    expect(rows3[0].textContent).toContain("Alice");
    expect(rows3[1].textContent).toContain("Bob");
  });

  test("pagination limits rows per page", () => {
    render(<DataTable columns={columns} data={data} pageSize={2} />);
    // Only first 2 rows on page 1
    expect(screen.getByText("Alice")).toBeDefined();
    expect(screen.getByText("Bob")).toBeDefined();
    expect(screen.queryByText("Charlie")).toBeNull();
  });

  test("pagination navigation shows next page", () => {
    render(<DataTable columns={columns} data={data} pageSize={2} />);
    const next = screen.getByLabelText("Next page");
    fireEvent.click(next);
    expect(screen.getByText("Charlie")).toBeDefined();
    expect(screen.getByText("Dave")).toBeDefined();
    expect(screen.queryByText("Alice")).toBeNull();
  });

  test("pagination previous button disabled on first page", () => {
    render(<DataTable columns={columns} data={data} pageSize={2} />);
    const prev = screen.getByLabelText("Previous page");
    expect((prev as HTMLButtonElement).disabled).toBe(true);
  });

  test("pagination next button disabled on last page", () => {
    render(<DataTable columns={columns} data={data} pageSize={3} />);
    const next = screen.getByLabelText("Next page");
    fireEvent.click(next); // go to page 2 (last)
    expect((next as HTMLButtonElement).disabled).toBe(true);
  });

  test("no pagination controls when all data fits on one page", () => {
    render(<DataTable columns={columns} data={data} pageSize={10} />);
    expect(screen.queryByLabelText("Next page")).toBeNull();
    expect(screen.queryByLabelText("Previous page")).toBeNull();
  });

  test("row selection: enableSelection shows checkboxes", () => {
    render(<DataTable columns={columns} data={data} enableSelection />);
    const checkboxes = screen.getAllByRole("checkbox");
    // header checkbox + 5 row checkboxes
    expect(checkboxes).toHaveLength(6);
  });

  test("row selection: clicking row checkbox calls onSelectionChange", () => {
    const onChange = vi.fn();
    render(
      <DataTable
        columns={columns}
        data={data}
        enableSelection
        onSelectionChange={onChange}
      />
    );
    const rowCheckbox = screen.getByLabelText("Select row 1");
    fireEvent.click(rowCheckbox);
    expect(onChange).toHaveBeenCalledOnce();
    const selected = onChange.mock.calls[0][0] as Set<string | number>;
    expect(selected.has(1)).toBe(true);
  });

  test("row selection: clicking again deselects", () => {
    const onChange = vi.fn();
    render(
      <DataTable
        columns={columns}
        data={data}
        enableSelection
        onSelectionChange={onChange}
      />
    );
    const rowCheckbox = screen.getByLabelText("Select row 1");
    fireEvent.click(rowCheckbox); // select
    fireEvent.click(rowCheckbox); // deselect
    const lastCall = onChange.mock.calls[1][0] as Set<string | number>;
    expect(lastCall.has(1)).toBe(false);
  });

  test("row selection: select all checks all rows", () => {
    const onChange = vi.fn();
    render(
      <DataTable
        columns={columns}
        data={data}
        enableSelection
        onSelectionChange={onChange}
      />
    );
    const selectAll = screen.getByLabelText("Select all");
    fireEvent.click(selectAll);
    const selected = onChange.mock.calls[0][0] as Set<string | number>;
    expect(selected.size).toBe(5);
  });

  test("row selection: select all then click again deselects all", () => {
    const onChange = vi.fn();
    render(
      <DataTable
        columns={columns}
        data={data}
        enableSelection
        onSelectionChange={onChange}
      />
    );
    const selectAll = screen.getByLabelText("Select all");
    fireEvent.click(selectAll); // select all
    fireEvent.click(selectAll); // deselect all
    const lastCall = onChange.mock.calls[1][0] as Set<string | number>;
    expect(lastCall.size).toBe(0);
  });

  test("getItemId prop uses custom id function", () => {
    interface NamedItem {
      code: string;
      label: string;
    }
    const customCols: ColumnDef<NamedItem>[] = [
      { key: "label", header: "Label", accessor: (r) => r.label },
    ];
    const customData: NamedItem[] = [
      { code: "A", label: "Apple" },
      { code: "B", label: "Banana" },
    ];
    const onChange = vi.fn();
    render(
      <DataTable
        columns={customCols}
        data={customData}
        enableSelection
        getItemId={(item) => item.code}
        onSelectionChange={onChange}
      />
    );
    const rowCheckbox = screen.getByLabelText("Select row A");
    fireEvent.click(rowCheckbox);
    const selected = onChange.mock.calls[0][0] as Set<string | number>;
    expect(selected.has("A")).toBe(true);
  });
});
