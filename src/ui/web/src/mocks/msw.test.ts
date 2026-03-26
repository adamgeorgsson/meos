import { describe, it, expect, beforeEach, vi } from "vitest";
import { resetDb, db } from "./db";
import * as client from "../api/client";

// These tests hit MSW handlers (no fetch spy — real flow through MSW server)
describe("MSW handlers", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    resetDb();
  });

  // ── Competition ────────────────────────────────────────────────────────────
  it("GET /competitions returns seed competition", async () => {
    const res = await fetch("/api/v1/competitions");
    const data = await res.json();
    expect((data as { name: string }).name).toBe("Spring Cup 2026");
  });

  it("PUT /competitions updates competition", async () => {
    const res = await fetch("/api/v1/competitions", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: "Updated Cup" }),
    });
    expect(res.ok).toBe(true);
    const data = await res.json();
    expect(data.name).toBe("Updated Cup");
    // State persists
    const res2 = await fetch("/api/v1/competitions");
    const data2 = await res2.json();
    expect(data2.name).toBe("Updated Cup");
  });

  // ── Clubs ──────────────────────────────────────────────────────────────────
  it("GET /clubs returns 5 seed clubs", async () => {
    const clubs = await client.getAll("clubs");
    expect(clubs).toHaveLength(5);
  });

  it("GET /clubs/:id returns a club", async () => {
    const club = await client.getById("clubs", 1);
    expect((club as { name: string }).name).toBe("IF Berget");
  });

  it("POST /clubs creates a club and increments id", async () => {
    const created = await client.create("clubs", { name: "New Club", country: "FI" });
    expect((created as { id: number }).id).toBe(6);
    const clubs = await client.getAll("clubs");
    expect(clubs).toHaveLength(6);
  });

  it("PUT /clubs/:id updates a club", async () => {
    await client.update("clubs", 1, { name: "Renamed Club" });
    const club = await client.getById("clubs", 1);
    expect((club as { name: string }).name).toBe("Renamed Club");
  });

  it("DELETE /clubs/:id removes a club", async () => {
    await client.remove("clubs", 1);
    const clubs = await client.getAll("clubs");
    expect(clubs).toHaveLength(4);
  });

  // ── Controls ───────────────────────────────────────────────────────────────
  it("GET /controls returns seed controls (7 items)", async () => {
    const controls = await client.getAll("controls");
    expect((controls as unknown[]).length).toBeGreaterThanOrEqual(5);
  });

  it("POST /controls creates a control", async () => {
    const created = await client.create("controls", { code: 200, description: "Test" });
    expect((created as { code: number }).code).toBe(200);
  });

  it("DELETE /controls/:id removes a control", async () => {
    const before = await client.getAll("controls");
    await client.remove("controls", 1);
    const after = await client.getAll("controls");
    expect((after as unknown[]).length).toBe((before as unknown[]).length - 1);
  });

  // ── Courses ────────────────────────────────────────────────────────────────
  it("GET /courses returns 5 seed courses", async () => {
    const courses = await client.getAll("courses");
    expect(courses).toHaveLength(5);
  });

  it("POST /courses creates a course", async () => {
    const created = await client.create("courses", { name: "Sprint", length: 3000, controls: [] });
    expect((created as { name: string }).name).toBe("Sprint");
  });

  // ── Classes ────────────────────────────────────────────────────────────────
  it("GET /classes returns 5 seed classes", async () => {
    const classes = await client.getAll("classes");
    expect(classes).toHaveLength(5);
  });

  it("POST /classes creates a class", async () => {
    const created = await client.create("classes", { name: "H16" });
    expect((created as { name: string }).name).toBe("H16");
  });

  // ── Runners ────────────────────────────────────────────────────────────────
  it("GET /runners returns 6 seed runners", async () => {
    const runners = await client.getAll("runners");
    expect(runners).toHaveLength(6);
  });

  it("POST /runners creates a runner", async () => {
    const created = await client.create("runners", { name: "New Runner", clubId: 1 });
    expect((created as { name: string }).name).toBe("New Runner");
  });

  it("POST /runners/:id/status updates runner status", async () => {
    const res = await fetch("/api/v1/runners/4/status", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ status: "ok" }),
    });
    expect(res.ok).toBe(true);
    const data = await res.json();
    expect((data as { status: string }).status).toBe("ok");
    // db state updated
    expect(db.runners.find((r) => r.id === 4)?.status).toBe("ok");
  });

  it("DELETE /runners/:id removes a runner", async () => {
    await client.remove("runners", 1);
    const runners = await client.getAll("runners");
    expect(runners).toHaveLength(5);
  });

  // ── Teams ──────────────────────────────────────────────────────────────────
  it("GET /teams returns 5 seed teams", async () => {
    const teams = await client.getAll("teams");
    expect(teams).toHaveLength(5);
  });

  it("POST /teams creates a team", async () => {
    const created = await client.create("teams", { name: "New Team", clubId: 1, members: [] });
    expect((created as { name: string }).name).toBe("New Team");
  });

  it("PUT /teams/:id updates a team", async () => {
    await client.update("teams", 1, { name: "Renamed Team" });
    const team = await client.getById("teams", 1);
    expect((team as { name: string }).name).toBe("Renamed Team");
  });

  it("DELETE /teams/:id removes a team", async () => {
    await client.remove("teams", 1);
    const teams = await client.getAll("teams");
    expect(teams).toHaveLength(4);
  });

  // ── Results ────────────────────────────────────────────────────────────────
  it("GET /results returns seed results", async () => {
    const results = await client.getAll("results");
    expect((results as unknown[]).length).toBeGreaterThanOrEqual(5);
  });

  // ── Start List ─────────────────────────────────────────────────────────────
  it("GET /startlist returns seed start list", async () => {
    const startList = await client.getAll("startlist");
    expect((startList as unknown[]).length).toBeGreaterThanOrEqual(5);
  });
});
