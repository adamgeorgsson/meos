import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { RunnersPage } from "./RunnersPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("RunnersPage", () => {
  it("renders the Runners heading", () => {
    render(<RunnersPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Runners" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<RunnersPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays runners list after loading", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
    });
    expect(screen.getByText("Erik Johansson")).toBeInTheDocument();
    expect(screen.getByText("Maria Karlsson")).toBeInTheDocument();
  });

  it("shows Add Runner button", () => {
    render(<RunnersPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add runner/i })).toBeInTheDocument();
  });

  it("opens Add Runner dialog when Add Runner is clicked", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByRole("button", { name: /add runner/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Runner")).toBeInTheDocument();
  });

  it("shows validation error when name is empty and Save is clicked", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByRole("button", { name: /add runner/i }));
    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new runner and closes dialog", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByRole("button", { name: /add runner/i }));

    const dialog = screen.getByRole("dialog");
    const nameInput = within(dialog).getByPlaceholderText("Full name");
    fireEvent.change(nameInput, { target: { value: "New Test Runner" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("New Test Runner")).toBeInTheDocument();
    });
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByLabelText("Edit runner Anna Lindström"));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Edit Runner")).toBeInTheDocument();

    const nameInput = within(dialog).getByPlaceholderText("Full name") as HTMLInputElement;
    expect(nameInput.value).toBe("Anna Lindström");

    const startTimeInput = within(dialog).getByPlaceholderText("HH:MM:SS") as HTMLInputElement;
    expect(startTimeInput.value).toBe("10:00:00");
  });

  it("updates a runner and closes dialog", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByLabelText("Edit runner Anna Lindström"));

    const dialog = screen.getByRole("dialog");
    const nameInput = within(dialog).getByPlaceholderText("Full name");
    fireEvent.change(nameInput, { target: { value: "Anna Lindström Updated" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Anna Lindström Updated")).toBeInTheDocument();
    });
  });

  it("displays club and class columns from resolved names", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));
    // Anna and Maria both belong to IF Berget and D21E — use getAllByText
    expect(screen.getAllByText("IF Berget").length).toBeGreaterThan(0);
    expect(screen.getAllByText("D21E").length).toBeGreaterThan(0);
  });

  it("opens club SearchableSelect in form and selects a club", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByRole("button", { name: /add runner/i }));

    const dialog = screen.getByRole("dialog");

    // Click the club SearchableSelect button (has placeholder "Select a club")
    const clubBtn = within(dialog).getByText("Select a club").closest("button")!;
    fireEvent.click(clubBtn);

    const listbox = screen.getByRole("listbox");
    const firstOption = within(listbox).getAllByRole("option")[0];
    fireEvent.click(firstOption);

    expect(screen.queryByRole("listbox")).not.toBeInTheDocument();
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByLabelText("Delete runner Anna Lindström"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete runner "Anna Lindström"/i)).toBeInTheDocument();
  });

  it("deletes a runner on confirm", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByLabelText("Delete runner Anna Lindström"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("Anna Lindström")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the runner", async () => {
    render(<RunnersPage />, { wrapper });
    await waitFor(() => screen.getByText("Anna Lindström"));

    fireEvent.click(screen.getByLabelText("Delete runner Anna Lindström"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("Anna Lindström")).toBeInTheDocument();
  });
});
