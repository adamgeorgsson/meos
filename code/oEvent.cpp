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

  std::tm st = getLocalTm();
  wchar_t bf[64];
  swprintf(bf, 64, L"%d-%02d-%02d", st.tm_year+1900, st.tm_mon+1, st.tm_mday);

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
        std::tm st{};
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
    std::tm st{};
    convertDateYMD(date, st, false);

    st.tm_hour = atime / timeConstHour;
    st.tm_min = (atime / timeConstMinute) % 60;
    st.tm_sec = (atime / timeConstSecond) % 60;
    int subMs = (timeConstSecond > 1) ? (atime % timeConstSecond) * (1000 / timeConstSecond) : 0;
    st.tm_isdst = 0;
    time_t utcTime = mkgmtime(st); // Treat st as UTC
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &utcTime);
#else
    localtime_r(&utcTime, &localTime);
#endif
    atime = localTime.tm_hour*timeConstHour + localTime.tm_min * timeConstMinute +
      localTime.tm_sec * timeConstSecond + (timeConstSecond > 1 ? subMs / (1000 / timeConstSecond) : 0);
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

void oEvent::removeRunner(const vector<int> &ids)
{
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  oRunnerList::iterator it;

  set<int> toRemove;
  for (size_t k = 0; k < ids.size(); k++) {
    int Id = ids[k];
    pRunner r=getRunner(Id, 0);

    if (r==0)
      continue;
    
    if (r->tInTeam) // XXX
      r = r->tParentRunner ? r->tParentRunner : r;
    else if (r->tParentRunner) {      
      r->tParentRunner->createMultiRunner(true, true);
      r = getRunner(Id, 0);
      if (r == nullptr)
        continue;
      else {
        auto &mlr = r->tParentRunner->multiRunner;
        mlr.erase(std::remove(mlr.begin(), mlr.end(), r), mlr.end());
      }
    }
    if (toRemove.count(r->getId()))
      continue; //Already found.

    //Remove a singe runner team
    for (size_t k = 0; k < r->multiRunner.size(); k++) {
      if (r->multiRunner[k])
        toRemove.insert(r->multiRunner[k]->getId());
    }
    autoRemoveTeam(r);
    toRemove.insert(r->Id);
  }

  if (toRemove.empty())
    return;

  dataRevision++;
  set<pClass> affectedCls;
  for (it=Runners.begin(); it != Runners.end();){
    oRunner &cr = *it;
    if (toRemove.count(cr.getId())> 0) {
      if (cr.Class)
        affectedCls.insert(cr.Class);
      if (hasDBConnection())
        sqlRemove(&cr);
      toRemove.erase(cr.getId());
      runnerById.erase(cr.getId());
      if (cr.Card) {
        assert( cr.Card->tOwner == &cr );
        cr.Card->tOwner = nullptr;
      }
      // Reset team runner (this should not happen)
      if (it->tInTeam) {
        if (it->tInTeam->Runners[it->tLeg]==&*it)
          it->tInTeam->Runners[it->tLeg] = nullptr;
      }

      oRunnerList::iterator next = it;
      ++next;

      Runners.erase(it);
      if (toRemove.empty()) {
        break;
      }
      else
      it = next;
    }
    else
      ++it;
  }

  for (set<pClass>::iterator it = affectedCls.begin(); it != affectedCls.end(); ++it) {
    (*it)->clearCache(true);
    (*it)->markSQLChanged(-1,-1);
  }

  oe->updateTabs();
}

void oEvent::removeCourse(int Id)
{
  oCourseList::iterator it;

  for (it=Courses.begin(); it != Courses.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      dataRevision++;
      Courses.erase(it);
      courseIdIndex.erase(Id);
      return;
    }
  }
}

void oEvent::removeClass(int Id)
{
  oClassList::iterator it;
  vector<int> subRemove;
  for (it = Classes.begin(); it != Classes.end(); ++it){
    if (it->Id==Id){
      if (it->getQualificationFinal()) {
        for (int n = 0; n < it->getNumQualificationFinalClasses(); n++) {
          const oClass *pc = it->getVirtualClass(n);
          if (pc && pc != &*it)
            subRemove.push_back(pc->getId());
        }
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Classes.erase(it);
      dataRevision++;
      updateTabs();
      break;
    }
  }
  for (int id : subRemove) {
    removeClass(id);
  }
}

void oEvent::removeControl(int Id)
{
  oControlList::iterator it;

  for (it=Controls.begin(); it != Controls.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      Controls.erase(it);
      dataRevision++;
      return;
    }
  }
}

void oEvent::removeClub(int Id)
{
  oClubList::iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it){
    if (it->Id==Id) {
      if (hasDBConnection())
        sqlRemove(&*it);
      Clubs.erase(it);
      clubIdIndex.erase(Id);
      dataRevision++;
      return;
    }
  }
  if (vacantId == Id)
    vacantId = 0; // Clear vacant id

  if (noClubId == Id)
    noClubId = 0;
}

void oEvent::removeCard(int Id) {
  for (auto it = Cards.begin(); it != Cards.end(); ++it) {
    if (it->getOwner() == 0 && it->Id == Id) {
      if (it->tOwner) {
        if (it->tOwner->Card == &*it)
          it->tOwner->Card = nullptr;
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Cards.erase(it);
      dataRevision++;
      return;
    }
  }
}

bool oEvent::isCourseUsed(int Id) const
{
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (it->isCourseUsed(Id))
      return true;
  }

  oRunnerList::const_iterator rit;

  for (rit=Runners.begin(); rit != Runners.end(); ++rit){
    pCourse pc=rit->getCourse(false);
    if (pc && pc->Id==Id)
      return true;
  }
  return false;
}

bool oEvent::isClassUsed(int Id) const
{
  pClass cl = getClass(Id);
  if (cl && cl->parentClass) {
    if (isClassUsed(cl->parentClass->Id))
      return true;
  }

  set<int> idToCheck;
  idToCheck.insert(Id);
  if (cl) {
    for (int i = 0; i < cl->getNumQualificationFinalClasses(); i++)
      idToCheck.insert(cl->getVirtualClass(i)->getId());
  }
  //Search runners
  for (auto it=Runners.begin(); it != Runners.end(); ++it){
    if (it->isRemoved())
      continue;
    if (idToCheck.count(it->getClassId(false)))
      return true;
  }

  //Search teams
  for (auto tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->isRemoved())
      continue;
    if (idToCheck.count(tit->getClassId(false)))
      return true;
  }
  return false;
}

bool oEvent::isClubUsed(int Id) const
{
  //Search runners
  oRunnerList::const_iterator it;
  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getClubId()==Id)
      return true;
  }

  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->getClubId()==Id)
      return true;
  }

  return false;
}

bool oEvent::isRunnerUsed(int Id) const
{
  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->isRunnerUsed(Id)) {
      if (tit->Class && tit->Class->isSingleRunnerMultiStage())
        //Don't report single-runner-teams as blocking
        continue;
      return true;
    }
  }

  return false;
}

bool oEvent::isControlUsed(int Id) const {
  for (auto& crs : Courses) {
    if (crs.isRemoved())
      continue;
    for (pControl ctrl : crs.controls) {
      if (ctrl && ctrl->Id == Id)
        return true;
    }

    if (crs.finish && crs.finish->Id == Id)
      return true;

    if (crs.start && crs.start->Id == Id)
      return true;
  }
  return false;
}

bool oEvent::classHasResults(int Id) const {
  oRunnerList::const_iterator it;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if ( (Id == 0 || it->getClassId(true) == Id) && (it->getCard() || it->FinishTime))
      return true;
  }

  return false;
}

bool oEvent::classHasTeams(int Id) const
{
  pClass pc = oe->getClass(Id);
  if (pc == 0)
    return false;

  if (pc->getQualificationFinal() != 0)
    return false;

  oTeamList::const_iterator it;
  for (it=Teams.begin(); it != Teams.end(); ++it)
    if (!it->isRemoved() && it->getClassId(false)==Id)
      return true;

  return false;
}


void oEvent::reEvaluateAll(const set<int> &cls, bool doSync)
{
  if (disableRecalculate)
    return;

  if (doSync)
    autoSynchronizeLists(false);

  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it) {
    if (cls.empty() || cls.count(it->Id)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!cls.empty() && cls.count(tit->getClassId(false)) == 0)
      continue;

    if (!tit->isRemoved()) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  oRunnerList::iterator it;

  if (cls.size() < 5) {
    vector<pRunner> runners;
    getRunners(cls, runners);
    for (pRunner it : runners) {
      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }
  else {
    for (it = Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  vector<pair<int, pControl>> mp;
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
         continue;

      if (!it->isRemoved()) {
        if (it->tLeg == leg) {
          it->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
          it->storeTimes();
        }
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }

  // Mark info as complete
  for (auto& c : Classes) {
    if (!c.isRemoved() && (cls.empty() || cls.count(c.Id)))
      for (auto &i : c.tLeaderTime)
        i.setComplete();
  }

  // Update team start times etc.
  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!tit->isRemoved()) {
      if (!cls.empty() && cls.count(tit->getClassId(true)) == 0)
        continue;

      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved()) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && (it->Class->isQualificationFinalBaseClass())))
        it->apply(ChangeType::Quiet, nullptr);
      it->storeTimes();
      it->clearOnChangedRunningTime();
    }
  }
  //reCalculateLeaderTimes(0);
}

void oEvent::reEvaluateChanged()
{
  if (sqlClasses.changed || sqlCourses.changed || sqlControls.changed) {
    reEvaluateAll(set<int>(), false);
    globalModification = true;
    return;
  }

  if (sqlClubs.changed)
    globalModification = true;


  if (!sqlCards.changed && !sqlRunners.changed && !sqlTeams.changed)
    return; // Nothing to do

  map<int, bool> resetClasses;
  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it)  {
    if (it->wasSQLChanged(-1, oPunch::PunchFinish)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
      resetClasses[it->getId()] = it->hasClassGlobalDependence();
      it->updateLeaderTimes();
    }
  }

  unordered_set<int> addedTeams;

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (tit->isRemoved() || !tit->wasSQLChanged())
      continue;

    addedTeams.insert(tit->getId());
    
    tit->apply(ChangeType::Quiet, nullptr);
  }

  oRunnerList::iterator it;
  vector< vector<pRunner> > legRunners(maxRunnersTeam);

  if (Teams.size() > 0) {
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved())
        continue;
      int clz = it->getClassId(true);
      //if (resetClasses.count(clz))
      //  it->storeTimes();

      if (!it->wasSQLChanged() && !resetClasses[clz])
        continue;

      pTeam t = it->tInTeam;
      if (t && !addedTeams.count(t->getId())) {
        addedTeams.insert(t->getId());
        t->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    pRunner r = &*it;
    if (r->isRemoved())
      continue;

    if (r->wasSQLChanged() || (r->tInTeam && addedTeams.count(r->tInTeam->getId()))) {
      unsigned leg = r->tLeg;
      if (leg <0 || leg >= maxRunnersTeam)
        leg = 0;

      if (legRunners[leg].empty())
        legRunners[leg].reserve(Runners.size() / (leg+1));

      legRunners[leg].push_back(r);
      if (!r->tInTeam) {
        r->apply(ChangeType::Quiet, nullptr);
      }
    }
    else {
      if (r->Class && r->Class->wasSQLChanged(-1, oPunch::PunchFinish)) {
        it->storeTimes();
      }
    }
  }

  vector<pair<int, pControl>> mp;

  // Reevaluate
  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      lr[k]->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (addedTeams.count(tit->getId())) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }

  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      if (!lr[k]->tInTeam)
        lr[k]->apply(ChangeType::Quiet, nullptr);
      lr[k]->clearOnChangedRunningTime();
    }
  }
}

void oEvent::reCalculateLeaderTimes(int classId)
{
  if (disableRecalculate)
    return;

  if (classId) {
    pClass cls = getClass(classId);
    if (cls)
      cls->resetLeaderTime();
  }
  else {
    for (auto &c : Classes) {
      if (!c.isRemoved())
        c.resetLeaderTime();
    }
  }
  
  /*
#ifdef _DEBUG
  wchar_t bf[128];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Calculate leader times %d\n", classId);
  OutputDebugString(bf);
#endif
  for (oClassList::iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->isRemoved() && (classId==it->getId() || classId==0))
      it->resetLeaderTime();
  }
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (oRunnerList::iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (!it->isRemoved() && (classId==0 || classId==it->getClassId(true))) {
        if (it->tLeg == leg)
          it->storeTimes();
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }*/
}


wstring oEvent::getCurrentTimeS() const
{
  std::tm st = getLocalTm();

  wchar_t bf[64];
  swprintf(bf, 64, L"%02d:%02d:%02d", st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

int oEvent::findBestClass(const SICard &card, vector<pClass> &classes) const
{
  classes.clear();
  int Distance=-1000;
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it) {
    vector<pCourse> courses;
    it->getCourses(0, courses);
    bool insertClass = false; // Make sure a class is only included once

    for (size_t k = 0; k<courses.size(); k++) {
      pCourse pc = courses[k];
      if (pc) {
        int d=pc->distance(card);

        if (d>=0) {
          if (Distance<0) Distance=1000;

          if (d<Distance) {
            Distance=d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (d == Distance) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
        else {
          if (Distance<0 && d>Distance) {
            Distance = d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (Distance == d) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
      }
    }
  }
  return Distance;
}

void oEvent::convertTimes(pRunner runner, SICard &sic) const
{
  assert(sic.convertedTime != ConvertedTimeStatus::Unknown);
  if (sic.convertedTime == ConvertedTimeStatus::Done)
    return;

  if (sic.convertedTime == ConvertedTimeStatus::Hour12) {

    int startTime = ZeroTime + 2*timeConstHour; //Add two hours. Subtracted below
    if (useLongTimes())
      startTime = 7 * timeConstHour; // Avoid midnight as default. Prefer morning

    int st = -1;
    if (runner) {
      st = runner->getStartTime();
      if (st > 0) {
        if (sic.StartPunch.Code == -1)
          startTime = (ZeroTime + st) % (timeConstHour * 24); // No start punch
        else {
          // Got start punch. If this is close to specified start time,
          // use specified start time
          const int stPunch = sic.StartPunch.Time; // 12 hour
          const int stStart = startTime = (ZeroTime + st) % (timeConstHour * 12); // 12 hour
          if (std::abs(stPunch - stStart) < timeConstHour / 2) {
            startTime = (ZeroTime + st) % (timeConstHour * 24); // Use specified start time (for conversion)
          }
          else {
            st = -1; // Ignore start time
          }
        }
      }
      else {
        st = -1;
      }
    }

    if (st <= -1) {
      // Fallback for no start time. Take from card. Will be wrong if more than 12 hour after ZeroTime
      if (sic.StartPunch.Code != -1) {
        st = sic.StartPunch.Time;
      }
      else if (sic.nPunch > 0 && sic.Punch[0].Time >= 0) {
        st = sic.Punch[0].Time;
      }

      if (st >= 0) { // Optimize local zero time w.r.t first punch
        int relT12 = (st - ZeroTime + timeConstHour * 24) % (timeConstHour * 12);
        startTime = (ZeroTime + relT12) % (timeConstHour * 24);
      }
    }
    int zt = (startTime + 22 * timeConstHour) % (24 * timeConstHour); // Subtract two hours from start time
    sic.analyseHour12Time(zt);
  }
  sic.convertedTime = ConvertedTimeStatus::Done;

  if (sic.CheckPunch.Code!=-1){
    if (sic.CheckPunch.Time<unsigned(ZeroTime))
      sic.CheckPunch.Time+=(24*timeConstHour);

    sic.CheckPunch.Time-=ZeroTime;
  }

   // Support times longer than 24 hours
  int maxLegTime = useLongTimes() ? 22 * timeConstHour : 0;
  
  if (maxLegTime > 0) {

    const int START = 1000;
    const int FINISH = 1001;
    vector<pair<int, int> > times;
    
    if (sic.StartPunch.Code!=-1) {
      if (sic.StartPunch.Time != -1)
        times.push_back(make_pair(sic.StartPunch.Time, START));
    }

    for (unsigned k=0; k <sic.nPunch; k++){
      if (sic.Punch[k].Code!=-1 && sic.Punch[k].Time != -1) {
        times.push_back(make_pair(sic.Punch[k].Time, k));
      }
    }

    if (sic.FinishPunch.Code!=-1 && sic.FinishPunch.Time != 1) {
      times.push_back(make_pair(sic.FinishPunch.Time, FINISH));

    if (!times.empty()) {
      int dayOffset = 0;
      if (times.front().first < int(ZeroTime)) {
        dayOffset = timeConstHour * 24;
        times.front().first += dayOffset;
      }
      for (size_t k = 1; k < times.size(); k++) {
        int delta = times[k].first - (times[k-1].first - dayOffset);
        if (delta < (maxLegTime - 24 * timeConstHour)) {
          dayOffset += 24 * timeConstHour;
        }
        times[k].first += dayOffset;
      }

      // Update card times
      for (size_t k = 0; k < times.size(); k++) {
        if (times[k].second == START)
          sic.StartPunch.Time = times[k].first;
        else if (times[k].second == FINISH)
          sic.FinishPunch.Time = times[k].first;
        else 
          sic.Punch[times[k].second].Time = times[k].first;
        }
      }
    }
  }

  if (sic.StartPunch.Code != -1) {
    if (sic.StartPunch.Time<unsigned(ZeroTime))
      sic.StartPunch.Time+=(24*timeConstHour);

    sic.StartPunch.Time-=ZeroTime;
  }

  for (unsigned k = 0; k < sic.nPunch; k++){
    if (sic.Punch[k].Code!=-1){
      if (sic.Punch[k].Time<unsigned(ZeroTime))
        sic.Punch[k].Time+=(24*timeConstHour);

      sic.Punch[k].Time-=ZeroTime;
    }
  }

  if (sic.FinishPunch.Code!=-1){
    if (sic.FinishPunch.Time<unsigned(ZeroTime))
      sic.FinishPunch.Time+=(24*timeConstHour);

    sic.FinishPunch.Time-=ZeroTime;
  }
}

int oEvent::getFirstStart(int classId, bool considerStartPunches) const {
  int key = (classId + 1) * (considerStartPunches ? -1 : 1);
  
  auto& [revision, firstStart] = cachedFirstStart[key];
  if (dataRevision == revision)
    return firstStart;

  int minTime = timeConstHour * 240;

  for (auto &r : Runners) {
    if (r.isRemoved() || !(classId == 0 || r.getClassId(true) == classId)) 
      continue;
      
    if (r.tStartTime > minTime || r.tStatus == StatusNotCompeting || r.tStartTime <= 0)
      continue;

    if (!considerStartPunches && r.Card && r.tUseStartPunch) {
      int startCode = oPunch::SpecialPunch::PunchStart;
      if (auto c = r.getCourse(false); c && c->useFirstAsStart() && c->getControl(0)) {
        startCode = c->getControl(0)->getFirstNumber();
      }
      bool skip = false;
      for (auto p : r.Card->punches) {
        if (p.getTypeCode() == startCode) {
          skip = true;
          break;
        }
      }
      if (skip)
        continue;
    }
        
    minTime = r.tStartTime;
  }

  if (minTime == timeConstHour * 240)
    minTime = 0;

  revision = dataRevision;
  firstStart = minTime;

  return minTime;
}

bool oEvent::hasRank() const {
  for (auto &r : Runners){
    if (!r.isRemoved()) {
      int rank = r.getDCI().getInt("Rank");
      if (rank > 0 && rank < MaxOrderRank)
        return true;
    }
  }
  return false;
}

void oEvent::setMaximalTime(const wstring &t)
{
  getDI().setInt("MaxTime", convertAbsoluteTime(t));
}

int oEvent::getMaximalTime() const
{
  return getDCI().getInt("MaxTime");
}

wstring oEvent::getMaximalTimeS() const
{
  return formatTime(getMaximalTime());
}


bool oEvent::hasBib(bool runnerBib, bool teamBib) const
{
  if (runnerBib) {
    oRunnerList::const_iterator it;
    for (it=Runners.begin(); it != Runners.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  if (teamBib) {
    oTeamList::const_iterator it;
    for (it=Teams.begin(); it != Teams.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  return false;
}

bool oEvent::hasTeam() const
{
  return Teams.size() > 0;
}

void oEvent::addBib(int ClassId, int leg, const wstring& firstNumber, int limit, bool assignToVacant) {
  if (!classHasTeams(ClassId)) {
    sortRunners(ClassStartTimeClub);

    pClass cls = getClass(ClassId);
    if (cls == 0)
      throw meosException("Class not found");

    if (cls->getParentClass()) {
      cls->getParentClass()->setBibMode(BibFree);
      cls->getParentClass()->synchronize(true);
    }
    if (!firstNumber.empty()) {
      cls->setBibMode(BibFree);
      cls->synchronize(true);
      wchar_t pattern[32];
      int num = oClass::extractBibPattern(firstNumber, pattern);
      int count = 0;
      for (auto& r : Runners) {
        if (r.isRemoved())
          continue;
        if ((ClassId == 0 || r.getClassId(true) == ClassId) && (r.legToRun() == leg || leg == -1)) {
          bool skip = !assignToVacant && r.isVacant();
          wchar_t bib[32];
          swprintf(bib, sizeof(bib)/sizeof(wchar_t), pattern, num);

          pClass pc = r.getClassRef(true);
          if ((limit == 0 || count < limit) && !skip) {
            r.setBib(bib, num, pc ? !pc->lockedForking() : true);
            count++;
            num++;
          }
          else {
            r.setBib(L"", 0, false);//Update only bib
          }

          r.synchronize(true);
        }
      }
    }
    else {
      for (auto r : Runners) {
        if (r.isRemoved())
          continue;
        if (ClassId == 0 || r.getClassId(true) == ClassId) {
          r.setBib(L"", 0, false);//Update only bib
          r.synchronize(true);
        }
      }
    }
  }
  else {
    map<int, int> teamStartNo;

    if (!firstNumber.empty()) {
      // Clear out start number temporarily, to not use it for sorting
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;
        if (!assignToVacant && it->isVacant())
          continue;
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          if (it->getClassRef(false) && it->getClassRef(false)->getBibMode() != BibFree) {
            for (size_t i = 0; i < it->Runners.size(); i++) {
              if (it->Runners[i]) {
                it->Runners[i]->setStartNo(0, ChangeType::Update);
                it->Runners[i]->setBib(L"", 0, false);
              }
            }
          }
          teamStartNo[it->getId()] = it->getStartNo();
          it->setStartNo(0, ChangeType::Update);
        }
      }
    }

    sortTeams(ClassStartTimeClub, 0, true); // Sort on first leg starttime and sortindex

    if (!firstNumber.empty()) {
      wchar_t pattern[32];
      int num = oClass::extractBibPattern(firstNumber, pattern);
      int count = 0;
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;

        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          if ((!assignToVacant && it->isVacant()) || (limit > 0 && count >= limit)) {
            it->setBib(L"", 0, false); // Does nothing, already cleared
            it->applyBibs();
            it->evaluate(ChangeType::Update);
            continue;
          }

          count++;
          wchar_t bib[32];
          swprintf(bib, sizeof(bib)/sizeof(wchar_t), pattern, num);
          bool lockedStartNo = it->Class && it->Class->lockedForking();
          if (lockedStartNo) {
            it->setBib(bib, num, false);
            it->setStartNo(teamStartNo[it->getId()], ChangeType::Update);
          }
          else {
            it->setBib(bib, num, true);
          }
          num++;
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
    else {
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          it->getDI().setString("Bib", L""); //Update only bib
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
  }
}

void oEvent::addAutoBib() {
  bool noBibToVacant = oe->getDCI().getInt("NoVacantBib") != 0;

  sortRunners(ClassStartTimeClub);
  oRunnerList::iterator it;
  int clsId = -1;
  const int bibGap = oe->getBibClassGap();
  int numBibPerClass = oe->getDCI().getInt("BibsPerClass");
  if (numBibPerClass <= 0)
    numBibPerClass = numeric_limits<int>::max();

  int interval = 1;
  set<int> isTeamCls;
  wchar_t pattern[32] = {0};
  wchar_t storedPattern[32] = {0};
  wcscpy_s(storedPattern, L"%d");

  
  map<int, int> teamStartNo;
  // Clear out start number temporarily, to not use it for sorting
  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    pClass cls = tit->getClassRef(false);
    if (cls == 0)
      continue;

    teamStartNo[tit->getId()] = tit->getStartNo();

    wstring bibInfo = cls->getDCI().getString("Bib");
  
    bool teamAssign = !bibInfo.empty() && cls->getNumStages() > 1;

    bool freeMode = cls->getBibMode()==BibFree;
    if (!teamAssign && freeMode)
      continue; // Manul or none
    isTeamCls.insert(cls->getId());

    bool addBib = bibInfo != L"-";

    if (addBib && teamAssign)
      tit->setStartNo(0, ChangeType::Update);

    if (tit->getClassRef(false) && tit->getClassRef(false)->getBibMode() != BibFree) {
      for (size_t i = 0; i < tit->Runners.size(); i++) {
        if (tit->Runners[i]) {
          if (addBib && teamAssign)
            tit->Runners[i]->setStartNo(0, ChangeType::Update);
          if (!freeMode)
            tit->Runners[i]->setBib(L"", 0, false);
        }
      }
    }
  }

  sortTeams(ClassStartTimeClub, 0, true); // Sort on first leg starttime and sortindex
  map<int, vector<pTeam> > cls2TeamList;

  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    int clsId = tit->getClassId(false);
    cls2TeamList[clsId].push_back(&*tit);
  }

  map<int, vector<pRunner> > cls2RunnerList;
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved() || !it->getClassId(false))
      continue;
    int clsId = it->getClassId(true);
    cls2RunnerList[clsId].push_back(&*it);
  }

  Classes.sort();
  int number = 0;

  for (auto &cls : Classes) {
    if (cls.isRemoved())
      continue;
  
    clsId = cls.getId();

    wstring bibInfo = cls.getDCI().getString("Bib");
    if (bibInfo.empty()) {
      // Skip class
      continue;
    }
    else if (bibInfo == L"*") {
      if (number == 0)
        number = 1;
      else 
        number += bibGap;

      if (pattern[0] == 0) {
        wcscpy_s(pattern, storedPattern);
      }
    }
    else if (bibInfo == L"-") {
      if (pattern[0]) {
        wcscpy_s(storedPattern, pattern);
      }
      pattern[0] = 0; // Clear bibs in class

    }
    else {
      number = oClass::extractBibPattern(bibInfo, pattern); 
    }
      
    if (isTeamCls.count(clsId)) {
      vector<pTeam> &tl = cls2TeamList[clsId]; 

      if (cls.getBibMode() == BibAdd) {
        int ns = cls.getNumStages();
        if (ns <= 10)
          interval = 10;
        else
          interval = 100;

        if (bibInfo == L"*") {
          int add = interval - number % interval;
          number += add;
        }
      }
      else {
        interval = 1;
      }

      if (pattern[0] == 0) {
        // Remove bib
        for (size_t k = 0; k < tl.size(); k++) {
          tl[k]->getDI().setString("Bib", L""); //Update only bib
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }
      else  {
        bool lockedForking = cls.lockedForking();
        
        for (size_t k = 0; k < tl.size(); k++) {
          if ( (noBibToVacant && tl[k]->isVacant()) || k >= numBibPerClass) {
            tl[k]->getDI().setString("Bib", L""); //Remove only bib
          }
          else {
            wchar_t buff[32];
            swprintf(buff, sizeof(buff)/sizeof(wchar_t), pattern, number);

            if (lockedForking) {
              tl[k]->setBib(buff, number, false);
              tl[k]->setStartNo(teamStartNo[tl[k]->getId()], ChangeType::Update);
            }
            else {
              tl[k]->setBib(buff, number, true);
            }
            number += interval;
          }
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }

      continue;
    }
    else {
      interval = 1;
    
      vector<pRunner> &rl = cls2RunnerList[clsId]; 
      bool locked = cls.lockedForking();
      if (pattern[0] && cls.getParentClass()) {
        // Switch to free mode if bib set for subclass
        cls.getParentClass()->setBibMode(BibFree);
        cls.setBibMode(BibFree);
        cls.getParentClass()->synchronize(true);
        cls.synchronize(true);
      }
      for (size_t k = 0; k < rl.size(); k++) {
        if (pattern[0] && (!noBibToVacant || !rl[k]->isVacant()) && k < numBibPerClass) {
          wchar_t buff[32];
          swprintf(buff, sizeof(buff)/sizeof(wchar_t), pattern, number);
          rl[k]->setBib(buff, number, !locked);
          number += interval;
        }
        else {
          rl[k]->getDI().setString("Bib", L""); //Update only bib
        }
        rl[k]->synchronize(true);
      }
    }
  }
}

void oEvent::checkOrderIdMultipleCourses(int ClassId) {
  sortRunners(ClassStartTime);
  int order = 1;
  oRunnerList::iterator it;

  //Find first free order
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId) {
      it->synchronize();//Ensure we are up-to-date
      order = max(order, it->StartNo);
    }
  }

  //Assign orders
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassRef(true) && it->getClassRef(true)->lockedForking())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId)
      if (it->StartNo == 0) {
        if (it->getTeam()) {
          if (it->getTeam()->getStartNo() == 0) {
            it->updateStartNo(++order);
          }
          else {
            it->setStartNo(it->getTeam()->getStartNo(), ChangeType::Update);
            it->synchronize(true);
          }
        }
        else {
          it->updateStartNo(++order);
        }
      }
  }
}

