#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/ControlRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/controls and /api/v1/controls/{id}.
void registerControlRoutes(ApiRouter& router, oEvent& oe, meos_db::ControlRepository& repo);

} // namespace meos_net
