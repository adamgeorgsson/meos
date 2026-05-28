#pragma once

// CSV import for runners (OE format) and relay teams (OS format).
// Depends on: meos_domain, meos_util (csvparser)

#include "csvparser.h"
#include <string>

class oEvent;

namespace meos::io {

class CsvImporter : public csvparser {
public:
  /** Import individual runners from an OE-format CSV file.
   *  Returns the number of imported runners.
   *  Throws meosException on read/parse errors. */
  int importOE_CSV(oEvent& oe, const std::wstring& file);

  /** Import relay teams (and their runner legs) from an OS-format CSV file.
   *  Returns the number of imported teams.
   *  Throws meosException on read/parse errors. */
  int importOS_CSV(oEvent& oe, const std::wstring& file);
};

}  // namespace meos::io
