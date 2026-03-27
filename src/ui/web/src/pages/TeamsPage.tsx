import { useState } from "react";
import { useForm, Controller } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Team, Club, Class, Runner } from "../types";
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
});

type FormValues = z.infer<typeof schema>;

export function TeamsPage() {
  const [formOpen, setFormOpen] = useState(false);
  const [editing, setEditing] = useState<Team | null>(null);
  const [deleteTarget, setDeleteTarget] = useState<Team | null>(null);
  const [memberIds, setMemberIds] = useState<number[]>([]);

  const { data: teams = [], isLoading } = useEntities<Team>("teams");
  const { data: clubs = [] } = useEntities<Club>("clubs");
  const { data: classes = [] } = useEntities<Class>("classes");
  const { data: runners = [] } = useEntities<Runner>("runners");
  const createMutation = useCreateEntity<Team>("teams");
  const updateMutation = useUpdateEntity<Team>("teams");
  const deleteMutation = useDeleteEntity("teams");

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
  const runnerOptions = runners
    .filter((r) => !memberIds.includes(r.id))
    .map((r) => ({ value: String(r.id), label: r.name }));

  const COLUMNS: ColumnDef<Team>[] = [
    { key: "name", header: "Name", accessor: (t) => t.name },
    {
      key: "club",
      header: "Club",
      accessor: (t) => {
        if (t.clubId == null) return "";
        return clubs.find((c) => c.id === t.clubId)?.name ?? String(t.clubId);
      },
    },
    {
      key: "class",
      header: "Class",
      accessor: (t) => {
        if (t.classId == null) return "";
        return classes.find((c) => c.id === t.classId)?.name ?? String(t.classId);
      },
    },
    {
      key: "members",
      header: "Members",
      accessor: (t) => String(t.members.length),
    },
  ];

  function openAdd() {
    setEditing(null);
    setMemberIds([]);
    reset({ name: "", clubId: "", classId: "" });
    setFormOpen(true);
  }

  function openEdit(team: Team) {
    setEditing(team);
    setMemberIds([...team.members]);
    reset({
      name: team.name,
      clubId: team.clubId != null ? String(team.clubId) : "",
      classId: team.classId != null ? String(team.classId) : "",
    });
    setFormOpen(true);
  }

  function addMember(runnerId: string) {
    if (!runnerId) return;
    const id = Number(runnerId);
    if (!memberIds.includes(id)) {
      setMemberIds((prev) => [...prev, id]);
    }
  }

  function removeMember(runnerId: number) {
    setMemberIds((prev) => prev.filter((id) => id !== runnerId));
  }

  function onSubmit(values: FormValues) {
    const clubId = values.clubId ? Number(values.clubId) : undefined;
    const classId = values.classId ? Number(values.classId) : undefined;
    const data: Omit<Team, "id"> = {
      name: values.name,
      ...(clubId != null ? { clubId } : {}),
      ...(classId != null ? { classId } : {}),
      members: memberIds,
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
        <h1 className="text-2xl font-bold">Teams</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Add Team
        </button>
      </div>

      <DataTable
        columns={COLUMNS}
        data={teams}
        isLoading={isLoading}
        renderRowActions={(team) => (
          <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
            <button
              onClick={() => openEdit(team)}
              className="px-2 py-1 text-xs bg-blue-100 text-blue-700 rounded hover:bg-blue-200"
              aria-label={`Edit team ${team.name}`}
            >
              Edit
            </button>
            <button
              onClick={() => setDeleteTarget(team)}
              className="px-2 py-1 text-xs bg-red-100 text-red-700 rounded hover:bg-red-200"
              aria-label={`Delete team ${team.name}`}
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
        title={editing ? "Edit Team" : "Add Team"}
        size="md"
      >
        <div className="grid grid-cols-2 gap-4">
          <div className="col-span-2">
            <FormField label="Name" error={errors.name?.message}>
              <FormInput {...register("name")} placeholder="Team name" />
            </FormField>
          </div>
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
        </div>

        <div className="mt-4">
          <FormField label="Members">
            <SearchableSelect
              options={runnerOptions}
              value=""
              onChange={addMember}
              placeholder="Add a runner..."
            />
          </FormField>
          {memberIds.length > 0 && (
            <ul aria-label="Team members" className="mt-2 space-y-1">
              {memberIds.map((runnerId) => {
                const runner = runners.find((r) => r.id === runnerId);
                return (
                  <li
                    key={runnerId}
                    className="flex items-center justify-between px-2 py-1 bg-gray-50 rounded text-sm"
                  >
                    <span>{runner?.name ?? `Runner ${runnerId}`}</span>
                    <button
                      type="button"
                      onClick={() => removeMember(runnerId)}
                      className="text-red-500 hover:text-red-700 text-xs"
                      aria-label={`Remove ${runner?.name ?? `runner ${runnerId}`} from team`}
                    >
                      ×
                    </button>
                  </li>
                );
              })}
            </ul>
          )}
        </div>
      </FormDialog>

      <ConfirmDialog
        open={deleteTarget !== null}
        message={`Delete team "${deleteTarget?.name ?? ""}"?`}
        onConfirm={confirmDelete}
        onCancel={() => setDeleteTarget(null)}
      />
    </div>
  );
}
