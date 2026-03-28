// meos_domain — stub oEvent constructor/destructor.
// The full oEvent implementation will be provided in US-003i.

#include "oEvent.h"
#include "oControl.h"       // for oControl::dataSize
#include "oClub.h"          // for oClub::dataSize
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
}

oEvent::~oEvent() {
  delete oControlData;
  oControlData = nullptr;
  delete oClubData;
  oClubData = nullptr;
}
