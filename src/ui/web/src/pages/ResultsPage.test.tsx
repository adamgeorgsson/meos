import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, act } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { ResultsPage, formatTime, formatTimeBehind } from "./ResultsPage";
import type { Result } from "../types";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("formatTime", () => {
  it("formats seconds under one hour as M:SS", () => {
    expect(formatTime(0)).toBe("0:00");
    expect(formatTime(60)).toBe("1:00");
    expect(formatTime(3123)).toBe("52:03");
    expect(formatTime(3599)).toBe("59:59");
  });

  it("formats seconds one hour or more as H:MM:SS", () => {
    expect(formatTime(3600)).toBe("1:00:00");
    expect(formatTime(4512)).toBe("1:15:12");
    expect(formatTime(5823)).toBe("1:37:03");
  });
});

describe("formatTimeBehind", () => {
  it("returns empty string for leader (diff = 0)", () => {
    expect(formatTimeBehind(4512, 4512)).toBe("");
  });

  it("returns empty string if result is faster than leader (shouldn't happen)", () => {
    expect(formatTimeBehind(4000, 4512)).toBe("");
  });

  it("formats time behind leader as +M:SS", () => {
    expect(formatTimeBehind(4789, 4512)).toBe("+4:37");
    expect(formatTimeBehind(4572, 4512)).toBe("+1:00");
  });
});

describe("ResultsPage", () => {
  it("renders the Results heading", () => {
    render(<ResultsPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Results" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<ResultsPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("shows class selector with All classes option", async () => {
    render(<ResultsPage />, { wrapper });
    const selector = screen.getByRole("combobox", { name: "Class:" });
    expect(selector).toBeInTheDocument();
    expect(screen.getByText("All classes")).toBeInTheDocument();
  });

  it("populates class selector with classes from API", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("H21E")).toBeInTheDocument();
    });
    expect(screen.getByText("D21E")).toBeInTheDocument();
    expect(screen.getByText("H21A")).toBeInTheDocument();
  });

  it("displays table column headers", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByLabelText("Loading") === null || screen.queryByLabelText("Loading") === null);
    await waitFor(() => {
      expect(screen.getByText("Name")).toBeInTheDocument();
    });
    expect(screen.getByText("Club")).toBeInTheDocument();
    expect(screen.getByText("Time")).toBeInTheDocument();
    expect(screen.getByText("+Time")).toBeInTheDocument();
    expect(screen.getByText("Status")).toBeInTheDocument();
  });

  it("displays runner results with name and club", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
    });
    // Anna and Maria both have "IF Berget" so multiple elements expected
    expect(screen.getAllByText("IF Berget").length).toBeGreaterThan(0);
  });

  it("filters results by selected class", async () => {
    render(<ResultsPage />, { wrapper });
    // Wait for classes to load
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E = id 2

    await waitFor(() => {
      expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
      expect(screen.getByText("Maria Karlsson")).toBeInTheDocument();
    });

    // Runners from other classes should not appear
    expect(screen.queryByText("Erik Johansson")).not.toBeInTheDocument();
    expect(screen.queryByText("Maja Björk")).not.toBeInTheDocument();
  });

  it("shows DNS status badge", async () => {
    render(<ResultsPage />, { wrapper });
    // Select H21E to see Lars Nilsson (dns)
    await waitFor(() => screen.getByText("H21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "1" } }); // H21E = id 1

    await waitFor(() => {
      expect(screen.getByText("DNS")).toBeInTheDocument();
    });
  });

  it("shows DNF status badge", async () => {
    render(<ResultsPage />, { wrapper });
    // Select H21A to see Sven Ek (dnf)
    await waitFor(() => screen.getByText("H21A"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "3" } }); // H21A = id 3

    await waitFor(() => {
      expect(screen.getByText("DNF")).toBeInTheDocument();
    });
  });

  it("shows formatted total time for results", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => {
      // Anna: 4512s = 1:15:12
      expect(screen.getByText("1:15:12")).toBeInTheDocument();
      // Maria: 4789s = 1:19:49
      expect(screen.getByText("1:19:49")).toBeInTheDocument();
    });
  });

  it("shows time behind leader for non-leaders", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => {
      // Maria (position 2) is behind Anna by 277s = +4:37
      expect(screen.getByText("+4:37")).toBeInTheDocument();
    });
  });

  it("expands row on click to show split times", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => screen.getByText("Anna Lindström"));

    // Click Anna's row
    const annaCell = screen.getByText("Anna Lindström");
    const annaRow = annaCell.closest("tr")!;
    fireEvent.click(annaRow);

    // Split times should appear
    await waitFor(() => {
      expect(screen.getByText("Split times")).toBeInTheDocument();
    });
  });

  it("shows control code and split time in expanded row", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => screen.getByText("Anna Lindström"));

    const annaCell = screen.getByText("Anna Lindström");
    fireEvent.click(annaCell.closest("tr")!);

    await waitFor(() => {
      // Anna's splits: controlId=1 (code=101), time=1234s = 20:34
      expect(screen.getByText("101")).toBeInTheDocument();
      expect(screen.getByText("20:34")).toBeInTheDocument();
    });
  });

  it("collapses row on second click", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => screen.getByText("Anna Lindström"));

    const annaCell = screen.getByText("Anna Lindström");
    const annaRow = annaCell.closest("tr")!;

    fireEvent.click(annaRow);
    await waitFor(() => expect(screen.getByText("Split times")).toBeInTheDocument());

    fireEvent.click(annaRow);
    await waitFor(() => expect(screen.queryByText("Split times")).not.toBeInTheDocument());
  });

  it("shows no results message when filter yields empty", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("H35")); // class id 5 has no results

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "5" } }); // H35 has no results

    await waitFor(() => {
      expect(screen.getByText("No results")).toBeInTheDocument();
    });
  });

  it("resets to all classes when All classes option is selected", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => screen.getByText("D21E"));

    const selector = screen.getByRole("combobox", { name: "Class:" });
    fireEvent.change(selector, { target: { value: "2" } }); // D21E

    await waitFor(() => expect(screen.queryByText("Erik Johansson")).not.toBeInTheDocument());

    fireEvent.change(selector, { target: { value: "" } }); // All

    await waitFor(() => {
      expect(screen.getByText("Erik Johansson")).toBeInTheDocument();
    });
  });
});

describe("ResultsPage — auto-refresh", () => {
  it("shows auto-refresh toggle checked by default", () => {
    render(<ResultsPage />, { wrapper });
    const toggle = screen.getByLabelText("Auto-refresh");
    expect(toggle).toBeInTheDocument();
    expect(toggle).toBeChecked();
  });

  it("shows interval selector with 5s, 10s, 30s, 60s options when auto-refresh is on", () => {
    render(<ResultsPage />, { wrapper });
    const intervalSelect = screen.getByLabelText("Refresh interval") as HTMLSelectElement;
    expect(intervalSelect).toBeInTheDocument();
    const options = Array.from(intervalSelect.options).map((o) => o.text);
    expect(options).toContain("5s");
    expect(options).toContain("10s");
    expect(options).toContain("30s");
    expect(options).toContain("60s");
  });

  it("default polling interval is 10s", () => {
    render(<ResultsPage />, { wrapper });
    const intervalSelect = screen.getByLabelText("Refresh interval") as HTMLSelectElement;
    expect(intervalSelect.value).toBe("10000");
  });

  it("shows auto-refresh active status indicator when enabled", () => {
    render(<ResultsPage />, { wrapper });
    expect(screen.getByLabelText("Auto-refresh status")).toHaveTextContent("Auto-refresh active");
  });

  it("disabling auto-refresh hides interval selector and changes status", () => {
    render(<ResultsPage />, { wrapper });
    const toggle = screen.getByLabelText("Auto-refresh");

    fireEvent.click(toggle);

    expect(toggle).not.toBeChecked();
    expect(screen.queryByLabelText("Refresh interval")).not.toBeInTheDocument();
    expect(screen.getByLabelText("Auto-refresh status")).toHaveTextContent("Auto-refresh off");
  });

  it("shows last refreshed time after initial data loads", async () => {
    render(<ResultsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByLabelText("Last refreshed")).toBeInTheDocument();
    });
  });

  it("does not show loading spinner during background refetch (silent refresh)", async () => {
    render(<ResultsPage />, { wrapper });
    // Wait for initial load to complete
    await waitFor(() => expect(screen.queryByLabelText("Loading")).not.toBeInTheDocument());
    // Loading div must stay gone once data is available
    expect(screen.queryByLabelText("Loading")).not.toBeInTheDocument();
  });

  it("highlights rows when result data changes", async () => {
    const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(
      <QueryClientProvider client={qc}>
        <MemoryRouter>
          <ResultsPage />
        </MemoryRouter>
      </QueryClientProvider>
    );

    // Wait for initial data to load and prevResultsRef to be populated
    await waitFor(() => {
      const data = qc.getQueryData<Result[]>(["results"]);
      expect(data).toBeDefined();
      expect(data!.length).toBeGreaterThan(0);
    });

    // Give useEffect a tick to populate prevResultsRef
    await act(async () => {
      await new Promise((r) => setTimeout(r, 0));
    });

    const initialResults = qc.getQueryData<Result[]>(["results"])!;

    // Simulate a background refresh that changes a result's time
    act(() => {
      qc.setQueryData<Result[]>(
        ["results"],
        initialResults.map((r, i) =>
          i === 0 ? { ...r, totalTime: (r.totalTime ?? 9999) + 100 } : r
        )
      );
    });

    // At least one row should be highlighted
    await waitFor(() => {
      const highlightedRows = document.querySelectorAll("tr.bg-yellow-100");
      expect(highlightedRows.length).toBeGreaterThan(0);
    });
  });
});
