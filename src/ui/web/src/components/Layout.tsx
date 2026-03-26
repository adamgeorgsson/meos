import { useState } from "react";
import { NavLink, Outlet } from "react-router-dom";

const NAV_ITEMS = [
  { to: "/competition", label: "Competition" },
  { to: "/classes", label: "Classes" },
  { to: "/courses", label: "Courses" },
  { to: "/controls", label: "Controls" },
  { to: "/clubs", label: "Clubs" },
  { to: "/runners", label: "Runners" },
  { to: "/teams", label: "Teams" },
  { to: "/results", label: "Results" },
  { to: "/startlist", label: "Start List" },
] as const;

function navLinkClass({ isActive }: { isActive: boolean }) {
  return [
    "block px-4 py-2 rounded text-sm font-medium transition-colors",
    isActive
      ? "bg-blue-600 text-white"
      : "text-gray-700 hover:bg-gray-100",
  ].join(" ");
}

export function Layout() {
  const [sidebarOpen, setSidebarOpen] = useState(false);

  return (
    <div className="flex h-screen overflow-hidden">
      {/* Mobile overlay */}
      {sidebarOpen && (
        <div
          className="fixed inset-0 bg-black/40 z-20 md:hidden"
          onClick={() => setSidebarOpen(false)}
          aria-hidden="true"
        />
      )}

      {/* Sidebar */}
      <nav
        className={[
          "fixed top-0 left-0 h-full w-56 bg-white border-r border-gray-200 z-30 flex flex-col transition-transform duration-200",
          "md:static md:translate-x-0",
          sidebarOpen ? "translate-x-0" : "-translate-x-full",
        ].join(" ")}
        aria-label="Main navigation"
      >
        <div className="flex items-center justify-between px-4 py-4 border-b border-gray-200">
          <span className="font-bold text-lg text-blue-700">MeOS</span>
          <button
            className="md:hidden text-gray-500 hover:text-gray-700"
            onClick={() => setSidebarOpen(false)}
            aria-label="Close menu"
          >
            ✕
          </button>
        </div>
        <ul className="flex-1 overflow-y-auto p-2 space-y-1">
          {NAV_ITEMS.map((item) => (
            <li key={item.to}>
              <NavLink
                to={item.to}
                className={navLinkClass}
                onClick={() => setSidebarOpen(false)}
              >
                {item.label}
              </NavLink>
            </li>
          ))}
        </ul>
      </nav>

      {/* Main content */}
      <div className="flex-1 flex flex-col min-w-0 overflow-hidden">
        {/* Mobile header */}
        <header className="md:hidden flex items-center px-4 py-3 bg-white border-b border-gray-200">
          <button
            className="text-gray-600 hover:text-gray-800 mr-3"
            onClick={() => setSidebarOpen(true)}
            aria-label="Open menu"
          >
            <span className="block w-5 h-0.5 bg-current mb-1" />
            <span className="block w-5 h-0.5 bg-current mb-1" />
            <span className="block w-5 h-0.5 bg-current" />
          </button>
          <span className="font-bold text-blue-700">MeOS</span>
        </header>

        <main className="flex-1 overflow-y-auto bg-gray-50">
          <Outlet />
        </main>
      </div>
    </div>
  );
}
