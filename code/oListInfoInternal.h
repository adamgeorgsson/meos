/********************i****************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

// Internal helpers shared between oListInfo.cpp and oListInfoGen.cpp.
// Not part of the public API.

#pragma once

#include "oListInfo.h"
#include "oEvent.h"
#include "meos_util.h"
#include "localizer.h"
#include <string>
#include <utility>

static void generateNBestHead(const oListParam &par, oListInfo &li, int ypos) {
  if (par.filterMaxPer > 0)
    li.addHead(oPrintPost(lString, lang.tl(L"Visar de X bästa#" + itow(par.filterMaxPer)), normalText, 0, ypos));
}

static std::pair<std::wstring, bool> getControlName(const oEvent &oe, int courseContolId) {
  std::pair<int, int> idt = oControl::getIdIndexFromCourseControlId(courseContolId);
  pControl to = oe.getControl(idt.first);
  std::wstring toS;
  bool name = false;
  if (to) {
    if (to->hasName()) {
      toS = to->getName();
      name = true;
    }
    else if (to->getFirstNumber()>0)
      toS = itow(to->getFirstNumber());
    else
      toS = itow(idt.first);

    if (to->getNumberDuplicates() > 0)
      toS += L"-" + itow(idt.second + 1);
  }
  else
    toS = itow(idt.first);

  return make_pair(toS, name);
}

static std::wstring getFullControlName(const oEvent &oe, int ctrl) {
  std::pair<std::wstring, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl(L"Kontroll X#" + toS.first);
}

static void getResultTitle(const oEvent &oe, const oListParam &lp, std::wstring &title) {
  if (lp.useControlIdResultTo <= 0 && lp.useControlIdResultFrom <= 0)
    title = lang.tl(L"Resultat - %s");
  else if (lp.useControlIdResultTo>0 && lp.useControlIdResultFrom<=0){
    std::pair<std::wstring, bool> toS = getControlName(oe, lp.useControlIdResultTo);
    if (toS.second)
      title = lang.tl(L"Resultat - %s") + L", " + toS.first;
    else
      title = lang.tl(L"Resultat - %s") + L", " + lang.tl(L"vid kontroll X#" + toS.first);
  }
  else {
    std::wstring fromS = lang.tl(L"Start"), toS = lang.tl(L"Mål");
    if (lp.useControlIdResultTo>0) {
      toS = getControlName(oe, lp.useControlIdResultTo).first;
    }
    if (lp.useControlIdResultFrom>0) {
      fromS = getControlName(oe, lp.useControlIdResultFrom).first;
    }
    title = lang.tl(L"Resultat mellan X och Y#" + fromS + L"#" + toS);
  }
}
