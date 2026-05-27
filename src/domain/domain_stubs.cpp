// Stub implementations for domain entity placeholders.
// Full implementations come in later migration stories.

#include "oRunner.h"
#include "oCard.h"
#include "oDataContainer.h"
static oDataContainer& oRunnerContainer() {
  static oDataContainer dc(8);
  return dc;
}

oDataContainer& oRunner::getDataBuffers(pvoid& data, pvoid& olddata,
                                         pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return oRunnerContainer();
}

// oCard stub DataContainer
static oDataContainer& oCardContainer() {
  static oDataContainer dc(8);
  return dc;
}

oDataContainer& oCard::getDataBuffers(pvoid& data, pvoid& olddata,
                                       pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return oCardContainer();
}
