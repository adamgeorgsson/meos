import { useState } from "react";
import { useForm, Controller } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Runner, Club, Class } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { SearchableSelect } from "../components/SearchableSelect";
import { ConfirmDialog } from "../components/ConfirmDialog";

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  clubId: z.string().optional().or(z.literal("")),
  classId: z.string().optional().or(z.literal("")),
  startTime: z.string().optional().or(z.literal("")),
  cardNumber: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

export function RunnersPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Runner | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Runner | null>(null);

  const { data: runners = [], isLoading } = useEntities<Runner>("runners");
  const { data: clubs = [] } = useEntities<Club>("clubs");
  const { data: classes = [] } = useEntities<Class>("classes");
  const createMutation = useCreateEntity<Runner>("runners");
  const updateMutation = useUpdateEntity<Runner>("runners");
  const deleteMutation = useDeleteEntity("runners");

  const {
    register,
    handleSubmit,
    reset,
    control,
    formState: { errors },
  } = useForm<FormValues>({
    resolver: zodResolver(schema) as never,
  });

  const clubOptions = clubs.map((c) => ({ value: String(c.id), label: c.name }));
  const classOptions = classes.map((c) => ({ value: String(c.id), label: c.name }));

  const COLUMNS: ColumnDef<Runner>[] = [
    { key: "name", header: "Name", accessor: (r) => r.name },
    {
      key: "club",
      header: "Club",
      accessor: (r) => {
        if (r.clubId == null) return "";
        return clubs.find((c) => c.id === r.clubId)?.name ?? String(r.clubId);
      },
    },
    {
      key: "class",
      header: "Class",
      accessor: (r) => {
        if (r.classId == null) return "";
        return classes.find((c) => c.id === r.classId)?.name ?? String(r.classId);
      },
    },
    { key: "startTime", header: "Start Time", accessor: (r) => r.startTime ?? "" },
    { key: "cardNumber", header: "Card #", accessor: (r) => r.cardNumber != null ? String(r.cardNumber) : "" },
  ];

  function openAdd() {
    setEditing(null);
    reset({ name: "", clubId: "", classId: "", startTime: "", cardNumber: "" });
    setFormOpen(true);
  }

  function openEdit(runner: Runner) {
    setEditing(runner);
    reset({
      name: runner.name,
      clubId: runner.clubId != null ? String(runner.clubId) : "",
      classId: runner.classId != null ? String(runner.classId) : "",
      startTime: runner.startTime ?? "",
      cardNumber: runner.cardNumber != null ? String(runner.cardNumber) : "",
    });
    setFormOpen(true);
  }

  function onSubmit(values: FormValues) {
    const clubId = values.clubId ? Number(values.clubId) : undefined;
    const classId = values.classId ? Number(values.classId) : undefined;
    const cardNumber = values.cardNumber ? Number(values.cardNumber) : undefined;
    const data: Omit<Runner, "id"> = {
      name: values.name,
      ...(clubId != null ? { clubId } : {}),
      ...(classId != null ? { classId } : {}),
      ...(values.startTime ? { startTime: values.startTime } : {}),
      ...(cardNumber != null ? { cardNumber } : {}),
    };
    setFormOpen(false);
    if (editing) {
      updateMutation.mutate({ id: editing.id, data });
    } else {
      createMutation.mutate(data);
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
        <h1 className="text-2xl font-bold">Runners</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Runner
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={runners}
        isLoading={isLoading}
        renderRowActions={(runner) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(runner)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit runner ${runner.name}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(runner)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete runner ${runner.name}`}
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
        title={editing ? "Edit Runner" : "Add Runner"}
        size="md"
      >
        <div className="grid grid-cols-2 gap-4">
          <FormField label="Name" error={errors.name?.message}>
            <FormInput {...register("name")} placeholder="Full name" />
          </FormField>
          <FormField label="Card Number" error={errors.cardNumber?.message}>
            <FormInput {...register("cardNumber")} placeholder="e.g. 2001234" type="text" />
          </FormField>
          <FormField label="Club" error={errors.clubId?.message}>
            <Controller
              name="clubId"
              control={control}
              render={({ field }) => (
                <SearchableSelect
                  options={clubOptions}
                  value={field.value ?? ""}
                  onChange={field.onChange}
                  placeholder="Select a club"
                />
              )}
            />
          </FormField>
          <FormField label="Class" error={errors.classId?.message}>
            <Controller
              name="classId"
              control={control}
              render={({ field }) => (
                <SearchableSelect
                  options={classOptions}
                  value={field.value ?? ""}
                  onChange={field.onChange}
                  placeholder="Select a class"
                />
              )}
            />
          </FormField>
          <FormField label="Start Time" error={errors.startTime?.message}>
            <FormInput {...register("startTime")} placeholder="HH:MM:SS" />
          </FormField>
        </div>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete runner "${deleteTarget?.name ?? ""}"?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
