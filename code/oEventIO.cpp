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

bool oEvent::writeControls(xmlparser &xml)
{
  oControlList::iterator it;

  xml.startTag("ControlList");

  for (it=Controls.begin(); it != Controls.end(); ++it)
    it->write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeCourses(xmlparser &xml)
{
  oCourseList::iterator it;

  xml.startTag("CourseList");

  for (it=Courses.begin(); it != Courses.end(); ++it)
    it->Write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeClasses(xmlparser &xml)
{
  oClassList::iterator it;

  xml.startTag("ClassList");

  for (it=Classes.begin(); it != Classes.end(); ++it)
    it->Write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeClubs(xmlparser &xml)
{
  oClubList::iterator it;

  xml.startTag("ClubList");

  for (it=Clubs.begin(); it != Clubs.end(); ++it)
    it->write(xml);

  xml.endTag();

  return true;
}

bool oEvent::writeRunners(xmlparser &xml, ProgressWindow &pw)
{
  oRunnerList::iterator it;

  xml.startTag("RunnerList");
  int k=0;
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->tDuplicateLeg) //Duplicates is written by the ruling runner.
      it->Write(xml);
    if (++k%300 == 200)
      pw.setSubProgress( (1000*k)/ Runners.size());
  }
  xml.endTag();

  return true;
}


bool oEvent::writePunches(xmlparser &xml, ProgressWindow &pw)
{
  oFreePunchList::iterator it;

  xml.startTag("PunchList");
  int k = 0;
  for (it=punches.begin(); it != punches.end(); ++it) {
    it->Write(xml);
    if (++k%300 == 200)
      pw.setSubProgress( (1000*k)/ (punches.size()));
  }
  xml.endTag();

  return true;
}

//Write free cards not owned by a runner.
bool oEvent::writeCards(xmlparser &xml)
{
  oCardList::iterator it;

  xml.startTag("CardList");

  for (it=Cards.begin(); it != Cards.end(); ++it) {
    if (it->getOwner() == 0)
      it->Write(xml);
  }

  xml.endTag();
  return true;
}

void oEvent::duplicate(const wstring &annotationIn, bool keepTags) {
  wchar_t file[260];
  wchar_t filename[64];
  wchar_t nameid[64];

  auto nowChron = std::chrono::system_clock::now();
  time_t nowT = std::chrono::system_clock::to_time_t(nowChron);
  std::tm st{};
#ifdef _WIN32
  localtime_s(&st, &nowT);
#else
  localtime_r(&nowT, &st);
#endif
  int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(nowChron.time_since_epoch()).count() % 1000);

  swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
    st.tm_year+1900, st.tm_mon+1, st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, ms);

  getUserFile(file, filename);

  _wsplitpath_s(filename, NULL, 0, NULL,0, nameid, 64, NULL, 0);
  int i=0;
  while (nameid[i]) {
    if (nameid[i]=='.') {
      nameid[i]=0;
      break;
    }
    i++;
  }

  wchar_t oldFile[260];
  wstring oldId;
  wcscpy_s(oldFile, CurrentFile);
  oldId = currentNameId;
  wstring oldAnno = getAnnotation();

  wcscpy_s(CurrentFile, file);
  if (!keepTags)
    currentNameId = nameid;

  swprintf(filename, sizeof(filename)/sizeof(wchar_t), L"%d/%d %d:%02d",
                      st.wDay, st.wMonth, st.wHour, st.wMinute);

  if (annotationIn.empty()) {
    wstring anno = lang.tl(L"Kopia (X)#" + wstring(filename));
    anno = oldAnno.empty() ? anno : oldAnno + L" " + anno;
    setAnnotation(anno);
  }
  else {
    setAnnotation(annotationIn);
  }
  wstring oldTag = getMergeTag();
  try {
    if (!keepTags)
      getMergeTag(true);
    save();
  }
  catch(...) {
    getDI().setString("MergeTag", oldTag);
    // Restore in case of error
    wcscpy_s(CurrentFile, oldFile);
    currentNameId = oldId;
    setAnnotation(oldAnno);
    synchronize(true);
    throw;
  }

  // Restore
  wcscpy_s(CurrentFile, oldFile);
  currentNameId = oldId;
  setAnnotation(oldAnno);
  getDI().setString("MergeTag", oldTag);
  synchronize(true);
}

bool oEvent::save()
{
  if (empty() || gdibase.isTest())
    return true;

  autoSynchronizeLists(true);

  if (!CurrentFile[0])
    throw std::exception("Felaktigt filnamn");

  int f=0;
  _wsopen_s(&f, CurrentFile, _O_RDONLY, _SH_DENYNO, _S_IWRITE);

  wchar_t fn1[260];
  wchar_t fn2[260];
  wstring finalRenameTarget;

  if (f!=-1) {
    _close(f);
    time_t currentTime = time(0);
    const int baseAge = 3; // Three minutes
    time_t allowedAge = baseAge*60;
    time_t oldAge = allowedAge + 60;
    const int maxBackup = 8;
    int toDelete = maxBackup;

    for(int k = 0; k <= maxBackup; k++) {
      swprintf(fn1, sizeof(fn1)/sizeof(wchar_t), L"%s.bu%d", CurrentFile, k);
      struct _stat st;
      int ret = _wstat(fn1, &st);
      if (ret==0) {
        time_t age = currentTime - st.st_mtime;
        // If file is too young or to old at its
        // position, it is possible to delete.
        // The oldest old file (or youngest young file if none is old)
        // possible to delete is deleted.
        // If no file is possible to delete, the oldest
        // file is deleted.
        if ( (age<allowedAge && toDelete==maxBackup) || age>oldAge)
          toDelete = k;
        allowedAge *= 2;
        oldAge*=2;

        if (k==maxBackup-3)
          oldAge = 24*timeConstSecPerHour; // Allow a few old copies
      }
      else {
        toDelete = k; // File does not exist. No file need be deleted
        break;
      }
    }

    swprintf(fn1, sizeof(fn1)/sizeof(wchar_t), L"%s.bu%d", CurrentFile, toDelete);
    ::_wremove(fn1);

    for(int k=toDelete;k>0;k--) {
      swprintf(fn1, sizeof(fn1)/sizeof(wchar_t), L"%s.bu%d", CurrentFile, k-1);
      swprintf(fn2, sizeof(fn2)/sizeof(wchar_t), L"%s.bu%d", CurrentFile, k);
      _wrename(fn1, fn2);
    }

    finalRenameTarget = fn1;
    //rename(CurrentFile, fn1);
  }
  bool res;
  if (finalRenameTarget.empty()) {
    res = save(CurrentFile, true, true);
    if (!(hasDBConnection() || hasPendingDBConnection))
      openFileLock->lockFile(CurrentFile);
  }
  else {
    wstring tmpName = wstring(CurrentFile) + L".~tmp";
    res = save(tmpName, true, true);
    if (res) {
      openFileLock->unlockFile();
      _wrename(CurrentFile, finalRenameTarget.c_str());
      _wrename(tmpName.c_str(), CurrentFile);
  
      if (!(hasDBConnection() || hasPendingDBConnection))
        openFileLock->lockFile(CurrentFile);
    }
  }

  return res;
}

bool oEvent::save(const wstring &fileArg, bool internalFormat, bool isAutoSave) {
  if (isAutoSave && gdibase.isTest())
    return true;

  const wchar_t *file = fileArg.c_str();
  xmlparser xml;
  ProgressWindow pw(gdibase.getHWNDTarget(), gdibase.getScale());

  if (Runners.size()>200)
    pw.init();

  xml.openOutput(file, true);
  xml.startTag("meosdata", "version", getMajorVersion());
  xml.write("Name", Name);
  xml.write("Date", Date);
  xml.writeTime("ZeroTime", ZeroTime);
  xml.write("NameId", currentNameId);
  xml.write("Annotation", Annotation);
  xml.write("Id", Id);
  xml.write("Updated", getStamp());

  oEventData->write(this, xml);

  int i = 0;
  vector<int> p;
  p.resize(10);
  p[0] = 2; //= {2, 20, 50, 80, 180, 400,500,700,800,1000};
  p[1] = Controls.size();
  p[2] = Courses.size();
  p[3] = Classes.size();
  p[4] = Clubs.size();
  p[5] = Runners.size() + Cards.size();
  p[6] = Teams.size();
  p[7] = punches.size();
  p[8] = Cards.size();
  p[9] = Runners.size()/2;

  int sum = 0;
  for (int k = 0; k<10; k++)
    sum += p[k];

  for (int k = 1; k<10; k++)
    p[k] = p[k-1] + (1000 * p[k]) / sum;

  p[9] = 1000;

  pw.setProgress(p[i++]);
  writeControls(xml);
  pw.setProgress(p[i++]);
  writeCourses(xml);
  pw.setProgress(p[i++]);
  writeClasses(xml);
  pw.setProgress(p[i++]);
  writeClubs(xml);
  pw.initSubProgress(p[i], p[i+1]);
  pw.setProgress(p[i++]);
  writeRunners(xml, pw);
  pw.setProgress(p[i++]);
  writeTeams(xml);
  pw.initSubProgress(p[i], p[i+1]);
  pw.setProgress(p[i++]);
  writePunches(xml, pw);
  pw.setProgress(p[i++]);
  writeCards(xml);

  xml.startTag("Lists");
  listContainer->save(MetaListContainer::ExternalList, xml, this);
  xml.endTag();

  set<uint64_t> img;
  listContainer->getUsedImages(img);

  if (renderMaps) {
    renderMaps->getUsedImage(img);
    renderMaps->serialize(xml);
  }

  wstring error;

  if (!img.empty()) {
    xml.startTag("Images");
    Encoder92 binEncoder;
    for (auto imgId : img) {
      if (!image.hasImage(imgId))
        loadImage(imgId);
      
      if (!image.hasImage(imgId))
        continue;

      wstring fileName = image.getFileName(imgId);
      auto rawData = image.getRawData(imgId);
      vector<pair<string, wstring>> props;
      props.emplace_back("filename", fileName);
      props.emplace_back("id", itow(imgId));
      bool added = false;
      if (internalFormat) {        
        int lp = 0;
        for (int i = 0; i < fileArg.length(); i++) {
          if (fileArg[i] == '\\' || fileArg[i] == '/')
            lp = i;
        }

        wstring imgFile = fileArg.substr(0, lp + 1)  + itow(imgId) + L".png";
        {
          std::ofstream fout(std::filesystem::path(imgFile), std::ios::binary);
          if (!fout)
            error = L"Error opening " + imgFile;
          else {
            if (!fout.write(reinterpret_cast<const char*>(rawData.data()), rawData.size()))
              error = L"Error writing image.";
            else
              added = true;
          }
        }

        if (added) {
          // Use shared binary file
          props.emplace_back("external", L"true");
          xml.write("Image", props, L"");
        }
      }

      if (!added) {
        string encoded;
        binEncoder.encode92(rawData, encoded);
        xml.writeAscii("Image", props, encoded);
      }
    }
    xml.endTag();
  }

  if (machineContainer) {
    xml.startTag("Machines");
    machineContainer->save(xml);
    xml.endTag();
  }

  xml.closeOut();
  pw.setProgress(p[i++]);
  updateRunnerDatabase();
  pw.setProgress(p[i++]);

  return true;
}

wstring oEvent::getNameId(int id) const {
  if (id == 0)
    return currentNameId;

  list<CompetitionInfo>::const_iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id)
        return it->NameId;
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        return it->NameId;
      }
    }
  }
  return _EmptyWString;
}

int oEvent::getIdFromNameId(const wstring& nameId) const {
  
  for (auto &ci : cinfo) {
    if (ci.NameId == nameId)
      return ci.Id;
  }
  return -1;
}

const wstring &oEvent::getFileNameFromId(int id) const {

  list<CompetitionInfo>::const_iterator it;
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id)
        return it->FullPath;
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        return _EmptyWString;
      }
    }
  }
  return _EmptyWString;
}


bool oEvent::open(int id)
{
  list<CompetitionInfo>::iterator it;

  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Server.empty()) {
      if (id == it->Id) {
        CompetitionInfo ci=*it; //Take copy
        if (open(ci.FullPath.c_str(), false, false, false)) {
          supportSubSeconds(supportSubSeconds());
          return true;
        }
        return false;
      }
    }
    else if (!it->Server.empty()) {
      if (id == (10000000+it->Id)) {
        CompetitionInfo ci=*it; //Take copy
        if (readSynchronize(ci)) {
          getMergeTag();
          supportSubSeconds(supportSubSeconds());
          return true;
        }
        return false;
      }
    }
  }

  return false;
}

static std::chrono::steady_clock::time_point timer;
static string mlog;

static void tic() {
  timer = std::chrono::steady_clock::now();
  mlog.clear();
}

static void toc(const string &str) {
  auto t = std::chrono::steady_clock::now();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t - timer).count();
  if (!mlog.empty())
    mlog += ",\n";
  else
    mlog = "Tid (hundradels sekunder):\n";

  mlog += str + "=" + itos( (int)(elapsed_ms / 10) );
  timer = t;
}

namespace {
  void getNewFileName(wstring &fn, wstring &nameId) {
    auto nowChron = std::chrono::system_clock::now();
    time_t nowT = std::chrono::system_clock::to_time_t(nowChron);
    std::tm st{};
#ifdef _WIN32
    localtime_s(&st, &nowT);
#else
    localtime_r(&nowT, &st);
#endif
    int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(nowChron.time_since_epoch()).count() % 1000);

    wchar_t file[260];
    wchar_t filename[64];
    swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
               st.tm_year+1900, st.tm_mon+1, st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, ms);

    //strcpy_s(CurrentNameId, filename);
    getUserFile(file, filename);

    wchar_t CurrentNameId[64];
    _wsplitpath_s(file, NULL, 0, NULL, 0, CurrentNameId, 64, NULL, 0);
    int i = 0;
    while (CurrentNameId[i]) {
      if (CurrentNameId[i] == '.') {
        CurrentNameId[i] = 0;
        break;
      }
      i++;
    }

    fn = file;
    nameId = CurrentNameId;
  }
}

bool oEvent::open(const wstring &file, bool doImport, bool forMerge, bool forceNew) {
  if (!doImport)
    openFileLock->lockFile(file);

  xmlparser xml;
  xml.setProgress(gdibase.getHWNDTarget());
  tic();
  string log;
  xml.read(file);

  string tag = xml.getObject(0).getName();
  wstring iof;
  xml.getObject(0).getObjectString("iofVersion", iof);
  if (tag == "EntryList" || tag == "StartList" || iof.length() > 0)
    throw meosException(L"Filen (X) innehåller IOF-XML tävlingsdata och kan importeras i en existerande tävling#" + file);

  if (tag == "MeOSListDefinition")
    throw meosException(L"Filen (X) är en listdefinition#" + file);

  if (tag == "MeOSResultCalculationSet")
    throw meosException(L"Filen (X) är en resultatmodul#" + file);

  if (tag != "meosdata")
    throw meosException(L"Filen (X) är inte en MeOS-tävling#" + file);

  xmlattrib ver = xml.getObject(0).getAttrib("version");
  if (ver) {
    wstring vs = ver.getWStr();
    if (vs > getMajorVersion()) {
      // Tävlingen är skapad i MeOS X. Data kan gå förlorad om du öppnar tävlingen.\n\nVill du fortsätta?
      bool cont = gdibase.ask(L"warn:opennewversion#" + vs);
      if (!cont)
        return false;
    }
  }
  toc("parse");
  //This generates a new file name
  newCompetition(L"-");
  auto newNameId = currentNameId;
  if (!doImport) {
    wcscpy_s(CurrentFile, 260, file.c_str()); //Keep new file name, if imported

    wchar_t CurrentNameId[64];
    _wsplitpath_s(CurrentFile, NULL, 0, NULL,0, CurrentNameId, 64, NULL, 0);
    int i=0;
    while (CurrentNameId[i]) {
      if (CurrentNameId[i]=='.') {
        CurrentNameId[i]=0;
        break;
      }
      i++;
    }
    currentNameId = CurrentNameId;
  }
  bool res = open(xml, file);
  if (res && !doImport)
    openFileLock->lockFile(file);

  if (forceNew) {
    newNameId.swap(currentNameId);
  }
  else if (doImport && !oe->gdiBase().isTest()) {
    for (auto &cmp : cinfo) {
      if (cmp.NameId == currentNameId) {
        if (!gdibase.ask(L"ask:importcopy#" + cmp.Name + L", " + cmp.Date)) {
          wstring fn;
          getNewFileName(fn, currentNameId);
        }
        break;
      }
    }
  }

  getMergeTag(doImport && !forMerge);

  if (forceNew) {
    getDI().setString("ImportStamp", L"");
  }
  else if (doImport && !forMerge) {
    getDI().setString("ImportStamp", gdibase.widen(getLastModified()));
  }

  return res;
}

void oEvent::clearData(bool runnerTeam, bool courses) {
  Cards.clear();

  list<oFreePunch> op;
  for (auto& p : punches) {
    if (p.isHiredCard())
      op.push_back(p);
  }
  punchIndex.clear();
  punches.clear();
  punches.swap(op);

  if (courses) {
    Controls.clear();
    Courses.clear();
  }

  if (runnerTeam) {
    Clubs.clear();
    Runners.clear();
    Teams.clear();
  }

  if (courses) {
    for (auto& c : Classes) {
      c.setCourse(nullptr);
      for (auto& mc : c.MultiCourse)
        mc.clear();
    }

    for (auto& r : Runners)
      r.Course = nullptr;
  }

  for (auto& r : Runners) {
    r.Card = nullptr;
    r.setFinishTime(0);
    r.setStatus(StatusUnknown, true, oBase::ChangeType::Update, false);
  }

  clubIdIndex.clear();
  runnerById.clear();
  teamById.clear();
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  classIdToRunnerHash.reset();
  readPunchHash.clear();
  courseIdIndex.clear();
  updateFreeId();
}

void oEvent::restoreBackup()
{
  wstring cfile = wstring(CurrentFile) + L".meos";
  wcscpy_s(CurrentFile, cfile.c_str());
}

bool oEvent::open(const xmlparser &xml, const wstring &fileArg) {
  xmlobject xo;
  ZeroTime = 0;

  xo = xml.getObject("Date");
  if (xo) {
    wstring fDate = xo.getWStr();
    if (convertDateYMD(fDate, true) > 0)
      Date = fDate;
  }
  Name.clear();
  xo = xml.getObject("Name");
  if (xo)  Name=xo.getWStr();

  if (Name.empty()) {
    Name = lang.tl("Ny tävling");
  }

  xo = xml.getObject("Annotation");
  if (xo) Annotation = xo.getWStr();

  xo=xml.getObject("ZeroTime");
  if (xo) ZeroTime=xo.getRelativeTime();

  xo=xml.getObject("Id");
  if (xo) Id=xo.getInt();

  xo=xml.getObject("oData");

  if (xo)
    oEventData->set(this, xo);

  setCurrency(-1, L"", L",", false);

  xo = xml.getObject("NameId");
  if (xo)
    currentNameId = xo.getWStr();

  toc("event");
  //Get controls
  xo = xml.getObject("ControlList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownControls;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Control")){
        oControl c(this);
        c.set(&*it);

        if (c.Id>0 && knownControls.insert(c.Id).second) {
          addControl(c);
        }
      }
    }
  }

  toc("controls");

  //Get courses
  xo=xml.getObject("CourseList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownCourse;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Course")){
        oCourse c(this);
        c.Set(&*it);
        if (c.Id>0 && knownCourse.count(c.Id) == 0) {
          addCourse(c);
          knownCourse.insert(c.Id);
        }
      }
    }
  }

  toc("course");

  //Get classes
  xo=xml.getObject("ClassList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    set<int> knownClass;
    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Class")){
        oClass c(this);
        c.Set(&*it);
        if (c.Id>0 && knownClass.count(c.Id) == 0) {
          Classes.push_back(c);
          Classes.back().addToEvent(this, &c);
          knownClass.insert(c.Id);
        }
      }
    }
  }

  toc("class");
  reinitializeClasses();

  //Get clubs
  xo=xml.getObject("ClubList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Club")){
        oClub c(this);
        c.set(*it);
        if (c.Id>0)
          addClub(c);
      }
    }
  }

  toc("club");

  //Get runners
  xo=xml.getObject("RunnerList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Runner")){
        oRunner r(this, 0);
        r.Set(*it);
        if (r.Id>0)
          addRunner(r, false);
        else if (r.Card)
          r.Card->tOwner=0;
      }
    }
  }

  toc("runner");

  //Get teams
  xo=xml.getObject("TeamList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Team")){
        oTeam t(this, 0);
        t.set(*it);
        if (t.Id>0){
          //Teams.push_back(t);
          addTeam(t, false);
          Teams.back().apply(ChangeType::Quiet, nullptr);
        }
      }
    }
  }

  for (oRunner &r : Runners)
    r.apply(ChangeType::Quiet, nullptr);

  toc("team");

  xo=xml.getObject("PunchList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);

    xmlList::const_iterator it;
    oFreePunch::disableHashing = true;
    try {
      for(it=xl.begin(); it != xl.end(); ++it){
        if (it->is("Punch")){
          oFreePunch p(this, 0, 0, 0, 0);
          p.Set(&*it);
          addFreePunch(p);
        }
      }
    }
    catch(...) {
      oFreePunch::disableHashing = false;
      throw;
    }
    oFreePunch::disableHashing = false;
    oFreePunch::rehashPunches(*this, 0, 0);
  }

  toc("punch");

  xo=xml.getObject("CardList");
  if (xo){
    xmlList xl;
    xo.getObjects(xl);
    xmlList::const_iterator it;

    for(it=xl.begin(); it != xl.end(); ++it){
      if (it->is("Card")){
        oCard c(this);
        c.Set(*it);
        assert(c.Id>=0);
        addCard(c);
      }
    }
  }

  toc("card");

  xo=xml.getObject("Updated");
  if (xo) Modified.setStamp(xo.getRawStr());

  adjustTeamMultiRunners(0);
  updateFreeId();
  reEvaluateAll(set<int>(), true); //True needed to update data for sure

  toc("update");
  wstring err;

  try {
    xmlobject xList = xml.getObject("Lists");
    if (xList) {
      if (!listContainer->load(MetaListContainer::ExternalList, xList, true)) {
        err = L"Visa listor är gjorda i en senare version av MeOS och kunde inte laddas.";
      }
    }
  }
  catch (const meosException &ex) {
    if (err.empty())
      err = ex.wwhat();
  }
  catch (const std::exception &ex) {
    if (err.empty())
      err = gdibase.widen(ex.what());
  }

  renderMaps = make_shared<MapDataContainer>();
  if (!renderMaps->deserialize(xml.getObject("Maps")))
    renderMaps.reset();

  getMeOSFeatures().deserialize(getDCI().getString("Features"), *this);

  xmlobject xImage = xml.getObject("Images");
  if (xImage) {
    xmlList imgs;
    xImage.getObjects("Image", imgs);

    Encoder92 binEncoder;
    vector<uint8_t> bytes;
    for (auto& img : imgs) {
      try {
        wstring fileName, id;
        img.getObjectString("filename", fileName);
        img.getObjectString("id", id);
        uint64_t imgId = _wcstoui64(id.c_str(), nullptr, 10);
        
        if (!img.getObjectBool("external")) {
          string data = img.getRawStr();
          binEncoder.decode92(data, bytes);
          image.provideFromMemory(imgId, fileName, bytes);
        }
        else {
          int lp = 0;
          for (int i = 0; i < fileArg.length(); i++) {
            if (fileArg[i] == '\\' || fileArg[i] == '/')
              lp = i;
          }

          wstring imgFile = fileArg.substr(0, lp + 1) + itow(imgId) + L".png";

          {
            std::ifstream pFile(std::filesystem::path(imgFile), std::ios::binary);
            if (pFile) {
              pFile.seekg(0, std::ios::end);
              size_t pos = (size_t)pFile.tellg();
              pFile.seekg(0, std::ios::beg);
              bytes.resize(pos);
              bool ok = static_cast<bool>(pFile.read(reinterpret_cast<char*>(bytes.data()), pos));
              if (ok)
                image.provideFromMemory(imgId, fileName, bytes);
              else if (err.empty())
                err = L"Failed to load attached image: " + imgFile;
            }
          }
        }
      }
      catch (const meosException& ex) {
        if (err.empty())
          err = ex.wwhat();
      }
      catch (const std::exception& ex) {
        if (err.empty())
          err = gdibase.widen(ex.what());
      }
    }
  }
  
  try {
    xmlobject xMachine = xml.getObject("Machines");
    if (xMachine) {
      getMachineContainer().load(xMachine);
    }
  }
  catch (const meosException &ex) {
    if (err.empty())
      err = ex.wwhat();
  }
  catch (const std::exception &ex) {
    if (err.empty())
      err = gdibase.widen(ex.what());
  }


  if (!err.empty())
    throw meosException(err);

  return true;
}

bool oEvent::openRunnerDatabase(const wchar_t* filename)
{
  wchar_t file[260];
  getUserFile(file, filename);

  wchar_t fclub[260];
  wchar_t fwclub[260];
  wchar_t frunner[260];
  wchar_t fwrunner[260];

  wcscpy_s(fclub, file);
  wcscat_s(fclub, L".clubs");

  wcscpy_s(fwclub, file);
  wcscat_s(fwclub, L".wclubs");

  wcscpy_s(frunner, file);
  wcscat_s(frunner, L".persons");

  wcscpy_s(fwrunner, file);
  wcscat_s(fwrunner, L".wpersons");

  try {
    if ((fileExists(fwclub) || fileExists(fclub)) && (fileExists(frunner) || fileExists(fwrunner)) ) {
      if (fileExists(fwclub))
        runnerDB->loadClubs(fwclub);
      else
        runnerDB->loadClubs(fclub);

      if (fileExists(fwrunner))
        runnerDB->loadRunners(fwrunner);
      else
        runnerDB->loadRunners(frunner);
    }
  }
  catch (meosException &ex) {
    gdibase.alert(ex.wwhat());
  }
  catch(std::exception &ex) {
    gdibase.alert(ex.what());
  }
  return true;
}

pRunner oEvent::dbLookUpById(__int64 extId) const
{
  if (!useRunnerDb())
    return 0;
  oEvent *toe = const_cast<oEvent *>(this);
  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  sRunner.setTemporary();
  RunnerWDBEntry *dbr = runnerDB->getRunnerById(int(extId));
  if (dbr != 0) {
    sRunner.init(*dbr, false);
    return &sRunner;
  }
  else
    return 0;
}

pRunner oEvent::dbLookUpByCard(int cardNo) const
{
  if (!useRunnerDb())
    return 0;

  oEvent *toe = const_cast<oEvent *>(this);
  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  RunnerWDBEntry *dbr = runnerDB->getRunnerByCard(cardNo);
  if (dbr != 0) {
    dbr->getName(sRunner.sName);
    sRunner.getRealName(sRunner.sName, sRunner.tRealName);
    sRunner.init(*dbr, false);
    sRunner.cardNumber = cardNo;
    return &sRunner;
  }
  else
    return 0;
}

pRunner oEvent::dbLookUpByName(const wstring &name, int clubId, int classId, int birthYear) const
{
  if (!useRunnerDb())
    return 0;

  oEvent *toe = const_cast<oEvent *>(this);

  static oRunner sRunner = oRunner(toe, 0);
  sRunner = oRunner(toe, 0);
  sRunner.setTemporary();

  if (birthYear == 0) {
    pClass pc = getClass(classId);

    int expectedAge = pc ? pc->getExpectedAge() : 0;

    if (expectedAge>0)
      birthYear = getThisYear() - expectedAge;
  }

  pClub pc = getClub(clubId);

  if (pc && pc->getExtIdentifier()>0)
    clubId = (int)pc->getExtIdentifier();

  RunnerWDBEntry *dbr = runnerDB->getRunnerByName(name, clubId, birthYear);

  if (dbr) {
    sRunner.init(*dbr, false);
    return &sRunner;
  }

  return 0;
}

bool oEvent::saveRunnerDatabase(const wchar_t *filename, bool onlyLocal)
{
  wchar_t file[260];
  getUserFile(file, filename);

  wchar_t fclub[260];
  wchar_t frunner[260];
  wcscpy_s(fclub, file);
  wcscat_s(fclub, L".wclubs");

  wcscpy_s(frunner, file);
  wcscat_s(frunner, L".wpersons");

  if (!onlyLocal || !runnerDB->isFromServer()) {
    runnerDB->saveClubs(fclub);
    runnerDB->saveRunners(frunner);
  }
  return true;
}

void oEvent::updateRunnerDatabase()
{
  if (Name == L"!TESTTÄVLING")
    return;

  if (useRunnerDb()) {
    map<int, int> clubIdMap;
    for (auto it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved())
        continue;
      if (it->hasFlag(oAbstractRunner::TransferFlags::FlagNoDatabase))
        continue;

      if (it->Card && it->Card->cardNo == it->cardNumber &&
          it->getDI().getInt("CardFee") == 0 && it->Card->getNumPunches() > 5)
          updateRunnerDatabase(&*it, clubIdMap);
    }
    runnerDB->refreshTables();
  }
  if (listContainer) {
    for (int k = 0; k < listContainer->getNumLists(); k++) {
      if (listContainer->isExternal(k)) {
        MetaList& ml = listContainer->getList(k);
        wstring uid = gdibase.widen(ml.getUniqueId()) + L".meoslist";
        wchar_t file[260];
        getUserFile(file, uid.c_str());
        if (!fileExists(file)) {
          ml.save(file, this);
        }
      }
    }
    vector<pair<string, shared_ptr<DynamicResult>>> freeMod;
    listContainer->getFreeResultModules(freeMod);

    for (size_t k = 0; k < freeMod.size(); k++) {
      wstring uid = gdibase.widen(freeMod[k].first) + L".rules";
      wchar_t file[260];
      getUserFile(file, uid.c_str());
      if (!fileExists(file)) {
        freeMod[k].second->save(file);
      }
    }
  }
}

void oEvent::updateRunnerDatabase(pRunner r, map<int, int> &clubIdMap)
{
  if (!r->cardNumber)
    return;
  runnerDB->updateAdd(*r, clubIdMap);
}

void oEvent::backupRunnerDatabase() {
  if (!runnerDBCopy)
    runnerDBCopy = make_shared<RunnerDB>(*runnerDB);
}

void oEvent::restoreRunnerDatabase() {
  if (runnerDBCopy && *runnerDB != *runnerDBCopy) {
    runnerDB = make_shared<RunnerDB>(*runnerDBCopy);
  }
}

