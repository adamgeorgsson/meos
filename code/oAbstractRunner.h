#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include <vector>

enum RunnerStatus {
  StatusOK = 1, StatusDNS = 20, StatusCANCEL = 21, StatusOutOfCompetition = 15, StatusMP = 3,
  StatusDNF = 4, StatusDQ = 5, StatusMAX = 6, StatusNoTiming = 2,
  StatusUnknown = 0, StatusNotCompeting = 99
};

enum class DynamicRunnerStatus {
  StatusInactive,
  StatusActive,
  StatusFinished
};

/** Returns true for a status that might or might not indicate a result. */
template<int dummy=0>
bool isPossibleResultStatus(RunnerStatus st) {
  return st == StatusNoTiming || st == StatusOutOfCompetition;
}

template<int dummy=0>
std::vector<RunnerStatus> getAllRunnerStatus() {
  return { StatusOK, StatusDNS, StatusCANCEL, StatusOutOfCompetition, StatusMP,
           StatusDNF, StatusDQ, StatusMAX,
           StatusUnknown, StatusNotCompeting , StatusNoTiming};
}

template<int dummy = 0>
bool showResultTime(RunnerStatus st, int time) {
  return st == StatusOK || (st == StatusOutOfCompetition && time > 0);
}

enum SortOrder {
  ClassStartTime,
  ClassTeamLeg,
  ClassResult,
  ClassDefaultResult,
  ClassCourseResult,
  ClassTotalResult,
  ClassTeamLegResult,
  ClassFinishTime,
  ClassStartTimeClub,
  ClassPoints,
  ClassLiveResult,
  ClassKnockoutTotalResult,
  SortByName,
  SortByLastName,
  SortByFinishTime,
  SortByFinishTimeReverse,
  SortByStartTime,
  SortByStartTimeClass,
  CourseResult,
  CourseStartTime,
  SortByEntryTime,
  ClubClassStartTime,
  SortByBib,
  Custom,
  SortEnumLastItem
};

static bool orderByClass(SortOrder so) {
  switch (so) {
  case ClassStartTime:
  case ClassTeamLeg:
  case ClassResult:
  case ClassDefaultResult:
  case ClassCourseResult:
  case ClassTotalResult:
  case ClassTeamLegResult:
  case ClassFinishTime:
  case ClassStartTimeClub:
  case ClassPoints:
  case ClassLiveResult:
  case ClassKnockoutTotalResult:
    return true;
  }
  return false;
}
