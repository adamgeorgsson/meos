import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { TeamsPage } from "./TeamsPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("TeamsPage", () => {
  it("renders the Teams heading", () => {
    render(<TeamsPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Teams" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<TeamsPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays teams list after loading", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Berget Red")).toBeInTheDocument();
    });
    expect(screen.getByText("Älgen Elite")).toBeInTheDocument();
    expect(screen.getByText("Göteborg A")).toBeInTheDocument();
  });

  it("shows Add Team button", () => {
    render(<TeamsPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add team/i })).toBeInTheDocument();
  });

  it("opens Add Team dialog when Add Team is clicked", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByRole("button", { name: /add team/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Team")).toBeInTheDocument();
  });

  it("shows validation error when name is empty and Save is clicked", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByRole("button", { name: /add team/i }));
    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new team and closes dialog", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByRole("button", { name: /add team/i }));

    const dialog = screen.getByRole("dialog");
    const nameInput = within(dialog).getByPlaceholderText("Team name");
    fireEvent.change(nameInput, { target: { value: "New Test Team" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("New Test Team")).toBeInTheDocument();
    });
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByLabelText("Edit team Berget Red"));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Edit Team")).toBeInTheDocument();

    const nameInput = within(dialog).getByPlaceholderText("Team name") as HTMLInputElement;
    expect(nameInput.value).toBe("Berget Red");
  });

  it("updates a team and closes dialog", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByLabelText("Edit team Berget Red"));

    const dialog = screen.getByRole("dialog");
    const nameInput = within(dialog).getByPlaceholderText("Team name");
    fireEvent.change(nameInput, { target: { value: "Berget Red Updated" } });

    fireEvent.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Berget Red Updated")).toBeInTheDocument();
    });
  });

  it("shows existing members when editing a team", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    // Berget Red has members: [2] -> Erik Johansson
    fireEvent.click(screen.getByLabelText("Edit team Berget Red"));

    const dialog = screen.getByRole("dialog");
    await waitFor(() => {
      const memberList = within(dialog).getByLabelText("Team members");
      expect(memberList).toBeInTheDocument();
    });
  });

  it("adds a runner to team members via SearchableSelect", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByRole("button", { name: /add team/i }));

    const dialog = screen.getByRole("dialog");

    // Wait for runners to load (needed for the SearchableSelect options)
    await waitFor(() => {
      expect(within(dialog).getByText("Add a runner...")).toBeInTheDocument();
    });

    const addRunnerBtn = within(dialog).getByText("Add a runner...").closest("button")!;
    fireEvent.click(addRunnerBtn);

    const listbox = screen.getByRole("listbox");
    const firstOption = within(listbox).getAllByRole("option")[0];
    fireEvent.click(firstOption);

    // The member list should now have one entry
    await waitFor(() => {
      const memberList = within(dialog).getByLabelText("Team members");
      expect(memberList).toBeInTheDocument();
    });
  });

  it("removes a runner from team members", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    // Berget Red has members: [2] -> Erik Johansson
    fireEvent.click(screen.getByLabelText("Edit team Berget Red"));

    const dialog = screen.getByRole("dialog");
    await waitFor(() => {
      expect(within(dialog).getByLabelText("Team members")).toBeInTheDocument();
    });

    // Find and click the remove button for Erik Johansson
    const removeBtn = within(dialog).getByLabelText(/remove Erik Johansson/i);
    fireEvent.click(removeBtn);

    await waitFor(() => {
      expect(within(dialog).queryByLabelText("Team members")).not.toBeInTheDocument();
    });
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByLabelText("Delete team Berget Red"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete team "Berget Red"/i)).toBeInTheDocument();
  });

  it("deletes a team on confirm", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByLabelText("Delete team Berget Red"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("Berget Red")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the team", async () => {
    render(<TeamsPage />, { wrapper });
    await waitFor(() => screen.getByText("Berget Red"));

    fireEvent.click(screen.getByLabelText("Delete team Berget Red"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("Berget Red")).toBeInTheDocument();
  });
});
