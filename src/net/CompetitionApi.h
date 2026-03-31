#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/EventRepository.h"

namespace meos_net {

/// Register GET and PUT routes for /api/v1/competition (singleton competition metadata).
void registerCompetitionRoutes(ApiRouter& router, oEvent& oe, meos_db::EventRepository& repo);

} // namespace meos_net
