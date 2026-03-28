// oClub.h — Club domain entity.
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

#include <map>
#include "oBase.h"

class oEvent;
class oClub;
class oRunner;
typedef oRunner* pRunner;
class oTeam;
typedef oTeam* pTeam;
typedef oClub* pClub;

class oDataInterface;
class oDataConstInterface;
class xmlparser;
class xmlobject;

class oClub : public oBase {
protected:
  wstring name;
  vector<wstring> altNames;
  wstring tPrettyName;
  wstring tCompactName;

  static map<wstring, wstring> manualCompactNameMap;

  // On Linux wchar_t is 4 bytes; all string fields need double space vs Windows.
  // 768 was the Windows size; use 2048 to hold the same strings on Linux.
  static const int dataSize = 2048;
  int getDISize() const final { return dataSize; }
  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  int tNumRunners = 0;
  int tFee = 0;
  int tPaid = 0;

  virtual int getTableId() const;

  pair<int, bool> inputData(int id, const wstring &input, int inputId,
                            wstring &output, bool noUpdate) override;

  void fillInput(int id, vector<pair<wstring, size_t>> &elements, size_t &selected) override;

  /** Set name internally, and update pretty name */
  void internalSetName(const wstring &n);

  void changeId(int newId);

  /** Get internal data buffers for DI */
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;

  void changedObject();

public:
  static void loadNameMap();

  void nameChanged() { internalSetName(name); }

  int getStartGroup() const;
  void setStartGroup(int sg);

  /** Assign invoice numbers to all clubs. */
  static void assignInvoiceNumber(oEvent &oe, bool reset);

  static int getFirstInvoiceNumber(oEvent &oe);

  /** Remove all clubs from a competition (and all belong-to-club relations) */
  static void clearClubs(oEvent &oe);

  void remove();
  bool canRemove() const;

  void updateFromDB();

  bool operator<(const oClub &c) const;

  wstring getInfo() const;
  bool sameClub(const oClub &c);

  const wstring &getName() const { return name; }
  const wstring &getDisplayName() const { return tPrettyName.empty() ? name : tPrettyName; }
  const wstring &getCompactName() const { return tCompactName.empty() ? name : tCompactName; }

  void setName(const wstring &n);

  void merge(const oBase &input, const oBase *base) final;

  void set(const xmlobject &xo);
  bool write(xmlparser &xml);

  bool isVacant() const;
  oClub(oEvent *poe);
  oClub(oEvent *poe, int id);
  virtual ~oClub();

  friend class oAbstractRunner;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class oClass;
  friend class MeosSQL;
};
