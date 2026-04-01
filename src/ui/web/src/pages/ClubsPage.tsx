import { useState } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Club } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { ConfirmDialog } from "../components/ConfirmDialog";

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  country: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

const COLUMNS: ColumnDef<Club>[] = [
  { key: "name", header: "Name", accessor: (c) => c.name },
  { key: "country", header: "Country", accessor: (c) => c.country ?? "" },
];

export function ClubsPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Club | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Club | null>(null);

  const { data: clubs = [], isLoading } = useEntities<Club>("clubs");
  const createMutation = useCreateEntity<Club>("clubs");
  const updateMutation = useUpdateEntity<Club>("clubs");
  const deleteMutation = useDeleteEntity("clubs");

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
    reset({ name: "", country: "" });
    setFormOpen(true);
  }

  function openEdit(club: Club) {
    setEditing(club);
    reset({ name: club.name, country: club.country ?? "" });
    setFormOpen(true);
  }

  function onSubmit(values: FormValues) {
    setFormOpen(false);
    if (editing) {
      updateMutation.mutate({ id: editing.id, data: values });
    } else {
      createMutation.mutate(values as Omit<Club, "id">);
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
        <h1 className="text-2xl font-bold">Clubs</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Club
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={clubs}
        isLoading={isLoading}
        renderRowActions={(club) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(club)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit club ${club.name}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(club)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete club ${club.name}`}
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
        title={editing ? "Edit Club" : "Add Club"}
        size="sm"
      >
        <FormField label="Name" error={errors.name?.message}>
          <FormInput {...register("name")} placeholder="Club name" />
        </FormField>
        <FormField label="Country" error={errors.country?.message}>
          <FormInput {...register("country")} placeholder="e.g. SE" />
        </FormField>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete club "${deleteTarget?.name ?? ""}"?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
