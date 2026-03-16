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

// Domain header: automation machine base types.
// Extracted from TabAuto.h so domain code can use AutoMachine/Machines without
// pulling in Tab* UI headers. TabAuto.h continues to include this header.

#include "gdioutput.h"
#include "oListInfo.h"
#include "meosexception.h"
#include <string>
#include <memory>
#include <cstdint>

using namespace std;

class TabAuto; // forward declaration (friend class in machine subclasses)
class gdioutput;
class oEvent;

enum AutoSyncType {SyncNone, SyncTimer, SyncDataUp};

enum Machines {
  mPunchMachine,
  mPrintResultsMachine,
  mSplitsMachine,
  mPrewarningMachine,
  mOnlineResults,
  mOnlineInput,
  mSaveBackup,
  mInfoService,

  mMySQLReconnect,
  Unknown = -1,
};

class AutoMachine
{
public:
  enum class Status {
    Good,
    Error,
    Warning,
  };

  enum class State {
    Edit,
    Create,
    Load
  };

private:
  int myid;
  static int uniqueId;
  const Machines type;
  bool isSaved = false;

protected:
  Status lastRunStatus = Status::Good;
  wstring lastStatusMsg;

  bool editMode;

  void settingsTitle(gdioutput &gdi, const char *title);
  enum class IntervalType {IntervalNone, IntervalMinute, IntervalSecond};
  void startCancelInterval(gdioutput &gdi, oEvent &oe, const char *startCommand, State state, IntervalType type, const wstring &interval);

  virtual bool hasSaveMachine() const {
    return false;
  }

  template<typename OP>
  void processProtected(gdioutput& gdi, AutoSyncType ast, OP op) {
    lastRunStatus = Status::Good;
    lastStatusMsg.clear();

    try {
      op();
    }
    catch (const meosException& ex) {
      lastRunStatus = Status::Error;
      lastStatusMsg = ex.wwhat();
      if (ast == AutoSyncType::SyncNone)
        throw;
    }
    catch (const std::exception& ex) {
      lastRunStatus = Status::Error;
      lastStatusMsg = gdioutput::widen(ex.what());
      if (ast == AutoSyncType::SyncNone)
        throw;
    }
    catch (...) {
      lastRunStatus = Status::Error;
      throw;
    }

    if (!lastStatusMsg.empty()) {
      string id = getTypeString() + "_warning";
      gdi.removeFirstInfoBox(id);
      gdi.addInfoBox(id, lastStatusMsg, getDescription(), BoxStyle::HeaderWarning, 10000);
    }
  }

public:

  Status getStatus() const {
    return lastRunStatus;
  }

  const wstring &getStatusMsg() const {
    return lastStatusMsg;
  }

  virtual bool requireList(EStdListType type) const {
    return false;
  }

  virtual void saveMachine(oEvent &oe, const wstring &guiInterval) {
    isSaved = true;
  }

  virtual void loadMachine(oEvent &oe, const wstring &name) {
    if (name != L"default")
      machineName = name;

    isSaved = true;
  }

  bool wasSaved() {
    return isSaved;
  }

  // Return true to auto-remove
  virtual bool removeMe() const {
    return false;
  }

  static AutoMachine *getMachine(int id);
  static void resetGlobalId() {uniqueId = 1;}
  int getId() const {return myid;}
  static shared_ptr<AutoMachine> construct(Machines);
  static Machines getType(const string &typeStr);
  static wstring getDescription(Machines type);
  static string getTypeString(Machines type);
  string getTypeString() const { return getTypeString(type); }
  wstring getMachineName() const {
    return machineName.empty() ? L"default" : machineName;
  }
  wstring getDescription() const {
    return getDescription(type);
  }


  void setEditMode(bool em) {editMode = em;}
  string name;
  wstring machineName;
  DWORD interval; //Interval seconds
  uint64_t timeout; //Timeout (TickCount)
  bool synchronize;
  bool synchronizePunches;

  virtual void settings(gdioutput &gdi, oEvent &oe, State state) = 0;
  virtual void save(oEvent &oe, gdioutput &gdi, bool doProcess) = 0;
  virtual void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) = 0;
  virtual bool isEditMode() const {return editMode;}
  virtual void status(gdioutput &gdi) = 0;
  virtual bool stop() {return true;}
  virtual shared_ptr<AutoMachine> clone() const = 0;
  virtual void cancelEdit() {}

  Machines getType() {
    return type;
  };

  AutoMachine(const string &s, Machines type) : myid(uniqueId++), type(type), name(s), interval(0), timeout(0),
            synchronize(false), synchronizePunches(false), editMode(false) {}
  virtual ~AutoMachine() = 0 {}

  // Returns milliseconds elapsed since timeout was set (for status display)
  int64_t getTimeoutElapsedMs() const {
    return int64_t(GetTickCount64()) - int64_t(timeout);
  }
};

class MySQLReconnect :
  public AutoMachine
{
protected:
  wstring error;
  wstring timeError;
  wstring timeReconnect;
  void* hThread; // sentinel only, always nullptr (was HANDLE in original Win32 code)
  bool toRemove = false;

public:
  void settings(gdioutput &gdi, oEvent &oe, State state);
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<MySQLReconnect>(*this);
  }

  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  bool stop();
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final {
  }

  bool removeMe() const final {
    return toRemove;
  }

  MySQLReconnect(const wstring &error);
  virtual ~MySQLReconnect();
  friend class TabAuto;
};

bool isThreadReconnecting();
