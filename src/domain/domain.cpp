// meos_domain — stub oEvent constructor/destructor.
// The full oEvent implementation will be provided in US-003i.

#include "oEvent.h"
#include "oControl.h"       // for oControl::dataSize
#include "oClub.h"          // for oClub::dataSize
#include "oCourse.h"        // for oCourse::dataSize
#include "oClass.h"         // for oClass::dataSize
#include "oDataContainer.h" // for oDataContainer

oEvent::oEvent() {
  // ── oControl data fields ──────────────────────────────────────────────────
  oControlData = new oDataContainer(oControl::dataSize);
  oControlData->addVariableInt("TimeAdjust", oDataContainer::oISTimeAdjust, "Tidsjustering");
  oControlData->addVariableInt("MinTime",    oDataContainer::oISTime,       "Minitid");
  oControlData->addVariableDecimal("xpos",    "x",        1);
  oControlData->addVariableDecimal("ypos",    "y",        1);
  oControlData->addVariableDecimal("latcrd",  "Latitud",  6);
  oControlData->addVariableDecimal("longcrd", "Longitud", 6);
  oControlData->addVariableInt("Rogaining",  oDataContainer::oIS32,  "Poäng");
  oControlData->addVariableInt("Radio",      oDataContainer::oIS8U,  "Radio");
  oControlData->addVariableInt("Unit",       oDataContainer::oIS16U, "Enhet");

  // ── oClub data fields ─────────────────────────────────────────────────────
  oClubData = new oDataContainer(oClub::dataSize);
  oClubData->addVariableInt("District",    oDataContainer::oIS32,    "Organisation");
  oClubData->addVariableString("ShortName",   8,  "Kortnamn");
  oClubData->addVariableString("CareOf",      31, "c/o");
  oClubData->addVariableString("Street",      41, "Gata");
  oClubData->addVariableString("City",        23, "Stad");
  oClubData->addVariableString("State",       23, "Region");
  oClubData->addVariableString("ZIP",         11, "Postkod");
  oClubData->addVariableString("EMail",       64, "E-post");
  oClubData->addVariableString("Phone",       32, "Telefon");
  oClubData->addVariableString("Nationality",  3, "Nationalitet");
  oClubData->addVariableString("Country",     23, "Land");
  oClubData->addVariableString("Type",        20, "Typ");
  oClubData->addVariableInt("ExtId",       oDataContainer::oIS64,    "Externt Id");
  oClubData->addVariableInt("InvoiceNo",   oDataContainer::oIS16U,   "Fakturanummer");
  oClubData->addVariableInt("StartGroup",  oDataContainer::oIS32,    "Startgrupp");

  // ── oCourse data fields (matches legacy oEvent.cpp exactly) ─────────────
  // Legacy dataSize = 128 bytes. On Linux wchar_t = 4 bytes, so StartName(16) = 64 bytes.
  // Total ~ 90 bytes, well within 128.
  oCourseData = new oDataContainer(oCourse::dataSize);
  oCourseData->addVariableInt("NumberMaps",       oDataContainer::oIS16,   "Kartor");
  oCourseData->addVariableString("StartName",     16,                       "Start");
  oCourseData->addVariableInt("Climb",            oDataContainer::oIS16,   "Stigning");
  oCourseData->addVariableInt("RPointLimit",      oDataContainer::oIS32,   "Poänggräns");
  oCourseData->addVariableInt("RTimeLimit",       oDataContainer::oISTime, "Tidsgräns");
  oCourseData->addVariableInt("RReduction",       oDataContainer::oIS32,   "Poängreduktion");
  oCourseData->addVariableInt("RReductionMethod", oDataContainer::oIS8U,   "Reduktionsmetod");
  oCourseData->addVariableInt("NoLatePoints",     oDataContainer::oIS8U,   "Inga sena poäng");
  oCourseData->addVariableInt("FirstAsStart",     oDataContainer::oIS8U,   "Från första");
  oCourseData->addVariableInt("LastAsFinish",     oDataContainer::oIS8U,   "Till sista");
  oCourseData->addVariableInt("CControl",         oDataContainer::oIS16U,  "Varvningskontroll");
  oCourseData->addVariableInt("Shorten",          oDataContainer::oIS32,   "Avkortning");

  // ── oClass data fields ───────────────────────────────────────────────────
  oClassData = new oDataContainer(oClass::dataSize);
  oClassData->addVariableInt("ExtId",        oDataContainer::oIS64,  "Externt Id");
  oClassData->addVariableString("LongName",  32, "Långt namn");
  oClassData->addVariableInt("LowAge",       oDataContainer::oIS8U,  "Undre ålder");
  oClassData->addVariableInt("HighAge",      oDataContainer::oIS8U,  "Övre ålder");
  oClassData->addVariableInt("HasPool",      oDataContainer::oIS8U,  "Banpool");
  oClassData->addVariableInt("AllowQuickEntry", oDataContainer::oIS8U, "Direktanmälan");
  oClassData->addVariableString("ClassType", 40, "Klasstyp");

  vector<pair<wstring, wstring>> sexClass;
  sexClass.push_back(make_pair(L"M", L"Män"));
  sexClass.push_back(make_pair(L"F", L"Kvinnor"));
  sexClass.push_back(make_pair(L"B", L"Alla"));
  sexClass.push_back(make_pair(L"",  makeDash(L"-")));
  oClassData->addVariableEnum("Sex", 1, "Kön", sexClass);

  oClassData->addVariableString("StartName",  16, "Start");
  oClassData->addVariableInt("StartBlock",    oDataContainer::oIS8U,  "Block");
  oClassData->addVariableInt("NoTiming",      oDataContainer::oIS8U,  "Ej tidtagning");
  oClassData->addVariableInt("FreeStart",     oDataContainer::oIS8U,  "Fri starttid");
  oClassData->addVariableInt("RequestStart",  oDataContainer::oIS8U,  "Boka starttid");
  oClassData->addVariableInt("IgnoreStart",   oDataContainer::oIS8U,  "Ej startstämpling");
  oClassData->addVariableInt("FirstStart",    oDataContainer::oISTime, "Första start");
  oClassData->addVariableInt("StartInterval", oDataContainer::oISTime, "Intervall");
  oClassData->addVariableInt("Vacant",        oDataContainer::oIS8U,  "Vakanser");
  oClassData->addVariableInt("Reserved",      oDataContainer::oIS16U, "Extraplatser");
  oClassData->addVariableCurrency("ClassFee",              "Avgift");
  oClassData->addVariableCurrency("HighClassFee",          "Sen avgift");
  oClassData->addVariableCurrency("SecondHighClassFee",    "Sen avgift\u00d72");
  oClassData->addVariableCurrency("ClassFeeRed",           "Red. avgift");
  oClassData->addVariableCurrency("HighClassFeeRed",       "Sen red. avgift");
  oClassData->addVariableCurrency("SecondHighClassFeeRed", "Sen red. avgift\u00d72");
  oClassData->addVariableInt("SortIndex",    oDataContainer::oIS32,  "Sortering");
  oClassData->addVariableInt("MaxTime",      oDataContainer::oISTime, "Maxtid");

  vector<pair<wstring, wstring>> statusClass;
  oClass::fillClassStatus(statusClass);
  oClassData->addVariableEnum("Status", 2, "Status", statusClass);

  oClassData->addVariableInt("DirectResult", oDataContainer::oIS8,   "Resultat vid målstämpling");
  oClassData->addVariableString("Bib",       8,  "Nummerlapp");

  vector<pair<wstring, wstring>> bibMode;
  bibMode.push_back(make_pair(L"",  L"Från lag"));
  bibMode.push_back(make_pair(L"A", L"Lag + sträcka"));
  bibMode.push_back(make_pair(L"F", L"Fritt"));
  oClassData->addVariableEnum("BibMode", 1, "Nummerlappshantering", bibMode);

  oClassData->addVariableInt("Unordered",     oDataContainer::oIS8U,  "Oordnade parallella");
  oClassData->addVariableInt("Heat",          oDataContainer::oIS8U,  "Heat");
  oClassData->addVariableInt("Locked",        oDataContainer::oIS8U,  "Låst gaffling");
  oClassData->addVariableString("Qualification", "Kvalschema");
  oClassData->addVariableInt("NumberMaps",    oDataContainer::oIS16,  "Kartor");
  oClassData->addVariableString("Result",     24, "Result module");
  oClassData->addVariableInt("TransferFlags", oDataContainer::oIS32,  "Överföring");
  oClassData->addVariableString("SplitPrint", 40, "Sträcktidslista");
  oClassData->addVariableInt("DataA",         oDataContainer::oIS32,  "Data A");
  oClassData->addVariableInt("DataB",         oDataContainer::oIS32,  "Data B");
  oClassData->addVariableString("TextA",      40, "Text");
  oClassData->addVariableInt("NoTotalResult", oDataContainer::oIS8U,  "Endast etappresultat");

  // ── oEvent own data fields (subset used by oClass fee calculations) ─────────
  // Full oEvent data initialization deferred to US-003i.
  // On Linux sizeof(wchar_t)==4 so fixed strings cost double vs Windows.
  oEventData = new oDataContainer(eventDataSize);
  memset(oEventDataBuf, 0, sizeof(oEventDataBuf));
  oEventData->addVariableCurrency("EliteFee",         "Elitavgift");
  oEventData->addVariableCurrency("EntryFee",         "Normalavgift");
  oEventData->addVariableCurrency("YouthFee",         "Reducerad avgift");
  oEventData->addVariableDate("OrdinaryEntry",        "Ordinarie anmälningsdatum");
  oEventData->addVariableDate("SecondEntryDate",      "Stoppdatum två");
  oEventData->addVariableString("LateEntryFactor",   6, "Avgiftshöjning (procent)");
  oEventData->addVariableString("SecondEntryFactor", 6, "Avgiftshöjning två (procent)");
}

oDataConstInterface oEvent::getDCI() const {
  return oEventData->getConstInterface(oEventDataBuf, eventDataSize, nullptr);
}

oEvent::~oEvent() {
  delete oControlData;
  oControlData = nullptr;
  delete oClubData;
  oClubData = nullptr;
  delete oCourseData;
  oCourseData = nullptr;
  delete oClassData;
  oClassData = nullptr;
  delete oEventData;
  oEventData = nullptr;
}
