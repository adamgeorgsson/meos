export interface Competition {
  id: number;
  name: string;
  date: string;
  organizer: string;
  location: string;
  description?: string;
}

export interface Class {
  id: number;
  name: string;
  courseId?: number;
  startMethod?: string;
}

export interface Course {
  id: number;
  name: string;
  length?: number;
  controls: number[];
}

export interface Control {
  id: number;
  code: number;
  description?: string;
  type?: string;
}

export interface Club {
  id: number;
  name: string;
  country?: string;
}

export interface Runner {
  id: number;
  name: string;
  clubId?: number;
  classId?: number;
  startTime?: string;
  cardNumber?: number;
  status?: string;
}

export interface Team {
  id: number;
  name: string;
  clubId?: number;
  classId?: number;
  members: number[];
}

export interface SplitTime {
  controlId: number;
  time: number;
}

export interface Result {
  id: number;
  runnerId: number;
  classId: number;
  position?: number;
  totalTime?: number;
  status: string;
  splits: SplitTime[];
}

export interface StartListEntry {
  id: number;
  runnerId: number;
  classId: number;
  startTime: string;
  bib?: number;
}

export interface ApiError {
  message: string;
  code?: string;
  status?: number;
}
