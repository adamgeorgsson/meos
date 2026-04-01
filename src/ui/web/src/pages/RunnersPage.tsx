import { useState } from "react";
import { useForm, Controller } from "react-hook-form";
import { useQueryClient } from "@tanstack/react-query";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Runner, Club, Class } from "../types";
import { useEntities, useCreateEntity, useUpdateEntity, useDeleteEntity } from "../api/hooks";
import { update as apiUpdate } from "../api/client";
import { DataTable } from "../components/DataTable";
import type { ColumnDef } from "../components/DataTable";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";
import { FormSelect } from "../components/FormSelect";
import { SearchableSelect } from "../components/SearchableSelect";
import { ConfirmDialog } from "../components/ConfirmDialog";
import { ImportDialog } from "../components/ImportDialog";

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  clubId: z.string().optional().or(z.literal("")),
  classId: z.string().optional().or(z.literal("")),
  startTime: z.string().optional().or(z.literal("")),
  cardNumber: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

const STATUS_OPTIONS = [
  { value: "ok", label: "OK" },
  { value: "dns", label: "DNS" },
  { value: "dnf", label: "DNF" },
  { value: "mp", label: "MP" },
  { value: "disq", label: "DISQ" },
  { value: "ot", label: "OT" },
];

function parseHMS(s: string): number | null {
  const parts = s.split(":").map(Number);
  if (parts.length === 3 && parts.every((p) => !isNaN(p))) {
    return parts[0] * 3600 + parts[1] * 60 + parts[2];
  }
  return null;
}

function formatHMS(totalSecs: number): string {
  const h = Math.floor(totalSecs / 3600);
  const m = Math.floor((totalSecs % 3600) / 60);
  const s = totalSecs % 60;
  return `${String(h).padStart(2, "0")}:${String(m).padStart(2, "0")}:${String(s).padStart(2, "0")}`;
}

export function RunnersPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Runner | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Runner | null>(null);
  const [importOpen, setImportOpen] = useState(false);

  // Bulk selection state
  const [selectedIds, setSelectedIds] = useState<Set<string | number>>(new Set());
  const [clearTrigger, setClearTrigger] = useState(0);

  // Bulk action dialogs
  const [bulkDialog, setBulkDialog] = useState<"class" | "starttime" | "status" | null>(null);
  const [bulkClassId, setBulkClassId] = useState("");
  const [bulkStatus, setBulkStatus] = useState("ok");
  const [bulkStartTime, setBulkStartTime] = useState("");
  const [bulkInterval, setBulkInterval] = useState("2");

  const queryClient = useQueryClient();

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

  function clearSelection() {
    setSelectedIds(new Set());
    setClearTrigger((n) => n + 1);
  }

  async function executeBulkAssignClass() {
    if (!bulkClassId) return;
    const classId = Number(bulkClassId);
    const ids = Array.from(selectedIds).map(Number);
    await Promise.all(ids.map((id) => apiUpdate<Runner>("runners", id, { classId })));
    setBulkDialog(null);
    setBulkClassId("");
    clearSelection();
    void queryClient.invalidateQueries({ queryKey: ["runners"] });
  }

  async function executeBulkChangeStatus() {
    const ids = Array.from(selectedIds).map(Number);
    await Promise.all(ids.map((id) => apiUpdate<Runner>("runners", id, { status: bulkStatus })));
    setBulkDialog(null);
    clearSelection();
    void queryClient.invalidateQueries({ queryKey: ["runners"] });
  }

  async function executeBulkDrawStartTimes() {
    const start = parseHMS(bulkStartTime);
    if (start === null) return;
    const interval = Number(bulkInterval) * 60;
    if (isNaN(interval) || interval <= 0) return;
    const ids = Array.from(selectedIds).map(Number);
    await Promise.all(
      ids.map((id, i) => apiUpdate<Runner>("runners", id, { startTime: formatHMS(start + i * interval) }))
    );
    setBulkDialog(null);
    setBulkStartTime("");
    clearSelection();
    void queryClient.invalidateQueries({ queryKey: ["runners"] });
  }

  const selectedCount = selectedIds.size;

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-bold">Runners</h1>
        <div className="flex gap-2">
          <button
            onClick={() => setImportOpen(true)}
            className="px-4 py-2 bg-white border border-gray-300 text-gray-700 rounded hover:bg-gray-50 text-sm"
          >
            Import
          </button>
          <button
            onClick={openAdd}
            className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
          >
            Add Runner
          </button>
        </div>
      </div>

      {selectedCount > 0 && (
        <div
          className="flex items-center gap-3 mb-4 px-4 py-2 bg-blue-50 border border-blue-200 rounded"
          aria-label="Bulk actions"
        >
          <span className="text-sm font-medium text-blue-700">
            {selectedCount} runner{selectedCount !== 1 ? "s" : ""} selected
          </span>
          <button
            onClick={() => setBulkDialog("class")}
            className="px-3 py-1 text-sm bg-white border border-gray-300 rounded hover:bg-gray-50"
          >
            Assign Class
          </button>
          <button
            onClick={() => setBulkDialog("starttime")}
            className="px-3 py-1 text-sm bg-white border border-gray-300 rounded hover:bg-gray-50"
          >
            Draw Start Times
          </button>
          <button
            onClick={() => setBulkDialog("status")}
            className="px-3 py-1 text-sm bg-white border border-gray-300 rounded hover:bg-gray-50"
          >
            Change Status
          </button>
          <button
            onClick={clearSelection}
            className="ml-auto text-sm text-gray-500 hover:text-gray-700"
          >
            Clear
          </button>
        </div>
      )}

      <DataTable
        columns={COLUMNS}
        data={runners}
        isLoading={isLoading}
        enableSelection
        getItemId={(r) => r.id}
        onSelectionChange={setSelectedIds}
        clearSelectionTrigger={clearTrigger}
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

      <ImportDialog
        open={importOpen}
        onClose={() => setImportOpen(false)}
        onImported={() => void queryClient.invalidateQueries({ queryKey: ["runners"] })}
        runners={runners}
        clubs={clubs}
        classes={classes}
      />

      {/* Bulk assign class dialog */}
      <FormDialog
        open={bulkDialog === "class"}
        onClose={() => setBulkDialog(null)}
        onSave={() => void executeBulkAssignClass()}
        title={`Assign Class to ${selectedCount} Runner${selectedCount !== 1 ? "s" : ""}`}
        size="sm"
      >
        <FormField label="Class">
          <FormSelect
            options={classOptions}
            placeholder="Select a class"
            value={bulkClassId}
            onChange={(e) => setBulkClassId(e.target.value)}
          />
        </FormField>
      </FormDialog>

      {/* Bulk draw start times dialog */}
      <FormDialog
        open={bulkDialog === "starttime"}
        onClose={() => setBulkDialog(null)}
        onSave={() => void executeBulkDrawStartTimes()}
        title={`Draw Start Times for ${selectedCount} Runner${selectedCount !== 1 ? "s" : ""}`}
        size="sm"
      >
        <div className="space-y-4">
          <FormField label="First Start Time (HH:MM:SS)">
            <FormInput
              value={bulkStartTime}
              onChange={(e) => setBulkStartTime(e.target.value)}
              placeholder="10:00:00"
            />
          </FormField>
          <FormField label="Interval (minutes)">
            <FormInput
              type="number"
              value={bulkInterval}
              onChange={(e) => setBulkInterval(e.target.value)}
              min="1"
            />
          </FormField>
        </div>
      </FormDialog>

      {/* Bulk change status dialog */}
      <FormDialog
        open={bulkDialog === "status"}
        onClose={() => setBulkDialog(null)}
        onSave={() => void executeBulkChangeStatus()}
        title={`Change Status for ${selectedCount} Runner${selectedCount !== 1 ? "s" : ""}`}
        size="sm"
      >
        <FormField label="Status">
          <FormSelect
            options={STATUS_OPTIONS}
            value={bulkStatus}
            onChange={(e) => setBulkStatus(e.target.value)}
          />
        </FormField>
      </FormDialog>
    </div>
  );
}
