import { useState } from "react";
import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import type { Competition } from "../types";
import { FormDialog } from "../components/FormDialog";
import { FormField } from "../components/FormField";
import { FormInput } from "../components/FormInput";

const BASE_URL = "/api/v1";

async function fetchCompetition(): Promise<Competition> {
  const res = await fetch(`${BASE_URL}/competitions`);
  if (!res.ok) throw new Error("Failed to fetch competition");
  return res.json() as Promise<Competition>;
}

async function saveCompetition(data: Omit<Competition, "id">): Promise<Competition> {
  const res = await fetch(`${BASE_URL}/competitions`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });
  if (!res.ok) throw new Error("Failed to save competition");
  return res.json() as Promise<Competition>;
}

const schema = z.object({
  name: z.string().min(1, "Name is required"),
  date: z.string().min(1, "Date is required"),
  organizer: z.string().optional().or(z.literal("")),
  location: z.string().optional().or(z.literal("")),
  description: z.string().optional().or(z.literal("")),
});

type FormValues = z.infer<typeof schema>;

function InfoRow({ label, value }: { label: string; value?: string }) {
  return (
    <div className="flex flex-col sm:flex-row sm:items-baseline gap-1 py-3 border-b last:border-b-0">
      <dt className="text-sm font-medium text-gray-500 sm:w-40 shrink-0">{label}</dt>
      <dd className="text-sm text-gray-900">{value || <span className="text-gray-400 italic">—</span>}</dd>
    </div>
  );
}

export function CompetitionPage() {
  const [editOpen, setEditOpen] = useState(false);
  const queryClient = useQueryClient();

  const { data: competition, isLoading, isError } = useQuery<Competition>({
    queryKey: ["competition"],
    queryFn: fetchCompetition,
  });

  const mutation = useMutation({
    mutationFn: saveCompetition,
    onSuccess: () => {
      void queryClient.invalidateQueries({ queryKey: ["competition"] });
      setEditOpen(false);
    },
  });

  const {
    register,
    handleSubmit,
    reset,
    formState: { errors },
  } = useForm<FormValues>({
    resolver: zodResolver(schema) as never,
  });

  function openEdit() {
    if (competition) {
      reset({
        name: competition.name,
        date: competition.date,
        organizer: competition.organizer ?? "",
        location: competition.location ?? "",
        description: competition.description ?? "",
      });
    }
    setEditOpen(true);
  }

  function onSubmit(values: FormValues) {
    mutation.mutate(values as Omit<Competition, "id">);
  }

  if (isLoading) {
    return (
      <div className="p-6">
        <h1 className="text-2xl font-bold mb-6">Competition</h1>
        <p className="text-gray-500">Loading…</p>
      </div>
    );
  }

  if (isError || !competition) {
    return (
      <div className="p-6">
        <h1 className="text-2xl font-bold mb-6">Competition</h1>
        <p className="text-red-600">Failed to load competition data.</p>
      </div>
    );
  }

  return (
    <div className="p-6 max-w-2xl">
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-bold">Competition</h1>
        <button
          onClick={openEdit}
          className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 text-sm"
        >
          Edit
        </button>
      </div>

      <div className="bg-white border rounded-lg px-6 py-2">
        <dl>
          <InfoRow label="Name" value={competition.name} />
          <InfoRow label="Date" value={competition.date} />
          <InfoRow label="Organizer" value={competition.organizer} />
          <InfoRow label="Location" value={competition.location} />
          <InfoRow label="Description" value={competition.description} />
        </dl>
      </div>

      <FormDialog
        open={editOpen}
        onClose={() => setEditOpen(false)}
        onSave={() => handleSubmit(onSubmit)()}
        title="Edit Competition"
        size="md"
      >
        <FormField label="Name" error={errors.name?.message}>
          <FormInput {...register("name")} placeholder="Competition name" />
        </FormField>
        <FormField label="Date" error={errors.date?.message}>
          <FormInput {...register("date")} type="date" />
        </FormField>
        <FormField label="Organizer" error={errors.organizer?.message}>
          <FormInput {...register("organizer")} placeholder="Organizing club" />
        </FormField>
        <FormField label="Location" error={errors.location?.message}>
          <FormInput {...register("location")} placeholder="Venue / area" />
        </FormField>
        <FormField label="Description" error={errors.description?.message}>
          <FormInput {...register("description")} placeholder="Optional description" />
        </FormField>
      </FormDialog>
    </div>
  );
}
