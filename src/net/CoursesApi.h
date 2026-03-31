#pragma once
#include "ApiRouter.h"
#include "../domain/oEvent.h"
#include "../db/CourseRepository.h"

namespace meos_net {

/// Register CRUD routes for /api/v1/courses and /api/v1/courses/{id}.
void registerCourseRoutes(ApiRouter& router, oEvent& oe, meos_db::CourseRepository& repo);

} // namespace meos_net
