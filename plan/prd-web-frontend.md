# PRD: MeOS Web Frontend

## Introduction

This PRD covers the React + TypeScript web frontend that replaces MeOS's Win32/GDI desktop GUI. The frontend is a standalone Single Page Application (SPA) that communicates exclusively through the JSON REST API — it has **zero dependency on the legacy `code/` directory** and can be developed entirely in parallel with the C++ migration work.

### Context

MeOS currently uses a Win32/GDI GUI built around a custom `gdioutput` wrapper with tab-based navigation (`TabRunner`, `TabClass`, etc.). The new web frontend mirrors this tab structure but uses modern web technologies, making it accessible from any browser on any platform.

The frontend connects to the C++ backend via `http://localhost:<port>/api/v1/...` endpoints. During development, a Vite dev server with API proxying enables frontend work without a running C++ backend (using mock data or a stub server).

### Constraints

- All development happens in `src/ui/web/` — no changes to `code/` or other `src/` directories
- Communication with the backend is exclusively through the JSON REST API (no direct C++ calls)
- The API contract (endpoint paths, request/response JSON shapes) is the only coupling point
- Production builds must output static files that the C++ server can serve (US-011 in main PRD)
- The frontend must work without any build-time dependency on the C++ codebase

## Goals

- Provide a modern, responsive web interface for competition management
- Mirror the existing tab-based workflow for familiarity (Competition, Classes, Courses, Controls, Clubs, Runners, Teams, Results)
- Support tablet use for field operations
- Enable frontend development fully decoupled from C++ backend work

## Codebase Patterns (from Previous Runs)

These patterns were discovered during previous Ralph runs and should be followed:

- Use `const object + type` pattern for enums (Vite 7 compatibility)
- Use `import type` for all TypeScript interfaces/types
- Backend: XML API at `/meos`, Frontend: JSON REST API at `/api/v1`
- Use `NavLink` for active route highlighting
- Tailwind 4 for styling (CSS-first approach)
- Use generic `DataTable` component for entity lists with sorting, filtering, and pagination.
- Use `zod` for form validation and `react-hook-form` for form management.
- Reuse standard form components (`FormField`, `FormInput`, `FormSelect`, `SearchableSelect`) for consistent styling and validation.
- Use `size` property on `FormDialog` ('sm', 'md', 'lg', 'xl') to handle complex forms with varying width requirements.
- `DataTable` supports row selection with `enableSelection` prop.

## User Stories

### US-007: React + TypeScript Web GUI Shell

**Description:** As a user, I want a modern web-based interface so that I can manage competitions from any browser on any platform. The React + Vite project is already scaffolded (`src/ui/web/`). This story adds routing, layout, and API client.

**Acceptance Criteria:**
- [ ] Routing with React Router (tabs map to routes: `/runners`, `/classes`, `/clubs`, etc.)
- [ ] API client layer with typed interfaces matching REST API
- [ ] Basic layout: navigation sidebar/tabs + content area
- [ ] Responsive design (works on tablets for field use)
- [ ] Production build outputs static files that the C++ server serves

**Implementation Notes:**
- Use a component library (e.g., Radix UI, shadcn/ui) for consistent, accessible UI components
- API client should be generated from or match the API contract types
- Vite proxy config for development: `proxy: { '/api': 'http://localhost:<port>' }`
- Consider a mock API layer (MSW or similar) for frontend-only development

### US-008: Web GUI — Competition Management

**Description:** As a competition organizer, I want to create and configure competitions, manage classes, courses, and clubs through the web interface.

**Acceptance Criteria:**
- [ ] Create/open/save competition
- [ ] CRUD interface for classes (name, course assignment, start method)
- [ ] CRUD interface for courses (name, length, controls)
- [ ] CRUD interface for clubs
- [ ] CRUD interface for controls
- [ ] Form validation with user-friendly error messages

**Implementation Notes:**
- Each entity type maps to a tab/route
- Table views should support sorting, filtering, and inline editing for efficiency
- Form validation should provide immediate feedback (client-side) with server-side validation as backup
- Course editor should include a control sequence builder (drag-and-drop or ordered list)

**Learnings from Previous Runs:**
- `useMutation` only takes one argument for `mutate`. Options must be passed to `useMutation` hook itself, or handled via `async/await` in the caller.
- `z.coerce.number()` might cause type inference issues with `zodResolver`; sometimes `valueAsNumber: true` in `register` with `z.number()` is cleaner.
- `FormSelect` values are always strings; remember to convert back to numbers if needed for the API.
- `DataTable` needs `isLoading` state to avoid flashing "No data" while fetching.
- `FormDialog` needed more flexibility for width; added a `size` prop which is useful for complex editors.
- `ControlSequenceBuilder` uses `SearchableSelect` for adding items, which works well for potentially large lists of controls.
- Hover states (`opacity-0 group-hover:opacity-100`) provide a clean UI for per-item actions in a list.
- The DataTable and FormDialog components are highly reusable for standard CRUD entities — follow the established pattern.
- Standardized the `useUpdateEntity` hook pattern: use `{ id, data }` object for update mutations.
- Optional string fields in zod should use `.optional().or(z.literal(''))` if the form might return an empty string.

### US-009: Web GUI — Runner & Team Management

**Description:** As a competition organizer, I want to manage runners and teams through the web interface.

> **Note:** Split into separate stories for basic CRUD, import, and bulk operations — each has different complexity.

#### US-009a: Runner & Team CRUD

**Description:** Basic create/read/update/delete for runners and teams.

**Acceptance Criteria:**
- [ ] CRUD interface for runners (name, club, class, start time, card number)
- [ ] CRUD interface for teams (name, club, class, members)
- [ ] Search/filter runners by name, club, class
- [ ] Form validation with user-friendly error messages

**Implementation Notes:**
- Runner list is the most data-heavy view — needs virtualized scrolling for large competitions (1000+ runners)
- Club and class fields should use searchable dropdowns with typeahead

**Learnings from Previous Runs:**
- `SearchableSelect` is essential for entities with large numbers of related items (like Clubs and Classes).
- Standardizing hook patterns (like the `{ id, data }` pattern for updates) prevents implementation errors.
- Using a grid layout in `FormDialog` (`grid-cols-2`) is better for forms with many fields to avoid excessive vertical scrolling.
- `FormDialog` and `ConfirmDialog` use `open` instead of `isOpen`, and `FormDialog` uses `onSave` instead of `onSubmit`.
- `handleSubmit` from `react-hook-form` returns a function that expects an optional event; when used in a custom `onSave` handler, it must be called explicitly as `() => handleSubmit(onSubmit)()`.
- Used `as any` for `zodResolver` to bypass complex type inference issues between Zod's coerced types and the expected form values — pragmatic for rapid development but should be investigated for a cleaner fix.

#### US-009b: Import Runners

**Description:** Import runners from external data sources.

**Acceptance Criteria:**
- [ ] Import runners from CSV
- [ ] Import runners from IOF XML
- [ ] Preview before import with conflict detection
- [ ] Error reporting for invalid data

**Implementation Notes:**
- CSV parsing can happen client-side (Papa Parse or similar)
- IOF XML parsing may be better handled server-side with a dedicated upload endpoint
- Preview step should show a diff-like view: new runners, updated runners, conflicts

**Learnings from Previous Runs:**
- `DataTable` does not use a `pagination` object prop; it expects `pageSize` directly.
- CSV header mapping should be flexible (e.g., supporting "Name", "name", "Full Name").
- Using a multi-step dialog (Upload -> Preview -> Import) provides a much better UX than immediate import.
- MSW handlers for bulk operations and file uploads were essential for testing this feature without a real backend.

#### US-009c: Bulk Operations

**Description:** Efficient operations on multiple runners at once.

**Acceptance Criteria:**
- [ ] Select multiple runners
- [ ] Bulk assign class
- [ ] Bulk assign start times (draw)
- [ ] Bulk status changes

**Implementation Notes:**
- Checkbox selection in table views
- Bulk operations should show a confirmation dialog with the number of affected runners

**Learnings from Previous Runs:**
- `DataTable` needs a stable `getItemId` prop (defaulting to `id`) to track selection correctly across filtering and sorting.
- Using a dedicated selection state (`selectedItems`) in `DataTable` makes it easy to implement bulk actions in the parent page.

### US-010: Web GUI — Results & Live View

**Description:** As a competition organizer, I want to view results and start lists in the web interface.

> **Note:** Split into basic view vs. real-time updates vs. export — each is a distinct capability.

#### US-010a: Results & Start List Views

**Description:** Static display of results and start lists.

**Acceptance Criteria:**
- [ ] Results view per class with split times
- [ ] Start list view per class
- [ ] Print-friendly result formatting

**Implementation Notes:**
- Split time display should use standard orienteering formatting (time behind leader, +/- optimal)
- Print styles via CSS `@media print`
- Consider a dedicated print layout component

#### US-010b: Live Results

**Description:** Auto-updating results for live competition monitoring.

**Acceptance Criteria:**
- [ ] Auto-refresh for live results (polling initially)
- [ ] Visual indication of recent changes
- [ ] Configurable refresh interval

**Implementation Notes:**
- Start with polling (`setInterval` + `fetch`), upgrade to WebSocket/SSE later if needed
- Highlight recently changed rows with a fade animation
- Default refresh interval: 10 seconds, configurable in settings

**Learnings from Previous Runs:**
- Silent refresh (not setting `isLoading`) is crucial for live updates to avoid flickering the entire UI.
- Tracking previous state with `Map` and `useEffect` is an effective way to detect changes in a list.
- Using CSS transitions (`transition-colors duration-1000`) makes the highlight fade-out smooth.

#### US-010c: Results Export

**Description:** Export results in standard formats.

**Acceptance Criteria:**
- [ ] Export results as CSV
- [ ] Export results as IOF XML
- [ ] Export start lists

**Implementation Notes:**
- CSV export can be client-side (generate and trigger download)
- IOF XML export should call a backend endpoint that generates the XML
- Provide download buttons in the results/start list views

**Learnings from Previous Runs:**
- `Papa.unparse` is a reliable way to generate CSV from objects.
- Blobs and temporary `<a>` elements are the standard way to trigger client-side downloads.
- Reusing the API client for export URLs keeps the endpoint logic centralized.

## Functional Requirements

- FR-1: The frontend must be a standalone React + TypeScript SPA with no C++ build dependencies
- FR-2: All data access must go through the JSON REST API (`/api/v1/...`)
- FR-3: The frontend must work with Vite dev server (proxy to backend) and as static files served by the C++ server
- FR-4: The UI must be responsive and usable on tablets (min 768px width)
- FR-5: Form validation must provide immediate user feedback
- FR-6: Table views must support sorting, filtering, and pagination
- FR-7: The production build must output to a directory that CMake can bundle with the executable

## Non-Goals

- Native mobile app (responsive web only)
- Offline mode / PWA (may be added later)
- Dark mode (nice-to-have, not required initially)
- Map rendering or course visualization
- SportIdent hardware UI (deferred)
- Speaker/announcer view (deferred)
- Multi-language frontend i18n (backend handles localization initially)

## Design Considerations

- Mirror the existing MeOS tab structure: Competition, Classes, Courses, Controls, Clubs, Runners, Teams, Results, Lists
- Use a component library (Radix UI, shadcn/ui) for consistent, accessible components
- Table views are the primary interaction pattern — optimize for data-heavy displays
- The UI should work well on tablets (orienteering events often use tablets in the field)
- Use TypeScript strict mode for type safety
- API client types should be the single source of truth for data shapes

## Technical Considerations

### Project Structure

```
src/ui/web/
├── src/
│   ├── api/           # API client, types, hooks
│   ├── components/    # Shared UI components
│   ├── pages/         # Route-level components (one per tab)
│   │   ├── Competition.tsx
│   │   ├── Classes.tsx
│   │   ├── Courses.tsx
│   │   ├── Controls.tsx
│   │   ├── Clubs.tsx
│   │   ├── Runners.tsx
│   │   ├── Teams.tsx
│   │   ├── Results.tsx
│   │   └── StartList.tsx
│   ├── hooks/         # Custom React hooks
│   ├── types/         # TypeScript type definitions
│   └── App.tsx
├── package.json
├── tsconfig.json
├── vite.config.ts
└── vitest.config.ts
```

### API Contract (Key Endpoints)

The frontend depends on these REST API endpoints (defined in the main PRD as US-005/US-006):

```
GET/POST       /api/v1/clubs
GET/PUT/DELETE /api/v1/clubs/{id}
GET/POST       /api/v1/controls
GET/PUT/DELETE /api/v1/controls/{id}
GET/POST       /api/v1/courses
GET/PUT/DELETE /api/v1/courses/{id}
GET/POST       /api/v1/classes
GET/PUT/DELETE /api/v1/classes/{id}
GET/POST       /api/v1/runners
GET/PUT/DELETE /api/v1/runners/{id}
GET/POST       /api/v1/teams
GET/PUT/DELETE /api/v1/teams/{id}
GET/PUT         /api/v1/competitions
GET             /api/v1/results
GET             /api/v1/startlist
POST            /api/v1/cards
POST            /api/v1/runners/{id}/status
```

### Development Workflow

The frontend can be developed independently using:
1. **Vite dev server** with hot module replacement
2. **Mock API** (MSW or json-server) for frontend-only development
3. **Proxy to real backend** when C++ server is available

This means frontend work can start immediately and run fully in parallel with backend/migration work.

### Testing

- **Vitest** for unit tests
- **React Testing Library** for component tests
- **MSW** (Mock Service Worker) for API mocking in tests
- ESLint + Prettier for code quality

## Dependency Order

```
US-007 (GUI shell)     — start here, no backend needed
US-008 (competition)   — after US-007, needs mock API
US-009a (runner CRUD)  — after US-007, parallel with US-008
US-009b (import)       — after US-009a
US-009c (bulk ops)     — after US-009a
US-010a (results view) — after US-007, parallel with US-008/009
US-010b (live results) — after US-010a
US-010c (export)       — after US-010a
```

US-007 is the foundation. After that, US-008, US-009a, and US-010a can be developed in parallel.

## Success Metrics

- All CRUD operations work through the web interface for every entity type
- Competition workflow (create → configure → add runners → view results) works end-to-end via the web GUI
- UI is responsive and usable on tablets (768px+)
- Vitest test suite passes with good coverage on components and API client
- Production build produces static files under 2MB (gzipped)
- Page load time under 2 seconds on a local connection
