#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"

namespace meos_net {

/// Register POST /api/v1/cards (card readout submission) and
/// POST /api/v1/runners/{id}/status (manual status override) routes.
void registerCardsRoutes(ApiRouter& router, oEvent& oe);

} // namespace meos_net
