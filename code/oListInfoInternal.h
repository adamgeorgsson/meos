#pragma once
// Internal helpers shared between oListInfo.cpp and oListInfoGen.cpp.
// Include this AFTER all other includes in each .cpp file.
// All callers must have included oListInfo.h, oEvent.h, localizer.h, meos_util.h.

struct PrintPostInfo {
  PrintPostInfo(gdioutput &gdi, const oListParam &par) :
              gdi(gdi), par(par), keepToghether(false) {}

  gdioutput &gdi;
  const oListParam &par;
  oCounter counter;
  bool keepToghether;
  void reset() {keepToghether = false;}
private:
  PrintPostInfo &operator=(const PrintPostInfo &a) {}
};

static inline pair<wstring, bool> getControlName(const oEvent &oe, int courseContolId) {
  pair<int, int> idt = oControl::getIdIndexFromCourseControlId(courseContolId);
  pControl to = oe.getControl(idt.first);
  wstring toS;
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

static inline wstring getFullControlName(const oEvent &oe, int ctrl) {
  pair<wstring, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl(L"Kontroll X#" + toS.first);
}

static inline void getResultTitle(const oEvent &oe, const oListParam &lp, wstring &title) {
  if (lp.useControlIdResultTo <= 0 && lp.useControlIdResultFrom <= 0)
    title = lang.tl(L"Resultat - %s");
  else if (lp.useControlIdResultTo>0 && lp.useControlIdResultFrom<=0){
    pair<wstring, bool> toS = getControlName(oe, lp.useControlIdResultTo);
    if (toS.second)
      title = lang.tl(L"Resultat - %s") + L", " + toS.first;
    else
      title = lang.tl(L"Resultat - %s") + L", " + lang.tl(L"vid kontroll X#" + toS.first);
  }
  else {
    wstring fromS = lang.tl(L"Start"), toS = lang.tl(L"Mål");
    if (lp.useControlIdResultTo>0) {
      toS = getControlName(oe, lp.useControlIdResultTo).first;
    }
    if (lp.useControlIdResultFrom>0) {
      fromS = getControlName(oe, lp.useControlIdResultFrom).first;
    }
    title = lang.tl(L"Resultat mellan X och Y#" + fromS + L"#" + toS);
  }
}

static inline void generateNBestHead(const oListParam &par, oListInfo &li, int ypos) {
  if (par.filterMaxPer > 0)
    li.addHead(oPrintPost(lString, lang.tl(L"Visar de X bästa#" + itow(par.filterMaxPer)), normalText, 0, ypos));
}
