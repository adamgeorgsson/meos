// Stub implementations for domain entity placeholders.
// Full implementations come in later migration stories.

#include "oTeam.h"
#include "oDataContainer.h"
static oDataContainer& oTeamContainer() {
  static oDataContainer dc(8);
  return dc;
}

oDataContainer& oTeam::getDataBuffers(pvoid& data, pvoid& olddata,
                                       pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return oTeamContainer();
}
