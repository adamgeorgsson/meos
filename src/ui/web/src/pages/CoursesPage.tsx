import { useState } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Course, Control } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { ConfirmDialog } from "../components/ConfirmDialog";
import { ControlSequenceBuilder } from "../components/ControlSequenceBuilder";

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  length: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

const COLUMNS: ColumnDef<Course>[] = [
  { key: "name", header: "Name", accessor: (c) => c.name },
  { key: "length", header: "Length (m)", accessor: (c) => (c.length != null ? String(c.length) : "") },
  { key: "controls", header: "Controls", accessor: (c) => String(c.controls.length) },
];

export function CoursesPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Course | null>(null);
  const [controlIds, setControlIds] = useState<number[]>([]);
  const [deleteTarget, setDeleteTarget] = useState<Course | null>(null);

  const { data: courses = [], isLoading } = useEntities<Course>("courses");
  const { data: controls = [] } = useEntities<Control>("controls");
  const createMutation = useCreateEntity<Course>("courses");
  const updateMutation = useUpdateEntity<Course>("courses");
  const deleteMutation = useDeleteEntity("courses");

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
    reset({ name: "", length: "" });
    setControlIds([]);
    setFormOpen(true);
  }

  function openEdit(course: Course) {
    setEditing(course);
    reset({ name: course.name, length: course.length != null ? String(course.length) : "" });
    setControlIds([...course.controls]);
    setFormOpen(true);
  }

  function onSubmit(values: FormValues) {
    const length = values.length ? Number(values.length) : undefined;
    const data = { name: values.name, ...(length != null ? { length } : {}), controls: controlIds };
    setFormOpen(false);
    if (editing) {
      updateMutation.mutate({ id: editing.id, data });
    } else {
      createMutation.mutate(data as Omit<Course, "id">);
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
        <h1 className="text-2xl font-bold">Courses</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Course
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={courses}
        isLoading={isLoading}
        renderRowActions={(course) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(course)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit course ${course.name}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(course)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete course ${course.name}`}
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
        title={editing ? "Edit Course" : "Add Course"}
        size="lg"
      >
        <FormField label="Name" error={errors.name?.message}>
          <FormInput {...register("name")} placeholder="Course name" />
        </FormField>
        <FormField label="Length (m)" error={errors.length?.message}>
          <FormInput {...register("length")} type="number" placeholder="e.g. 5000" />
        </FormField>
        <FormField label="Control sequence">
          <ControlSequenceBuilder
            controlIds={controlIds}
            onControlIdsChange={setControlIds}
            controls={controls}
          />
        </FormField>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete course "${deleteTarget?.name ?? ""}"?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
