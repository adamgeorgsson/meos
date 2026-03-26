import { http, HttpResponse } from "msw";
import { db, nextId } from "./db";

const BASE = "/api/v1";

function notFound() {
  return new HttpResponse(null, { status: 404 });
}

function noContent() {
  return new HttpResponse(null, { status: 204 });
}

export const handlers = [
  // ── Competition ────────────────────────────────────────────────────────────
  http.get(`${BASE}/competitions`, () => HttpResponse.json(db.competition)),

  http.put(`${BASE}/competitions`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    Object.assign(db.competition, body);
    return HttpResponse.json(db.competition);
  }),

  // ── Clubs ──────────────────────────────────────────────────────────────────
  http.get(`${BASE}/clubs`, () => HttpResponse.json(db.clubs)),

  http.get(`${BASE}/clubs/:id`, ({ params }) => {
    const club = db.clubs.find((c) => c.id === Number(params.id));
    return club ? HttpResponse.json(club) : notFound();
  }),

  http.post(`${BASE}/clubs`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.clubs) };
    db.clubs.push(item as (typeof db.clubs)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/clubs/:id`, async ({ params, request }) => {
    const idx = db.clubs.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.clubs[idx] = { ...db.clubs[idx], ...body };
    return HttpResponse.json(db.clubs[idx]);
  }),

  http.delete(`${BASE}/clubs/:id`, ({ params }) => {
    const idx = db.clubs.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    db.clubs.splice(idx, 1);
    return noContent();
  }),

  // ── Controls ───────────────────────────────────────────────────────────────
  http.get(`${BASE}/controls`, () => HttpResponse.json(db.controls)),

  http.get(`${BASE}/controls/:id`, ({ params }) => {
    const item = db.controls.find((c) => c.id === Number(params.id));
    return item ? HttpResponse.json(item) : notFound();
  }),

  http.post(`${BASE}/controls`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.controls) };
    db.controls.push(item as (typeof db.controls)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/controls/:id`, async ({ params, request }) => {
    const idx = db.controls.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.controls[idx] = { ...db.controls[idx], ...body };
    return HttpResponse.json(db.controls[idx]);
  }),

  http.delete(`${BASE}/controls/:id`, ({ params }) => {
    const idx = db.controls.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    db.controls.splice(idx, 1);
    return noContent();
  }),

  // ── Courses ────────────────────────────────────────────────────────────────
  http.get(`${BASE}/courses`, () => HttpResponse.json(db.courses)),

  http.get(`${BASE}/courses/:id`, ({ params }) => {
    const item = db.courses.find((c) => c.id === Number(params.id));
    return item ? HttpResponse.json(item) : notFound();
  }),

  http.post(`${BASE}/courses`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.courses) };
    db.courses.push(item as (typeof db.courses)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/courses/:id`, async ({ params, request }) => {
    const idx = db.courses.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.courses[idx] = { ...db.courses[idx], ...body };
    return HttpResponse.json(db.courses[idx]);
  }),

  http.delete(`${BASE}/courses/:id`, ({ params }) => {
    const idx = db.courses.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    db.courses.splice(idx, 1);
    return noContent();
  }),

  // ── Classes ────────────────────────────────────────────────────────────────
  http.get(`${BASE}/classes`, () => HttpResponse.json(db.classes)),

  http.get(`${BASE}/classes/:id`, ({ params }) => {
    const item = db.classes.find((c) => c.id === Number(params.id));
    return item ? HttpResponse.json(item) : notFound();
  }),

  http.post(`${BASE}/classes`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.classes) };
    db.classes.push(item as (typeof db.classes)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/classes/:id`, async ({ params, request }) => {
    const idx = db.classes.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.classes[idx] = { ...db.classes[idx], ...body };
    return HttpResponse.json(db.classes[idx]);
  }),

  http.delete(`${BASE}/classes/:id`, ({ params }) => {
    const idx = db.classes.findIndex((c) => c.id === Number(params.id));
    if (idx === -1) return notFound();
    db.classes.splice(idx, 1);
    return noContent();
  }),

  // ── Runners ────────────────────────────────────────────────────────────────
  http.get(`${BASE}/runners`, () => HttpResponse.json(db.runners)),

  http.get(`${BASE}/runners/:id`, ({ params }) => {
    const item = db.runners.find((r) => r.id === Number(params.id));
    return item ? HttpResponse.json(item) : notFound();
  }),

  http.post(`${BASE}/runners`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.runners) };
    db.runners.push(item as (typeof db.runners)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/runners/:id`, async ({ params, request }) => {
    const idx = db.runners.findIndex((r) => r.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.runners[idx] = { ...db.runners[idx], ...body };
    return HttpResponse.json(db.runners[idx]);
  }),

  http.delete(`${BASE}/runners/:id`, ({ params }) => {
    const idx = db.runners.findIndex((r) => r.id === Number(params.id));
    if (idx === -1) return notFound();
    db.runners.splice(idx, 1);
    return noContent();
  }),

  http.post(`${BASE}/runners/:id/status`, async ({ params, request }) => {
    const idx = db.runners.findIndex((r) => r.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as { status: string };
    db.runners[idx] = { ...db.runners[idx], status: body.status };
    return HttpResponse.json(db.runners[idx]);
  }),

  // ── Teams ──────────────────────────────────────────────────────────────────
  http.get(`${BASE}/teams`, () => HttpResponse.json(db.teams)),

  http.get(`${BASE}/teams/:id`, ({ params }) => {
    const item = db.teams.find((t) => t.id === Number(params.id));
    return item ? HttpResponse.json(item) : notFound();
  }),

  http.post(`${BASE}/teams`, async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const item = { ...body, id: nextId(db.teams) };
    db.teams.push(item as (typeof db.teams)[0]);
    return HttpResponse.json(item, { status: 201 });
  }),

  http.put(`${BASE}/teams/:id`, async ({ params, request }) => {
    const idx = db.teams.findIndex((t) => t.id === Number(params.id));
    if (idx === -1) return notFound();
    const body = (await request.json()) as Record<string, unknown>;
    db.teams[idx] = { ...db.teams[idx], ...body };
    return HttpResponse.json(db.teams[idx]);
  }),

  http.delete(`${BASE}/teams/:id`, ({ params }) => {
    const idx = db.teams.findIndex((t) => t.id === Number(params.id));
    if (idx === -1) return notFound();
    db.teams.splice(idx, 1);
    return noContent();
  }),

  // ── Results ────────────────────────────────────────────────────────────────
  http.get(`${BASE}/results`, () => HttpResponse.json(db.results)),

  // ── Start List ─────────────────────────────────────────────────────────────
  http.get(`${BASE}/startlist`, () => HttpResponse.json(db.startList)),
];
