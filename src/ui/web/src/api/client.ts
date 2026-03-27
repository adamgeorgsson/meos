const BASE_URL = (import.meta as { env?: { VITE_API_BASE_URL?: string } }).env
  ?.VITE_API_BASE_URL ?? "/api/v1";

async function request<T>(url: string, options?: RequestInit): Promise<T> {
  const response = await fetch(url, {
    headers: {
      "Content-Type": "application/json",
      ...options?.headers,
    },
    ...options,
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ message: response.statusText }));
    const apiError = {
      message: (error as { message?: string }).message ?? response.statusText,
      status: response.status,
    };
    throw apiError;
  }

  if (response.status === 204) {
    return undefined as T;
  }

  return response.json() as Promise<T>;
}

export function getAll<T>(endpoint: string): Promise<T[]> {
  return request<T[]>(`${BASE_URL}/${endpoint}`);
}

export function getById<T>(endpoint: string, id: number): Promise<T> {
  return request<T>(`${BASE_URL}/${endpoint}/${id}`);
}

export function create<T>(endpoint: string, data: unknown): Promise<T> {
  return request<T>(`${BASE_URL}/${endpoint}`, {
    method: "POST",
    body: JSON.stringify(data),
  });
}

export function update<T>(endpoint: string, id: number, data: unknown): Promise<T> {
  return request<T>(`${BASE_URL}/${endpoint}/${id}`, {
    method: "PUT",
    body: JSON.stringify(data),
  });
}

export function remove(endpoint: string, id: number): Promise<void> {
  return request<void>(`${BASE_URL}/${endpoint}/${id}`, {
    method: "DELETE",
  });
}
