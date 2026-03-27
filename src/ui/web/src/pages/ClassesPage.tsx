import { useState } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Class, Course } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { FormSelect } from "../components/FormSelect";
import { ConfirmDialog } from "../components/ConfirmDialog";

const START_METHODS = [
  { value: "individual", label: "Individual" },
  { value: "mass", label: "Mass start" },
  { value: "pursuit", label: "Pursuit" },
  { value: "interval", label: "Interval" },
];

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  courseId: z.string().optional().or(z.literal("")),
  startMethod: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

export function ClassesPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Class | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Class | null>(null);

  const { data: classes = [], isLoading } = useEntities<Class>("classes");
  const { data: courses = [] } = useEntities<Course>("courses");
  const createMutation = useCreateEntity<Class>("classes");
  const updateMutation = useUpdateEntity<Class>("classes");
  const deleteMutation = useDeleteEntity("classes");

  const {
    register,
    handleSubmit,
    reset,
    formState: { errors },
  } = useForm<FormValues>({
    resolver: zodResolver(schema) as never,
  });

  const courseOptions = courses.map((c) => ({ value: String(c.id), label: c.name }));

  const COLUMNS: ColumnDef<Class>[] = [
    { key: "name", header: "Name", accessor: (c) => c.name },
    {
      key: "course",
      header: "Course",
      accessor: (c) => {
        if (c.courseId == null) return "";
        const course = courses.find((co) => co.id === c.courseId);
        return course?.name ?? String(c.courseId);
      },
    },
    { key: "startMethod", header: "Start Method", accessor: (c) => c.startMethod ?? "" },
  ];

  function openAdd() {
    setEditing(null);
    reset({ name: "", courseId: "", startMethod: "" });
    setFormOpen(true);
  }

  function openEdit(cls: Class) {
    setEditing(cls);
    reset({
      name: cls.name,
      courseId: cls.courseId != null ? String(cls.courseId) : "",
      startMethod: cls.startMethod ?? "",
    });
    setFormOpen(true);
  }

  function onSubmit(values: FormValues) {
    const courseId = values.courseId ? Number(values.courseId) : undefined;
    const startMethod = values.startMethod || undefined;
    const data = {
      name: values.name,
      ...(courseId != null ? { courseId } : {}),
      ...(startMethod ? { startMethod } : {}),
    };
    setFormOpen(false);
    if (editing) {
      updateMutation.mutate({ id: editing.id, data });
    } else {
      createMutation.mutate(data as Omit<Class, "id">);
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
        <h1 className="text-2xl font-bold">Classes</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Class
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={classes}
        isLoading={isLoading}
        renderRowActions={(cls) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(cls)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit class ${cls.name}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(cls)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete class ${cls.name}`}
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
        title={editing ? "Edit Class" : "Add Class"}
        size="sm"
      >
        <FormField label="Name" error={errors.name?.message}>
          <FormInput {...register("name")} placeholder="Class name" />
        </FormField>
        <FormField label="Course" error={errors.courseId?.message}>
          <FormSelect
            {...register("courseId")}
            options={courseOptions}
            placeholder="Select a course"
          />
        </FormField>
        <FormField label="Start Method" error={errors.startMethod?.message}>
          <FormSelect
            {...register("startMethod")}
            options={START_METHODS}
            placeholder="Select start method"
          />
        </FormField>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete class "${deleteTarget?.name ?? ""}"?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
