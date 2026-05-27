#pragma once

#include "oBase.h"
#include "oDataContainer.h"
#include <map>
#include <vector>

class oEvent;

class oClub : public oBase {
protected:
  wstring name;
  vector<wstring> altNames;
  wstring tPrettyName;    // "Skid o OK" → "SOK" variant
  wstring tCompactName;   // stripped-down display name

  static map<wstring, wstring> manualCompactNameMap;

  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;

  static oDataContainer& container();

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const final { return 0; }

  // Recomputes tPrettyName and tCompactName from `name` and ShortName field.
  void internalSetName(const wstring& n);

  void changedObject() override {}

public:
  const wstring& getName() const { return name; }
  const wstring& getDisplayName() const { return tPrettyName.empty() ? name : tPrettyName; }
  const wstring& getCompactName() const { return tCompactName.empty() ? name : tCompactName; }

  void setName(const wstring& n);

  // Trigger compact-name recompute (call after ShortName data field changes).
  void nameChanged() { internalSetName(name); }

  wstring getInfo() const override { return L"Club: " + name; }

  bool sameClub(const oClub& c) const;
  bool operator<(const oClub& c) const;

  int getStartGroup() const;
  void setStartGroup(int sg);

  bool isVacant() const;

  void remove() override;
  bool canRemove() const override;

  // Assign sequential invoice numbers to all clubs in oe.Clubs that lack one.
  static void assignInvoiceNumber(oEvent& oe, bool reset);
  static int getFirstInvoiceNumber(oEvent& oe);

  // Read/write the invoice date stored on the event (simple wstring stub).
  static wstring getInvoiceDate(oEvent& oe);
  static void setInvoiceDate(oEvent& oe, const wstring& date);

  void merge(const oBase& input, const oBase* base) final {}

  explicit oClub(oEvent* poe);
  oClub(oEvent* poe, int id);
  virtual ~oClub();

  friend class oEvent;
};

using pClub = oClub*;
