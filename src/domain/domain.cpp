// meos_domain — stub oEvent constructor/destructor.
// The full oEvent implementation will be provided in US-003i.

#include "oEvent.h"
#include "oControl.h"       // for oControl::dataSize
#include "oClub.h"          // for oClub::dataSize
#include "oCourse.h"        // for oCourse::dataSize
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
}

oEvent::~oEvent() {
  delete oControlData;
  oControlData = nullptr;
  delete oClubData;
  oClubData = nullptr;
  delete oCourseData;
  oCourseData = nullptr;
}
