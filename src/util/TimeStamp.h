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

#include <string>

class TimeStamp {
  unsigned int Time;
  mutable std::string stampCode;
  mutable int stampCodeTime = -1;
public:
  TimeStamp();
  virtual ~TimeStamp();

  void setStamp(const std::string& s);

  // Returns "YYYYMMDDhhmmss" for the stored time, clamped to [2009, thisYear].
  const std::string& getStamp() const;

  // Extracts only digit characters from sqlStampIn, resized to actual count written.
  const std::string& getStamp(const std::string& sqlStampIn) const;

  const std::wstring getUpdateTime() const;
  std::wstring getStampString() const;
  std::string getStampStringN() const;

  int getAge() const;
  unsigned int getModificationTime() const { return Time; }

  void update();
  void update(TimeStamp& ts);
};
