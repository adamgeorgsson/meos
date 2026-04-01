import { describe, it, expect } from "vitest";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { CoursesPage } from "./CoursesPage";

function wrapper({ children }: { children: React.ReactNode }) {
  const qc = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return (
    <QueryClientProvider client={qc}>
      <MemoryRouter>{children}</MemoryRouter>
    </QueryClientProvider>
  );
}

describe("CoursesPage", () => {
  it("renders the Courses heading", () => {
    render(<CoursesPage />, { wrapper });
    expect(screen.getByRole("heading", { name: "Courses" })).toBeInTheDocument();
  });

  it("shows loading state initially", () => {
    render(<CoursesPage />, { wrapper });
    expect(screen.getByLabelText("Loading")).toBeInTheDocument();
  });

  it("displays courses list after loading", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => {
      expect(screen.getByText("Long")).toBeInTheDocument();
    });
    expect(screen.getByText("Medium")).toBeInTheDocument();
    expect(screen.getByText("Short")).toBeInTheDocument();
  });

  it("shows Add Course button", () => {
    render(<CoursesPage />, { wrapper });
    expect(screen.getByRole("button", { name: /add course/i })).toBeInTheDocument();
  });

  it("opens Add Course dialog when Add Course is clicked", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByRole("button", { name: /add course/i }));

    const dialog = screen.getByRole("dialog");
    expect(dialog).toBeInTheDocument();
    expect(within(dialog).getByText("Add Course")).toBeInTheDocument();
  });

  it("shows validation error when name is empty and Save is clicked", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByRole("button", { name: /add course/i }));
    fireEvent.click(screen.getByRole("button", { name: /^save$/i }));

    await waitFor(() => {
      expect(screen.getByRole("alert")).toBeInTheDocument();
    });
  });

  it("adds a new course and closes dialog", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByRole("button", { name: /add course/i }));

    const nameInput = screen.getByPlaceholderText("Course name");
    fireEvent.change(nameInput, { target: { value: "Test Course" } });

    const lengthInput = screen.getByPlaceholderText("e.g. 5000");
    fireEvent.change(lengthInput, { target: { value: "3000" } });

    fireEvent.click(screen.getByRole("button", { name: /^save$/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Test Course")).toBeInTheDocument();
    });
  });

  it("adds a control to the sequence builder", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByRole("button", { name: /add course/i }));

    // Open the SearchableSelect dropdown in the sequence builder
    const dialog = screen.getByRole("dialog");
    const addControlBtn = within(dialog).getByText("Add a control...");
    fireEvent.click(addControlBtn);

    // Select a control from the dropdown
    const listbox = screen.getByRole("listbox");
    expect(listbox).toBeInTheDocument();
    // Click the first option (control 101 – Fork junction)
    fireEvent.click(within(listbox).getAllByRole("option")[0]);

    // The control should now appear in the sequence
    const sequence = within(dialog).getByLabelText("Control sequence");
    expect(sequence).not.toHaveTextContent("No controls added yet");
  });

  it("removes a control from the sequence", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    // Edit the "Long" course which has controls
    fireEvent.click(screen.getByLabelText("Edit course Long"));

    const dialog = screen.getByRole("dialog");
    const sequence = within(dialog).getByLabelText("Control sequence");

    // Should have some controls
    expect(sequence).not.toHaveTextContent("No controls added yet");

    // Remove the first control
    const removeButtons = within(sequence).getAllByRole("button", { name: /^Remove/i });
    fireEvent.click(removeButtons[0]);

    // Should have one fewer control
    const remainingRemoveButtons = within(sequence).queryAllByRole("button", { name: /^Remove/i });
    expect(remainingRemoveButtons.length).toBe(removeButtons.length - 1);
  });

  it("reorders controls in the sequence", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Edit course Long"));

    const dialog = screen.getByRole("dialog");
    const sequence = within(dialog).getByLabelText("Control sequence");

    // Get initial text content
    const initialText = sequence.textContent ?? "";

    // Move the second control up
    const moveUpButtons = within(sequence).getAllByRole("button", { name: /^Move .* up$/i });
    fireEvent.click(moveUpButtons[1]); // second item's "up" button

    // Content should have changed
    expect(sequence.textContent).not.toBe(initialText);
  });

  it("opens Edit dialog pre-filled when Edit is clicked", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Edit course Long"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Edit Course")).toBeInTheDocument();

    const nameInput = screen.getByPlaceholderText("Course name") as HTMLInputElement;
    expect(nameInput.value).toBe("Long");

    const lengthInput = screen.getByPlaceholderText("e.g. 5000") as HTMLInputElement;
    expect(lengthInput.value).toBe("12500");
  });

  it("updates a course and closes dialog", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Edit course Long"));

    const nameInput = screen.getByPlaceholderText("Course name");
    fireEvent.change(nameInput, { target: { value: "Long Updated" } });

    fireEvent.click(screen.getByRole("button", { name: /^save$/i }));

    await waitFor(() => {
      expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText("Long Updated")).toBeInTheDocument();
    });
  });

  it("opens ConfirmDialog when Delete is clicked", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Delete course Long"));

    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText(/delete course "Long"/i)).toBeInTheDocument();
  });

  it("deletes a course on confirm", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Delete course Long"));
    fireEvent.click(screen.getByRole("button", { name: /confirm/i }));

    await waitFor(() => {
      expect(screen.queryByText("Long")).not.toBeInTheDocument();
    });
  });

  it("cancels delete and keeps the course", async () => {
    render(<CoursesPage />, { wrapper });
    await waitFor(() => screen.getByText("Long"));

    fireEvent.click(screen.getByLabelText("Delete course Long"));
    fireEvent.click(screen.getByRole("button", { name: /cancel/i }));

    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    expect(screen.getByText("Long")).toBeInTheDocument();
  });
});
