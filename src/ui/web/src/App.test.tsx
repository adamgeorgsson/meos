import { render, screen } from "@testing-library/react";
import { expect, test } from "vitest";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { Layout } from "./components/Layout";
import { App } from "./App";

function renderWithProviders(ui: React.ReactElement, { initialUrl = "/" } = {}) {
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return render(
    <QueryClientProvider client={queryClient}>
      <MemoryRouter initialEntries={[initialUrl]}>
        {ui}
      </MemoryRouter>
    </QueryClientProvider>
  );
}

test("renders navigation sidebar", () => {
  renderWithProviders(<Layout />, { initialUrl: "/competition" });
  expect(screen.getByRole("navigation", { name: "Main navigation" })).toBeDefined();
});

test("renders nav links for all routes", () => {
  renderWithProviders(<Layout />, { initialUrl: "/competition" });
  const nav = screen.getByRole("navigation", { name: "Main navigation" });
  expect(nav.textContent).toContain("Competition");
  expect(nav.textContent).toContain("Classes");
  expect(nav.textContent).toContain("Courses");
  expect(nav.textContent).toContain("Controls");
  expect(nav.textContent).toContain("Clubs");
  expect(nav.textContent).toContain("Runners");
  expect(nav.textContent).toContain("Teams");
  expect(nav.textContent).toContain("Results");
  expect(nav.textContent).toContain("Start List");
});

test("root route redirects to /competition", () => {
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  render(
    <QueryClientProvider client={queryClient}>
      <App />
    </QueryClientProvider>
  );
  // The page heading "Competition" confirms the redirect worked
  expect(screen.getByRole("heading", { name: "Competition" })).toBeDefined();
});
