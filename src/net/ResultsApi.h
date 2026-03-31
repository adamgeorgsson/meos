#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"

namespace meos_net {

/// Register GET /api/v1/results and GET /api/v1/startlist routes.
/// Results are computed on-demand using oe.calculateResults().
void registerResultsRoutes(ApiRouter& router, oEvent& oe);

} // namespace meos_net
