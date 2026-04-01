import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { ControlsPage } from "./ControlsPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("ControlsPage", () => {
  it("renders the Controls heading", () => {
    render(<ControlsPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Controls" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<ControlsPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays controls list after loading", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Fork junction")).toBeInTheDocument();
    });
    expect(screen.getByText("101")).toBeInTheDocument();
    expect(screen.getByText("Stone wall corner")).toBeInTheDocument();
  });

  it("shows Add Control button", async () => {
    render(<ControlsPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add control/i })).toBeInTheDocument();
  });

  it("opens Add Control dialog when Add Control is clicked", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByRole("button", { name: /add control/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Control")).toBeInTheDocument();
  });

  it("shows validation error when code is empty and Save is clicked", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByRole("button", { name: /add control/i }));
    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new control and closes dialog", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByRole("button", { name: /add control/i }));

    const codeInput = screen.getByPlaceholderText("e.g. 101");
    fireEvent.change(codeInput, { target: { value: "200", valueAsNumber: 200 } });

    const descInput = screen.getByPlaceholderText("Optional description");
    fireEvent.change(descInput, { target: { value: "New test control" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("New test control")).toBeInTheDocument();
    });
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByLabelText("Edit control 101"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Edit Control")).toBeInTheDocument();

    const codeInput = screen.getByPlaceholderText("e.g. 101") as HTMLInputElement;
    expect(codeInput.value).toBe("101");

    const descInput = screen.getByPlaceholderText("Optional description") as HTMLInputElement;
    expect(descInput.value).toBe("Fork junction");
  });

  it("updates a control and closes dialog", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByLabelText("Edit control 101"));

    const descInput = screen.getByPlaceholderText("Optional description");
    fireEvent.change(descInput, { target: { value: "Updated description" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Updated description")).toBeInTheDocument();
    });
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByLabelText("Delete control 101"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete control 101/i)).toBeInTheDocument();
  });

  it("deletes a control on confirm", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByLabelText("Delete control 101"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("Fork junction")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the control", async () => {
    render(<ControlsPage />, { wrapper });
    await waitFor(() => screen.getByText("Fork junction"));

    fireEvent.click(screen.getByLabelText("Delete control 101"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("Fork junction")).toBeInTheDocument();
  });
});
