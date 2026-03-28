// meos_domain — stub oEvent constructor/destructor (US-003b).
// The full oEvent implementation will be provided in US-003i.

#include "oEvent.h"
#include "oControl.h"       // for oControl::dataSize
#include "oDataContainer.h" // for oDataContainer

oEvent::oEvent() {
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
}

oEvent::~oEvent() {
  delete oControlData;
  oControlData = nullptr;
}
