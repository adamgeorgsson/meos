/************************************************************************
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
#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

class xmlobject;
using xmlList = std::vector<xmlobject>;
class xmlparser;

const int buff_pre_alloc = 1024 * 10;

struct xmldata {
  xmldata(const char *t, char *d);
  const char *tag;
  char       *data;
  int         parent;
  int         next;
};

struct xmlattrib {
  xmlattrib(const char *t, char *d, const xmlparser *p);
  const char *tag;
  char       *data;

  operator bool() const { return data != nullptr; }

  int     getInt()    const { return data ? atoi(data) : 0; }
  int64_t getInt64()  const { return data ? strtoll(data, nullptr, 10)  : 0; }
  uint64_t getInt64u()const { return data ? strtoull(data, nullptr, 10) : 0; }
  double  getDouble() const { return data ? strtod(data, nullptr) : 0.0; }

  const char    *getPtr()  const;
  const wchar_t *getWPtr() const;

  std::string  getStr()  const { const char    *p = getPtr();  return p ? p : ""; }
  std::wstring getWStr() const { const wchar_t *p = getWPtr(); return p ? p : L""; }

private:
  const xmlparser *parser;
};

class xmlparser {
protected:
  static char       *ltrim(char *s);
  static const char *ltrim(const char *s);

  std::string tagStack[32];
  int         tagStackPointer = 0;

  bool               toString = false;
  std::ofstream      foutFile;
  std::ostringstream foutString;
  std::ifstream      fin;

  std::ostream &fOut() {
    if (toString) return foutString;
    return foutFile;
  }

  int         lineNumber = 0;
  std::string doctype;

  std::vector<int>     parseStack;
  std::vector<xmldata> xmlinfo;
  std::vector<char>    xbf;

  bool processTag(char *start, char *end);

  // Returns true if the content is UTF-8 encoded.
  // hasDeclaration is set to true if an XML/PI declaration was found and consumed.
  bool checkUTF(const char *ptr, bool &hasDeclaration) const;
  bool parse(int maxobj);

  void convertString(const char *in, char    *out, int maxlen) const;
  void convertString(const char *in, wchar_t *out, int maxlen) const;

  bool cutMode = false;
  bool isUTF   = false;

  std::vector<char>    strbuff;
  std::vector<wchar_t> strbuffw;

  int lastIndex = 0;

  mutable std::wstring encodeString;

public:
  void access(int index);

  const xmlobject getObject(const char *pname) const;

  void read(const std::wstring &file, int maxobj = 0);
  void readMemory(const std::string &mem, int maxobj);

  void write(const char *tag, const char *prop, const std::string  &value);
  void write(const char *tag, const char *prop, const std::wstring &value);
  void write(const char *tag);
  void write(const char *tag, const char *prop, const wchar_t *value);
  void writeBool(const char *tag, const char *prop, const bool value);

  void writeAscii(const char *tag,
                  const std::vector<std::pair<std::string, std::wstring>> &propValue,
                  const std::string &valueAscii);

  void write(const char *tag, const char *prop,
             const std::wstring &propValue, const std::wstring &value);
  void writeBool(const char *tag, const char *prop,
                 bool propValue, const std::wstring &value);
  void write(const char *tag, const char *prop,
             const wchar_t *propValue, const std::wstring &value);
  void write(const char *tag,
             const std::vector<std::pair<std::string, std::wstring>> &propValue,
             const std::wstring &value);

  void write(const char *tag, const std::string  &value);
  void write(const char *tag, const char         *value);
  void write(const char *tag, const std::wstring &value);
  void write(const char *tag, double value);
  void write(const char *tag, int    value);
  void write(const char *tag, size_t value) { write(tag, int(value)); }

  void writeTime(const char *tag, int relativeTime);

  void writeBool(const char *tag, bool value);
  void write64(const char *tag, int64_t  value);
  void write64u(const char *tag, uint64_t value);

  void startTag(const char *tag);
  void startTag(const char *tag, const char *Property, const std::string  &Value);
  void startTag(const char *tag, const char *Property, const std::wstring &Value);
  void startTag(const char *tag, const std::vector<std::wstring> &propvalue);

  void endTag();
  int  closeOut();

  void openOutput(const wchar_t *file, bool useCutMode);
  void openOutputT(const wchar_t *file, bool useCutMode, const std::string &type);
  void openMemoryOutput(bool useCutMode);
  void getMemoryOutput(std::string &res);

  const std::string &encodeXML(const std::string  &input);
  const std::string &encodeXML(const std::wstring &input);

  xmlparser();
  virtual ~xmlparser();

  bool skipDefault() const { return cutMode; }

  friend class  xmlobject;
  friend struct xmlattrib;
};

class xmlobject {
protected:
  xmlobject(xmlparser *p)        { parser = p; index = 0; }
  xmlobject(xmlparser *p, int i) { parser = p; index = i; }

  xmlparser *parser;
  int        index;

public:
  const char *getName() const { return parser->xmlinfo[index].tag; }

  xmlobject getObject(const char *pname) const;
  xmlattrib getAttrib(const char *pname) const;

  int getObjectInt(const char *pname) const {
    xmlobject x(getObject(pname));
    if (x) return x.getInt();
    xmlattrib xa(getAttrib(pname));
    if (xa)  return xa.getInt();
    return 0;
  }

  int64_t  getObjectInt64(const char  *pname) const;
  uint64_t getObjectInt64u(const char *pname) const;
  double   getObjectDouble(const char *pname) const;

  bool got(const char *pname) const {
    if (getObject(pname)) return true;
    return (bool)getAttrib(pname);
  }

  bool getObjectBool(const char *pname) const;

  std::string  &getObjectString(const char *pname, std::string  &out) const;
  char         *getObjectString(const char *pname, char  *out, int maxlen) const;
  std::wstring &getObjectString(const char *pname, std::wstring &out) const;
  wchar_t      *getObjectString(const char *pname, wchar_t *out, int maxlen) const;

  void getObjects(xmlList &objects) const;
  void getObjects(const char *tag, xmlList &objects) const;

  bool is(const char *pname) const {
    const char *n = getName();
    return n[0] == pname[0] && strcmp(n, pname) == 0;
  }

  const char *getRawPtr() const { return parser->xmlinfo[index].data; }
  std::string getRawStr() const { const char *p = getRawPtr(); return p ? p : ""; }

  const char    *getPtr()  const;
  const wchar_t *getWPtr() const;

  std::string  getStr()  const { const char    *p = getPtr();  return p ? p : ""; }
  std::wstring getWStr() const { const wchar_t *p = getWPtr(); return p ? p : L""; }

  int      getInt()    const { const char *d = parser->xmlinfo[index].data; return d ? atoi(d) : 0; }
  int64_t  getInt64()  const { const char *d = parser->xmlinfo[index].data; return d ? strtoll(d,  nullptr, 10) : 0; }
  uint64_t getInt64u() const { const char *d = parser->xmlinfo[index].data; return d ? strtoull(d, nullptr, 10) : 0; }
  double   getDouble() const { const char *d = parser->xmlinfo[index].data; return d ? strtod(d, nullptr) : 0.0; }

  int getRelativeTime() const;

  bool isnull()       const { return parser == nullptr; }
  operator bool()     const { return parser != nullptr; }

  xmlobject();
  virtual ~xmlobject();

  friend class xmlparser;
};
