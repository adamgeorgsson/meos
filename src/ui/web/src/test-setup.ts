import "@testing-library/jest-dom";
import { beforeAll, afterEach, afterAll } from "vitest";
import { server } from "./mocks/server";
import { resetDb } from "./mocks/db";

// jsdom does not implement URL.createObjectURL / revokeObjectURL
Object.defineProperty(URL, "createObjectURL", { configurable: true, writable: true, value: () => "" });
Object.defineProperty(URL, "revokeObjectURL", { configurable: true, writable: true, value: () => {} });

beforeAll(() => server.listen({ onUnhandledRequest: "warn" }));
afterEach(() => {
  server.resetHandlers();
  resetDb();
});
afterAll(() => server.close());
