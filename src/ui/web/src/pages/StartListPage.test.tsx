import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { StartListPage } from "./StartListPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("StartListPage", () => {
  it("renders the Start List heading", () => {
    render(<StartListPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Start List" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<StartListPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("shows class selector with All classes option", () => {
    render(<StartListPage />, { wrapper });
    expect(screen.getByRole("combobox")).toBeInTheDocument();
    expect(screen.getByText("All classes")).toBeInTheDocument();
  });

  it("populates class selector with classes from API", async () => {
    render(<StartListPage />, { wrapper });
    // Class names appear in both selector options and table rows — use getAllByText
    await waitFor(() => {
      expect(screen.getAllByText("H21E").length).toBeGreaterThan(0);
    });
    expect(screen.getAllByText("D21E").length).toBeGreaterThan(0);
    expect(screen.getAllByText("H21A").length).toBeGreaterThan(0);
  });

  it("displays table column headers", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Bib")).toBeInTheDocument();
    });
    expect(screen.getByText("Start Time")).toBeInTheDocument();
    expect(screen.getByText("Name")).toBeInTheDocument();
    expect(screen.getByText("Club")).toBeInTheDocument();
  });

  it("displays start list entries with runner names", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
    });
    expect(screen.getByText("Erik Johansson")).toBeInTheDocument();
    expect(screen.getByText("Maja Björk")).toBeInTheDocument();
  });

  it("displays bib numbers", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));
    // Seed data bibs: 1–6
    expect(screen.getByText("1")).toBeInTheDocument();
    expect(screen.getByText("2")).toBeInTheDocument();
  });

  it("displays club names", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getAllByText("IF Berget").length).toBeGreaterThan(0);
    });
    expect(screen.getByText("OK Älgen")).toBeInTheDocument();
  });

  it("displays class names in table rows", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));
    // D21E appears in selector option AND table row — multiple is fine
    expect(screen.getAllByText("D21E").length).toBeGreaterThan(0);
    expect(screen.getAllByText("H21E").length).toBeGreaterThan(0);
  });

  it("displays start times", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));
    expect(screen.getByText("10:00:00")).toBeInTheDocument();
    expect(screen.getByText("10:02:00")).toBeInTheDocument();
  });

  it("filters start list by selected class", async () => {
    render(<StartListPage />, { wrapper });
    // Wait for data — class names appear in both selector and table, use getAllByText
    await waitFor(() => screen.getAllByText("D21E").length > 0);

    const selector = screen.getByRole("combobox");
    fireEvent.change(selector, { target: { value: "2" } }); // D21E = id 2

    await waitFor(() => {
      expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
      expect(screen.getByText("Maria Karlsson")).toBeInTheDocument();
    });
    expect(screen.queryByText("Erik Johansson")).not.toBeInTheDocument();
    expect(screen.queryByText("Maja Björk")).not.toBeInTheDocument();
  });

  it("shows no start list entries message when filter yields empty", async () => {
    render(<StartListPage />, { wrapper });
    // H35 = id 5 has no start list entries in seed data; wait for classes to load
    await waitFor(() => screen.getAllByText("H35").length > 0);

    const selector = screen.getByRole("combobox");
    fireEvent.change(selector, { target: { value: "5" } });

    await waitFor(() => {
      expect(screen.getByText("No start list entries")).toBeInTheDocument();
    });
  });

  it("resets to all classes when All classes option selected", async () => {
    render(<StartListPage />, { wrapper });
    await waitFor(() => screen.getAllByText("D21E").length > 0);

    const selector = screen.getByRole("combobox");
    fireEvent.change(selector, { target: { value: "2" } });
    await waitFor(() => expect(screen.queryByText("Erik Johansson")).not.toBeInTheDocument());

    fireEvent.change(selector, { target: { value: "" } });
    await waitFor(() => {
      expect(screen.getByText("Erik Johansson")).toBeInTheDocument();
    });
  });

  it("has a Print button", () => {
    render(<StartListPage />, { wrapper });
    expect(screen.getByRole("button", { name: "Print" })).toBeInTheDocument();
  });

  it("calls window.print when Print button is clicked", () => {
    const printSpy = vi.spyOn(window, "print").mockImplementation(() => {});
    render(<StartListPage />, { wrapper });
    fireEvent.click(screen.getByRole("button", { name: "Print" }));
    expect(printSpy).toHaveBeenCalledOnce();
    printSpy.mockRestore();
  });
});
