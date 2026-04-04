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

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "oDataContainer.h"
#include "metalist.h"
#include "cardsystem.h"

#include "SportIdent.h"

#include "meosexception.h"
#include "oFreeImport.h"
// TabBase.h removed — decoupled via cbBaseButtons callback
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
// TabAuto.h, TabSI.h, TabList.h removed — decoupled via callbacks
#include "binencoder.h"
#include "image.h"
#include "datadefiners.h"
#include "maprenderer.h"
#include "xmlparser.h"

#include <chrono>
#include <random>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "Table.h"
#include <cstdint>
#include <ctime>
#include <iostream>
#include <filesystem>

extern Image image;

//Version of database
int oEvent::dbVersion = 100;

oEvent::oEvent(gdioutput &gdi) : oBase(nullptr), gdibase(gdi) {
  readOnly = false;
  tLongTimesCached = -1;
  directSocket = 0;
  ZeroTime=0;
  vacantId = 0;
  noClubId = 0;
  dataRevision = 0;
  
  disableRecalculate = false;

  initProperties();

#ifndef MEOSDB
  listContainer = new MetaListContainer(this);
#else
  throw std::exception();
#endif


  tCurrencyFactor = 1;
  tCurrencySymbol = L"kr";
  tCurrencySeparator = L",";
  tCurrencyPreSymbol = false;

  tClubDataRevision = -1;

  nextFreeStartNo = 0;

  std::tm st = {};
  meos_localtime_now(&st);

  wchar_t bf[64];
  swprintf(bf, 64, L"%d-%02d-%02d", (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday);

  Date=bf;
  ZeroTime=st.tm_hour*timeConstHour;
  oe=this;

  runnerDB = make_shared<RunnerDB>(this);
  meosFeatures = new MeOSFeatures();
  openFileLock = new MeOSFileLock();

  wchar_t cp[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(cp, &size);
  clientName = cp;

  isConnectedToServer = false;
  hasPendingDBConnection = false;
  currentNameMode = FirstLast;

  nextTimeLineEvent = 0;
  //These object must be initialized on creation of any oObject,
  //but we need to create (dummy) objects to get the sizeof their
  //oData[]-sets...

  // --- REMEMBER TO UPDATE dvVersion when these are changed.

  oEventData=new oDataContainer(dataSize);
  oEventData->addVariableCurrency("CardFee", "Brickhyra");
  oEventData->addVariableCurrency("EliteFee", "Elitavgift");
  oEventData->addVariableCurrency("EntryFee", "Normalavgift");
  oEventData->addVariableCurrency("YouthFee", "Reducerad avgift");
  oEventData->addVariableInt("YouthAge", oDataContainer::oIS8U, "Åldersgräns ungdom");
  oEventData->addVariableInt("SeniorAge", oDataContainer::oIS8U, "Åldersgräns äldre");

  oEventData->addVariableString("Account", 30, "Konto");
  oEventData->addVariableDate("PaymentDue", "Sista betalningsdatum");
  oEventData->addVariableDate("OrdinaryEntry", "Ordinarie anmälningsdatum");
  oEventData->addVariableDate("SecondEntryDate", "Stoppdatum två");

  oEventData->addVariableString("LateEntryFactor", 6, "Avgiftshöjning (procent)");
  oEventData->addVariableString("SecondEntryFactor", 6, "Avgiftshöjning två (procent)");

  oEventData->addVariableString("Organizer", "Arrangör");
  oEventData->addVariableString("CareOf", 31, "c/o");

  oEventData->addVariableString("Street", 32, "Adress");
  oEventData->addVariableString("Address", 32, "Postadress");
  oEventData->addVariableString("EMail", "E-post");
  oEventData->addVariableString("Homepage", "Hemsida");
  oEventData->addVariableString("Phone", 32, "Telefon");

  oEventData->addVariableInt("UseEconomy", oDataContainer::oIS8U, "Ekonomi");
  oEventData->addVariableInt("UseSpeaker", oDataContainer::oIS8U, "Speaker");
  oEventData->addVariableInt("SkipRunnerDb", oDataContainer::oIS8U, "Databas");
  oEventData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");

  oEventData->addVariableInt("MaxTime", oDataContainer::oISTime, "Gräns för maxtid");
  oEventData->addVariableInt("DiffTime", oDataContainer::oISTime, "Stämplingsintervall, rogaining-patrull");

  oEventData->addVariableString("PreEvent", 64, "");
  oEventData->addVariableString("PostEvent", 64, "");
  oEventData->addVariableString("ImportStamp", 14, "Stamp");

  // Positive number -> stage number, negative number -> no stage number. Zero = unknown
  oEventData->addVariableInt("EventNumber", oDataContainer::oIS8, "");

  oEventData->addVariableInt("CurrencyFactor", oDataContainer::oIS16, "Valutafaktor");
  oEventData->addVariableString("CurrencySymbol", 5, "Valutasymbol");
  oEventData->addVariableString("CurrencySeparator", 2, "Decimalseparator");
  oEventData->addVariableInt("CurrencyPreSymbol", oDataContainer::oIS8, "Symbolläge");
  oEventData->addVariableString("CurrencyCode", 5, "Valutakod");
  oEventData->addVariableInt("UTC", oDataContainer::oIS8, "UTC");
  
  oEventData->addVariableInt("Analysis", oDataContainer::oIS8, "Utan analys");
  // With split time analysis (0 = default, with analysis, with min/km)
  // bit 1: without analysis
  // bit 2: without min/km
  // bit 4: without result

  oEventData->addVariableString("SPExtra", "Extra rader");
  oEventData->addVariableString("IVExtra", "Fakturainfo");
  oEventData->addVariableString("Features", "Funktioner");
  oEventData->addVariableString("EntryExtra", "Extra rader");
  oEventData->addVariableInt("NumStages", oDataContainer::oIS8, "Antal etapper");
  oEventData->addVariableInt("BibGap", oDataContainer::oIS8U, "Nummerlappshopp");
  oEventData->addVariableInt("BibsPerClass", oDataContainer::oIS8U, "Antal nummerlappar");
  oEventData->addVariableInt("LongTimes", oDataContainer::oIS8U, "Långa tider");
  oEventData->addVariableInt("SubSeconds", oDataContainer::oIS8U, "Tiondelar");  
  oEventData->addVariableString("PayModes", "Betalsätt");
  oEventData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring");
  auto &pd = oEventData->addVariableInt("ScoreDecimal", oDataContainer::oIS8U, "Poängdecimaler").fixedSet;
  pd.emplace_back(L"Inga", 0);
  pd.emplace_back(L"1", 1);
  pd.emplace_back(L"2", 2);
  pd.emplace_back(L"3", 3);

  oEventData->addVariableDate("InvoiceDate", "Fakturadatum");
  oEventData->addVariableString("StartGroups", "Startgrupper");
  oEventData->addVariableString("MergeTag", 12, "Tag");
  oEventData->addVariableString("MergeInfo", "MergeInfo");
  oEventData->addVariableString("SplitPrint", 40, "Sträcktidslista"); // Id from MetaListContainer::getUniqueId
  oEventData->addVariableInt("NoVacantBib", oDataContainer::oIS8U, "Inga vakanta nummerlappar");
  oEventData->addVariableString("RunnerIdTypes", "External ID types");
  oEventData->addVariableString("ExtraFields", "Extra fields");
  oEventData->addVariableString("ControlMap", "Kontrollmappning");
  oEventData->addVariableInt("OldCards", oDataContainer::oIS8U, "Brickversionsvarning");
  oEventData->initData(this, dataSize);

  oClubData=new oDataContainer(oClub::dataSize);
  oClubData->addVariableInt("District", oDataContainer::oIS32, "Organisation");

  oClubData->addVariableString("ShortName", 8, "Kortnamn", make_shared<ShortNameFormatter>()).dataNotifier = make_shared<ShortNameChangedNf>();
  oClubData->addVariableString("CareOf", 31, "c/o");
  oClubData->addVariableString("Street", 41, "Gata");
  oClubData->addVariableString("City", 23, "Stad");
  oClubData->addVariableString("State", 23, "Region");
  oClubData->addVariableString("ZIP", 11, "Postkod");
  oClubData->addVariableString("EMail", 64, "E-post");
  oClubData->addVariableString("Phone", 32, "Telefon");
  oClubData->addVariableString("Nationality", 3, "Nationalitet");
  oClubData->addVariableString("Country", 23, "Land");
  oClubData->addVariableString("Type", 20, "Typ");
  oClubData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");

  vector< pair<wstring,wstring> > eInvoice;
  eInvoice.push_back(make_pair(L"E", L"Elektronisk"));
  eInvoice.push_back(make_pair(L"A", L"Elektronisk godkänd"));
  eInvoice.push_back(make_pair(L"P", L"Ej elektronisk"));
  eInvoice.push_back(make_pair(L"", makeDash(L"-")));
  oClubData->addVariableEnum("Invoice", 1, "Faktura", eInvoice);
  oClubData->addVariableInt("InvoiceNo", oDataContainer::oIS16U, "Fakturanummer");
  oClubData->addVariableInt("StartGroup", oDataContainer::oIS32, "Startgrupp", make_shared<StartGroupFormatter>());

  oRunnerData=new oDataContainer(oRunner::dataSize);
  oRunnerData->addVariableCurrency("Fee", "Anm. avgift").dataNotifier = make_shared<FeeChangedNf>();
  oRunnerData->addVariableCurrency("CardFee", "Brickhyra");
  oRunnerData->addVariableCurrency("Paid", "Betalat").dataNotifier = make_shared<PaymentChangedNf>();
  oRunnerData->addVariableInt("PayMode", oDataContainer::oIS8U, "Betalsätt", make_shared<PayMethodFormatter>());
  oRunnerData->addVariableCurrency("Taxable", "Skattad avgift");
  oRunnerData->addVariableInt("BirthYear", oDataContainer::oISDateOrYear, "RunnerBirthDate");
  oRunnerData->addVariableString("Bib", 8, "Nummerlapp").zeroSortPadding = 5;
  oRunnerData->addVariableInt("Rank", oDataContainer::oIS32, "Ranking", make_shared<RankScoreFormatter>());
  
  oRunnerData->addVariableDate("EntryDate", "Anm. datum");
  oRunnerData->addVariableInt("EntryTime", oDataContainer::oISTime, "Anm. tid",  make_shared<AbsoluteTimeFormatter>("EntryTime", true, SubSecond::Off));

  vector<pair<wstring,wstring>> sex;
  sex.push_back(make_pair(L"M", L"Man"));
  sex.push_back(make_pair(L"F", L"Kvinna"));
  sex.push_back(make_pair(L"", makeDash(L"-")));

  oRunnerData->addVariableEnum("Sex", 1, "Kön", sex);
  oRunnerData->addVariableString("Nationality", 3, "Nationalitet");
  oRunnerData->addVariableString("Country", 23, "Land");
  oRunnerData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oRunnerData->addVariableInt("ExtId2", oDataContainer::oIS64, "Externt Id 2");

  oRunnerData->addVariableInt("Priority", oDataContainer::oIS8U, "Prioritering");
  oRunnerData->addVariableString("Phone", 20, "Telefon");

  oRunnerData->addVariableInt("RaceId", oDataContainer::oIS32, "Lopp-id", make_shared<oRunner::RaceIdFormatter>());

  oRunnerData->addVariableInt("TimeAdjust", oDataContainer::oISTimeAdjust, "Tidsjustering");
  oRunnerData->addVariableInt("PointAdjust", oDataContainer::oIS32, "Poängjustering", make_shared<ScoreFormatter>("PointAdjust"));
  oRunnerData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring", make_shared<TransferFlagsFormatter>(false));
  oRunnerData->addVariableInt("Shorten", oDataContainer::oIS8U, "Avkortning");
  oRunnerData->addVariableInt("EntrySource", oDataContainer::oIS32, "Källa");
  oRunnerData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oRunnerData->addVariableInt("Reference", oDataContainer::oIS32, "Referens", make_shared<oRunner::RunnerReference>());
  oRunnerData->addVariableInt("NoRestart", oDataContainer::oIS8U, "Ej omstart", make_shared<DataBoolean>("NoRestart"));
  oRunnerData->addVariableString("InputResult", "Tidigare resultat", make_shared<DataHider>());
  oRunnerData->addVariableInt("StartGroup", oDataContainer::oIS32, "Startgrupp", make_shared<StartGroupFormatter>());
  oRunnerData->addVariableInt("Family", oDataContainer::oIS32, "Familj");
  oRunnerData->addVariableInt("DrawnTime", oDataContainer::oISTime, "Lottad tid", make_shared<DataHider>());

  oRunnerData->addVariableInt("DataA", oDataContainer::oIS32, "RunnerDataA");
  oRunnerData->addVariableInt("DataB", oDataContainer::oIS32, "RunnerDataB");
  oRunnerData->addVariableString("TextA", 40, "RunnerTextA");
  oRunnerData->addVariableString("Annotation", "Kommentarer", make_shared<AnnotationFormatter>());

  oControlData=new oDataContainer(oControl::dataSize);
  oControlData->addVariableInt("TimeAdjust", oDataContainer::oISTimeAdjust, "Tidsjustering");
  oControlData->addVariableInt("MinTime", oDataContainer::oISTime, "Minitid");
  oControlData->addVariableDecimal("xpos", "x", 1);
  oControlData->addVariableDecimal("ypos", "y", 1);
  oControlData->addVariableDecimal("latcrd", "Latitud", 6);
  oControlData->addVariableDecimal("longcrd", "Longitud", 6);

  oControlData->addVariableInt("Rogaining", oDataContainer::oIS32, "Poäng", make_shared<ScoreFormatter>("Rogaining"));
  oControlData->addVariableInt("Radio", oDataContainer::oIS8U, "Radio");
  oControlData->addVariableInt("Unit", oDataContainer::oIS16U, "Enhet");

  oCourseData = new oDataContainer(oCourse::dataSize);
  oCourseData->addVariableInt("NumberMaps", oDataContainer::oIS16, "Kartor");
  oCourseData->addVariableString("StartName", 16, "Start");
  oCourseData->addVariableInt("Climb", oDataContainer::oIS16, "Stigning");
  oCourseData->addVariableInt("RPointLimit", oDataContainer::oIS32, "Poänggräns", make_shared<ScoreFormatter>("RPointLimit"));
  oCourseData->addVariableInt("RTimeLimit", oDataContainer::oISTime, "Tidsgräns");
  oCourseData->addVariableInt("RReduction", oDataContainer::oIS32, "Poängreduktion", make_shared<ScoreFormatter>("RReduction"));
  oCourseData->addVariableInt("RReductionMethod", oDataContainer::oIS8U, "Reduktionsmetod");
  oCourseData->addVariableInt("NoLatePoints", oDataContainer::oIS8U, "Inga sena poäng");

  oCourseData->addVariableInt("FirstAsStart", oDataContainer::oIS8U, "Från första", make_shared<DataBoolean>("FirstAsStart"));
  oCourseData->addVariableInt("LastAsFinish", oDataContainer::oIS8U, "Till sista", make_shared<DataBoolean>("LastAsFinish"));

  oCourseData->addVariableInt("CControl", oDataContainer::oIS16U, "Varvningskontroll"); //Common control index
  oCourseData->addVariableInt("Shorten", oDataContainer::oIS32, "Avkortning"); 
 
  oClassData = new oDataContainer(oClass::dataSize);
  oClassData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oClassData->addVariableString("LongName", 32, "Långt namn");
  oClassData->addVariableInt("LowAge", oDataContainer::oIS8U, "Undre ålder");
  oClassData->addVariableInt("HighAge", oDataContainer::oIS8U, "Övre ålder");
  oClassData->addVariableInt("HasPool", oDataContainer::oIS8U, "Banpool", make_shared<DataBoolean>("HasPool"));
  oClassData->addVariableInt("AllowQuickEntry", oDataContainer::oIS8U, "Direktanmälan", make_shared<DataBoolean>("AllowQuickEntry"));

  oClassData->addVariableString("ClassType", 40, "Klasstyp");

  vector< pair<wstring,wstring> > sexClass;
  sexClass.push_back(make_pair(L"M", L"Män"));
  sexClass.push_back(make_pair(L"F", L"Kvinnor"));
  sexClass.push_back(make_pair(L"B", L"Alla"));
  sexClass.push_back(make_pair(L"", makeDash(L"-")));

  oClassData->addVariableEnum("Sex", 1, "Kön", sexClass);
  oClassData->addVariableString("StartName", 16, "Start");
  oClassData->addVariableInt("StartBlock", oDataContainer::oIS8U, "Block");
  oClassData->addVariableInt("NoTiming", oDataContainer::oIS8U, "Ej tidtagning", make_shared<DataBoolean>("NoTiming"));
  oClassData->addVariableInt("FreeStart", oDataContainer::oIS8U, "Fri starttid", make_shared<DataBoolean>("FreeStart"));
  oClassData->addVariableInt("RequestStart", oDataContainer::oIS8U, "Boka starttid", make_shared<DataBoolean>("RequestStart"));

  oClassData->addVariableInt("IgnoreStart", oDataContainer::oIS8U, "Ej startstämpling", 
                             make_shared<DataBoolean>("IgnoreStart")).dataNotifier = make_shared<IgnoredStartPunchNf>();

  oClassData->addVariableInt("FirstStart", oDataContainer::oISTime, "Första start", make_shared<RelativeTimeFormatter>("FirstStart"));
  oClassData->addVariableInt("StartInterval", oDataContainer::oISTime, "Intervall", make_shared<AbsoluteTimeFormatter>("StartInterval", false, SubSecond::Auto));
  oClassData->addVariableInt("Vacant", oDataContainer::oIS8U, "Vakanser");
  oClassData->addVariableInt("Reserved", oDataContainer::oIS16U, "Extraplatser");

  oClassData->addVariableCurrency("ClassFee", "Avgift");
  oClassData->addVariableCurrency("HighClassFee", "Sen avgift");
  oClassData->addVariableCurrency("SecondHighClassFee", "Sen avgift\u00d72");
  oClassData->addVariableCurrency("ClassFeeRed", "Red. avgift");
  oClassData->addVariableCurrency("HighClassFeeRed", "Sen red. avgift");
  oClassData->addVariableCurrency("SecondHighClassFeeRed", "Sen red. avgift\u00d72");

  oClassData->addVariableInt("SortIndex", oDataContainer::oIS32, "Sortering");
  oClassData->addVariableInt("MaxTime", oDataContainer::oISTime, "Maxtid");

  vector<pair<wstring, wstring>> statusClass;
  oClass::fillClassStatus(statusClass);
  oClassData->addVariableEnum("Status", 2, "Status", statusClass);
  oClassData->addVariableInt("DirectResult", oDataContainer::oIS8, "Resultat vid målstämpling", make_shared<DataBoolean>("DirectResult"));
  oClassData->addVariableString("Bib", 8, "Nummerlapp");

  vector< pair<wstring,wstring> > bibMode;
  bibMode.push_back(make_pair(L"", L"Från lag"));
  bibMode.push_back(make_pair(L"A", L"Lag + sträcka"));
  bibMode.push_back(make_pair(L"F", L"Fritt"));

  oClassData->addVariableEnum("BibMode", 1, "Nummerlappshantering", bibMode);
  oClassData->addVariableInt("Unordered", oDataContainer::oIS8U, "Oordnade parallella");
  oClassData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oClassData->addVariableInt("Locked", oDataContainer::oIS8U, "Låst gaffling", make_shared<DataBoolean>("Locked"));
  oClassData->addVariableString("Qualification", "Kvalschema", make_shared<DataHider>());
  oClassData->addVariableInt("NumberMaps", oDataContainer::oIS16, "Kartor");
  oClassData->addVariableString("Result", 24, "Result module", make_shared<ResultModuleFormatter>());
  oClassData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring", make_shared<DataHider>());
  oClassData->addVariableString("SplitPrint", 40, "Sträcktidslista", make_shared<SplitPrintListFormatter>());
  oClassData->addVariableInt("DataA", oDataContainer::oIS32, "Data A");
  oClassData->addVariableInt("DataB", oDataContainer::oIS32, "Data B");
  oClassData->addVariableString("TextA", 40, "Text");
  oClassData->addVariableInt("NoTotalResult", oDataContainer::oIS8U, "Endast etappresultat", make_shared<DataBoolean>("NoTotalResult"));

  oTeamData = new oDataContainer(oTeam::dataSize);
  oTeamData->addVariableCurrency("Fee", "Anm. avgift");
  oTeamData->addVariableCurrency("Paid", "Betalat");
  oTeamData->addVariableInt("PayMode", oDataContainer::oIS8U, "Betalsätt");
  oTeamData->addVariableCurrency("Taxable", "Skattad avgift");
  oTeamData->addVariableDate("EntryDate", "Anm. datum");
  oTeamData->addVariableInt("EntryTime", oDataContainer::oISTime, "Anm. tid", make_shared<AbsoluteTimeFormatter>("EntryTime", true, SubSecond::Off));
  oTeamData->addVariableString("Nationality", 3, "Nationalitet");
  oTeamData->addVariableString("Country", 23, "Land");
  oTeamData->addVariableString("Bib", 8, "Nummerlapp").zeroSortPadding = 5;
  oTeamData->addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
  oTeamData->addVariableInt("Priority", oDataContainer::oIS8U, "Prioritering");
  oTeamData->addVariableInt("SortIndex", oDataContainer::oIS16, "Sortering");
  oTeamData->addVariableInt("TimeAdjust", oDataContainer::oISTimeAdjust, "Tidsjustering");
  oTeamData->addVariableInt("PointAdjust", oDataContainer::oIS32, "Poängjustering", make_shared<ScoreFormatter>("PointAdjust"));
  oTeamData->addVariableInt("TransferFlags", oDataContainer::oIS32, "Överföring", make_shared<TransferFlagsFormatter>(true));
  oTeamData->addVariableInt("EntrySource", oDataContainer::oIS32, "Källa");
  oTeamData->addVariableInt("Heat", oDataContainer::oIS8U, "Heat");
  oTeamData->addVariableInt("NoRestart", oDataContainer::oIS8U, "Ej omstart");
  oTeamData->addVariableString("InputResult", "Tidigare resultat", make_shared<DataHider>());

  oTeamData->addVariableInt("DataA", oDataContainer::oIS32, "TeamDataA");
  oTeamData->addVariableInt("DataB", oDataContainer::oIS32, "TeamDataB");
  oTeamData->addVariableString("TextA", 40, "TeamTextA");
  oTeamData->addVariableString("Annotation", "Kommentarer", make_shared<AnnotationFormatter>());

  generalResults.push_back(GeneralResultCtr("atcontrol", L"Result at a control", make_shared<ResultAtControl>()));
  generalResults.push_back(GeneralResultCtr("totatcontrol", L"Total/team result at a control", make_shared<TotalResultAtControl>()));

  currentClientCS = 0;
  memset(CurrentFile, 0, sizeof(CurrentFile));
}

oEvent::~oEvent() {
  //Clean up things in the right order.
  clear();
  runnerDB.reset();
  delete meosFeatures;
  meosFeatures = 0;

  delete oEventData;
  delete oRunnerData;
  delete oClubData;
  delete oControlData;
  delete oCourseData;
  delete oClassData;
  delete oTeamData;

  delete openFileLock;
  delete listContainer;

  return;
}

bool oEvent::useSubSecond() const {
  if (useSubsecondsVersion == dataRevision)
    return useSubSecondsCache;

  auto check = [](int rt) {
    return rt > 0 && (rt % timeConstSecond) != 0;
    };

  for (auto& r : Runners) {
    if (!r.isRemoved()) {
      if (check(r.getFinishTime()) || check(r.getStartTime())) {
        useSubSecondsCache = true;
        useSubsecondsVersion = dataRevision;
        return true;
      }
    }
  }

  useSubSecondsCache = false;
  useSubsecondsVersion = dataRevision;
  return false;
}

void oEvent::initProperties() {
  setProperty("Language", getPropertyString("Language", L"103"));

  setProperty("Interactive", getPropertyString("Interactive", L"1"));
  setProperty("Database", getPropertyString("Database", L"1"));

  // Setup some defaults
  getPropertyInt("SplitLateFees", false);
  getPropertyInt("DirectPort", 21338);
  getPropertyInt("UseHourFormat", 1);
  getPropertyInt("UseDirectSocket", true);
  getPropertyInt("UseEventorUTC", 0);
  getPropertyInt("UseHourFormat", 1);
  getPropertyInt("NameMode", FirstLast);
  getPropertyBool("CompactClubName", false);
  getPropertyBool("PreferShortClubName", true);
}

void oEvent::listProperties(bool userProps, vector< pair<string, PropertyType> > &propNames) const {
  
  
  set<string> filter;
  if (userProps) {
    filter.insert("Language");
    filter.insert("apikey");
    filter.insert("Colors");
    filter.insert("xpos");
    filter.insert("ypos");
    filter.insert("xsize");
    filter.insert("ysize");
    filter.insert("ListType");
    filter.insert("LastCompetition");
    filter.insert("DrawTypeDefault");
    filter.insert("Email"); 
    filter.insert("TextSize");
    filter.insert("TextFont");
    filter.insert("PayModes");
    filter.insert("ReadVoltageExp");
    filter.insert("ControlMap");
    filter.insert("InputServer");
  }

  // Boolean and integer properties
  set<string> b, i;

  // Booleans
  b.insert("AdvancedClassSettings");
  b.insert("AutoTie");
  b.insert("CurrencyPreSymbol");
  b.insert("Database");
  b.insert("Interactive");
  b.insert("intertime");
  b.insert("ManualInput");
  b.insert("PageBreak");
  b.insert("RentCard");
  b.insert("SpeakerShortNames");
  b.insert("splitanalysis");
  b.insert("UseDirectSocket");
  b.insert("UseEventor");
  b.insert("UseEventorUTC");
  b.insert("UseHourFormat");
  b.insert("SplitLateFees");
  b.insert("WideSplitFormat");
  b.insert("pagebreak");
  b.insert("FirstTime");
  b.insert("ExportCSVSplits");
  b.insert("DrawInterlace");
  b.insert("PlaySound");
  b.insert("showheader");
  b.insert("AutoTieRent");
  b.insert("ExpWithRaceNo");
  b.insert("IncludePreliminary");
  b.insert("CompactClubName");
  b.insert("OldCards");
  b.insert("PreferShortClubName");

  // Integers
  i.insert("YouthFee");
  i.insert("YouthAge");
  i.insert("TextSize");
  i.insert("SynchronizationTimeOut");
  i.insert("SeniorAge");
  i.insert("Port");
  i.insert("MaximumSpeakerDelay");
  i.insert("FirstInvoice");
  i.insert("EntryFee");
  i.insert("EliteFee");
  i.insert("DirectPort");
  i.insert("DatabaseUpdate");
  i.insert("ControlTo");
  i.insert("ControlFrom");
  i.insert("CardFee");
  i.insert("addressypos");
  i.insert("addressxpos");
  i.insert("AutoSaveTimeOut");
  i.insert("ServicePort");
  i.insert("CodePage");

  propNames.clear();
  for(map<string, wstring>::const_iterator it = eventProperties.begin(); 
      it != eventProperties.end(); ++it) {
    if (it->first.size() > 1 && it->first[0] == '@')
      continue;
    if (!filter.count(it->first)) {
      if (b.count(it->first)) {
        assert(!i.count(it->first));
        propNames.push_back(make_pair(it->first, Boolean));
      }
      else if (i.count(it->first)) {
        propNames.push_back(make_pair(it->first, Integer));
      }
      else
        propNames.push_back(make_pair(it->first, String));
    }
  }
}

pControl oEvent::addControl(int Id, int Number, const wstring &Name)
{
  if (Id<=0)
    Id=getFreeControlId();
  else
    qFreeControlId = max (qFreeControlId, Id);

  oControl c(this);
  c.set(Id, Number, Name);
  addControl(c);

  oe->updateTabs();
  return &Controls.back();
}

int oEvent::getNextControlNumber() const
{
  int c = 31;
  for (oControlList::const_iterator it = Controls.begin(); it!=Controls.end(); ++it)
    c = max(c, it->maxNumber()+1);

  return c;
}

pControl oEvent::addControl(const oControl &oc) {
  if (oc.Id<=0)
    return nullptr;

  if (&oc != tmpControl.get()) {
    if (getControl(oc.Id, false, false))
      return nullptr;
  }
  qFreeControlId = max(qFreeControlId, Id);
  Controls.push_back(oc);
  oe->Controls.back().addToEvent(this, &oc);

  return &Controls.back();
}

DirectSocket &oEvent::getDirectSocket() {
  if (directSocket == 0)
    directSocket = new DirectSocket(getId(), getPropertyInt("DirectPort", 21338));

  return *directSocket;
}

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

  std::tm st = {};
  meos_localtime_now(&st);

  swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
    (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, 0 /* TODO: std::tm has no milliseconds */);

  getUserFile(file, filename);

  {
    wstring nameIdStr = std::filesystem::path(filename).stem().wstring();
    wcsncpy(nameid, nameIdStr.c_str(), 63);
    nameid[63] = L'\0';
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
                      st.tm_mday, (st.tm_mon + 1), st.tm_hour, st.tm_min);

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
      swprintf(fn1, 260, L"%s.bu%d", CurrentFile, k);
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

    swprintf(fn1, 260, L"%s.bu%d", CurrentFile, toDelete);
    ::_wremove(fn1);

    for(int k=toDelete;k>0;k--) {
      swprintf(fn1, 260, L"%s.bu%d", CurrentFile, k-1);
      swprintf(fn2, 260, L"%s.bu%d", CurrentFile, k);
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
        std::ofstream fout(std::filesystem::path(imgFile), std::ios::binary);
        if (!fout)
          error = L"Error opening " + imgFile;
        else {
          if (!fout.write(reinterpret_cast<const char*>(rawData.data()), rawData.size()))
            error = L"Error writing image.";
          else
            added = true;
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

static uint64_t timer;
static string mlog;

static void tic() {
  timer = meos_steady_clock_ms();
  mlog.clear();
}

static void toc(const string &str) {
  uint64_t t = meos_steady_clock_ms();
  if (!mlog.empty())
    mlog += ",\n";
  else
    mlog = "Tid (hundradels sekunder):\n";

  mlog += str + "=" + itos( (t-timer)/10 );
  timer = t;
}

#include "oEventInternal.h"

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
    {
      wstring nameIdStr = std::filesystem::path(CurrentFile).stem().wstring();
      wcsncpy(CurrentNameId, nameIdStr.c_str(), 63);
      CurrentNameId[63] = L'\0';
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

          std::ifstream pFile(std::filesystem::path(imgFile), std::ios::binary | std::ios::ate);
          if (pFile) {
            auto pos = static_cast<size_t>(pFile.tellg());
            pFile.seekg(0);
            bytes.resize(pos);
            bool ok = static_cast<bool>(pFile.read(reinterpret_cast<char*>(bytes.data()), pos));
            pFile.close();

            if (ok)
              image.provideFromMemory(imgId, fileName, bytes);
            else if (err.empty())
              err = L"Failed to load attached image: " + imgFile;
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

pCourse oEvent::addCourse(const wstring &pname, int plengh, int id) {
  oCourse c(this, id);
  c.length = plengh;
  c.name = pname;
  return addCourse(c);
}

pCourse oEvent::addCourse(const oCourse &oc)
{
  if (oc.Id==0)
    return 0;
  else {
    pCourse pOld=getCourse(oc.getId());
    if (pOld)
      return 0;
  }
  Courses.push_back(oc);
  qFreeCourseId=max(qFreeCourseId, oc.getId());

  pCourse pc = &Courses.back();
  pc->addToEvent(this, &oc);

  if (hasDBConnection() && !pc->existInDB() && !pc->isImplicitlyCreated()) {
    pc->changed = true;
    pc->synchronize();
  }
  courseIdIndex[oc.Id] = pc;
  return pc;
}

void oEvent::autoAddTeam(pRunner pr)
{
  //Warning: make sure there is no team already in DB that has not yet been applied yet...
  if (pr && pr->Class) {
    pClass pc = pr->Class;
    if (pc->isSingleRunnerMultiStage()) {
      //Auto create corresponding team
      pTeam t = addTeam(pr->getName(), pr->getClubId(), pc->getId());
      if (pr->StartNo == 0)
        pr->StartNo = Teams.size();
      t->setStartNo(pr->StartNo, ChangeType::Update);
      t->setRunner(0, pr, true);
    }
  }
}

void oEvent::autoRemoveTeam(pRunner pr)
{
  if (pr && pr->Class) {
    pClass pc = pr->Class;
    if (pc->isSingleRunnerMultiStage()) {
      if (pr->tInTeam) {
        // A team may have more than this runner -> do not remove
        bool canRemove = true;
        const auto &runners = pr->tInTeam->Runners;
        for (size_t k = 0; k<runners.size(); k++) {
          if (runners[k] && runners[k]->sName != pr->sName)
            canRemove = false;
        }
        if (canRemove)
          removeTeam(pr->tInTeam->getId());
      }
    }
  }
}

pRunner oEvent::addRunner(const wstring &name, int clubId, int classId,
                          int cardNo, const wstring &birthDate, bool autoAdd)
{
  int birthYear = 0;
  if (!birthDate.empty()) {
    int numY = wtoi(birthDate.c_str());
    if (numY > 0 || (numY==0 && birthDate[0]=='0'))
      birthYear = extendYear(numY);
  }
  pRunner db_r = oe->dbLookUpByCard(cardNo);

  if (db_r && !db_r->matchName(name))
    db_r = 0; // "Existing" card, but different runner


  if (db_r == 0 && getNumberSuffix(name) == 0)
    db_r = oe->dbLookUpByName(name, clubId, classId, birthYear);

  if (db_r) {
    // We got name from DB. Other parameters might have changed from DB.
    if (clubId>0)
      db_r->Club = getClub(clubId);
    db_r->Class = getClass(classId);
    if (cardNo>0)
      db_r->cardNumber = cardNo;
    if (birthYear>0)
      db_r->setBirthDate(birthDate);
    return addRunnerFromDB(db_r, classId, autoAdd);
  }
  oRunner r(this);
  //r.sName = name;
  r.setName(name, false);
  r.getRealName(r.sName, r.tRealName);
  r.Club = getClub(clubId);
  r.Class = getClass(classId);
  if (cardNo>0)
    r.cardNumber = cardNo;
  if (birthYear>0)
    r.setBirthDate(birthDate);
  pRunner pr = addRunner(r, true);
  
  if (pr->getDI().getInt("EntryDate") == 0 && !pr->isVacant()) {
    pr->getDI().setDate("EntryDate", getLocalDate());
    pr->getDI().setInt("EntryTime", getLocalAbsTime());
  }
  if (pr->Class) {
    int heat = pr->Class->getDCI().getInt("Heat");
    if (heat != 0)
      pr->getDI().setInt("Heat", heat);
  }

  pr->updateChanged();

  if (autoAdd)
    autoAddTeam(pr);
  return pr;
}

pRunner oEvent::addRunner(const wstring &pname, const wstring &pclub, int classId,
                          int cardNo, const wstring &birthDate, bool autoAdd)
{
  if (!pclub.empty() || getMeOSFeatures().hasFeature(MeOSFeatures::Clubs)) {
    
    int clubId = 0;
    if (pclub.empty())
      clubId = getVacantClubIfExist(true);
    else
      clubId = getClubCreate(0, pclub)->getId();
    return addRunner(pname, clubId, classId, cardNo, birthDate, autoAdd);
  }
  else
    return addRunner(pname, 0, classId, cardNo, birthDate, autoAdd);
}

pRunner oEvent::addRunnerFromDB(const pRunner db_r,
                                int classId, bool autoAdd)
{
  oRunner r(this);
  r.sName = db_r->sName;
  r.getRealName(r.sName, r.tRealName);
  r.cardNumber = db_r->cardNumber;

  if (db_r->Club) {
    r.Club = getClub(db_r->getClubId());
    if (!r.Club)
      r.Club = addClub(*db_r->Club);
  }

  r.Class=classId ? getClass(classId) : 0;
  memcpy(r.oData, db_r->oData, sizeof(r.oData));

  pRunner pr = addRunner(r, true);
  if (pr->getDI().getInt("EntryDate") == 0 && !pr->isVacant()) {
    pr->getDI().setDate("EntryDate", getLocalDate());
    pr->getDI().setInt("EntryTime", getLocalAbsTime());
  }
  if (r.Class) {
    int heat = r.Class->getDCI().getInt("Heat");
    if (heat != 0)
      pr->getDI().setInt("Heat", heat);
  }

  pr->updateChanged();

  if (autoAdd)
    autoAddTeam(pr);
  return pr;
}

pRunner oEvent::addRunner(const oRunner &r, bool updateStartNo) {
  bool needUpdate = Runners.empty();
  
  Runners.push_back(r);
  pRunner pr=&Runners.back();
  pr->addToEvent(this, &r);

  for (size_t i = 0; i < pr->multiRunner.size(); i++) {
    if (pr->multiRunner[i]) {
      assert(pr->multiRunner[i]->tParentRunner == nullptr || pr->multiRunner[i]->tParentRunner == &r);
      pr->multiRunner[i]->tParentRunner = pr;
    }
  }

  //cardToRunnerHash.reset();
  if (cardToRunnerHash && r.getCardNo() != 0) {
    cardToRunnerHash->emplace(r.getCardNo(), pr);
  } 
  if (classIdToRunnerHash && r.getClassId(false)) {
    (*classIdToRunnerHash)[r.getClassId(true)].push_back(pr);
  }

  if (pr->StartNo == 0 && updateStartNo) {
    pr->StartNo = ++nextFreeStartNo; // Need not be unique
  }
  else {
    nextFreeStartNo = max(nextFreeStartNo, pr->StartNo);
  }

  if (pr->Card)
    pr->Card->tOwner = pr;

  if (hasDBConnection()) {
    if (!pr->existInDB() && !pr->isImplicitlyCreated())
      pr->synchronize();
  }
  if (needUpdate)
    oe->updateTabs();

  if (pr->Class)
    pr->Class->tResultInfo.clear();

  bibStartNoToRunnerTeam.clear();
  runnerById[pr->Id] = pr;

  // Notify runner database that runner has entered
  getRunnerDatabase().hasEnteredCompetition(r.getExtIdentifier());
  return pr;
}

pRunner oEvent::addRunnerVacant(int classId) {
  pRunner r = addRunner(lang.tl(L"Vakant"), getVacantClub(false), classId, 0, L"", true);
  if (r) {
    r->apply(ChangeType::Update, nullptr);
    r->synchronize(true);
  }
  return r;
}

int oEvent::getFreeCourseId()
{
  qFreeCourseId++;
  return qFreeCourseId;
}

int oEvent::getFreeControlId()
{
  qFreeControlId++;
  return qFreeControlId;
}

wstring oEvent::getAutoCourseName() const
{
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), lang.tl("Bana %d").c_str(), Courses.size()+1);
  return bf;
}

int oEvent::getFreeClassId()
{
  qFreeClassId++;
  return qFreeClassId;
}

int oEvent::getFirstClassId(bool teamClass) const {
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->getQualificationFinal())
      return it->Id; // Both team and single

    int ns = it->getNumStages();
    if (ns > 0 && it->getNumDistinctRunners() == 1)
      return it->Id; // Both team and single

    if (teamClass && ns > 0)
      return it->Id;
    else if (!teamClass && ns == 0)
      return it->Id;
  }
  return 0;
}

int oEvent::getFreeCardId()
{
  qFreeCardId++;
  return qFreeCardId;
}

int oEvent::getFreePunchId()
{
  qFreePunchId++;
  return qFreePunchId;
}

wstring oEvent::getAutoClassName() const
{
  wchar_t bf[32];
  swprintf(bf, 32, lang.tl(L"Klass %d").c_str(), Classes.size()+1);
  return bf;
}

wstring oEvent::getAutoTeamName() const
{
  wchar_t bf[32];
  swprintf(bf, 32, lang.tl("Lag %d").c_str(), Teams.size()+1);
  return bf;
}

wstring oEvent::getAutoRunnerName() const
{
  wchar_t bf[32];
  swprintf(bf, 32, lang.tl(L"Deltagare %d").c_str(), Runners.size()+1);
  return bf;
}

int oEvent::getFreeClubId()
{
  qFreeClubId++;
  return qFreeClubId;
}

int oEvent::getFreeRunnerId()
{
  qFreeRunnerId++;
  return qFreeRunnerId;
}

void oEvent::updateFreeId(oBase *obj)
{
  if (typeid(*obj)==typeid(oRunner)){
    qFreeRunnerId=max(obj->Id, qFreeRunnerId);
  }
  else if (typeid(*obj)==typeid(oClass)){
    qFreeClassId=max(obj->Id % MaxClassId, qFreeClassId);
  }
  else if (typeid(*obj)==typeid(oCourse)){
    qFreeCourseId=max(obj->Id, qFreeCourseId);
  }
  else if (typeid(*obj)==typeid(oControl)){
    qFreeControlId=max(obj->Id, qFreeControlId);
  }
  else if (typeid(*obj)==typeid(oClub)){
    if (obj->Id != cVacantId && obj->Id != cVacantId)
      qFreeClubId=max(obj->Id, qFreeClubId);
  }
  else if (typeid(*obj)==typeid(oCard)){
    qFreeCardId=max(obj->Id, qFreeCardId);
  }
  else if (typeid(*obj)==typeid(oFreePunch)){
    qFreePunchId=max(obj->Id, qFreePunchId);
  }
  else if (typeid(*obj)==typeid(oTeam)){
    qFreeTeamId=max(obj->Id, qFreeTeamId);
  }
  /*else if (typeid(*obj)==typeid(oEvent)){
    qFree
  }*/
}

void oEvent::updateFreeId()
{
  {
    oRunnerList::iterator it;
    qFreeRunnerId=0;
    nextFreeStartNo = 0;

    for (it=Runners.begin(); it != Runners.end(); ++it) {
      qFreeRunnerId = max(qFreeRunnerId, it->Id);
      nextFreeStartNo = max(nextFreeStartNo, it->StartNo);
    }
  }
  {
    oClassList::iterator it;
    qFreeClassId=0;
    for (it=Classes.begin(); it != Classes.end(); ++it)
      qFreeClassId=max(qFreeClassId, it->Id  % MaxClassId);
  }
  {
    oCourseList::iterator it;
    qFreeCourseId=0;
    for (it=Courses.begin(); it != Courses.end(); ++it)
      qFreeCourseId=max(qFreeCourseId, it->Id);
  }
  {
    oControlList::iterator it;
    qFreeControlId=0;
    for (it=Controls.begin(); it != Controls.end(); ++it)
      qFreeControlId=max(qFreeControlId, it->Id);
  }
  {
    oClubList::iterator it;
    qFreeClubId=0;
    for (it=Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->Id != cVacantId && it->Id != cNoClubId)
        qFreeClubId=max(qFreeClubId, it->Id);
    }
  }
  {
    oCardList::iterator it;
    qFreeCardId=0;
    for (it=Cards.begin(); it != Cards.end(); ++it)
      qFreeCardId=max(qFreeCardId, it->Id);
  }
  {
    oFreePunchList::iterator it;
    qFreePunchId=0;
    for (it=punches.begin(); it != punches.end(); ++it)
      qFreePunchId=max(qFreePunchId, it->Id);
  }

  {
    oTeamList::iterator it;
    qFreeTeamId=0;
    for (it=Teams.begin(); it != Teams.end(); ++it)
      qFreeTeamId=max(qFreeTeamId, it->Id);
  }
}

int oEvent::getVacantClub(bool returnNoClubClub) {
  if (returnNoClubClub) {
    if (noClubId > 0) {
      pClub pc = getClub(noClubId);
      if (pc != 0 && !pc->isRemoved())
        return noClubId;
    }
    pClub pc = getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(L"No club"); //eng
    if (pc == 0)
      pc = getClub(lang.tl("Klubblös")); //other lang?

    if (pc == 0)
      pc=getClubCreate(cNoClubId, lang.tl("Klubblös"));

    noClubId = pc->getId();
    return noClubId;
  }
  else {
    if (vacantId > 0) {
      pClub pc = getClub(vacantId);
      if (pc != 0 && !pc->isRemoved())
        return vacantId;
    }
    pClub pc = getClub(L"Vakant");
    if (pc == 0)
      pc = getClub(L"Vacant"); //eng
    if (pc == 0)
      pc = getClub(lang.tl("Vakant")); //other lang?

    if (pc == 0)
      pc=getClubCreate(cVacantId, lang.tl("Vakant"));

    vacantId = pc->getId();
    return vacantId;
  }
}

int oEvent::getVacantClubIfExist(bool returnNoClubClub) const
{
  if (returnNoClubClub) {
    if (noClubId > 0) {
      pClub pc = getClub(noClubId);
      if (pc != 0 && !pc->isRemoved())
        return noClubId;
    }
    if (noClubId == -1)
      return 0;
    pClub pc=getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(L"Klubblös");
    if (pc == 0)
      pc = getClub(lang.tl(L"Klubblös")); //other lang?

    if (!pc) {
      noClubId = -1;
      return 0;
    }
    noClubId = pc->getId();
    return noClubId;
  }
  else {
    if (vacantId > 0) {
      pClub pc = getClub(vacantId);
      if (pc != 0 && !pc->isRemoved())
        return vacantId;
    }
    if (vacantId == -1)
      return 0;
    pClub pc=getClub(L"Vakant");
    if (pc == 0)
      pc = getClub(L"Vacant");
    if (pc == 0)
      pc = getClub(lang.tl("Vakant")); //other lang?

    if (!pc) {
      vacantId = -1;
      return 0;
    }
    vacantId = pc->getId();
    return vacantId;
  }
}

pCard oEvent::allocateCard(pRunner owner)
{
  oCard c(this);
  c.tOwner = owner;
  Cards.push_back(c);
  pCard newCard = &Cards.back();
  newCard->addToEvent(this, &c);
  return newCard;
}

bool oEvent::sortRunners(SortOrder so) {
  reinitializeClasses();
  if (so == Custom)
    return false;
  CurrentSortOrder=so;
  Runners.sort();
  return true;
}

bool oEvent::sortRunners(SortOrder so, vector<const oRunner *> &runners) const {
  reinitializeClasses();
  auto oldSortOrder = CurrentSortOrder;
  CurrentSortOrder = so;
  sort(runners.begin(), runners.end(), [](const oRunner * &a, const oRunner * &b)->bool {return *a < *b; });
  CurrentSortOrder = oldSortOrder;
  return true;
}

bool oEvent::sortRunners(SortOrder so, vector<pRunner> &runners) const {
  reinitializeClasses();
  auto oldSortOrder = CurrentSortOrder;
  CurrentSortOrder = so;
  sort(runners.begin(), runners.end(), [](pRunner &a, pRunner &b)->bool {return *a < *b; });
  CurrentSortOrder = oldSortOrder;
  return true;
}

wstring oEvent::getZeroTime() const
{
  return getAbsTime(0);
}

void oEvent::setZeroTime(wstring m, bool manualSet)
{
  const unsigned nZeroTime = convertAbsoluteTime(m);
  if (nZeroTime!=ZeroTime && nZeroTime != -1) {
    if (manualSet)
      setFlag(TransferFlags::FlagManualDateTime, true);

    updateChanged();
    ZeroTime=nZeroTime;
  }
  else if (manualSet && !hasFlag(oEvent::TransferFlags::FlagManualDateTime)) {
    setFlag(TransferFlags::FlagManualDateTime, true);
  }
}

void oEvent::setName(const wstring &m, bool manualSet)
{ 
  wstring tn = trim(m);
  if (tn.empty())
    throw meosException("Tomt namn är inte tillåtet.");

  if (tn != getName()) {
    if (manualSet)
      setFlag(TransferFlags::FlagManualName, true);
    Name = tn;
    updateChanged();
  }
}

void oEvent::setAnnotation(const wstring &m)
{
  if (m!=Annotation) {
    Annotation=m;
    updateChanged();
  }
}

wstring oEvent::getTitleName() const {
  if (empty())
    return L"";
  if (hasPendingDBConnection)
    return getName() + lang.tl(L" (på server)") + lang.tl(L" DATABASE ERROR");
  else if (isClient())
    return getName() + lang.tl(L" (på server)");
  else
    return getName() + lang.tl(L" (lokalt)");
}

void oEvent::setDate(const wstring &m, bool manualSet)
{
  if (m!=Date) {
    int d = convertDateYMD(m, true);
    if (d <= 0)
      throw meosException(L"Felaktigt datumformat 'X' (Använd ÅÅÅÅ-MM-DD).#" + m);
    wstring nDate = formatDate(d, true);
    if (Date != nDate) {
      Date = nDate;
      if (manualSet)
        setFlag(TransferFlags::FlagManualDateTime, true);
      updateChanged();
    }
  }
}

const wstring &oEvent::getAbsTime(uint32_t time, SubSecond mode) const {
  uint32_t t = ZeroTime + time;
  if (int(t)<0)
    t = 0;
  int days = time/(timeConstHour*24);
  if (days <= 0)
    return formatTimeHMS(t % (24*timeConstHour), mode);
  else {
     wstring &res = StringCache::getInstance().wget();
     res = itow(days) + L"D " + formatTimeHMS(t % (24*timeConstHour), mode);
     return res;
  }
}

const wstring &oEvent::getTimeZoneString() const {
  if (!date2LocalTZ.count(Date))
    date2LocalTZ[Date] = ::getTimeZoneString(Date);
  return date2LocalTZ[Date];
}

wstring oEvent::getAbsDateTimeISO(uint32_t time, bool includeDate, bool useGMT) const
{
  int t = ZeroTime + time;
  wstring dateS, timeS;
  if (int(t)<0) {
    dateS = L"2000-01-01";
    if (useGMT)
      timeS = L"00:00:00Z";
    else
      timeS = L"00:00:00" + getTimeZoneString();
  }
  else {
    int extraDay;
    if (useGMT) {
      int offset = ::getTimeZoneInfo(Date) * timeConstSecond;
      t += offset;
      if (t < 0) {
        extraDay = -1;
        t += timeConstHour * 24;
      }
      else {
        extraDay = t / (timeConstHour*24);
      }
      wchar_t bf[64];
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%02d:%02d:%02d", (t/timeConstHour)%24, (t/timeConstMinute)%60, (t/timeConstSecond)%60);
      timeS = bf;
    }
    else {
      wchar_t bf[64];
      extraDay = t / (timeConstHour*24);
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%02d:%02d:%02d", (t/timeConstHour)%24, (t/timeConstMinute)%60, (t/timeConstSecond)%60);
      timeS = bf;
    }

    if (timeConstSecond > 1 && useSubSecond()) {
      wchar_t bf[64];
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L".%03d", (t%10) * (1000/timeConstSecond));
      timeS += bf;
    }

    if (useGMT)
      timeS += L"Z";
    else
      timeS += getTimeZoneString();

    if (includeDate) {
      if (extraDay == 0) {
        dateS = Date;
      }
      else {
        std::tm st = {};
        convertDateYMD(Date, st, false);
        __int64 sec = SystemTimeToInt64TenthSecond(st);
        sec = sec + (extraDay * timeConstHour * 24);
        st = Int64TenthSecondToSystemTime(sec);
        dateS = convertSystemDate(st);
      }
    }
  }

  if (includeDate)
    return dateS + L"T" + timeS;
  else
    return timeS;
}

const wstring& oEvent::formatScore(int score) const {
  if (score == 0)
    return _EmptyWString;
  if (scoreFactor.needsUpdate(*this)) {
    scoreFactor.update(*this, getDCI().getInt("ScoreDecimal"));
  }
  bool negative = score < 0;
  score = std::abs(score);
  wchar_t bfRaw[32];
  wchar_t* bf = bfRaw;
  if (negative) {
    *bf = '-';
    ++bf;
  }

  wchar_t decimal = '.';
  switch (scoreFactor.get()) {
  case 0:
    return itow(score);
  case 1:
    swprintf(bf, 30, L"%d%c%d", score/10, decimal, score%10);
    break;
  case 2:
    swprintf(bf, 30, L"%d%c%02d", score / 100, decimal, score % 100);
    break;
  case 3:
    swprintf(bf, 30, L"%d%c%03d", score / 1000, decimal, score % 1000);
    break;
  default:
    return itow(score);
  }

  wstring& res = StringCache::getInstance().wget();
  res = bfRaw;
  return res;
}

int oEvent::convertScore(const wstring &score) const {
  if (scoreFactor.needsUpdate(*this)) {
    scoreFactor.update(*this, getDCI().getInt("ScoreDecimal"));
  }
  int factor = 0;
  int nDeci = scoreFactor.get();
  switch (scoreFactor.get()) {
  case 0:
    return wtoi(score.c_str());
  case 1:
    factor = 10;
    break;
  case 2:
    factor = 100;
    break;
  case 3:
    factor = 1000;
    break;
  default:
    return wtoi(score.c_str());
  }

  int base = 0;
  int fraction = 0;
  bool decimal = false;
  bool negative = false;
  for (int i = 0; i < score.length(); i++) {
    if (isspace(score[i])) {
      if (base == 0 && fraction == 0)
        continue;
      else
        break;
    }
    if (score[i] == '-') {
      if (base == 0 && fraction == 0) {
        negative = true;
        continue;
      }
      else
        break;
    }

    if (score[i] == '.' || score[i] == ',') {
      decimal = true;
      continue;
    }
    int c = score[i] - '0';
    if (c >= 0 || c < 10) {
      if (!decimal)
        base = base * 10 + c;
      else if (nDeci > 0) {
        fraction = fraction * 10 + c;
        nDeci--;
      }
      else break;

    }
    else break;
  }

  while (nDeci > 0 && fraction != 0) {
    fraction *= 10;
    nDeci--;
  }

  if (negative)
    return -(base * factor + fraction);
  else
    return base * factor + fraction;
}

const wstring &oEvent::getAbsTimeHM(uint32_t time) const {
  uint32_t t=ZeroTime+time;

  if (int(t)<0)
    return makeDash(L"-");

  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%02d:%02d", (t/timeConstHour)%24, (t/timeConstMinute)%60);

  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

//Absolute time string to absolute time int (used by cvs-parser)
int oEvent::convertAbsoluteTime(const string &m)
{
  if (m.empty() || m[0]=='-')
    return -1;

  int len=m.length();
  int firstComma = -1;
  for (int k=0;k<len;k++) {
    BYTE b=m[k];
    if ( !(b==' ' || (b>='0' && b<='9')) ) {
      if (b==':' && firstComma < 0)
        continue;
      else if ((b==',' || b=='.') && firstComma < 0) {
        firstComma = k;
        continue;
      }
      return -1;
    }
  }

  int hour=atoi(m.c_str());

  if (hour<0 || hour>23)
    return -1;

  int minute=0;
  int second=0;

  int kp=m.find_first_of(':');

  if (kp>0)
  {
    string mtext=m.substr(kp+1);
    minute=atoi(mtext.c_str());

    if (minute<0 || minute>60)
      minute=0;

    kp=mtext.find_last_of(':');

    if (kp>0) {
      second=atoi(mtext.substr(kp+1).c_str());

      if (second<0 || second>60)
        second=0;
    }
  }
  int t=hour*timeConstHour+minute*timeConstMinute+second*timeConstSecond;

  if (timeConstSecond > 1 && firstComma > 0) {
    int sub = std::abs(atoi(m.c_str() + firstComma + 1));
    while (sub >= timeConstSecond)
      sub /= timeConstSecond;
  }

  if (t<0) return 0;

  return t;
}

int oEvent::convertAbsoluteTime(const wstring &m)
{
  if (m.empty() || m[0]=='-')
    return -1;

  int len=m.length();
  int firstComma = -1;
  bool anyColon = false;
  for (int k = 0; k < len; k++) {
    wchar_t b = m[k];
    if (!(b == ' ' || (b >= '0' && b <= '9'))) {
      if (b == ':' && firstComma < 0) {
        anyColon = true;
        continue;
      }
      else if ((b == ',' || b == '.') && firstComma < 0) {
        firstComma = k;
        continue;
      }
      return -1;
    }
  }

  int hour=wtoi(m.c_str());

  if (!anyColon && hour>=0 && len>=5) {
    int second = hour % 100;
    hour /= 100;
    int minute = hour % 100;
    hour /= 100;
    if (hour > 23 || minute >=60 || second >= 60)
      return -1;
    return hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond;
  }

  if (hour<0 || hour>23)
    return -1;

  int minute=0;
  int second=0;

  int kp=m.find_first_of(':');

  if (kp>0)
  {
    wstring mtext=m.substr(kp+1);
    minute=wtoi(mtext.c_str());

    if (minute<0 || minute>60)
      minute=0;

    kp=mtext.find_last_of(':');

    if (kp>0) {
      second=wtoi(mtext.substr(kp+1).c_str());

      if (second<0 || second>60)
        second=0;
    }
  }
  int t = hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond;

  if (timeConstSecond > 1 && firstComma > 0) {
    int sub = std::abs(wtoi(m.c_str() + firstComma + 1));
    while (sub >= timeConstSecond)
      sub /= timeConstSecond;
    t += sub;
  }

  if (t<0) return 0;

  return t;
}

int oEvent::getRelativeTime(const string &date, const string &absoluteTime, const string &timeZone) const {

  int atime = convertAbsoluteTime(absoluteTime);

  if ((timeZone == "Z" || timeZone == "z") && atime >= 0) {
    std::tm st = {};
    convertDateYMD(date, st, false);

    st.tm_hour = atime / timeConstHour;
    st.tm_min = (atime / timeConstMinute) % 60;
    st.tm_sec = (atime / timeConstSecond) % 60;
    // Convert UTC tm to local tm (replaces SystemTimeToTzSpecificLocalTime)
    std::tm st_copy = st;
    std::time_t _tt;
#ifdef _WIN32
    _tt = _mkgmtime(&st_copy);
#else
    _tt = timegm(&st_copy);
#endif
    std::tm localTime = {};
#ifdef _WIN32
    localtime_s(&localTime, &_tt);
#else
    localtime_r(&_tt, &localTime);
#endif

    atime = localTime.tm_hour*timeConstHour + localTime.tm_min * timeConstMinute +
      localTime.tm_sec * timeConstSecond;
  }

  if (atime >= 0 && atime < timeConstHour * 24) {
    int rtime = atime - ZeroTime;

    if (rtime <= 0)
      rtime += timeConstHour * 24;

    //Don't allow times just before zero time.
    if (rtime > timeConstHour * 23)
      return -1;

    return rtime;
  }
  else return -1;
}

int oEvent::getRelativeTime(const wstring &m) const {
  int dayIndex = 0;
  for (size_t k = 0; k + 1 < m.length(); k++) {
    int c = m[k];
    if (c == 'D' || c == 'd' || c == 'T' || c == 't') {
      dayIndex = k + 1;
      break;
    }
  }

  int atime;
  int days = 0;
  if (dayIndex == 0)
    atime = convertAbsoluteTime(m);
  else {
    atime = convertAbsoluteTime(m.substr(dayIndex));
    days = wtoi(m.c_str());
  }
  if (atime>=0 && atime <= timeConstHour*24){
    int rtime = atime-ZeroTime;

    if (rtime < 0)
      rtime += timeConstHour*24;

    rtime += days * timeConstHour * 24;
    
    return rtime;
  }
  else return -1;
}



