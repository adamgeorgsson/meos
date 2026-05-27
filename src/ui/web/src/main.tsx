import { createRoot } from "react-dom/client";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { App } from "./App";
import "./index.css";

async function bootstrap() {
  if (import.meta.env.DEV && !import.meta.env.VITE_USE_BACKEND) {
    const { worker } = await import("./mocks/browser");
    await worker.start({ onUnhandledRequest: "warn" });
  }

  const queryClient = new QueryClient();

  createRoot(document.getElementById("root")!).render(
    <QueryClientProvider client={queryClient}>
      <App />
    </QueryClientProvider>
  );
}

void bootstrap();
