#pragma once

// Common constants shared across the domain layer.

// Special club IDs matching legacy oEvent.h defines.
constexpr int cVacantId  = 888888888;
constexpr int cNoClubId  = 999999999;

// Sex enum (from legacy meos_util.h)
enum PersonSex { sFemale = 1, sMale, sBoth, sUnknown };
