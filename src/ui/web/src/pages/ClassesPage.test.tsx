import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { ClassesPage } from "./ClassesPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("ClassesPage", () => {
  it("renders the Classes heading", () => {
    render(<ClassesPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Classes" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<ClassesPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays classes list after loading", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("H21E")).toBeInTheDocument();
    });
    expect(screen.getByText("D21E")).toBeInTheDocument();
    expect(screen.getByText("H21A")).toBeInTheDocument();
  });

  it("shows Add Class button", () => {
    render(<ClassesPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add class/i })).toBeInTheDocument();
  });

  it("opens Add Class dialog when Add Class is clicked", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByRole("button", { name: /add class/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Class")).toBeInTheDocument();
  });

  it("shows validation error when name is empty and Save is clicked", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByRole("button", { name: /add class/i }));
    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new class and closes dialog", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByRole("button", { name: /add class/i }));

    const nameInput = screen.getByPlaceholderText("Class name");
    fireEvent.change(nameInput, { target: { value: "H10" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("H10")).toBeInTheDocument();
    });
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Edit class H21E"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Edit Class")).toBeInTheDocument();

    const nameInput = screen.getByPlaceholderText("Class name") as HTMLInputElement;
    expect(nameInput.value).toBe("H21E");
  });

  it("pre-fills course and start method in edit dialog", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Edit class H21E"));

    const dialog = screen.getByRole("dialog");
    const selects = within(dialog).getAllByRole("combobox") as HTMLSelectElement[];
    // courseId select should have value "1" (H21E has courseId: 1)
    const courseSelect = selects[0];
    expect(courseSelect.value).toBe("1");
    // startMethod select should have value "individual"
    const startMethodSelect = selects[1];
    expect(startMethodSelect.value).toBe("individual");
  });

  it("updates a class and closes dialog", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Edit class H21E"));

    const nameInput = screen.getByPlaceholderText("Class name");
    fireEvent.change(nameInput, { target: { value: "H21E Updated" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("H21E Updated")).toBeInTheDocument();
    });
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Delete class H21E"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete class "H21E"/i)).toBeInTheDocument();
  });

  it("deletes a class on confirm", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Delete class H21E"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("H21E")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the class", async () => {
    render(<ClassesPage />, { wrapper });
    await waitFor(() => screen.getByText("H21E"));

    fireEvent.click(screen.getByLabelText("Delete class H21E"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("H21E")).toBeInTheDocument();
  });
});
