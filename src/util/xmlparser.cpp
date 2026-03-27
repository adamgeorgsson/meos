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
    Eksoppsvägen 16, SE-75646 Uppsala, Sweden

************************************************************************/

// Cross-platform implementation of xmlparser.h
// All Win32 APIs (MultiByteToWideChar, HWND, gdioutput) replaced with
// standard C++17 and meos_util.h helpers.

#include "xmlparser.h"
#include "meos_util.h"
#include "meosexception.h"
#include "timeconstants.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
  #define meos_strncasecmp _strnicmp
  #define meos_strcasecmp  _stricmp
#else
  #include <strings.h>
  #define meos_strncasecmp strncasecmp
  #define meos_strcasecmp  strcasecmp
#endif

using std::string;
using std::wstring;
using std::vector;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline bool isBlankSpace(char b) {
  return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

// Format a double without excessive trailing zeros.
static string formatDouble(double v) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%.6g", v);
  return buf;
}

// In-place XML entity decode on a mutable char buffer.
static void inplaceDecodeXML(char *in) {
  char *bf = in;
  int outp = 0;
  for (int k = 0; bf[k]; k++) {
    if (bf[k] != '&') {
      bf[outp++] = bf[k];
    } else {
      if      (memcmp(&bf[k], "&amp;",  5) == 0) { bf[outp++] = '&';  k += 4; }
      else if (memcmp(&bf[k], "&lt;",   4) == 0) { bf[outp++] = '<';  k += 3; }
      else if (memcmp(&bf[k], "&gt;",   4) == 0) { bf[outp++] = '>';  k += 3; }
      else if (memcmp(&bf[k], "&quot;", 6) == 0) { bf[outp++] = '"';  k += 5; }
      else if (memcmp(&bf[k], "&#10;",  5) == 0) { bf[outp++] = '\n'; k += 4; }
      else if (memcmp(&bf[k], "&#13;",  5) == 0) { bf[outp++] = '\r'; k += 4; }
      else if (memcmp(&bf[k], "&apos;", 6) == 0) { bf[outp++] = '\''; k += 5; }
      else                                        { bf[outp++] = bf[k]; }
    }
  }
  bf[outp] = 0;
}

// Decode a UTF-8 char* into a static wchar_t buffer of size buff_pre_alloc.
// Returns pointer to the buffer, or nullptr if in == nullptr.
static const wchar_t *utf8CharToWBuf(const char *in, bool isUTF,
                                     wchar_t *buf, int bufSize) {
  if (!in) return nullptr;
  if (isUTF) {
    const wstring &ws = fromUTF8(string(in));
    int n = (int)std::min(ws.size(), (size_t)(bufSize - 1));
    wmemcpy(buf, ws.c_str(), n);
    buf[n] = 0;
  } else {
    // CP-1252 decode via widen()
    const wstring &ws = widen(string(in));
    int n = (int)std::min(ws.size(), (size_t)(bufSize - 1));
    wmemcpy(buf, ws.c_str(), n);
    buf[n] = 0;
  }
  return buf;
}

// ---------------------------------------------------------------------------
// xmlparser construction / destruction
// ---------------------------------------------------------------------------

xmlparser::xmlparser() {
  lastIndex = 0;
  tagStackPointer = 0;
  isUTF = false;
  cutMode = false;
  toString = false;
  strbuff.resize(buff_pre_alloc);
  strbuffw.resize(buff_pre_alloc);
}

xmlparser::~xmlparser() {
  fin.close();
  foutFile.close();
}

// ---------------------------------------------------------------------------
// Progress (stub — removed Win32 ProgressWindow dependency)
// ---------------------------------------------------------------------------

void xmlparser::access(int /*index*/) {
  // Progress tracking removed (no Win32 HWND available).
}

// ---------------------------------------------------------------------------
// encodeXML member wrappers
// ---------------------------------------------------------------------------

const string &xmlparser::encodeXML(const wstring &input) {
  // Convert wstring → UTF-8 then XML-encode.
  return ::encodeXML(toUTF8(input));
}

const string &xmlparser::encodeXML(const string &input) {
  // The string is already UTF-8; just XML-encode.
  return ::encodeXML(input);
}

// ---------------------------------------------------------------------------
// write overloads
// ---------------------------------------------------------------------------

void xmlparser::write(const char *tag, const wstring &Value) {
  if (!cutMode || !Value.empty()) {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">" << valEnc << "</" << tag << ">\n";
    } else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const string &Value) {
  if (!cutMode || Value != "") {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">" << valEnc << "</" << tag << ">\n";
    } else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Value) {
  if (!cutMode || (Value && Value[0] != '\0')) {
    auto &valEnc = encodeXML(string(Value ? Value : ""));
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">" << valEnc << "</" << tag << ">\n";
    } else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag) {
  char bf[128];
  snprintf(bf, sizeof(bf), "<%s/>\n", tag);
  fOut() << bf;
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const string &Value) {
  if (!cutMode || Value != "") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const wstring &Value) {
  if (!cutMode || !Value.empty()) {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *prop, const wchar_t *value) {
  encodeString = value;
  write(tag, prop, encodeString);
}

void xmlparser::writeBool(const char *tag, const char *prop, bool value) {
  if (!cutMode || value)
    write(tag, prop, value ? L"true" : L"false");
}

void xmlparser::writeAscii(const char *tag,
                           const vector<std::pair<string, wstring>> &propValue,
                           const string &valueAscii) {
  if (!cutMode || valueAscii != "") {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++)
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    fOut() << ">" << ::encodeXML(valueAscii) << "</" << tag << ">\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property,
                      const wstring &PropValue, const wstring &Value) {
  if (!cutMode || Value != L"" || PropValue != L"") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(PropValue) << "\">" << encodeXML(Value)
           << "</" << tag << ">\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::writeBool(const char *tag, const char *prop,
                          bool propValue, const wstring &value) {
  static const wstring wTrue  = L"true";
  static const wstring wFalse = L"false";
  write(tag, prop, propValue ? wTrue : wFalse, value);
}

void xmlparser::write(const char *tag, const char *prop,
                      const wchar_t *propValue, const wstring &value) {
  write(tag, prop, wstring(propValue), value);
}

void xmlparser::write(const char *tag,
                      const vector<std::pair<string, wstring>> &propValue,
                      const wstring &value) {
  if (!cutMode || value != L"" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++)
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    if (!value.empty())
      fOut() << ">" << encodeXML(value) << "</" << tag << ">\n";
    else
      fOut() << "/>\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, int Value) {
  if (!cutMode || Value != 0) {
    char bf[256];
    snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, Value, tag);
    fOut() << bf;
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, double value) {
  if (!cutMode || value != 0.0) {
    char bf[256];
    snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, formatDouble(value).c_str(), tag);
    fOut() << bf;
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::writeTime(const char *tag, int relativeTime) {
  if (!cutMode || relativeTime != 0) {
    char bf[256];
    int subSec = timeConstSecond == 1 ? 0 : relativeTime % timeConstSecond;

    if (timeConstSecond == 1 || relativeTime == -1) {
      snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, relativeTime, tag);
    } else if (subSec == 0 && relativeTime != -10) {
      snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, relativeTime / timeConstSecond, tag);
    } else if (relativeTime >= 0) {
      if (timeConstSecond == 10)
        snprintf(bf, sizeof(bf), "<%s>%d.%d</%s>\n",   tag, relativeTime/timeConstSecond, relativeTime%timeConstSecond, tag);
      else
        snprintf(bf, sizeof(bf), "<%s>%d.%02d</%s>\n", tag, relativeTime/timeConstSecond, relativeTime%timeConstSecond, tag);
    } else {
      int at = std::abs(relativeTime);
      if (timeConstSecond == 10)
        snprintf(bf, sizeof(bf), "<%s>-%d.%d</%s>\n",   tag, at/timeConstSecond, at%timeConstSecond, tag);
      else
        snprintf(bf, sizeof(bf), "<%s>-%d.%02d</%s>\n", tag, at/timeConstSecond, at%timeConstSecond, tag);
    }
    fOut() << bf;
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::writeBool(const char *tag, bool value) {
  if (!cutMode || value) {
    char bf[256];
    snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, value ? "true" : "false", tag);
    fOut() << bf;
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write64(const char *tag, int64_t value) {
  if (!cutMode || value != 0) {
    fOut() << "<" << tag << ">" << value << "</" << tag << ">\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

void xmlparser::write64u(const char *tag, uint64_t value) {
  if (!cutMode || value != 0) {
    fOut() << "<" << tag << ">" << value << "</" << tag << ">\n";
  }
  if (!fOut().good()) throw meosException("Writing to XML file failed.");
}

// ---------------------------------------------------------------------------
// startTag / endTag
// ---------------------------------------------------------------------------

void xmlparser::startTag(const char *tag, const char *prop, const wstring &Value) {
  if (tagStackPointer < 32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    } else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">\n";
    }
    tagStack[tagStackPointer++] = tag;
    if (!fOut().good()) throw meosException("Writing to XML file failed.");
  } else {
    throw meosException("Tag depth too large.");
  }
}

void xmlparser::startTag(const char *tag, const char *prop, const string &Value) {
  if (tagStackPointer < 32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    } else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">\n";
    }
    tagStack[tagStackPointer++] = tag;
    if (!fOut().good()) throw meosException("Writing to XML file failed.");
  } else {
    throw meosException("Tag depth too large.");
  }
}

void xmlparser::startTag(const char *tag, const vector<wstring> &propvalue) {
  if (tagStackPointer < 32) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propvalue.size(); k += 2)
      fOut() << " " << encodeXML(propvalue[k]) << "=\"" << encodeXML(propvalue[k+1]) << "\"";
    fOut() << ">\n";
    tagStack[tagStackPointer++] = tag;
    if (!fOut().good()) throw meosException("Writing to XML file failed.");
  } else {
    throw meosException("Tag depth too large.");
  }
}

void xmlparser::startTag(const char *tag) {
  if (tagStackPointer < 32) {
    char bf[128];
    snprintf(bf, sizeof(bf), "<%s>\n", tag);
    fOut() << bf;
    tagStack[tagStackPointer++] = tag;
    if (!fOut().good()) throw meosException("Writing to XML file failed.");
  } else {
    throw meosException("Tag depth too large.");
  }
}

void xmlparser::endTag() {
  if (tagStackPointer > 0) {
    char bf[128];
    const char *tag = tagStack[--tagStackPointer].c_str();
    snprintf(bf, sizeof(bf), "</%s>\n", tag);
    fOut() << bf;
    if (!fOut().good()) throw meosException("Writing to XML file failed.");
  } else {
    throw meosException("BAD XML CODE: endTag called with empty stack.");
  }
}

// ---------------------------------------------------------------------------
// Output open/close/memory
// ---------------------------------------------------------------------------

void xmlparser::openMemoryOutput(bool useCutMode) {
  cutMode = useCutMode;
  toString = true;
  foutString.str(string());
  foutString.clear();
  fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";
}

void xmlparser::getMemoryOutput(string &res) {
  res = foutString.str();
  foutString.str(string());
  foutString.clear();
}

void xmlparser::openOutput(const wchar_t *file, bool useCutMode) {
  openOutputT(file, useCutMode, "");
}

void xmlparser::openOutputT(const wchar_t *file, bool useCutMode, const string &type) {
  toString = false;
  cutMode = useCutMode;
  foutFile.open(std::filesystem::path(wstring(file)));
  checkWriteAccess(wstring(file));
  tagStackPointer = 0;

  if (foutFile.bad())
    throw meosException(L"Kunde inte skriva till 'X'.#" + wstring(file));

  fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";

  if (!type.empty())
    startTag(type.c_str());
}

int xmlparser::closeOut() {
  while (tagStackPointer > 0)
    endTag();
  int len = (int)foutFile.tellp();
  foutFile.close();
  return len;
}

// ---------------------------------------------------------------------------
// xmldata / xmlattrib constructors
// ---------------------------------------------------------------------------

xmldata::xmldata(const char *t, char *d) : tag(t), data(d) {
  parent = -1;
  next   = 0;
}

xmlattrib::xmlattrib(const char *t, char *d, const xmlparser *p)
    : tag(t), data(d), parser(p) {}

// ---------------------------------------------------------------------------
// read / readMemory
// ---------------------------------------------------------------------------

void xmlparser::read(const wstring &file, int maxobj) {
  fin.open(std::filesystem::path(file), std::ios::binary);

  if (!fin.good())
    throw meosException(L"Failed to open 'X' for reading.#" + file);

  char bf[1024];
  bf[0] = 0;
  do {
    fin.getline(bf, 1024, '>');
    lineNumber++;
  } while (fin.good() && bf[0] == 0);

  char *ptr = ltrim(bf);
  bool hasDecl = false;
  isUTF = checkUTF(ptr, hasDecl);

  // p1: byte offset to start of document content (after declaration if present).
  int p1 = hasDecl ? (int)fin.tellg() : 0;
  if (!hasDecl)
    fin.seekg(0);

  fin.seekg(0, std::ios::end);
  int p2 = (int)fin.tellg();
  fin.seekg(p1, std::ios::beg);

  int asize = p2 - p1;
  if (maxobj > 0)
    asize = std::min(asize, maxobj * 256);

  xbf.resize(asize + 1);
  xmlinfo.clear();
  xmlinfo.reserve(xbf.size() / 30);
  parseStack.clear();

  fin.read(&xbf[0], asize);
  xbf[asize] = 0;

  fin.close();

  parse(maxobj);
}

void xmlparser::readMemory(const string &mem, int maxobj) {
  if (mem.empty())
    return;

  char bf[1024];
  bf[0] = mem[0];
  int i = 1;
  int stop = std::min<int>(1020, (int)mem.length());
  while (i < stop && mem[i-1] != '>') {
    bf[i] = mem[i];
    i++;
  }
  bf[i] = 0;

  char *ptr = ltrim(bf);
  bool hasDecl = false;
  isUTF = checkUTF(ptr, hasDecl);

  int p1 = hasDecl ? i : 0;
  int p2 = (int)mem.size();

  int asize = p2 - p1;
  if (maxobj > 0)
    asize = std::min(asize, maxobj * 256);

  xbf.resize(asize + 1);
  xmlinfo.clear();
  xmlinfo.reserve(xbf.size() / 30);
  parseStack.clear();

  memcpy(&xbf[0], mem.c_str() + p1, asize);
  xbf[asize] = 0;

  parse(maxobj);
}

// ---------------------------------------------------------------------------
// checkUTF
// ---------------------------------------------------------------------------

bool xmlparser::checkUTF(const char *ptr, bool &hasDeclaration) const {
  bool utf = false;
  hasDeclaration = false;

  // UTF-8 BOM
  if ((unsigned char)ptr[0] == 0xEF &&
      (unsigned char)ptr[1] == 0xBB &&
      (unsigned char)ptr[2] == 0xBF) {
    utf = true;
    ptr += 3;
  }

  if (ptr[0] == '<' && ptr[1] == '?') {
    hasDeclaration = true;
    if (memcmp(ptr, "<?xml", 5) == 0) {
      bool hasEncode = false;
      int i = 5;
      while (ptr[i]) {
        if ((ptr[i] == 'U' || ptr[i] == 'u') &&
            meos_strncasecmp(ptr + i, "UTF-8", 5) == 0) {
          utf = true;
          break;
        }
        if (ptr[i] == 'e' && memcmp(ptr + i, "encoding", 8) == 0)
          hasEncode = true;
        i++;
      }
      if (!hasEncode)
        utf = true; // No encoding attribute → assume UTF-8
    } else {
      utf = true; // Some other PI → assume UTF-8
    }
  } else if (ptr[0] == '<') {
    // No XML declaration — treat as UTF-8 fragment.
    utf = true;
    hasDeclaration = false;
  } else if (ptr[0] == '\0') {
    utf = true; // Empty content
  } else {
    throw meosException("Invalid XML: content must start with '<'");
  }
  return utf;
}

// ---------------------------------------------------------------------------
// parse / processTag
// ---------------------------------------------------------------------------

bool xmlparser::parse(int maxobj) {
  lineNumber = 0;
  int pp = 0;
  const int size = (int)xbf.size() - 2;

  while (pp < size) {
    while (pp < size && xbf[pp] != '<') pp++;

    if (xbf[pp] == '<') {
      xbf[pp] = 0;
      char *start = &xbf[pp + 1];
      while (pp < size && xbf[pp] != '>') pp++;
      if (xbf[pp] == '>') xbf[pp] = 0;

      if (*start == '!') { pp++; continue; } // Comment/CDATA
      if (*start == '?') { pp++; continue; } // Processing instruction

      processTag(start, &xbf[pp - 1]);
    }

    if (maxobj > 0 && (int)xmlinfo.size() >= maxobj) {
      xbf[pp + 1] = 0;
      return true;
    }
    pp++;
  }

  lastIndex = 0;
  return true;
}

bool xmlparser::processTag(char *start, char *end) {
  static char err[128];
  bool onlyAttrib = (*end == '/');
  bool endTag     = (*start == '/');

  char *tag = start;
  if (endTag) tag++;

  while (start <= end && !isBlankSpace(*start))
    start++;

  *start = 0;

  if (!endTag && !onlyAttrib) {
    parseStack.push_back((int)xmlinfo.size());
    xmlinfo.push_back(xmldata(tag, end + 2));
    int p = (int)parseStack.size() - 2;
    xmlinfo.back().parent = (p >= 0) ? parseStack[p] : -1;
  } else if (endTag) {
    if (!parseStack.empty()) {
      xmldata &xd = xmlinfo[parseStack.back()];
      inplaceDecodeXML(xd.data);
      if (strcmp(tag, xd.tag) == 0) {
        parseStack.pop_back();
        xd.next = (int)xmlinfo.size();
      } else {
        snprintf(err, sizeof(err), "Unmatched tag '%s', expected '%s'.", tag, xd.tag);
        throw meosException(err);
      }
    } else {
      snprintf(err, sizeof(err), "Unmatched tag '%s'.", tag);
      throw meosException(err);
    }
  } else { // onlyAttrib
    *end = 0;
    xmlinfo.push_back(xmldata(tag, nullptr));
    int p = (int)parseStack.size() - 1;
    xmlinfo.back().parent = (p >= 0) ? parseStack[p] : -1;
    xmlinfo.back().next   = (int)xmlinfo.size();
  }
  return true;
}

// ---------------------------------------------------------------------------
// ltrim
// ---------------------------------------------------------------------------

char *xmlparser::ltrim(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}

const char *xmlparser::ltrim(const char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}

// ---------------------------------------------------------------------------
// xmlobject
// ---------------------------------------------------------------------------

xmlobject::xmlobject() {
  parser = nullptr;
  index  = 0;
}

xmlobject::~xmlobject() {}

xmlobject xmlobject::getObject(const char *pname) const {
  if (pname == nullptr) return *this;
  if (isnull()) throw meosException("Null pointer exception in xmlobject::getObject");

  vector<xmldata> &xi = parser->xmlinfo;
  parser->access(index);

  unsigned child = index + 1;
  while (child < xi.size() && xi[child].parent == index) {
    if (strcmp(xi[child].tag, pname) == 0)
      return xmlobject(parser, child);
    else
      child = xi[child].next;
  }
  return xmlobject(nullptr);
}

void xmlobject::getObjects(xmlList &obj) const {
  obj.clear();
  if (isnull()) throw meosException("Null pointer exception in xmlobject::getObjects");

  vector<xmldata> &xi = parser->xmlinfo;
  unsigned child = index + 1;
  parser->access(index);

  while (child < xi.size() && xi[child].parent == index) {
    obj.push_back(xmlobject(parser, child));
    child = xi[child].next;
  }
}

void xmlobject::getObjects(const char *tag, xmlList &obj) const {
  obj.clear();
  if (isnull()) throw meosException("Null pointer exception in xmlobject::getObjects");

  vector<xmldata> &xi = parser->xmlinfo;
  unsigned child = index + 1;
  parser->access(index);

  while (child < xi.size() && xi[child].parent == index) {
    if (strcmp(tag, xi[child].tag) == 0)
      obj.push_back(xmlobject(parser, child));
    child = xi[child].next;
  }
}

const xmlobject xmlparser::getObject(const char *pname) const {
  if (xmlinfo.size() > 0) {
    if (pname == nullptr || strcmp(xmlinfo[0].tag, pname) == 0)
      return xmlobject(const_cast<xmlparser *>(this), 0);
    else
      return xmlobject(const_cast<xmlparser *>(this), 0).getObject(pname);
  }
  return xmlobject(nullptr);
}

xmlattrib xmlobject::getAttrib(const char *pname) const {
  if (pname == nullptr)
    return xmlattrib(nullptr, nullptr, parser);

  char *start = const_cast<char *>(parser->xmlinfo[index].tag);
  const char *end = parser->xmlinfo[index].data;

  if (end)
    end -= 2;
  else {
    if (size_t(index + 1) < parser->xmlinfo.size())
      end = parser->xmlinfo[index + 1].tag - 1;
    else
      end = &parser->xbf.back();
  }

  // Scan past tag name (tag is null-terminated within xbf).
  while (start < end && *start != 0) start++;
  start++;

  char *oldStart = start;
  while (start < end) {
    while (start < end && isBlankSpace(*start)) start++;

    char *attrTag = start;
    while (start < end && *start != '=' && *start != 0) start++;

    if (start < end && (start[1] == '"' || start[1] == 0)) {
      *start = 0;
      ++start;
      char *value = ++start;

      while (start <= end && (*start != '"' && *start != 0))
        start++;

      if (start <= end) {
        *start = 0;
        if (strcmp(pname, attrTag) == 0)
          return xmlattrib(attrTag, value, parser);
        start++;
      }
    }

    if (oldStart == start) break;
    oldStart = start;
  }
  return xmlattrib(nullptr, nullptr, parser);
}

// ---------------------------------------------------------------------------
// getPtr / getWPtr on xmlobject and xmlattrib
// ---------------------------------------------------------------------------

const wchar_t *xmlobject::getWPtr() const {
  const char *ptr = getRawPtr();
  if (!ptr) return nullptr;
  static wchar_t buff[buff_pre_alloc];
  return utf8CharToWBuf(ptr, parser->isUTF, buff, buff_pre_alloc);
}

const char *xmlobject::getPtr() const {
  const char *ptr = getRawPtr();
  if (!ptr) return nullptr;
  if (!parser->isUTF) return ptr;

  // UTF-8 → wstring → narrow (Latin-1 approximation)
  static char buff[buff_pre_alloc];
  const wstring &ws = fromUTF8(string(ptr));
  size_t n = std::min(ws.size(), (size_t)(buff_pre_alloc - 1));
  for (size_t k = 0; k < n; k++)
    buff[k] = (char)(ws[k] & 0xFF);
  buff[n] = 0;
  return buff;
}

int xmlobject::getRelativeTime() const {
  const char *d = parser->xmlinfo[index].data;
  return parseRelativeTime(d);
}

const char *xmlattrib::getPtr() const {
  if (data)
    return decodeXML(data);
  return nullptr;
}

const wchar_t *xmlattrib::getWPtr() const {
  if (!data) return nullptr;
  const char *dec = decodeXML(data);
  static wchar_t xbf[buff_pre_alloc];
  return utf8CharToWBuf(dec, parser->isUTF, xbf, buff_pre_alloc);
}

// ---------------------------------------------------------------------------
// convertString
// ---------------------------------------------------------------------------

void xmlparser::convertString(const char *in, char *out, int maxlen) const {
  if (!in) throw meosException("Null pointer exception in convertString");
  if (!isUTF) {
    strncpy(out, in, maxlen - 1);
    out[maxlen - 1] = 0;
    return;
  }
  // UTF-8 → wstring → narrow (Latin-1 approximation)
  const wstring &ws = fromUTF8(string(in));
  int n = (int)std::min(ws.size(), (size_t)(maxlen - 1));
  for (int k = 0; k < n; k++)
    out[k] = (char)(ws[k] & 0xFF);
  out[n] = 0;
}

void xmlparser::convertString(const char *in, wchar_t *out, int maxlen) const {
  if (!in) throw meosException("Null pointer exception in convertString");
  const wstring &ws = isUTF ? fromUTF8(string(in)) : widen(string(in));
  int n = (int)std::min(ws.size(), (size_t)(maxlen - 1));
  wmemcpy(out, ws.c_str(), n);
  out[n] = 0;
}

// ---------------------------------------------------------------------------
// getObjectString / getObjectBool / getObjectInt64 / getObjectDouble
// ---------------------------------------------------------------------------

bool xmlobject::getObjectBool(const char *pname) const {
  string tmp;
  getObjectString(pname, tmp);
  return tmp == "true" ||
         atoi(tmp.c_str()) > 0 ||
         meos_strcasecmp(::trim(tmp).c_str(), "true") == 0;
}

string &xmlobject::getObjectString(const char *pname, string &out) const {
  xmlobject x = getObject(pname);
  if (x) {
    const char *bf = x.getRawPtr();
    if (bf) {
      parser->convertString(bf, parser->strbuff.data(), buff_pre_alloc);
      out = parser->strbuff.data();
      return out;
    }
  }
  xmlattrib xa(getAttrib(pname));
  if (xa && xa.data) {
    parser->convertString(xa.getPtr(), parser->strbuff.data(), buff_pre_alloc);
    out = parser->strbuff.data();
  } else {
    out = "";
  }
  return out;
}

wstring &xmlobject::getObjectString(const char *pname, wstring &out) const {
  xmlobject x = getObject(pname);
  if (x) {
    const wchar_t *bf = x.getWPtr();
    if (bf) {
      out = bf;
      return out;
    }
  }
  xmlattrib xa(getAttrib(pname));
  if (xa && xa.data) {
    parser->convertString(xa.getPtr(), parser->strbuffw.data(), buff_pre_alloc);
    out = parser->strbuffw.data();
  } else {
    out = L"";
  }
  return out;
}

char *xmlobject::getObjectString(const char *pname, char *out, int maxlen) const {
  xmlobject x = getObject(pname);
  if (x) {
    const char *bf = x.getRawPtr();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    out[0] = 0;
  } else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      parser->convertString(xa.data, out, maxlen);
      inplaceDecodeXML(out);
    } else {
      out[0] = 0;
    }
  }
  return out;
}

wchar_t *xmlobject::getObjectString(const char *pname, wchar_t *out, int maxlen) const {
  xmlobject x = getObject(pname);
  if (x) {
    const char *bf = x.getRawPtr();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    out[0] = 0;
  } else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      inplaceDecodeXML(xa.data);
      parser->convertString(xa.data, out, maxlen);
    } else {
      out[0] = 0;
    }
  }
  return out;
}

int64_t xmlobject::getObjectInt64(const char *pname) const {
  xmlobject x(getObject(pname));
  if (x) return x.getInt64();
  xmlattrib xa(getAttrib(pname));
  if (xa)  return xa.getInt64();
  return 0;
}

uint64_t xmlobject::getObjectInt64u(const char *pname) const {
  xmlobject x(getObject(pname));
  if (x) return x.getInt64u();
  xmlattrib xa(getAttrib(pname));
  if (xa)  return xa.getInt64u();
  return 0;
}

double xmlobject::getObjectDouble(const char *pname) const {
  xmlobject x(getObject(pname));
  if (x) return x.getDouble();
  xmlattrib xa(getAttrib(pname));
  if (xa)  return xa.getDouble();
  return 0.0;
}
