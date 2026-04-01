import type { Competition, Club, Control, Course, Class, Runner, Team, Result, StartListEntry } from "../types";

function makeDb() {
  const competition: Competition = {
    id: 1,
    name: "Spring Cup 2026",
    date: "2026-05-15",
    organizer: "IF Berget",
    location: "Björnparken, Stockholm",
    description: "Annual spring orienteering competition",
  };

  const clubs: Club[] = [
    { id: 1, name: "IF Berget", country: "SE" },
    { id: 2, name: "OK Älgen", country: "SE" },
    { id: 3, name: "IFK Göteborg SOK", country: "SE" },
    { id: 4, name: "Sundbybergs IK", country: "SE" },
    { id: 5, name: "Tullinge SK", country: "SE" },
  ];

  const controls: Control[] = [
    { id: 1, code: 101, description: "Fork junction", type: "normal" },
    { id: 2, code: 102, description: "Stone wall corner", type: "normal" },
    { id: 3, code: 103, description: "Hill top", type: "normal" },
    { id: 4, code: 104, description: "Stream crossing", type: "normal" },
    { id: 5, code: 105, description: "Marsh edge", type: "normal" },
    { id: 6, code: 31, description: "Start triangle", type: "start" },
    { id: 7, code: 900, description: "Finish", type: "finish" },
  ];

  const courses: Course[] = [
    { id: 1, name: "Long", length: 12500, controls: [6, 1, 2, 3, 4, 5, 7] },
    { id: 2, name: "Medium", length: 7800, controls: [6, 2, 3, 5, 7] },
    { id: 3, name: "Short", length: 4200, controls: [6, 3, 5, 7] },
    { id: 4, name: "Very Short", length: 2100, controls: [6, 5, 7] },
    { id: 5, name: "Ultra Long", length: 18000, controls: [6, 1, 2, 3, 4, 5, 1, 2, 7] },
  ];

  const classes: Class[] = [
    { id: 1, name: "H21E", courseId: 1, startMethod: "individual" },
    { id: 2, name: "D21E", courseId: 1, startMethod: "individual" },
    { id: 3, name: "H21A", courseId: 2, startMethod: "individual" },
    { id: 4, name: "D21A", courseId: 2, startMethod: "individual" },
    { id: 5, name: "H35", courseId: 3, startMethod: "individual" },
  ];

  const runners: Runner[] = [
    { id: 1, name: "Anna Lindström", clubId: 1, classId: 2, startTime: "10:00:00", cardNumber: 2001234, status: "ok" },
    { id: 2, name: "Erik Johansson", clubId: 2, classId: 1, startTime: "10:02:00", cardNumber: 2001235, status: "ok" },
    { id: 3, name: "Maria Karlsson", clubId: 1, classId: 2, startTime: "10:04:00", cardNumber: 2001236, status: "ok" },
    { id: 4, name: "Lars Nilsson", clubId: 3, classId: 1, startTime: "10:06:00", cardNumber: 2001237, status: "dns" },
    { id: 5, name: "Maja Björk", clubId: 4, classId: 3, startTime: "10:08:00", cardNumber: 2001238, status: "ok" },
    { id: 6, name: "Sven Ek", clubId: 5, classId: 3, startTime: "10:10:00", cardNumber: 2001239, status: "dnf" },
  ];

  const teams: Team[] = [
    { id: 1, name: "Berget Red", clubId: 1, classId: 1, members: [2] },
    { id: 2, name: "Älgen Elite", clubId: 2, classId: 1, members: [2] },
    { id: 3, name: "Göteborg A", clubId: 3, classId: 1, members: [4] },
    { id: 4, name: "Sundbyberg 1", clubId: 4, classId: 3, members: [5] },
    { id: 5, name: "Tullinge Masters", clubId: 5, classId: 3, members: [6] },
  ];

  const results: Result[] = [
    {
      id: 1, runnerId: 1, classId: 2, position: 1, totalTime: 4512, status: "ok",
      splits: [{ controlId: 1, time: 1234 }, { controlId: 2, time: 2456 }],
    },
    {
      id: 2, runnerId: 2, classId: 1, position: 1, totalTime: 5823, status: "ok",
      splits: [{ controlId: 1, time: 1500 }, { controlId: 2, time: 3200 }],
    },
    {
      id: 3, runnerId: 3, classId: 2, position: 2, totalTime: 4789, status: "ok",
      splits: [{ controlId: 1, time: 1300 }, { controlId: 2, time: 2700 }],
    },
    { id: 4, runnerId: 4, classId: 1, position: undefined, totalTime: undefined, status: "dns", splits: [] },
    {
      id: 5, runnerId: 5, classId: 3, position: 1, totalTime: 3123, status: "ok",
      splits: [{ controlId: 3, time: 900 }],
    },
    {
      id: 6, runnerId: 6, classId: 3, position: undefined, totalTime: undefined, status: "dnf",
      splits: [{ controlId: 3, time: 850 }],
    },
  ];

  const startList: StartListEntry[] = [
    { id: 1, runnerId: 1, classId: 2, startTime: "10:00:00", bib: 1 },
    { id: 2, runnerId: 2, classId: 1, startTime: "10:02:00", bib: 2 },
    { id: 3, runnerId: 3, classId: 2, startTime: "10:04:00", bib: 3 },
    { id: 4, runnerId: 4, classId: 1, startTime: "10:06:00", bib: 4 },
    { id: 5, runnerId: 5, classId: 3, startTime: "10:08:00", bib: 5 },
    { id: 6, runnerId: 6, classId: 3, startTime: "10:10:00", bib: 6 },
  ];

  return { competition, clubs, controls, courses, classes, runners, teams, results, startList };
}

export const db = makeDb();

export function resetDb(): void {
  const fresh = makeDb();
  Object.assign(db, fresh);
}

export function nextId<T extends { id: number }>(arr: T[]): number {
  return arr.length === 0 ? 1 : Math.max(...arr.map((x) => x.id)) + 1;
}
