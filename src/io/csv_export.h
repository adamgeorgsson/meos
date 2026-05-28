#pragma once

// CSV export for runners (OE format).
// Depends on: meos_domain, meos_util (csvparser, time_util)

#include "csvparser.h"
#include <set>
#include <string>

class oEvent;

namespace meos::io {

class CsvExporter : public csvparser {
public:
  /** Export runners from oEvent to an OE-format CSV file.
   *  If 'classes' is non-empty, only runners in those classes are exported.
   *  Returns the number of exported runners.
   *  Throws meosException on write errors. */
  int exportOE_CSV(oEvent& oe,
                   const std::wstring& file,
                   const std::set<int>& classes = {});
};

}  // namespace meos::io
