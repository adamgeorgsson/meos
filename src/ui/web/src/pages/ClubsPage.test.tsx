import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { ClubsPage } from "./ClubsPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("ClubsPage", () => {
  it("renders the Clubs heading", () => {
    render(<ClubsPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Clubs" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<ClubsPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays clubs list after loading", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("IF Berget")).toBeInTheDocument();
    });
    expect(screen.getByText("OK Älgen")).toBeInTheDocument();
    expect(screen.getByText("IFK Göteborg SOK")).toBeInTheDocument();
  });

  it("shows Add Club button", () => {
    render(<ClubsPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add club/i })).toBeInTheDocument();
  });

  it("opens Add Club dialog when Add Club is clicked", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByRole("button", { name: /add club/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Club")).toBeInTheDocument();
  });

  it("shows validation error when name is empty and Save is clicked", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByRole("button", { name: /add club/i }));
    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new club and closes dialog", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByRole("button", { name: /add club/i }));

    const nameInput = screen.getByPlaceholderText("Club name");
    fireEvent.change(nameInput, { target: { value: "New Test Club" } });

    const countryInput = screen.getByPlaceholderText("e.g. SE");
    fireEvent.change(countryInput, { target: { value: "NO" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("New Test Club")).toBeInTheDocument();
    });
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByLabelText("Edit club IF Berget"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Edit Club")).toBeInTheDocument();

    const nameInput = screen.getByPlaceholderText("Club name") as HTMLInputElement;
    expect(nameInput.value).toBe("IF Berget");

    const countryInput = screen.getByPlaceholderText("e.g. SE") as HTMLInputElement;
    expect(countryInput.value).toBe("SE");
  });

  it("updates a club and closes dialog", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByLabelText("Edit club IF Berget"));

    const nameInput = screen.getByPlaceholderText("Club name");
    fireEvent.change(nameInput, { target: { value: "IF Berget Updated" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("IF Berget Updated")).toBeInTheDocument();
    });
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByLabelText("Delete club IF Berget"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete club "IF Berget"/i)).toBeInTheDocument();
  });

  it("deletes a club on confirm", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByLabelText("Delete club IF Berget"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("IF Berget")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the club", async () => {
    render(<ClubsPage />, { wrapper });
    await waitFor(() => screen.getByText("IF Berget"));

    fireEvent.click(screen.getByLabelText("Delete club IF Berget"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("IF Berget")).toBeInTheDocument();
  });
});
