#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/ClassRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/classes and /api/v1/classes/{id}.
void registerClassRoutes(ApiRouter& router, oEvent& oe, meos_db::ClassRepository& repo);

} // namespace meos_net
