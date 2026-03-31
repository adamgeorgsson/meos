#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/TeamRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/teams and /api/v1/teams/{id}.
/// Supports filtering by ?name=, ?clubId=, ?classId= and pagination via ?page= and ?pageSize=.
void registerTeamRoutes(ApiRouter& router, oEvent& oe, meos_db::TeamRepository& repo);

} // namespace meos_net
