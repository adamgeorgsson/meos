import { useState } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Control } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { ConfirmDialog } from "../components/ConfirmDialog";

const schema = z.object({
  code: z.number().min(1, "Code is required"),
  description: z.string().optional().or(z.literal("")),
  type: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

const COLUMNS: ColumnDef<Control>[] = [
  { key: "code", header: "Code", accessor: (c) => c.code },
  { key: "description", header: "Description", accessor: (c) => c.description ?? "" },
  { key: "type", header: "Type", accessor: (c) => c.type ?? "" },
];

export function ControlsPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Control | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Control | null>(null);

  const { data: controls = [], isLoading } = useEntities<Control>("controls");
  const createMutation = useCreateEntity<Control>("controls");
  const updateMutation = useUpdateEntity<Control>("controls");
  const deleteMutation = useDeleteEntity("controls");

  const {
    register,
    handleSubmit,
    reset,
    formState: { errors },
  } = useForm<FormValues>({
    resolver: zodResolver(schema) as never,
  });

  function openAdd() {
    setEditing(null);
    reset({ code: NaN, description: "", type: "" });
    setFormOpen(true);
  }

  function openEdit(control: Control) {
    setEditing(control);
    reset({ code: control.code, description: control.description ?? "", type: control.type ?? "" });
    setFormOpen(true);
  }

  function onSubmit(values: FormValues) {
    setFormOpen(false);
    if (editing) {
      updateMutation.mutate({ id: editing.id, data: values });
    } else {
      createMutation.mutate(values as Omit<Control, "id">);
    }
  }

  function confirmDelete() {
    if (deleteTarget) {
      deleteMutation.mutate(deleteTarget.id);
      setDeleteTarget(null);
    }
  }

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-bold">Controls</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Control
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={controls}
        isLoading={isLoading}
        renderRowActions={(control) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(control)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit control ${control.code}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(control)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete control ${control.code}`}
            >
              Delete
            </button>
          </div>
        )}
      />

      <FormDialog
        open={formOpen}
        onClose={() => setFormOpen(false)}
        onSave={() => handleSubmit(onSubmit)()}
        title={editing ? "Edit Control" : "Add Control"}
        size="sm"
      >
        <FormField label="Code" error={errors.code?.message}>
          <FormInput
            {...register("code", { valueAsNumber: true })}
            type="number"
            placeholder="e.g. 101"
          />
        </FormField>
        <FormField label="Description" error={errors.description?.message}>
          <FormInput {...register("description")} placeholder="Optional description" />
        </FormField>
        <FormField label="Type" error={errors.type?.message}>
          <FormInput {...register("type")} placeholder="e.g. normal, start, finish" />
        </FormField>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete control ${deleteTarget?.code ?? ""}?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
