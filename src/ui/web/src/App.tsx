import { BrowserRouter, Routes, Route, Navigate } from "react-router-dom";
import { Layout } from "./components/Layout";
import { CompetitionPage } from "./pages/CompetitionPage";
import { ClassesPage } from "./pages/ClassesPage";
import { CoursesPage } from "./pages/CoursesPage";
import { ControlsPage } from "./pages/ControlsPage";
import { ClubsPage } from "./pages/ClubsPage";
import { RunnersPage } from "./pages/RunnersPage";
import { TeamsPage } from "./pages/TeamsPage";
import { ResultsPage } from "./pages/ResultsPage";
import { StartListPage } from "./pages/StartListPage";

export function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Layout />}>
          <Route index element={<Navigate to="/competition" replace />} />
          <Route path="competition" element={<CompetitionPage />} />
          <Route path="classes" element={<ClassesPage />} />
          <Route path="courses" element={<CoursesPage />} />
          <Route path="controls" element={<ControlsPage />} />
          <Route path="clubs" element={<ClubsPage />} />
          <Route path="runners" element={<RunnersPage />} />
          <Route path="teams" element={<TeamsPage />} />
          <Route path="results" element={<ResultsPage />} />
          <Route path="startlist" element={<StartListPage />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}
