#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/RunnerRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/runners and /api/v1/runners/{id}.
/// Supports filtering by ?name=, ?clubId=, ?classId= and pagination via ?page= and ?pageSize=.
void registerRunnerRoutes(ApiRouter& router, oEvent& oe, meos_db::RunnerRepository& repo);

} // namespace meos_net
