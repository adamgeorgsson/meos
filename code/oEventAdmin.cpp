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
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "StdAfx.h"
#include <iostream>

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "gdioutput.h"
#include <filesystem>
#include <fstream>
#include "gdifonts.h"
#include "oDataContainer.h"
#include "metalist.h"
#include "cardsystem.h"

#include "SportIdent.h"

#include "meosexception.h"
#include "oFreeImport.h"
#include "meos_util.h"
#include "RunnerDB.h"
#include "localizer.h"
#include "progress.h"
#include "intkeymapimpl.hpp"
#include "socket.h"

#include "machinecontainer.h"
#include "MeOSFeatures.h"
#include "generalresult.h"
#include "oEventDraw.h"
#include "MeosSQL.h"
#include "binencoder.h"
#include "image.h"
#include "datadefiners.h"
#include "maprenderer.h"
#include "xmlparser.h"
#include <chrono>
#include <ctime>

#include <chrono>
#include <random>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "Table.h"

extern Image image;

const wstring &oEvent::getName() const {
  if (Name.size() > 1 && Name.at(0) == '%') {
    return lang.tl(Name.substr(1));
  }
  else
    return Name;
}

bool oEvent::empty() const
{
  return Name.empty();
}

void oEvent::clearListedCmp()
{
  cinfo.clear();
}

bool oEvent::enumerateCompetitions(const wchar_t *file, const wchar_t *filetype)
{
  namespace fs = std::filesystem;
  namespace ch = std::chrono;

  fs::path dir(file);
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return false;

  wstring pattern(filetype);
  int id = 1;
  cinfo.clear();

  for (auto &entry : fs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file())
      continue;
    wstring fname = entry.path().filename().wstring();
    if (!matchWildcard(fname, pattern))
      continue;

    CompetitionInfo ci;
    ci.FullPath = entry.path().wstring();
    ci.Name = L"";
    ci.Date = L"2007-01-01";
    ci.Id = id++;

    {
      auto ftime = entry.last_write_time(ec);
      auto delta = ch::duration_cast<ch::system_clock::duration>(ftime - fs::file_time_type::clock::now());
      time_t fileTime = ch::system_clock::to_time_t(ch::system_clock::now() + delta);
      std::tm st{};
#ifdef _WIN32
      gmtime_s(&st, &fileTime);
#else
      gmtime_r(&fileTime, &st);
#endif
      ci.Modified = convertSystemTimeN(st);
    }
    xmlparser xp;

    try {
      xp.read(ci.FullPath.c_str(), 30);

      const xmlobject date=xp.getObject("Date");

      if (date) ci.Date=date.getWStr();

      const xmlobject name=xp.getObject("Name");

      if (name) {
        ci.Name = name.getWStr();
        if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
          ci.Name = lang.tl(ci.Name.substr(1));
        }
      }
      const xmlobject annotation=xp.getObject("Annotation");

      if (annotation)
        ci.Annotation=annotation.getWStr();

      const xmlobject nameid = xp.getObject("NameId");
      if (nameid)
        ci.NameId = nameid.getWStr();

      auto oData = xp.getObject("oData");
      if (oData) {
        auto preEvent = oData.getObject("PreEvent");
        if (preEvent)
          ci.preEvent = preEvent.getWStr();

        auto postEvent = oData.getObject("PostEvent");
        if (postEvent)
          ci.postEvent = postEvent.getWStr();

        auto importStamp = oData.getObject("ImportStamp");
        if (importStamp)
          ci.importTimeStamp = importStamp.getWStr();
      }
      cinfo.push_front(ci);
    }
    catch (std::exception &) {
      // XXX Do what??
    }
  }

  if (!getServerName().empty())
    sqlConnection->listCompetitions(this, true);

  for (list<CompetitionInfo>::iterator it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Name.size() > 1 && it->Name[0] == '%')
      it->Name = lang.tl(it->Name.substr(1));
  }

/*
  vector<pair<wstring, wstring>> cc;
  for (auto &c : cinfo) {
    cc.emplace_back(c.NameId, c.Date + L": " + c.Name);
  }
  sort(cc.begin(), cc.end());
  for (auto &c : cc) {
    OutputDebugString(c.first.c_str());
    OutputDebugString(L", ");
    OutputDebugString(c.second.c_str());
    OutputDebugString(L"\n");
  }
*/
  return true;
}

bool oEvent::enumerateBackups(const wstring &file) {
  backupInfo.clear();

  enumerateBackups(file, L"*.meos.bu?", 1);
  enumerateBackups(file, L"*.removed", 1);
  enumerateBackups(file, L"*.dbmeos*", 2);
  backupInfo.sort();

  int id = 1;
  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    it->backupId = id++;
  }
  return true;
}

const BackupInfo &oEvent::getBackup(int bid) const {
  for (list<BackupInfo>::const_iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (it->backupId == bid) {
      return *it;
    }
  }
  throw meosException("Internal error");
}

void oEvent::deleteBackups(const BackupInfo &bu) {
  wstring file = bu.fileName + bu.Name;
  list<wstring> toRemove;

  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (file == it->fileName + it->Name)
      toRemove.push_back(it->FullPath);
  }
  if (!toRemove.empty()) {
    wchar_t path[260];
    wchar_t drive[48];
    wchar_t filename[260];
    wchar_t ext[64];
    //_splitpath_s(toRemove.back().c_str(), drive, ds, path, dirs, filename, fns, ext, exts);
    _wsplitpath_s(toRemove.back().c_str(), drive, path, filename, ext);

    wstring dest = wstring(drive) + path;
    toRemove.push_back(dest + bu.fileName + L".persons");
    toRemove.push_back(dest + bu.fileName + L".clubs");
    toRemove.push_back(dest + bu.fileName + L".wclubs");
    toRemove.push_back(dest + bu.fileName + L".wpersons");

    for (list<wstring>::iterator it = toRemove.begin(); it != toRemove.end(); ++it) {
      std::error_code ec;
      std::filesystem::remove(std::filesystem::path(*it), ec);
    }
  }
}


bool oEvent::listBackups(gdioutput &gdi, GUICALLBACK cb)
{
  int y = gdi.getCY();
  int x = gdi.getCX();

  list<BackupInfo>::iterator it = backupInfo.begin();
  while (it != backupInfo.end()) {
    list<BackupInfo>::iterator sum_size = it;
    size_t s = 0;
    //string date = it->Modified;
    wstring file = it->fileName + it->Name;

    while(sum_size != backupInfo.end() && file == sum_size->fileName + sum_size->Name) {
      s += sum_size->fileSize;
      ++sum_size;
    }
    wstring type = lang.tl(it->type==1 ? L"backup" : L"serverbackup");
    string size;
    if (s < 1024) {
      size = itos(s) + " bytes";
    }
    else if (s < 1024*512) {
      size = itos(s/1024) + " kB";
    }
    else {
      size = itos(s/(1024*1024)) + "." + itos( ((10*(s/1024))/1024)%10) + " MB";
    }
    gdi.dropLine();
    gdi.addStringUT(gdi.getCY(), gdi.getCX(), boldText, it->Name + L" (" + it->Date + L") " + type, 400);
    
    gdi.pushX();
    gdi.fillRight();
    gdi.addString("", 0, "Utrymme: X#" + size);
    gdi.addString("EraseBackup", 0, "[Radera]", cb).setExtra(it->backupId);
    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(1.5);
    y = gdi.getCY();
    while(it != backupInfo.end() && file == it->fileName + it->Name) {
      gdi.addStringUT(y, x+30, 0, it->Modified, 400, cb).setExtra(it->backupId);
      ++it;
      y += gdi.getLineHeight();
    }
  }

  return true;
}

bool BackupInfo::operator<(const BackupInfo &ci)
{
  if (Date!=ci.Date)
    return Date>ci.Date;

  if (fileName!=ci.fileName)
    return fileName<ci.fileName;

  return Modified>ci.Modified;
}


bool oEvent::enumerateBackups(const wstring &file, const wstring &filetype, int type)
{
  namespace fs = std::filesystem;
  namespace ch = std::chrono;

  fs::path dir(file);
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return false;

  wstring pattern(filetype);
  bool found = false;
  for (auto &entry : fs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file())
      continue;
    wstring fname = entry.path().filename().wstring();
    if (!matchWildcard(fname, pattern))
      continue;

    found = true;
    BackupInfo ci;

    ci.type = type;
    ci.FullPath = entry.path().wstring();
    ci.Name = L"";
    ci.Date = L"2007-01-01";
    ci.fileName = fname;
    ci.fileSize = entry.file_size(ec);
    size_t pIndex = ci.fileName.find_first_of(L".");
    if (pIndex > 0 && pIndex < ci.fileName.size())
      ci.fileName = ci.fileName.substr(0, pIndex);

    {
      auto ftime = entry.last_write_time(ec);
      auto delta = ch::duration_cast<ch::system_clock::duration>(ftime - fs::file_time_type::clock::now());
      time_t fileTime = ch::system_clock::to_time_t(ch::system_clock::now() + delta);
      std::tm st{};
#ifdef _WIN32
      localtime_s(&st, &fileTime);
#else
      localtime_r(&fileTime, &st);
#endif
      ci.Modified = convertSystemTimeN(st);
    }
    xmlparser xp;

    try {
      xp.read(ci.FullPath.c_str(), 5);
      //xmlobject *xo=xp.getObject("meosdata");
      const xmlobject date=xp.getObject("Date");

      if (date) ci.Date=date.getWStr();

      const xmlobject name=xp.getObject("Name");

      if (name) {
        ci.Name=name.getWStr();
        if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
          ci.Name = lang.tl(ci.Name.substr(1));
        }
      }

      backupInfo.push_front(ci);
    }
    catch (std::exception &) {
      //XXX Do what?
    }
  }

  return found;
}

bool oEvent::fillCompetitions(gdioutput &gdi,
                              const string &name, int type,
                              const wstring &select,
                              bool doClear) {
  cinfo.sort();
  cinfo.reverse();
  list<CompetitionInfo>::iterator it;
  const CompetitionInfo *bestMatch = nullptr; 

  auto accept = [this, &bestMatch](const CompetitionInfo &ci) {
    if (bestMatch == nullptr)
      bestMatch = &ci;
    else {
      bool matchPrevNextId = bestMatch->preEvent == currentNameId || bestMatch->postEvent == currentNameId;
      bool ciMatchPrevNextId = ci.preEvent == currentNameId || ci.postEvent == currentNameId;
      if (matchPrevNextId != ciMatchPrevNextId) {
        if (ciMatchPrevNextId)
          bestMatch = &ci;
      }
      else {
        if (ci.Date > bestMatch->Date) {
          bestMatch = &ci;
        }
        else {
          if (ci.importTimeStamp > bestMatch->importTimeStamp)
            bestMatch = &ci;
        }
      }
    }
  };

  if (doClear)
    gdi.clearList(name);
  string b;
  //char bf[128];
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    wstring annotation;
    if (!it->Annotation.empty())
      annotation = L" (" + it->Annotation + L")";
    if (it->Server.length()==0) {
      if (type==0 || type==1) {
        if (it->NameId == select && !select.empty())
          accept(*it);
        wstring bf = L"[" + it->Date + L"] " + it->Name;
        gdi.addItem(name, bf + annotation, it->Id);
      }
    }
    else if (type==0 || type==2) {
      if (it->NameId == select && !select.empty())
        accept(*it);
      wstring bf;
      if (type==0)
        bf = lang.tl(L"Server: [X] Y#" + it->Date + L"#" + it->Name);
      else
         bf = L"[" + it->Date + L"] " + it->Name;

      gdi.addItem(name, bf + annotation, 10000000+it->Id);
    }
  }

  if (bestMatch)
    gdi.selectItemByData(name.c_str(), bestMatch->Id);

  return true;
}

void oEvent::checkDB()
{
  if (hasDBConnection()) {
    vector<wstring> err;
    int k=checkChanged(err);

#ifdef _DEBUG
    if (k>0) {
      wchar_t bf[256];
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Databasen innehåller %d osynkroniserade ändringar.", k);
      wstring msg(bf);
      for(int i=0;i < min<int>(err.size(), 10);i++)
        msg+=wstring(L"\n")+err[i];

      std::wcerr << msg << L"\n";
    }
#endif
  }
  updateTabs();
  gdibase.setWindowTitle(getTitleName());
}

void destroyExtraWindows();

void oEvent::clear()
{
  checkDB();

  if (hasDBConnection())
    sqlConnection->checkConnection(0);

  isConnectedToServer = false;
  hasPendingDBConnection = false;

  destroyExtraWindows();

  tables.clear();
  Table::resetTableIds();

  getRunnerDatabase().releaseTables();
  getMeOSFeatures().clear(*this);
  Id=0;
  dataRevision = 0;
  tClubDataRevision = -1;
  tCalcNumMapsDataRevision = -1;

  ZeroTime=0;
  Name.clear();
  Annotation.clear();

  //Make sure no daemon is hunting us.
  if (cbKillMachines) cbKillMachines();

  delete directSocket;
  directSocket = 0;

  tLongTimesCached = -1;

  //Order of destruction is extreamly important...
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  runnerById.clear();
  bibStartNoToRunnerTeam.clear();
  Runners.clear();
  Teams.clear();
  teamById.clear();

  Classes.clear();
  Courses.clear();
  courseIdIndex.clear();

  Controls.clear();

  Cards.clear();
  Clubs.clear();
  clubIdIndex.clear();

  punchIndex.clear();
  punches.clear();
  cachedFirstStart.clear();
  hiredCardHash.clear();

  updateFreeId();

  currentNameId.clear();
  wcscpy_s(CurrentFile, L"");

  sqlRunners.reset();
  sqlClasses.reset();
  sqlCourses.reset();
  sqlControls.reset();
  sqlClubs.reset();
  sqlCards.reset();
  sqlPunches.reset();
  sqlTeams.reset();
  
  vacantId = 0;
  noClubId = 0;
  oEventData->initData(this, sizeof(oData));
  timelineClasses.clear();
  timeLineEvents.clear();
  nextTimeLineEvent = 0;

  tCurrencyFactor = 1;
  tCurrencySymbol = L"kr";
  tCurrencySeparator = L",";
  tCurrencyPreSymbol = false;

  readPunchHash.clear();

  //Reset speaker data structures.
  listContainer->clearExternal();
  while(!generalResults.empty() && generalResults.back().isDynamic())
    generalResults.pop_back();

  // Cleanup user interface
  if (isMainEvent && cbClearCompetitionData)
    cbClearCompetitionData();
  
  machineContainer.release();
  renderMaps.reset();

  MeOSUtil::useHourFormat = getPropertyInt("UseHourFormat", 1) != 0;

  currentNameMode = (NameMode) getPropertyInt("NameMode", FirstLast);

  hasWarnedModifiedExtId = false;

  useSubsecondsVersion = -1; 
}

const shared_ptr<Table> &oEvent::getTable(const string &key) const {
  if (tables.count(key)) {
    tables.find(key)->second->update();
    return tables.find(key)->second;
  }
  throw meosException("Unknown table " + key);
}

void oEvent::setTable(const string &key, const shared_ptr<Table> &table) {
  tables[key] = table;
}

bool oEvent::deleteCompetition()
{
  if (!empty() && !hasDBConnection()) {
    wstring removed = wstring(CurrentFile)+L".removed";
    ::_wremove(removed.c_str()); //Delete old removed file
    openFileLock->unlockFile();
    ::_wrename(CurrentFile, removed.c_str());
    return true;
  }
  else return false;
}

void oEvent::newCompetition(const wstring &name)
{
  openFileLock->unlockFile();
  clear();

  std::tm st = getLocalTm();

  Date = convertSystemDate(st);
  ZeroTime = st.tm_hour*timeConstHour;

  Name = name;
  oEventData->initData(this, sizeof(oData));

  if (!name.empty() && name != L"-")
    getMergeTag();

  setCurrency(-1, L"", L"", 0);

  wstring file;
  getNewFileName(file, currentNameId);
  wcscpy_s(CurrentFile, 260, file.c_str());

  oe->updateTabs();
}

void oEvent::loadDefaults() {
  getDI().setString("Organizer", getPropertyString("Organizer", L""));
  getDI().setString("Street", getPropertyString("Street", L""));
  getDI().setString("Address", getPropertyString("Address", L""));
  getDI().setString("EMail", getPropertyString("EMail", L""));
  getDI().setString("Homepage", getPropertyString("Homepage", L""));

  getDI().setInt("CardFee", getPropertyInt("CardFee", 25));
  getDI().setInt("EliteFee", getPropertyInt("EliteFee", 130));
  getDI().setInt("EntryFee", getPropertyInt("EntryFee", 90));
  getDI().setInt("YouthFee", getPropertyInt("YouthFee", 50));

  getDI().setInt("SeniorAge", getPropertyInt("SeniorAge", 0));
  getDI().setInt("YouthAge", getPropertyInt("YouthAge", 16));

  getDI().setString("Account", getPropertyString("Account", L""));
  getDI().setString("LateEntryFactor", getPropertyString("LateEntryFactor", L"50 %"));

  getDI().setString("CurrencySymbol", getPropertyString("CurrencySymbol", L"kr"));
  getDI().setString("CurrencySeparator", getPropertyString("CurrencySeparator", L"."));
  getDI().setInt("CurrencyFactor", getPropertyInt("CurrencyFactor", 1));
  getDI().setInt("CurrencyPreSymbol", getPropertyInt("CurrencyPreSymbol", 0));
  getDI().setString("PayModes", getPropertyString("PayModes", L""));
  setCurrency(-1, L"", L"", 0);

  getDI().setInt("UTC", oe->getPropertyInt("UseEventorUTC", 0) != 0);
  getDI().setInt("OldCards", oe->getPropertyInt("OldCards", 0));
}

void oEvent::reEvaluateCourse(int CourseId, bool doSync)
{
  oRunnerList::iterator it;

  if (doSync)
    autoSynchronizeLists(false);

  vector<int> mp;
  set<int> classes;
  for(it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getCourse(false) && it->getCourse(false)->getId()==CourseId){
      classes.insert(it->getClassId(true));
    }
  }

  reEvaluateAll(classes, false);
}
