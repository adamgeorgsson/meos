// oFreePunch.h — Migrated from legacy code/oFreePunch.h (US-003f).
// Cross-platform, no Win32 dependencies.
#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
************************************************************************/

#include "oBase.h"
#include "oPunch.h"

class oFreePunch;
typedef oFreePunch* pFreePunch;
class oRunner;
typedef oRunner* pRunner;
class Table;
class xmlparser;
class xmlobject;

class oFreePunch final : public oPunch {
protected:
  int CardNo;
  int iHashType; // Index type used for lookup
  int tRunnerId; // Id of runner the punch is classified to.

  /** Class used to sort punches by time. */
  class FreePunchComp {
  public:
    bool operator()(pFreePunch a, pFreePunch b) {
      return a->getTimeInt() < b->getTimeInt();
    }
  };

  void changedObject();

public:

  static const shared_ptr<Table>& getTable(oEvent* oe);

  // Get control hash (itype) from course control id and race number
  static int getControlHash(int courseControlId, int race);

  // Get controlId or courseControlId from hash (itype)
  static int getControlIdFromHash(int hash, bool courseControlId);

  // Get the id of the control currently tied to this punch
  int getControlId() const override { return getControlIdFromHash(iHashType, false); }

  // Get the id of the course control currently tied to this punch
  int getCourseControlId() const { return getControlIdFromHash(iHashType, true); }

  // Get the id hash
  int getIHashType() const { return iHashType; }

  // Get the runner currently tied to this punch
  pRunner getTiedRunner() const;
  void addTableRow(Table& table) const;
  void fillInput(int id, vector<pair<wstring, size_t>>& out, size_t& selected) override;
  pair<int, bool> inputData(int id, const wstring& input, int inputId, wstring& output, bool noUpdate) override;

  void remove();
  bool canRemove() const;

  int getCardNo() const { return CardNo; }
  bool setCardNo(int cardNo, bool databaseUpdate = false);
  bool setType(const wstring& t, bool databaseUpdate = false);
  void setTimeInt(int newTime, bool databaseUpdate) final;

  static void rehashPunches(oEvent& oe, int cardNo, pFreePunch newPunch);
  static bool disableHashing;

  void merge(const oBase& input, const oBase* base) final;

  oFreePunch(oEvent* poe, int card, int time, int type, int unit);
  oFreePunch(oEvent* poe, int id);
  virtual ~oFreePunch();

  void Set(const xmlobject* xo);
  bool Write(xmlparser& xml);

  friend class oEvent;
  friend class oRunner;
  friend class MeosSQL;
};
