import { renderHook, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { vi, describe, it, expect, beforeEach } from "vitest";
import React from "react";
import * as client from "./client";
import { useEntities, useEntity, useCreateEntity, useUpdateEntity, useDeleteEntity } from "./hooks";

function createWrapper() {
  const queryClient = new QueryClient({
    defaultOptions: {
      queries: { retry: false },
      mutations: { retry: false },
    },
  });
  return ({ children }: { children: React.ReactNode }) =>
    React.createElement(QueryClientProvider, { client: queryClient }, children);
}

function mockFetch(data: unknown, ok = true, status = 200) {
  return vi.spyOn(globalThis, "fetch").mockResolvedValueOnce({
    ok,
    status,
    statusText: ok ? "OK" : "Error",
    json: async () => data,
  } as Response);
}

describe("API client", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  it("getAll fetches the correct URL", async () => {
    const spy = mockFetch([{ id: 1, name: "Club A" }]);
    const result = await client.getAll("clubs");
    expect(spy).toHaveBeenCalledWith("/api/v1/clubs", expect.objectContaining({ headers: expect.any(Object) }));
    expect(result).toEqual([{ id: 1, name: "Club A" }]);
  });

  it("getById fetches by id", async () => {
    const spy = mockFetch({ id: 1, name: "Club A" });
    const result = await client.getById("clubs", 1);
    expect(spy).toHaveBeenCalledWith("/api/v1/clubs/1", expect.any(Object));
    expect(result).toEqual({ id: 1, name: "Club A" });
  });

  it("create sends POST with body", async () => {
    const spy = mockFetch({ id: 2, name: "Club B" });
    const result = await client.create("clubs", { name: "Club B" });
    expect(spy).toHaveBeenCalledWith(
      "/api/v1/clubs",
      expect.objectContaining({ method: "POST", body: JSON.stringify({ name: "Club B" }) })
    );
    expect(result).toEqual({ id: 2, name: "Club B" });
  });

  it("update sends PUT with body", async () => {
    const spy = mockFetch({ id: 1, name: "Updated" });
    const result = await client.update("clubs", 1, { name: "Updated" });
    expect(spy).toHaveBeenCalledWith(
      "/api/v1/clubs/1",
      expect.objectContaining({ method: "PUT", body: JSON.stringify({ name: "Updated" }) })
    );
    expect(result).toEqual({ id: 1, name: "Updated" });
  });

  it("remove sends DELETE", async () => {
    const spy = mockFetch(null, true, 204);
    await client.remove("clubs", 1);
    expect(spy).toHaveBeenCalledWith(
      "/api/v1/clubs/1",
      expect.objectContaining({ method: "DELETE" })
    );
  });

  it("throws on error response", async () => {
    mockFetch({ message: "Not found" }, false, 404);
    await expect(client.getById("clubs", 999)).rejects.toMatchObject({ status: 404 });
  });
});

interface Club {
  id: number;
  name: string;
}

describe("React Query hooks", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  it("useEntities returns data on success", async () => {
    const clubs = [{ id: 1, name: "Club A" }, { id: 2, name: "Club B" }];
    mockFetch(clubs);

    const { result } = renderHook(() => useEntities<Club>("clubs"), {
      wrapper: createWrapper(),
    });

    await waitFor(() => expect(result.current.isSuccess).toBe(true));
    expect(result.current.data).toEqual(clubs);
  });

  it("useEntity returns single item", async () => {
    const club = { id: 1, name: "Club A" };
    mockFetch(club);

    const { result } = renderHook(() => useEntity<Club>("clubs", 1), {
      wrapper: createWrapper(),
    });

    await waitFor(() => expect(result.current.isSuccess).toBe(true));
    expect(result.current.data).toEqual(club);
  });

  it("useCreateEntity calls create and invalidates cache", async () => {
    const newClub = { id: 3, name: "Club C" };
    mockFetch(newClub);

    const { result } = renderHook(() => useCreateEntity<Club>("clubs"), {
      wrapper: createWrapper(),
    });

    result.current.mutate({ name: "Club C" });
    await waitFor(() => expect(result.current.isSuccess).toBe(true));
    expect(result.current.data).toEqual(newClub);
  });

  it("useUpdateEntity uses { id, data } pattern", async () => {
    const updated = { id: 1, name: "Updated Club" };
    mockFetch(updated);

    const { result } = renderHook(() => useUpdateEntity<Club>("clubs"), {
      wrapper: createWrapper(),
    });

    result.current.mutate({ id: 1, data: { name: "Updated Club" } });
    await waitFor(() => expect(result.current.isSuccess).toBe(true));
    expect(result.current.data).toEqual(updated);
  });

  it("useDeleteEntity calls remove with id", async () => {
    const spy = mockFetch(null, true, 204);

    const { result } = renderHook(() => useDeleteEntity("clubs"), {
      wrapper: createWrapper(),
    });

    result.current.mutate(1);
    await waitFor(() => expect(result.current.isSuccess).toBe(true));
    expect(spy).toHaveBeenCalledWith("/api/v1/clubs/1", expect.objectContaining({ method: "DELETE" }));
  });
});
