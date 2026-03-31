#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/ClubRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/clubs and /api/v1/clubs/{id}.
void registerClubRoutes(ApiRouter& router, oEvent& oe, meos_db::ClubRepository& repo);

} // namespace meos_net
