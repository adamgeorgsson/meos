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

// Domain machine types (AutoMachine, Machines, AutoSyncType, MySQLReconnect)
// have been extracted to automachine.h so domain code can include that
// header without pulling in Tab* UI headers.
#include "automachine.h"

#include "TabBase.h"
#include "gdioutput.h"
#include <string>
#include "oListInfo.h"
#include "importformats.h"

using namespace std;

class TabAuto;
class gdioutput;
class oEvent;

class SaveMachine :
  public AutoMachine
{
private:
  wstring baseFile;
  int saveIter = 0;

protected:
  bool hasSaveMachine() const final {
    return true;
  }
  void saveMachine(oEvent& oe, const wstring& guiInterval) final;
  void loadMachine(oEvent& oe, const wstring& name) final;

public:

  shared_ptr<AutoMachine> clone() const final {
    auto prm = make_shared<SaveMachine>(*this);
    return prm;
  }

  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  SaveMachine():AutoMachine("Säkerhetskopiera", Machines::mSaveBackup) {
  }
};


class PrewarningMachine :
  public AutoMachine
{
protected:
  wstring waveFolder;
  set<int> controls;
  set<int> controlsSI;
public:
  void settings(gdioutput &gdi, oEvent &oe, State state);
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<PrewarningMachine>(*this);
  }
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  PrewarningMachine():AutoMachine("Förvarningsröst", Machines::mPrewarningMachine) {}
  friend class TabAuto;
};

class PunchMachine :
  public AutoMachine
{
protected:
  bool fullRadio = false;
  int approxRunningTime = timeConstMinute * 10;
  vector<tuple<int, int, int>> punchesTimeCodeCardNo;
  vector<pair<int, shared_ptr<SICard>>> cardsByTime;
public:
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<PunchMachine>(*this);
  }

  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  PunchMachine() : AutoMachine("Stämplingsautomat", Machines::mPunchMachine) {}
  friend class TabAuto;
};

class SplitsMachine :
  public AutoMachine
{
protected:
  wstring file;
  set<int> classes;
  int leg;
  ExportSplitsData data;
public:
  shared_ptr<AutoMachine> clone() const final {
    return make_shared<SplitsMachine>(*this);
  }

  void settings(gdioutput &gdi, oEvent &oe, State state) final;
  void status(gdioutput &gdi) final;
  void process(gdioutput &gdi, oEvent *oe, AutoSyncType ast) final;
  void save(oEvent &oe, gdioutput &gdi, bool doProcess) final;

  SplitsMachine() : AutoMachine("Export av resultat/sträcktider", Machines::mSplitsMachine), leg(-1) {}
  friend class TabAuto;
};



class TabAuto :
  public TabBase
{
private:
  bool editMode = false;
  int currentMachineEditId = -1;
  bool wasCreated = false;
  bool wasSaved = false;
  bool synchronize = false;
  bool synchronizePunches = false;
  void updateSyncInfo();

  list<shared_ptr<AutoMachine>> machines;
  void setTimer(AutoMachine *am);

  void settings(gdioutput &gdi, AutoMachine *sm, AutoMachine::State state, Machines type);

protected:
  void clearCompetitionData();
  bool hasActiveReconnection() const;

public:

  // Timer and sync callbacks — public so they can be invoked via std::function
  // from AutoTask without a direct friend relationship.
  void timerCallback(gdioutput &gdi);
  void syncCallback(gdioutput &gdi);

  // Accessors for AutoTask callback lambdas (replaces friend class AutoTask).
  bool getSynchronize() const { return synchronize; }
  bool getSynchronizePunches() const { return synchronizePunches; }

  void removedList(EStdListType type);

  AutoMachine *getMachine(int id);
  bool stopMachine(AutoMachine *am);
  void killMachines();
  bool clearPage(gdioutput &gdi, bool postClear);

  AutoMachine &addMachine(const AutoMachine &am) {
    machines.push_back(am.clone());
    setTimer(machines.back().get());
    return *machines.back();
  }

  int processButton(gdioutput &gdi, const ButtonInfo &bu);
  int processListBox(gdioutput &gdi, const ListBoxInfo &bu);

  bool loadPage(gdioutput &gdi, bool showSettingsLast);
  bool loadPage(gdioutput &gdi) {
    return loadPage(gdi, false);
  }

  const char * getTypeStr() const {return "TAutoTab";}
  TabType getType() const {return TAutoTab;}

  TabAuto(oEvent *poe);
  ~TabAuto(void);

  friend void tabForceSync(gdioutput &gdi, pEvent oe);

  static void tabAutoKillMachines();
  static void tabAutoRegister(TabAuto* ta);
  static AutoMachine& tabAutoAddMachinge(const AutoMachine& am);
  static bool hasActiveReconnectionMachine();
};
