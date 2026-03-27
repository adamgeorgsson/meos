import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { CompetitionPage } from "./CompetitionPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("CompetitionPage", () => {
  it("displays competition info after loading", async () => {
    render(<CompetitionPage />, { wrapper });

    await waitFor(() => {
      expect(screen.getByText("Spring Cup 2026")).toBeInTheDocument();
    });

    expect(screen.getByText("2026-05-15")).toBeInTheDocument();
    expect(screen.getByText("IF Berget")).toBeInTheDocument();
    expect(screen.getByText("Björnparken, Stockholm")).toBeInTheDocument();
  });

  it("shows Edit button", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));
    expect(screen.getByRole("button", { name: /edit/i })).toBeInTheDocument();
  });

  it("opens edit dialog when Edit is clicked", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Edit Competition")).toBeInTheDocument();
  });

  it("pre-fills form with current competition data", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));

    const nameInput = screen.getByPlaceholderText("Competition name") as HTMLInputElement;
    expect(nameInput.value).toBe("Spring Cup 2026");

    const dateInput = screen.getByDisplayValue("2026-05-15") as HTMLInputElement;
    expect(dateInput).toBeInTheDocument();
  });

  it("shows validation error when name is cleared and saved", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));

    const nameInput = screen.getByPlaceholderText("Competition name");
    fireEvent.change(nameInput, { target: { value: "" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByText("Name is required")).toBeInTheDocument();
    });
  });

  it("shows validation error when date is cleared and saved", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));

    const dateInput = screen.getByDisplayValue("2026-05-15");
    fireEvent.change(dateInput, { target: { value: "" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByText("Date is required")).toBeInTheDocument();
    });
  });

  it("saves changes and closes dialog", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));

    const nameInput = screen.getByPlaceholderText("Competition name");
    fireEvent.change(nameInput, { target: { value: "Autumn Cup 2026" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Autumn Cup 2026")).toBeInTheDocument();
    });
  });

  it("closes dialog on Cancel", async () => {
    render(<CompetitionPage />, { wrapper });
    await waitFor(() => screen.getByText("Spring Cup 2026"));

    fireEvent.click(screen.getByRole("button", { name: /edit/i }));
    expect(screen.getByRole("dialog")).toBeInTheDocument();

    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
  });
});
