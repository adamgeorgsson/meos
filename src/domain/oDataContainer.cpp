// oDataContainer.cpp — Data field container implementation.

#include "oDataContainer.h"
#include "oEvent.h"
#include "../util/Table.h"      // Table stub + TID_* constants
#include "../util/xmlparser.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "../util/meosexception.h"
#include "timeconstants.hpp"  // timeConstHour, timeConstMinute, timeConstSecond, NOTIME

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <cinttypes>  // PRId64

// ── Construction ─────────────────────────────────────────────────────────────

oDataContainer::oDataContainer(int maxsize) {
  dataPointer = 0;
  dataMaxSize = maxsize;
  stringIndexPointer = 0;
  stringArrayIndexPointer = 2;
}

oDataContainer::~oDataContainer() {}

// ── oDataDefiner ─────────────────────────────────────────────────────────────

CellType oDataDefiner::getCellType(int /*index*/) const { return cellEdit; }

// ── oDataInfo ─────────────────────────────────────────────────────────────────

oDataInfo::oDataInfo() {
  memset(Name, 0, sizeof(Name));
  Index = 0;
  Size = 0;
  Type = 0;
  SubType = 0;
  decimalSize = 0;
  decimalScale = 1;
  zeroSortPadding = 0;
  memset(Description, 0, sizeof(Description));
}

oDataInfo::~oDataInfo() {}

// ── Add variables ─────────────────────────────────────────────────────────────

oDataInfo &oDataContainer::addVariableInt(const char *name, oIntSize isize,
                                           const char *description,
                                           const shared_ptr<oDataDefiner> &dataDef) {
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  odi.Index = dataPointer;
  strcpy_s(odi.Name, name);
  strcpy_s(odi.Description, description);

  odi.Size = (isize == oIS64) ? sizeof(__int64) : sizeof(int);
  odi.Type = oDTInt;
  odi.SubType = isize;

  if (dataPointer + odi.Size <= dataMaxSize) {
    dataPointer += odi.Size;
    return addVariable(odi);
  }
  throw std::runtime_error("oDataContainer: Out of bounds.");
}

oDataInfo &oDataContainer::addVariableDecimal(const char *name, const char *descr,
                                               int fixedDeci) {
  oDataInfo odi;
  odi.Index = dataPointer;
  strcpy_s(odi.Name, name);
  strcpy_s(odi.Description, descr);
  odi.Size = sizeof(double);
  odi.Type = oDTDouble;
  odi.SubType = 0;
  odi.decimalSize = fixedDeci;
  int &s = odi.decimalScale;
  s = 1;
  for (int k = 0; k < fixedDeci; k++)
    s *= 10;

  // Align double to 8-byte boundary
  if (dataPointer % 8 != 0)
    dataPointer += (8 - dataPointer % 8);

  if (dataPointer + odi.Size <= dataMaxSize) {
    dataPointer += odi.Size;
    return addVariable(odi);
  }
  throw std::runtime_error("oDataContainer: Out of bounds.");
}

oDataInfo &oDataContainer::addVariableString(const char *name, const char *descr,
                                              const shared_ptr<oDataDefiner> &dataDef) {
  return addVariableString(name, -1, descr, dataDef);
}

oDataInfo &oDataContainer::addVariableString(const char *name, int maxChar,
                                              const char *descr,
                                              const shared_ptr<oDataDefiner> &dataDef) {
  oDataInfo odi;
  odi.dataDefiner = dataDef;
  strcpy_s(odi.Name, name);
  strcpy_s(odi.Description, descr);
  if (maxChar > 0) {
    odi.Index = dataPointer;
    odi.Size = sizeof(wchar_t) * (maxChar + 1);
    odi.Type = oDTString;
    odi.SubType = oSSString;
    if (dataPointer + odi.Size <= dataMaxSize) {
      dataPointer += odi.Size;
      return addVariable(odi);
    }
    throw std::runtime_error("oDataContainer: Out of bounds.");
  } else {
    odi.Index = stringIndexPointer++;
    odi.Size = 0;
    odi.Type = oDTStringDynamic;
    odi.SubType = oSSString;
    return addVariable(odi);
  }
}

oDataInfo &oDataContainer::addVariableEnum(const char *name, int maxChar, const char *descr,
                                            const vector<pair<wstring, wstring>> enumValues) {
  oDataInfo &odi = addVariableString(name, maxChar, descr);
  odi.SubType = oSSEnum;
  for (size_t k = 0; k < enumValues.size(); k++)
    odi.enumDescription.push_back(enumValues[k]);
  return odi;
}

oDataInfo &oDataContainer::addVariable(oDataInfo &odi) {
  if (findVariable(odi.Name))
    throw std::runtime_error("oDataContainer: Variable already exist.");
  index.insert(hash(odi.Name), (int)ordered.size());
  ordered.push_back(odi);
  return ordered.back();
}

// ── Hash / find ───────────────────────────────────────────────────────────────

int oDataContainer::hash(const char *name) {
  int res = 0;
  while (*name != 0) {
    res = 31 * res + *name;
    name++;
  }
  return res;
}

oDataInfo *oDataContainer::findVariable(const char *name) {
  int res;
  if (index.lookup(hash(name), res))
    return &ordered[res];
  return nullptr;
}

const oDataInfo *oDataContainer::findVariable(const char *name) const {
  if (name == nullptr) return nullptr;
  int res;
  if (index.lookup(hash(name), res))
    return &ordered[res];
  return nullptr;
}

// ── Init data buffer ──────────────────────────────────────────────────────────

void oDataContainer::initData(oBase *ob, int datasize) {
  if (datasize < dataPointer)
    throw std::runtime_error("oDataContainer: Buffer too small.");

  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memset(data, 0, dataPointer);
  memset(oldData, 0, dataPointer);

  if (stringIndexPointer > 0 || stringArrayIndexPointer > 2) {
    vector<vector<wstring>> &str = *strptr;
    str.clear();
    str.resize(stringArrayIndexPointer);
    str[0].resize(stringIndexPointer);
    str[1].resize(stringIndexPointer);
  }
}

// ── Type queries ──────────────────────────────────────────────────────────────

bool oDataContainer::isInt(const char *name) const {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  return odi->Type == oDTInt;
}

bool oDataContainer::isString(const char *name) const {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  return odi->Type == oDTString || odi->Type == oDTStringDynamic;
}

// ── Integer accessors ─────────────────────────────────────────────────────────

bool oDataContainer::setInt(oBase *ob, void *data, const char *Name, int V) {
  oDataInfo *odi = findVariable(Name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");
  if (odi->SubType == oIS64) throw std::runtime_error("oDataContainer: Variable too large.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  int oldValue = *((int *)vd);
  if (oldValue != V) {
    *((int *)vd) = V;
    if (odi->dataNotifier)
      odi->dataNotifier->notify(ob, oldValue, V);
    return true;
  }
  return false;
}

bool oDataContainer::setInt64(void *data, const char *Name, __int64 V) {
  oDataInfo *odi = findVariable(Name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");
  if (odi->SubType != oIS64) throw std::runtime_error("oDataContainer: Variable too large.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  if (*((__int64 *)vd) != V) {
    *((__int64 *)vd) = V;
    return true;
  }
  return false;
}

int oDataContainer::getInt(const void *data, const char *Name) const {
  const oDataInfo *odi = findVariable(Name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");
  if (odi->SubType == oIS64) throw std::runtime_error("oDataContainer: Variable too large.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  return *((int *)vd);
}

__int64 oDataContainer::getInt64(const void *data, const char *Name) const {
  const oDataInfo *odi = findVariable(Name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  if (odi->SubType == oIS64)
    return *((__int64 *)vd);
  else
    return (int)*((int *)vd);
}

// ── Double accessors ──────────────────────────────────────────────────────────

bool oDataContainer::setDouble(oBase *ob, void *data, const char *name, double value) {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTDouble) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  if (*((double *)vd) != value) {
    *((double *)vd) = value;
    return true;
  }
  return false;
}

double oDataContainer::getDouble(const void *data, const char *name) const {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTDouble) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  return *((double *)vd);
}

// ── String accessors ──────────────────────────────────────────────────────────

bool oDataContainer::setString(oBase *ob, const char *name, const wstring &v) {
  oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");

  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (odi->Type == oDTString) {
    LPBYTE vd = LPBYTE(data) + odi->Index;
    if (wcscmp((wchar_t *)vd, v.c_str()) != 0) {
      wcsncpy_s((wchar_t *)vd, odi->Size / sizeof(wchar_t),
                v.c_str(), (odi->Size - 1) / sizeof(wchar_t));
      if (odi->dataNotifier)
        odi->dataNotifier->notify(ob, v);
      return true;
    }
    return false;
  } else if (odi->Type == oDTStringDynamic) {
    wstring &str = (*strptr)[0][odi->Index];
    if (str == v) return false;
    str = v;
    if (odi->dataNotifier)
      odi->dataNotifier->notify(ob, v);
    return true;
  }
  throw std::runtime_error("oDataContainer: Variable of wrong type.");
}

const wstring &oDataContainer::formatString(const oBase *ob, const char *Name) const {
  const oDataInfo *odi = findVariable(Name);
  if (odi->dataDefiner)
    return odi->dataDefiner->formatData(ob, 0);
  if (odi->Type == oDTString || odi->Type == oDTStringDynamic)
    return getString(ob, Name);
  if (odi->Type == oDTInt)
    return itow(getInt(ob, Name));
  throw std::runtime_error("oDataContainer: Formatting failed.");
}

const wstring &oDataContainer::getString(const oBase *ob, const char *Name) const {
  const oDataInfo *odi = findVariable(Name);

  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");

  if (odi->Type == oDTString) {
    LPBYTE vd = LPBYTE(data) + odi->Index;
    wstring &res = StringCache::getInstance().wget();
    res = (wchar_t *)vd;
    return res;
  } else if (odi->Type == oDTStringDynamic) {
    return (*strptr)[0][odi->Index];
  }
  throw std::runtime_error("oDataContainer: Variable of wrong type.");
}

// ── Date accessors ────────────────────────────────────────────────────────────

bool oDataContainer::setDate(void *data, const char *Name, const wstring &V) {
  oDataInfo *odi = findVariable(Name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  int C = convertDateYMD(V, true);
  if (C <= 0) {
    C = wtoi(V.c_str());
    if (V.length() >= 2 && ((C > 0 && C < 100) || (C == 0 && V[0] == '0' && V[1] == '0')))
      C = extendYear(C);
    if (C < 1900 || C > 9999)
      C = 0;
  }

  LPBYTE vd = LPBYTE(data) + odi->Index;
  if (*((int *)vd) != C) {
    *((int *)vd) = C;
    return true;
  }
  return false;
}

const wstring &oDataContainer::getDate(const void *data, const char *name) const {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  int C = *((int *)vd);

  wchar_t bf[24];
  if (odi->SubType == oISDateOrYear) {
    if (C > 9999 && C % 10000 != 0)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%04d-%02d-%02d",
               C / 10000, (C / 100) % 100, C % 100);
    else if (C > 9999)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%04d", C / 10000);
    else if (C > 1900)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%04d", C);
    else
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"");
  } else {
    if (C % 10000 != 0 || C == 0)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%04d-%02d-%02d",
               C / 10000, (C / 100) % 100, C % 100);
    else
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%04d", C / 10000);
  }
  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

int oDataContainer::getYear(const void *data, const char *name) const {
  const oDataInfo *odi = findVariable(name);
  if (!odi) throw std::runtime_error("oDataContainer: Variable not found.");
  if (odi->Type != oDTInt) throw std::runtime_error("oDataContainer: Variable of wrong type.");

  LPBYTE vd = LPBYTE(data) + odi->Index;
  int C = *((int *)vd);

  if (odi->SubType == oISDateOrYear) {
    if (C > 9999) return C / 10000;
    if (C > 1900) return C;
    return 0;
  }
  return C / 10000;
}

// ── XML serialization ─────────────────────────────────────────────────────────

bool oDataContainer::write(const oBase *ob, xmlparser &xml) const {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  xml.startTag("oData");

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTInt) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      if (di.SubType == oISTime || di.SubType == oISTimeAdjust) {
        int nr; memcpy(&nr, vd, sizeof(int));
        xml.writeTime(di.Name, nr);
      } else if (di.SubType == oISDateOrYear) {
        int nr; memcpy(&nr, vd, sizeof(int));
        if (nr < 9999)
          xml.write(di.Name, nr);
        else {
          char date[20];
          snprintf(date, sizeof(date), "%d-%02d-%02d", nr / 10000, (nr / 100) % 100, nr % 100);
          xml.write(di.Name, date);
        }
      } else if (di.SubType != oIS64) {
        int nr; memcpy(&nr, vd, sizeof(int));
        xml.write(di.Name, nr);
      } else {
        __int64 nr; memcpy(&nr, vd, sizeof(__int64));
        xml.write64(di.Name, nr);
      }
    } else if (di.Type == oDTDouble) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      char bf[64];
      double num = *(double *)vd;
      if (!xml.skipDefault() || !(num == 0.0)) {
        formatDouble(num, bf, true);
        xml.write(di.Name, bf);
      }
    } else if (di.Type == oDTString) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      wstring out = (wchar_t *)vd;
      xml.write(di.Name, out);
    } else if (di.Type == oDTStringDynamic) {
      const wstring &str = (*strptr)[0][di.Index];
      xml.write(di.Name, str);
    }
  }

  xml.endTag();
  return true;
}

void oDataContainer::set(oBase *ob, const xmlobject &xo) {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  xmlList xl;
  xo.getObjects(xl);

  for (xmlList::const_iterator it = xl.begin(); it != xl.end(); ++it) {
    oDataInfo *odi = findVariable(it->getName());
    if (odi) {
      if (odi->Type == oDTInt) {
        LPBYTE vd = LPBYTE(data) + odi->Index;
        if (odi->SubType == oISTime || odi->SubType == oISTimeAdjust)
          *((int *)vd) = it->getRelativeTime();
        else if (odi->SubType == oISDateOrYear) {
          int val = convertDateYMD(it->getStr(), true);
          if (val > 0) *((int *)vd) = val;
          else         *((int *)vd) = it->getInt();
        } else if (odi->SubType != oIS64)
          *((int *)vd) = it->getInt();
        else
          *((__int64 *)vd) = it->getInt64();
      } else if (odi->Type == oDTDouble) {
        LPBYTE vd = LPBYTE(data) + odi->Index;
        if (strchr(it->getRawPtr(), '.'))
          *((double *)vd) = it->getDouble();
        else {
          int fixed = it->getInt();
          *((double *)vd) = double(fixed) / double(odi->decimalScale);
        }
      } else if (odi->Type == oDTString) {
        LPBYTE vd = LPBYTE(data) + odi->Index;
        const wchar_t *ptr = it->getWPtr();
        if (ptr)
          wcsncpy_s((wchar_t *)vd, odi->Size / sizeof(wchar_t),
                    ptr, (odi->Size - 1) / sizeof(wchar_t));
      } else if (odi->Type == oDTStringDynamic) {
        wstring &str = (*strptr)[0][odi->Index];
        str = it->getWStr();
      }
    }
  }
  allDataStored(ob);
}

void oDataContainer::buildTableCol(Table * /*table*/) {}

int oDataContainer::fillTableCol(const oBase & /*owner*/, Table & /*table*/,
                                  bool /*canEdit*/) const {
  return 0;
}

pair<int, bool> oDataContainer::inputData(oBase * /*ob*/, int /*id*/,
                                           const wstring & /*input*/, int /*inputId*/,
                                           wstring & /*output*/, bool /*noUpdate*/) {
  return {0, false};
}

void oDataContainer::fillInput(const oBase * /*ob*/, int /*id*/, const char * /*name*/,
                                vector<pair<wstring, size_t>> & /*out*/,
                                size_t & /*selected*/) const {
  throw meosException("Invalid enum");
}

bool oDataContainer::setEnum(oBase *ob, const char *name, int selectedIndex) {
  const oDataInfo *info = findVariable(name);
  if (info && info->Type == oDTString && info->SubType == oSSEnum) {
    if (size_t(selectedIndex - 1) < info->enumDescription.size())
      return setString(ob, name, info->enumDescription[selectedIndex - 1].first);
  }
  throw meosException("Invalid enum");
}

// ── Measure ───────────────────────────────────────────────────────────────────

int oDataContainer::getDataAmountMeasure(const void *data) const {
  int amount = 0;
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTInt) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      int nr; memcpy(&nr, vd, sizeof(int));
      if (nr != 0) amount++;
    }
    if (di.Type == oDTDouble) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      double nr; memcpy(&nr, vd, sizeof(double));
      if (nr != 0.0) amount++;
    } else if (di.Type == oDTString) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      amount += (int)strlen((char *)vd);
    }
  }
  return amount;
}

// ── SQL helpers ───────────────────────────────────────────────────────────────

string oDataContainer::C_INT64(const string &name)    { return " `"+name+"` BIGINT NOT NULL DEFAULT 0, "; }
string oDataContainer::C_INT(const string &name)      { return " `"+name+"` INT NOT NULL DEFAULT 0, "; }
string oDataContainer::C_DOUBLE(const string &name)   { return " `"+name+"` DOUBLE NOT NULL DEFAULT 0.0, "; }
string oDataContainer::C_SMALLINT(const string &name) { return " `"+name+"` SMALLINT NOT NULL DEFAULT 0, "; }
string oDataContainer::C_TINYINT(const string &name)  { return " `"+name+"` TINYINT NOT NULL DEFAULT 0, "; }
string oDataContainer::C_SMALLINTU(const string &name){ return " `"+name+"` SMALLINT UNSIGNED NOT NULL DEFAULT 0, "; }
string oDataContainer::C_TINYINTU(const string &name) { return " `"+name+"` TINYINT UNSIGNED NOT NULL DEFAULT 0, "; }
string oDataContainer::C_STRING(const string &name, int len) {
  if (len > 0) {
    char bf[16]; snprintf(bf, sizeof(bf), "%d", len);
    return " `"+name+"` VARCHAR("+bf+") NOT NULL DEFAULT '', ";
  }
  return " `"+name+"` MEDIUMTEXT NOT NULL, ";
}

string oDataContainer::SQL_quote(const wchar_t *in) {
  // Replace WideCharToMultiByte with toUTF8()
  wstring ws(in);
  string utf = toUTF8(ws);
  string out;
  out.reserve(utf.size() * 2);
  for (char c : utf) {
    if (c == '\'') out += '\'';
    if (c == '\\') out += '\\';
    out += c;
  }
  return out;
}

string oDataContainer::generateSQLDefinition(const std::set<string> &exclude) const {
  string sql;
  bool addSyntx = !exclude.empty();

  for (size_t k = 0; k < ordered.size(); k++) {
    if (exclude.count(ordered[k].Name) == 0) {
      if (addSyntx) sql += "ADD COLUMN ";
      const oDataInfo &di = ordered[k];
      string name = di.Name;
      if (di.Type == oDTInt) {
        if (di.SubType == oIS32 || di.SubType == oISDate || di.SubType == oISCurrency ||
            di.SubType == oISTime || di.SubType == oISTimeAdjust ||
            di.SubType == oISDecimal || di.SubType == oISDateOrYear)
          sql += C_INT(name);
        else if (di.SubType == oIS16)  sql += C_SMALLINT(name);
        else if (di.SubType == oIS8)   sql += C_TINYINT(name);
        else if (di.SubType == oIS64)  sql += C_INT64(name);
        else if (di.SubType == oIS16U) sql += C_SMALLINTU(name);
        else if (di.SubType == oIS8U)  sql += C_TINYINTU(name);
      } else if (di.Type == oDTDouble)
        sql += C_DOUBLE(name);
      else if (di.Type == oDTString)
        sql += C_STRING(name, di.Size - 1);
      else if (di.Type == oDTStringDynamic || di.Type == oDTStringArray)
        sql += C_STRING(name, -1);
    }
  }

  if (addSyntx && !sql.empty())
    return sql.substr(0, sql.length() - 2); // strip trailing ", "
  return sql;
}

bool oDataContainer::isModified(const oDataInfo &di, const void *data, const void *oldData,
                                 vector<vector<wstring>> *strptr) const {
  if (di.Type == oDTInt) {
    LPBYTE vd = LPBYTE(data) + di.Index;
    LPBYTE vdOld = LPBYTE(oldData) + di.Index;
    return memcmp(vd, vdOld, (di.SubType != oIS64) ? 4 : 8) != 0;
  } else if (di.Type == oDTDouble) {
    LPBYTE vd = LPBYTE(data) + di.Index;
    LPBYTE vdOld = LPBYTE(oldData) + di.Index;
    return memcmp(vd, vdOld, 8) != 0;
  } else if (di.Type == oDTString) {
    LPBYTE vdB = LPBYTE(data) + di.Index;
    LPBYTE vdOldB = LPBYTE(oldData) + di.Index;
    return wcscmp((wchar_t *)vdB, (wchar_t *)vdOldB) != 0;
  } else if (di.Type == oDTStringDynamic) {
    return (*strptr)[0][di.Index] != (*strptr)[1][di.Index];
  } else if (di.Type == oDTStringArray) {
    return (*strptr)[di.Index] != (*strptr)[di.Index + 1];
  }
  return true;
}

void oDataContainer::allDataStored(const oBase *ob) {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  memcpy(oldData, data, ob->getDISize());
  if (stringIndexPointer > 0 || stringArrayIndexPointer > 2) {
    for (size_t k = 0; k < stringArrayIndexPointer; k += 2)
      (*strptr)[k + 1] = (*strptr)[k];
  }
}

namespace {
  char *ensureCapacity(int size, vector<char> &bfData, int &alloc) {
    assert(alloc > 100);
    if (size >= alloc) {
      alloc += size;
      bfData.resize(alloc);
    }
    return &bfData[0];
  }
}

string oDataContainer::generateSQLSet(const oBase *ob, bool forceSetAll) const {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);

  string sql;
  int alloc = 256;
  vector<char> bfData(alloc);
  char *bf = &bfData[0];

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (!forceSetAll && !isModified(di, data, oldData, strptr)) continue;

    if (di.Type == oDTInt) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      if (di.SubType == oIS8U)
        snprintf(bf, alloc, ", `%s`=%u", di.Name, (*((int *)vd)) & 0xFF);
      else if (di.SubType == oIS16U)
        snprintf(bf, alloc, ", `%s`=%u", di.Name, (*((int *)vd)) & 0xFFFF);
      else if (di.SubType == oIS8) {
        char r = (*((int *)vd)) & 0xFF;
        snprintf(bf, alloc, ", `%s`=%d", di.Name, (int)r);
      } else if (di.SubType == oIS16) {
        short r = (*((int *)vd)) & 0xFFFF;
        snprintf(bf, alloc, ", `%s`=%d", di.Name, (int)r);
      } else if (di.SubType != oIS64)
        snprintf(bf, alloc, ", `%s`=%d", di.Name, *((int *)vd));
      else {
        char tmp[32];
        _i64toa_s(*((__int64 *)vd), tmp, 32, 10);
        snprintf(bf, alloc, ", `%s`=%s", di.Name, tmp);
      }
      sql += bf;
    } else if (di.Type == oDTDouble) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      char tmp[64]; formatDouble(*(double *)vd, tmp, false);
      snprintf(bf, alloc, ", `%s`=%s", di.Name, tmp);
      sql += bf;
    } else if (di.Type == oDTString) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      snprintf(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote((wchar_t *)vd).c_str());
      sql += bf;
    } else if (di.Type == oDTStringDynamic) {
      const wstring &str = (*strptr)[0][di.Index];
      bf = ensureCapacity((int)(2 * str.length() + 30), bfData, alloc);
      snprintf(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote(str.c_str()).c_str());
      sql += bf;
    } else if (di.Type == oDTStringArray) {
      const wstring str2 = encodeArray((*strptr)[di.Index]);
      bf = ensureCapacity((int)(2 * str2.length() + 30), bfData, alloc);
      snprintf(bf, alloc, ", `%s`='%s'", di.Name, SQL_quote(str2.c_str()).c_str());
      sql += bf;
    }
  }
  return sql;
}

bool oDataContainer::merge(oBase &destination, const oBase &source, const oBase *base) const {
  bool modified = false;
  void *destdata, *oldDataDmy;
  vector<vector<wstring>> *deststrptr;
  destination.getDataBuffers(destdata, oldDataDmy, deststrptr);

  void *srcdata;
  vector<vector<wstring>> *srcstrptr;
  source.getDataBuffers(srcdata, oldDataDmy, srcstrptr);

  void *basedata = nullptr;
  vector<vector<wstring>> *basestrptr = nullptr;
  if (base) base->getDataBuffers(basedata, oldDataDmy, basestrptr);

  auto setData = [](void *d, void *s, void *b, int off, int size) {
    LPBYTE vd = LPBYTE(d) + off;
    LPBYTE vs = LPBYTE(s) + off;
    if (memcmp(vd, vs, size) != 0) {
      if (b == nullptr || memcmp(LPBYTE(b) + off, vs, size) != 0) {
        memcpy(vd, vs, size);
        return true;
      }
    }
    return false;
  };

  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTInt) {
      if (di.SubType != oIS64) { if (setData(destdata, srcdata, basedata, di.Index, sizeof(int))) modified = true; }
      else                     { if (setData(destdata, srcdata, basedata, di.Index, sizeof(int64_t))) modified = true; }
    } else if (di.Type == oDTDouble) {
      if (setData(destdata, srcdata, basedata, di.Index, sizeof(double))) modified = true;
    } else if (di.Type == oDTString) {
      if (setData(destdata, srcdata, basedata, di.Index, di.Size)) modified = true;
    } else if (di.Type == oDTStringDynamic) {
      const wstring &s = (*srcstrptr)[0][di.Index];
      wstring &d = (*deststrptr)[0][di.Index];
      if (s != d) {
        if (basestrptr == nullptr || (*basestrptr)[0][di.Index] != s) {
          d = s; modified = true;
        }
      }
    } else if (di.Type == oDTStringArray) {
      const auto &s = (*srcstrptr)[di.Index];
      auto &d = (*deststrptr)[di.Index];
      if (s != d) {
        if (basestrptr == nullptr || (*basestrptr)[di.Index] != s) {
          d = s; modified = true;
        }
      }
    }
  }
  return modified;
}

void oDataContainer::getVariableInt(const void *data, list<oVariableInt> &var) const {
  var.clear();
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTInt) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      var.emplace_back();
      oVariableInt &vi = var.back();
      memcpy(vi.name, di.Name, sizeof(vi.name));
      if (di.SubType != oIS64)
        vi.data32 = (int *)vd;
      else
        vi.data64 = (__int64 *)vd;
    }
  }
}

void oDataContainer::getVariableDouble(const void *data, list<oVariableDouble> &var) const {
  var.clear();
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTDouble) {
      LPBYTE vd = LPBYTE(data) + di.Index;
      var.emplace_back();
      auto &v = var.back();
      memcpy(v.name, di.Name, sizeof(v.name));
      v.data = (double *)vd;
    }
  }
}

void oDataContainer::getVariableString(const oBase *ob, list<oVariableString> &var) const {
  void *data, *oldData;
  vector<vector<wstring>> *strptr;
  ob->getDataBuffers(data, oldData, strptr);
  var.clear();
  for (size_t kk = 0; kk < ordered.size(); kk++) {
    const oDataInfo &di = ordered[kk];
    if (di.Type == oDTString) {
      LPBYTE vd = (LPBYTE)(data) + di.Index;
      var.emplace_back((wchar_t *)vd, di.Size);
      memcpy(var.back().name, di.Name, sizeof(var.back().name));
    } else if (di.Type == oDTStringDynamic) {
      var.emplace_back((*strptr)[0], di.Index);
      memcpy(var.back().name, di.Name, sizeof(var.back().name));
    } else if (di.Type == oDTStringArray) {
      var.emplace_back((*strptr)[di.Index]);
      memcpy(var.back().name, di.Name, sizeof(var.back().name));
    }
  }
}

// ── Interface factories ───────────────────────────────────────────────────────

oDataInterface oDataContainer::getInterface(void *data, int datasize, oBase *ob) {
  if (datasize < dataPointer) throw std::runtime_error("Out Of Bounds.");
  return oDataInterface(this, data, ob);
}

oDataConstInterface oDataContainer::getConstInterface(const void *data, int datasize,
                                                       const oBase *ob) const {
  if (datasize < dataPointer) throw std::runtime_error("Out Of Bounds.");
  return oDataConstInterface(this, data, ob);
}

oDataInterface::oDataInterface(oDataContainer *odc, void *data, oBase *ob)
    : oDC(odc), Data(data), oB(ob) {}

oDataInterface::~oDataInterface() {}

oDataConstInterface::oDataConstInterface(const oDataContainer *odc, const void *data,
                                          const oBase *ob)
    : oDC(odc), Data(data), oB(ob) {}

oDataConstInterface::~oDataConstInterface() {}

// ── formatDouble ──────────────────────────────────────────────────────────────

namespace {
  template<typename CH>
  void eraseTrail(CH *bf, bool keepDecimalPoint, int last) {
    int expPart = -1;
    for (int i = 0; i < last; i++) {
      if (bf[i] == 'e') { expPart = i; last = i; break; }
    }

    if (last > 12 && bf[last-1] != '.' && bf[last-1] != '0' && bf[last-1] != '9' &&
        (bf[last-2] == '0' || bf[last-2] == '9'))
      bf[last-1] = bf[last-2];

    bool trailed = false;
    if (last > 1) {
      if (bf[last-1] == '0') {
        while (last > 1) {
          if (bf[last-1] == '0') { bf[--last] = 0; trailed = true; }
          else if (!keepDecimalPoint && bf[last-1] == '.') { bf[--last] = 0; break; }
          else break;
        }
      } else if (bf[last-1] == '9') {
        CH fill = 0;
        int endP = -1;
        bool didRound = false;
        while (last > 0) {
          if (bf[last-1] == '9') { bf[--last] = fill; trailed = true; }
          else if (bf[last-1] == '.') {
            last--;
            if (!keepDecimalPoint) bf[last] = 0;
            endP = last;
            fill = '0';
          } else if (bf[last-1] == '-') { last = 1; break; }
          else {
            assert(bf[last-1] >= '0' && bf[last-1] <= '8');
            ++bf[last-1]; didRound = true; break;
          }
        }
        if (last <= 1 && !didRound) {
          assert(endP != -1);
          for (int i = 0; i < endP; i++) bf[endP+1-i] = bf[endP-i];
          endP++;
          bf[last] = '1';
        }
        if (endP != -1) last = endP;
      }
    }

    if (expPart != -1 && trailed) {
      while (bf[expPart]) bf[last++] = bf[expPart++];
      bf[last++] = 0;
    }
  }
}

void oDataContainer::formatDouble(double nr, wchar_t bf[64], bool keepDecimalPoint) {
  int last;
  if (nr > 1e7 || nr < -1e7) last = swprintf(bf, 64, L"%#.16e", nr);
  else                        last = swprintf(bf, 64, L"%#.16g", nr);
  eraseTrail(bf, keepDecimalPoint, last);
}

void oDataContainer::formatDouble(double nr, char bf[64], bool keepDecimalPoint) {
  int last;
  if (nr > 1e7 || nr < -1e7) last = snprintf(bf, 64, "%#.16e", nr);
  else                        last = snprintf(bf, 64, "%#.16g", nr);
  eraseTrail(bf, keepDecimalPoint, last);
}

string oDataContainer::formatDouble(double d) {
  char bf[64];
  formatDouble(d, bf, false);
  return bf;
}

// ── formatNumber ──────────────────────────────────────────────────────────────

bool oDataContainer::formatNumber(int nr, const oDataInfo &di, wchar_t bf[64]) const {
  if (di.SubType == oISDate) {
    if (nr == 0) bf[0] = 0;
    else if ((nr < 99999999 && nr % 10000 != 0) || nr == 0)
      swprintf(bf, 64, L"%d-%02d-%02d", nr/(100*100), (nr/100)%100, nr%100);
    else if (nr > 0 && nr < 9999)
      swprintf(bf, 64, L"%04d", nr / 10000);
    else { bf[0] = '-'; bf[1] = 0; }
    return true;
  } else if (di.SubType == oISDateOrYear) {
    if (nr == 0) bf[0] = 0;
    else if (nr > 9999 && nr % 10000 != 0)
      swprintf(bf, 64, L"%04d-%02d-%02d", nr/10000, (nr/100)%100, nr%100);
    else if (nr > 9999) swprintf(bf, 64, L"%04d", nr / 10000);
    else if (nr > 1900) swprintf(bf, 64, L"%04d", nr);
    else { bf[0] = '-'; bf[1] = 0; }
    return true;
  } else if (di.SubType == oISTime) {
    if (nr > 0 && nr < (30*24*timeConstHour)) {
      int cnt;
      if (nr < 24*timeConstHour)
        cnt = swprintf(bf, 64, L"%02d:%02d:%02d",
                       nr/timeConstHour, (nr/timeConstMinute)%60, (nr/timeConstSecond)%60);
      else {
        int days = nr / (24*timeConstHour); nr = nr % (24*timeConstHour);
        cnt = swprintf(bf, 64, L"%d+%02d:%02d:%02d",
                       days, nr/timeConstHour, (nr/timeConstSecond)%60, (nr/timeConstSecond)%60);
      }
      if (timeConstSecond > 1 && nr % 10 != 0)
        swprintf(bf+cnt, 64-cnt, L".%01d", nr % 10);
    } else { bf[0] = '-'; bf[1] = 0; }
    return true;
  } else if (di.SubType == oISTimeAdjust) {
    if (nr != 0 && nr != NOTIME) {
      int cnt = 0;
      if (nr < 0) { bf[0] = '-'; cnt = 1; nr = -nr; }
      if (nr < 24*timeConstHour)
        cnt += swprintf(bf+cnt, 64-cnt, L"%02d:%02d:%02d",
                        nr/timeConstHour, (nr/timeConstMinute)%60, (nr/timeConstSecond)%60);
      else {
        int days = nr / (24*timeConstHour); nr = nr % (24*timeConstHour);
        cnt += swprintf(bf+cnt, 64-cnt, L"%d+%02d:%02d:%02d",
                        days, nr/timeConstHour, (nr/timeConstSecond)%60, (nr/timeConstSecond)%60);
      }
      if (timeConstSecond > 1 && nr % 10 != 0)
        swprintf(bf+cnt, 64-cnt, L".%01d", nr % 10);
    } else { bf[0] = '-'; bf[1] = 0; }
    return true;
  } else if (di.SubType == oISDecimal) {
    if (nr) {
      int whole = nr / di.decimalScale;
      int part  = nr - whole * di.decimalScale;
      wstring ptrn = L"%d," + itow(di.decimalSize) + L"d";
      swprintf(bf, 64, (L"%d,%0" + itow(di.decimalSize) + L"d").c_str(), whole, abs(part));
    } else
      bf[0] = 0;
    return true;
  } else {
    if (nr) swprintf(bf, 64, L"%d", nr);
    else    bf[0] = 0;
    return true;
  }
}

// ── oVariableString::store ────────────────────────────────────────────────────

bool oVariableString::store(const wchar_t *in) {
  if (data) {
    if (wcscmp(in, data) != 0) {
      wcsncpy_s(data, maxSize / sizeof(wchar_t), in, (maxSize - 1) / sizeof(wchar_t));
      return true;
    }
    return false;
  } else {
    vector<wstring> &str = *strData;
    if (strIndex >= 0) {
      if (str[strIndex] != in) { str[strIndex] = in; return true; }
      return false;
    }
  }
  return false;
}

// ── Array encode/decode (stubs — used by MySQL serialization) ─────────────────

wstring oDataContainer::encodeArray(const vector<wstring> & /*input*/) { return L""; }
void oDataContainer::decodeArray(const string & /*input*/, vector<wstring> & /*output*/) {}
