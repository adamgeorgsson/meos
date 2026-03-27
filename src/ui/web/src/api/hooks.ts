import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query";
import * as client from "./client";

export function useEntities<T>(
  endpoint: string,
  options?: { refetchInterval?: number | false }
) {
  return useQuery<T[]>({
    queryKey: [endpoint],
    queryFn: () => client.getAll<T>(endpoint),
    ...options,
  });
}

export function useEntity<T>(endpoint: string, id: number) {
  return useQuery<T>({
    queryKey: [endpoint, id],
    queryFn: () => client.getById<T>(endpoint, id),
  });
}

export function useCreateEntity<T>(endpoint: string) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (data: Omit<T, "id">) => client.create<T>(endpoint, data),
    onSuccess: () => {
      void queryClient.invalidateQueries({ queryKey: [endpoint] });
    },
  });
}

export function useUpdateEntity<T>(endpoint: string) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: ({ id, data }: { id: number; data: Partial<T> }) =>
      client.update<T>(endpoint, id, data),
    onSuccess: () => {
      void queryClient.invalidateQueries({ queryKey: [endpoint] });
    },
  });
}

export function useDeleteEntity(endpoint: string) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (id: number) => client.remove(endpoint, id),
    onSuccess: () => {
      void queryClient.invalidateQueries({ queryKey: [endpoint] });
    },
  });
}
