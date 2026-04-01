import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import { FormDialog } from "./FormDialog";
import { FormField } from "./FormField";
import { FormInput } from "./FormInput";
import { FormSelect } from "./FormSelect";
import { SearchableSelect } from "./SearchableSelect";
import { ConfirmDialog } from "./ConfirmDialog";

// --- FormDialog ---
describe("FormDialog", () => {
  it("renders when open is true", () => {
    render(
      <FormDialog open title="Test Dialog" onClose={vi.fn()} onSave={vi.fn()}>
        <p>Content</p>
      </FormDialog>
    );
    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Test Dialog")).toBeInTheDocument();
    expect(screen.getByText("Content")).toBeInTheDocument();
  });

  it("does not render when open is false", () => {
    render(
      <FormDialog
        open={false}
        title="Test Dialog"
        onClose={vi.fn()}
        onSave={vi.fn()}
      >
        <p>Content</p>
      </FormDialog>
    );
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
  });

  it("calls onClose when Cancel is clicked", () => {
    const onClose = vi.fn();
    render(
      <FormDialog open title="Test" onClose={onClose} onSave={vi.fn()}>
        content
      </FormDialog>
    );
    fireEvent.click(screen.getByText("Cancel"));
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("calls onSave when Save is clicked", () => {
    const onSave = vi.fn();
    render(
      <FormDialog open title="Test" onClose={vi.fn()} onSave={onSave}>
        content
      </FormDialog>
    );
    fireEvent.click(screen.getByText("Save"));
    expect(onSave).toHaveBeenCalledOnce();
  });

  it("calls onClose when close button is clicked", () => {
    const onClose = vi.fn();
    render(
      <FormDialog open title="Test" onClose={onClose} onSave={vi.fn()}>
        content
      </FormDialog>
    );
    fireEvent.click(screen.getByLabelText("Close dialog"));
    expect(onClose).toHaveBeenCalledOnce();
  });

  it("applies size classes", () => {
    const { container } = render(
      <FormDialog open title="Test" onClose={vi.fn()} onSave={vi.fn()} size="lg">
        content
      </FormDialog>
    );
    expect(container.querySelector(".max-w-lg")).toBeInTheDocument();
  });
});

// --- FormField ---
describe("FormField", () => {
  it("renders label", () => {
    render(
      <FormField label="Name">
        <input />
      </FormField>
    );
    expect(screen.getByText("Name")).toBeInTheDocument();
  });

  it("renders error message when provided", () => {
    render(
      <FormField label="Name" error="Name is required">
        <input />
      </FormField>
    );
    expect(screen.getByRole("alert")).toHaveTextContent("Name is required");
  });

  it("does not render error when not provided", () => {
    render(
      <FormField label="Name">
        <input />
      </FormField>
    );
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });
});

// --- FormInput ---
describe("FormInput", () => {
  it("renders an input element", () => {
    render(<FormInput placeholder="Enter value" />);
    expect(screen.getByPlaceholderText("Enter value")).toBeInTheDocument();
  });

  it("forwards props to the input", () => {
    render(<FormInput type="number" defaultValue={42} />);
    const input = screen.getByDisplayValue("42");
    expect(input).toHaveAttribute("type", "number");
  });

  it("integrates with react-hook-form register", () => {
    function TestForm() {
      const { register } = useForm<{ name: string }>();
      return <FormInput {...register("name")} placeholder="Name" />;
    }
    render(<TestForm />);
    const input = screen.getByPlaceholderText("Name");
    fireEvent.change(input, { target: { value: "Alice" } });
    expect(input).toHaveValue("Alice");
  });
});

// --- FormSelect ---
describe("FormSelect", () => {
  const options = [
    { value: "a", label: "Option A" },
    { value: "b", label: "Option B" },
  ];

  it("renders all options", () => {
    render(<FormSelect options={options} />);
    expect(screen.getByRole("combobox")).toBeInTheDocument();
    expect(screen.getByText("Option A")).toBeInTheDocument();
    expect(screen.getByText("Option B")).toBeInTheDocument();
  });

  it("renders placeholder option", () => {
    render(<FormSelect options={options} placeholder="Select one" />);
    expect(screen.getByText("Select one")).toBeInTheDocument();
  });

  it("values are strings", () => {
    render(<FormSelect options={options} defaultValue="b" />);
    expect(screen.getByRole("combobox")).toHaveValue("b");
  });

  it("integrates with react-hook-form register", () => {
    function TestForm() {
      const { register } = useForm<{ opt: string }>();
      return <FormSelect options={options} {...register("opt")} />;
    }
    render(<TestForm />);
    const select = screen.getByRole("combobox");
    fireEvent.change(select, { target: { value: "b" } });
    expect(select).toHaveValue("b");
  });
});

// --- SearchableSelect ---
describe("SearchableSelect", () => {
  const options = [
    { value: "1", label: "Alice" },
    { value: "2", label: "Bob" },
    { value: "3", label: "Charlie" },
  ];

  it("renders the selected label", () => {
    render(
      <SearchableSelect options={options} value="2" onChange={vi.fn()} />
    );
    expect(screen.getByText("Bob")).toBeInTheDocument();
  });

  it("shows placeholder when no value selected", () => {
    render(
      <SearchableSelect
        options={options}
        value=""
        onChange={vi.fn()}
        placeholder="Pick a runner"
      />
    );
    expect(screen.getByText("Pick a runner")).toBeInTheDocument();
  });

  it("opens dropdown on button click", () => {
    render(
      <SearchableSelect options={options} value="" onChange={vi.fn()} />
    );
    fireEvent.click(screen.getByRole("button"));
    expect(screen.getByRole("listbox")).toBeInTheDocument();
  });

  it("filters options based on search query", () => {
    render(
      <SearchableSelect options={options} value="" onChange={vi.fn()} />
    );
    fireEvent.click(screen.getByRole("button"));
    fireEvent.change(screen.getByLabelText("Search options"), {
      target: { value: "ali" },
    });
    expect(screen.getByText("Alice")).toBeInTheDocument();
    expect(screen.queryByText("Bob")).not.toBeInTheDocument();
  });

  it("calls onChange when option is selected", () => {
    const onChange = vi.fn();
    render(<SearchableSelect options={options} value="" onChange={onChange} />);
    fireEvent.click(screen.getByRole("button"));
    fireEvent.click(screen.getByText("Charlie"));
    expect(onChange).toHaveBeenCalledWith("3");
  });

  it("closes dropdown after selection", () => {
    render(
      <SearchableSelect options={options} value="" onChange={vi.fn()} />
    );
    fireEvent.click(screen.getByRole("button"));
    fireEvent.click(screen.getByText("Alice"));
    expect(screen.queryByRole("listbox")).not.toBeInTheDocument();
  });
});

// --- ConfirmDialog ---
describe("ConfirmDialog", () => {
  it("renders when open is true", () => {
    render(
      <ConfirmDialog
        open
        message="Are you sure?"
        onConfirm={vi.fn()}
        onCancel={vi.fn()}
      />
    );
    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(screen.getByText("Are you sure?")).toBeInTheDocument();
  });

  it("does not render when open is false", () => {
    render(
      <ConfirmDialog
        open={false}
        message="Are you sure?"
        onConfirm={vi.fn()}
        onCancel={vi.fn()}
      />
    );
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
  });

  it("calls onConfirm when Confirm is clicked", () => {
    const onConfirm = vi.fn();
    render(
      <ConfirmDialog
        open
        message="Delete this?"
        onConfirm={onConfirm}
        onCancel={vi.fn()}
      />
    );
    fireEvent.click(screen.getByText("Confirm"));
    expect(onConfirm).toHaveBeenCalledOnce();
  });

  it("calls onCancel when Cancel is clicked", () => {
    const onCancel = vi.fn();
    render(
      <ConfirmDialog
        open
        message="Delete this?"
        onConfirm={vi.fn()}
        onCancel={onCancel}
      />
    );
    fireEvent.click(screen.getByText("Cancel"));
    expect(onCancel).toHaveBeenCalledOnce();
  });
});

// --- Integration with zod + react-hook-form ---
describe("Form integration with zod", () => {
  const schema = z.object({
    name: z.string().min(1, "Name is required"),
    age: z.number().min(0),
    role: z.string(),
    note: z.string().optional().or(z.literal("")),
  });

  type FormData = z.infer<typeof schema>;

  function TestForm({
    onSubmit,
  }: {
    onSubmit: (data: FormData) => void;
  }) {
    const {
      register,
      handleSubmit,
      formState: { errors },
    } = useForm<FormData>({
      resolver: zodResolver(schema) as never,
    });

    return (
      <form onSubmit={handleSubmit(onSubmit)}>
        <FormField label="Name" error={errors.name?.message}>
          <FormInput {...register("name")} />
        </FormField>
        <FormField label="Age" error={errors.age?.message}>
          <FormInput type="number" {...register("age", { valueAsNumber: true })} />
        </FormField>
        <FormField label="Role" error={errors.role?.message}>
          <FormSelect
            options={[
              { value: "admin", label: "Admin" },
              { value: "user", label: "User" },
            ]}
            {...register("role")}
          />
        </FormField>
        <FormField label="Note" error={errors.note?.message}>
          <FormInput {...register("note")} />
        </FormField>
        <button type="submit">Submit</button>
      </form>
    );
  }

  it("validates required fields and calls onSubmit with valid data", async () => {
    const onSubmit = vi.fn();
    render(<TestForm onSubmit={onSubmit} />);

    const nameInput = document.querySelector('input[name="name"]') as HTMLInputElement;
    const ageInput = document.querySelector('input[name="age"]') as HTMLInputElement;

    fireEvent.change(nameInput, { target: { value: "Alice" } });
    fireEvent.change(ageInput, { target: { value: "25" } });
    fireEvent.click(screen.getByText("Submit"));

    // After a tick, onSubmit should be called (no validation errors for valid data)
    await new Promise((r) => setTimeout(r, 0));
    expect(onSubmit).toHaveBeenCalledWith(
      expect.objectContaining({ name: "Alice", age: 25 }),
      expect.anything()
    );
  });

  it("shows validation errors for empty required fields", async () => {
    const onSubmit = vi.fn();
    render(<TestForm onSubmit={onSubmit} />);
    fireEvent.click(screen.getByText("Submit"));
    await new Promise((r) => setTimeout(r, 0));
    expect(await screen.findByText("Name is required")).toBeInTheDocument();
    expect(onSubmit).not.toHaveBeenCalled();
  });
});
