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

// xmlparser.cpp: implementation of the xmlparser class.
//
//////////////////////////////////////////////////////////////////////

#include "xmlparser.h"
#include "meos_util.h"
#include "meosexception.h"
#include "file_util.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

namespace {
constexpr int timeConstSecond = 10;

string trim(const string &in) {
  size_t begin = 0;
  while (begin < in.size() && isspace(static_cast<unsigned char>(in[begin])))
    ++begin;
  size_t end = in.size();
  while (end > begin && isspace(static_cast<unsigned char>(in[end - 1])))
    --end;
  return in.substr(begin, end - begin);
}

const string &encodeXML(const string &in) {
  static string out;
  const char *bf = in.c_str();
  int len = static_cast<int>(in.length());
  bool needEncode = false;
  for (int k = 0; k < len; k++)
    needEncode |= (bf[k] == '&') | (bf[k] == '>') | (bf[k] == '<') | (bf[k] == '"') | (bf[k] == 0) | (bf[k] == '\n') | (bf[k] == '\r');
  if (!needEncode)
    return in;
  out.clear();
  for (int k = 0; k < len; k++) {
    if (bf[k] == '&') out += "&amp;";
    else if (bf[k] == '<') out += "&lt;";
    else if (bf[k] == '>') out += "&gt;";
    else if (bf[k] == '"') out += "&quot;";
    else if (bf[k] == '\n') out += "&#10;";
    else if (bf[k] == '\r') out += "&#13;";
    else if (bf[k] == 0) out += ' ';
    else out += bf[k];
  }
  return out;
}

void inplaceDecodeXML(char *in) {
  char *bf = in;
  int outp = 0;
  for (int k = 0; bf[k]; k++) {
    if (bf[k] != '&') bf[outp++] = bf[k];
    else {
      if (memcmp(&bf[k], "&amp;", 5) == 0) bf[outp++] = '&', k += 4;
      else if (memcmp(&bf[k], "&lt;", 4) == 0) bf[outp++] = '<', k += 3;
      else if (memcmp(&bf[k], "&gt;", 4) == 0) bf[outp++] = '>', k += 3;
      else if (memcmp(&bf[k], "&quot;", 6) == 0) bf[outp++] = '"', k += 5;
      else if (memcmp(&bf[k], "&apos;", 6) == 0) bf[outp++] = '\'', k += 5;
      else if (memcmp(&bf[k], "&#10;", 5) == 0) bf[outp++] = '\n', k += 4;
      else if (memcmp(&bf[k], "&#13;", 5) == 0) bf[outp++] = '\r', k += 4;
      else bf[outp++] = bf[k];
    }
  }
  bf[outp] = 0;
}

const char *decodeXML(const char *in) {
  const char *bf = in;
  bool needDecode = false;
  for (int k = 0; bf[k]; k++)
    needDecode |= (bf[k] == '&');
  if (!needDecode)
    return in;
  static string out;
  out.clear();
  for (int k = 0; bf[k]; k++) {
    if (bf[k] == '&') {
      if (memcmp(&bf[k], "&amp;", 5) == 0) out += '&', k += 4;
      else if (memcmp(&bf[k], "&lt;", 4) == 0) out += '<', k += 3;
      else if (memcmp(&bf[k], "&gt;", 4) == 0) out += '>', k += 3;
      else if (memcmp(&bf[k], "&quot;", 6) == 0) out += '"', k += 5;
      else if (memcmp(&bf[k], "&apos;", 6) == 0) out += '\'', k += 5;
      else if (memcmp(&bf[k], "&#10;", 5) == 0) out += '\n', k += 4;
      else if (memcmp(&bf[k], "&#13;", 5) == 0) out += '\r', k += 4;
      else out += bf[k];
    } else {
      out += bf[k];
    }
  }
  return out.c_str();
}

int parseRelativeTime(const char *data) {
  if (data) {
    int ret = atoi(data);
    if (timeConstSecond == 10) {
      int j = 0;
      while (data[j]) {
        if (data[j] == '.') {
          int t = data[j + 1] - '0';
          if (t > 0 && t < 10) {
            if (ret < 0 || data[0] == '-')
              return ret * timeConstSecond - t;
            return ret * timeConstSecond + t;
          }
          break;
        }
        j++;
      }
    }
    if (ret == -1)
      return ret;
    return ret * timeConstSecond;
  }
  return 0;
}

string localFormatDouble(double d) {
  char bf[64];
  if (d > 1e7 || d < -1e7)
    snprintf(bf, sizeof(bf), "%#.16e", d);
  else
    snprintf(bf, sizeof(bf), "%#.16g", d);
  char *dot = strchr(bf, '.');
  if (dot) {
    char *end = bf + strlen(bf) - 1;
    while (end > dot && (*end == '0'))
      end--;
    *(end + 1) = 0;
  }
  return bf;
}
} // namespace


xmlparser::xmlparser()
{
  lastIndex = 0;
  tagStackPointer = 0;
  isUTF = false;
  cutMode = false;
  toString = false;
  strbuff.resize(buff_pre_alloc);
  strbuffw.resize(buff_pre_alloc);
}

xmlparser::~xmlparser()
{
  fin.close();
  foutFile.close();
}

inline bool isBlankSpace(char b) {
  return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

void xmlparser::access(int) {
}

xmlobject::xmlobject()
{
  parser = 0;
}

xmlobject::~xmlobject()
{
  //MessageBox(NULL, name.c_str(), "Destroying: ", MB_OK);

//	if (objects) delete objects;
}


const string &xmlparser::encodeXML(const wstring &input) {
  string utf8 = meos::util::toUTF8(input);
  return ::encodeXML(utf8);
}

const string &xmlparser::encodeXML(const string &input) {
  return ::encodeXML(input);
}

void xmlparser::write(const char *tag, const wstring &Value)
{
  if (!cutMode || !Value.empty()) {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
    /*fOut() << "<" << tag << ">"
           << encodeXML(Value)
           << "</" << tag << ">" << endl;*/
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const string &Value)
{
  if (!cutMode || Value!="") {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Value)
{
  if (!cutMode || Value != "") {
    auto &valEnc = encodeXML(Value);
    if (valEnc.length() > 400) {
      fOut() << "<" << tag << ">"
        << valEnc
        << "</" << tag << ">" << endl;
    }
    else {
      char bf[512];
      snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, valEnc.c_str(), tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}


void xmlparser::write(const char *tag)
{
  char bf[128];
  snprintf(bf, sizeof(bf), "<%s/>\n", tag);
  fOut() << bf;

  //fOut() << "<" << tag << "/>" << endl;
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const string &Value)
{
  if (!cutMode || Value!="") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const wstring &Value)
{
  if (!cutMode || !Value.empty()) {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(Value) << "\"/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *prop, const wchar_t *value)
{
  encodeString = value;
  write(tag, prop, encodeString);
}

void xmlparser::writeBool(const char *tag, const char *prop, bool value)
{
  if (!cutMode || value)
    write(tag, prop, value ? L"true" : L"false");
}

void xmlparser::writeAscii(const char *tag, const vector<pair<string, wstring>>& propValue,
                           const string &valueAscii) {
  if (!cutMode || valueAscii != "") {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    fOut() << ">" << ::encodeXML(valueAscii)
           << "</" << tag << ">" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const char *Property, const wstring &PropValue, const wstring &Value)
{
  if (!cutMode || Value != L"" || PropValue != L"") {
    fOut() << "<" << tag << " " << Property << "=\""
           << encodeXML(PropValue) << "\">" << encodeXML(Value)
           << "</" << tag << ">\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

/*
void xmlparser::write(const char *tag, const vector< pair<string, string> > &propValue, const string &value) {
  if (!cutMode || value != "" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">" << endl;
    }
    else
      fOut() << "/>" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char *tag, const vector< pair<string, string> > &propValue, const wstring &value) {
  if (!cutMode || value != L"" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">" << endl;
    }
    else
      fOut() << "/>" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}
*/
void xmlparser::write(const char *tag, const vector< pair<string, wstring> > &propValue, const wstring &value) {
  if (!cutMode || value != L"" || !propValue.empty()) {
    fOut() << "<" << tag;
    for (size_t k = 0; k < propValue.size(); k++) {
      fOut() << " " << propValue[k].first << "=\"" << encodeXML(propValue[k].second) << "\"";
    }
    if (!value.empty()) {
      fOut() << ">" << encodeXML(value)
             << "</" << tag << ">\n";
    }
    else
      fOut() << "/>\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

/*
void xmlparser::write(const char *tag, const char *prop,
                      bool propValue, const string &value) {
  write(tag, prop, propValue ? "true" : "false", value);
}*/

void xmlparser::writeBool(const char *tag, const char *prop,
                      bool propValue, const wstring &value) {
  static const wstring wTrue = L"true";
  static const wstring wFalse = L"false";

  write(tag, prop, propValue ? wTrue : wFalse, value);
}
/*
void xmlparser::write(const char *tag, const char *prop,
                      const char *propValue, const string &value) {
  write(tag, prop, string(propValue), value);
}*/

void xmlparser::write(const char *tag, const char *prop,
                      const wchar_t *propValue, const wstring &value) {
  write(tag, prop, wstring(propValue), value);
}

void xmlparser::write(const char *tag, int Value) {
  if (!cutMode || Value!=0) {
    char bf[256];
    snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, Value, tag);
    fOut() << bf;
    //fOut() << "<" << tag << ">"
    //       << Value
    //       << "</" << tag << ">" << endl;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write(const char* tag, double value) {
  if (!cutMode || value != 0.0) {
    char bf[256];
    snprintf(bf, sizeof(bf), "<%s>%s</%s>\n", tag, localFormatDouble(value).c_str(), tag);
    fOut() << bf;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::writeTime(const char *tag, int relativeTime) {
  if (!cutMode || relativeTime != 0) {
    char bf[256];
    int subSec = timeConstSecond == 1 ? 0 : relativeTime % timeConstSecond;

    if (timeConstSecond == 1 || relativeTime == -1)
      snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, relativeTime, tag);
    else if (subSec == 0 && relativeTime != -10) 
      snprintf(bf, sizeof(bf), "<%s>%d</%s>\n", tag, relativeTime / timeConstSecond, tag);
    else if (relativeTime >= 0)
      if (timeConstSecond == 10)
        snprintf(bf, sizeof(bf), "<%s>%d.%d</%s>\n", tag, (relativeTime / timeConstSecond), 
                                               (relativeTime % timeConstSecond), tag);
      else
        snprintf(bf, sizeof(bf), "<%s>%d.%02d</%s>\n", tag, (relativeTime / timeConstSecond),
                                                 (relativeTime % timeConstSecond), tag);
    else {
      int at = std::abs(relativeTime);
      if (timeConstSecond == 10)
        snprintf(bf, sizeof(bf), "<%s>-%d.%d</%s>\n", tag, (at / timeConstSecond),
                                                (at % timeConstSecond), tag);
      else
        snprintf(bf, sizeof(bf), "<%s>-%d.%02d</%s>\n", tag, (at / timeConstSecond),
                                                  (at % timeConstSecond), tag);
    }
    fOut() << bf;
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::writeBool(const char *tag, bool value)
{
  if (!cutMode || value) {
    if (value) {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s>true</%s>\n", tag, tag);
      fOut() << bf;
    }
    else {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s>false</%s>\n", tag, tag);
      fOut() << bf;
    }
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write64(const char* tag, int64_t value) {
  if (!cutMode || value!=0) {
    fOut() << "<" << tag << ">"
           << value
           << "</" << tag << ">\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::write64u(const char* tag, uint64_t value) {
  if (!cutMode || value != 0) {
    fOut() << "<" << tag << ">"
      << value
      << "</" << tag << ">\n";
  }
  if (!fOut().good())
    throw meosException("Writing to XML file failed.");
}

void xmlparser::startTag(const char *tag, const char *prop, const wstring &Value)
{
  if (tagStackPointer<32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    }
    else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">" << endl;
    }

    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}


void xmlparser::startTag(const char *tag, const char *prop, const string &Value)
{
  if (tagStackPointer<32) {
    const string &valEnc = encodeXML(Value);
    if (valEnc.length() < 128) {
      char bf[256];
      snprintf(bf, sizeof(bf), "<%s %s=\"%s\">\n", tag, prop, valEnc.c_str());
      fOut() << bf;
    }
    else {
      fOut() << "<" << tag << " " << prop << "=\"" << encodeXML(Value) << "\">" << endl;
    }

    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}
/*
void xmlparser::startTag(const char *tag, const vector<string> &propvalue)
{
  if (tagStackPointer<32) {
    fOut() << "<" << tag << " ";
    for (size_t k=0;k<propvalue.size(); k+=2) {
      fOut() << propvalue[k] << "=\"" << encodeXML(propvalue[k+1]) << "\" ";
    }
    fOut() << ">\n";
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}
*/
void xmlparser::startTag(const char *tag, const vector<wstring> &propvalue)
{
  if (tagStackPointer<32) {
    fOut() << "<" << tag ;
    for (size_t k=0;k<propvalue.size(); k+=2) {
      fOut() << " " << encodeXML(propvalue[k]) << "=\"" << encodeXML(propvalue[k+1]) << "\"";
    }
    fOut() << ">\n";
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}

void xmlparser::startTag(const char *tag)
{
  if (tagStackPointer<32) {
    char bf[128];
    snprintf(bf, sizeof(bf), "<%s>\n", tag);
    fOut() << bf;
    tagStack[tagStackPointer++]=tag;
    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else
    throw meosException("Tag depth too large.");
}

void xmlparser::endTag()
{
  if (tagStackPointer>0) {
    char bf[128];
    const char *tag = tagStack[--tagStackPointer].c_str();
    snprintf(bf, sizeof(bf), "</%s>\n", tag);
    fOut() << bf;

    if (!fOut().good())
      throw meosException("Writing to XML file failed.");
  }
  else throw std::runtime_error("BAD XML CODE");
}

void xmlparser::openMemoryOutput(bool useCutMode) {
  cutMode = useCutMode;
  toString = true;
  foutString.clear();
  fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";
}



void xmlparser::getMemoryOutput(string &res) {
  res = foutString.str();
  foutString.clear();
}

void xmlparser::openOutput(const wchar_t *file, bool useCutMode)
{
  openOutputT(file, useCutMode, "");
}

void xmlparser::openOutputT(const wchar_t *file, bool useCutMode, const string &type) {
  toString = false;
  cutMode = useCutMode;
  string utf8path = meos::util::toUTF8(wstring(file));
  foutFile.open(utf8path, ios::binary);
  tagStackPointer = 0;

  if (foutFile.bad())
    throw meosException(L"Kunde inte skriva till 'X'.#" + wstring(file));

  fOut() << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n\n";

  if (!type.empty())
    startTag(type.c_str());
}



int xmlparser::closeOut()
{
  while(tagStackPointer>0)
    endTag();

  int len = (int)foutFile.tellp();
  foutFile.close();

  return len;
}

xmldata::xmldata(const char *t, char *d) : tag(t), data(d)
{
  parent = -1;
  next = 0;
}

xmlattrib::xmlattrib(const char *t, char *d, const xmlparser *p) : tag(t), data(d), parser(p) {}

void xmlparser::read(const wstring &file, int maxobj)
{
  string utf8path = meos::util::toUTF8(file);
  fin.open(utf8path, ios::binary);

  if (!fin.good())
    throw meosException(L"Failed to open 'X' for reading.#" + file);

  std::ostringstream ss;
  ss << fin.rdbuf();
  fin.close();

  readMemory(ss.str(), maxobj);
}

void xmlparser::readMemory(const string &mem, int maxobj)
{
  if (mem.empty())
    return;

  const char *raw = mem.c_str();
  size_t offset = 0;
  if (mem.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
    offset = 3;

  const char *ptr = ltrim(raw + offset);
  isUTF = checkUTF(ptr);

  size_t p1 = 0;
  if (memcmp(ptr, "<?xml", 5) == 0) {
    const char *declEnd = strchr(ptr, '>');
    if (declEnd)
      p1 = size_t((declEnd + 1) - raw);
  }

  int asize = int(mem.size() - p1);
  if (maxobj > 0)
    asize = min(asize, maxobj * 256);

  xbf.resize(asize + 1);
  xmlinfo.clear();
  xmlinfo.reserve(xbf.size() / 30);
  parseStack.clear();

  memcpy(&xbf[0], mem.c_str() + p1, asize);
  xbf[asize] = 0;

  parse(maxobj);
}

bool xmlparser::checkUTF(const char *ptr) const {
  bool utf = false;
  if ((unsigned char)ptr[0] == 0xEF && (unsigned char)ptr[1] == 0xBB && (unsigned char)ptr[2] == 0xBF) {
    utf = true;
    ptr += 3;
  }
  if (memcmp(ptr, "<?xml", 5) == 0) {
    int i = 5;
    bool hasEncode = false;
    while (ptr[i]) {
      if ((ptr[i] == 'U' || ptr[i] == 'u') && meos_memicmp(ptr + i, "UTF-8", 5) == 0) {
        utf = true;
        break;
      }
      if (ptr[i] == 'e' && memcmp(ptr + i, "encoding", 8) == 0)
        hasEncode = true;
      i++;
    }
    if (!hasEncode)
      utf = true;
  } else if (ptr[0] == '<') {
    utf = true;
  } else {
    throw std::runtime_error("Invalid XML file.");
  }
  return utf;
}

bool xmlparser::parse(int maxobj) {
  lineNumber=0;
  int pp = 0;
  const int size = xbf.size()-2;
  while (pp < size) {
    while (pp < size && xbf[pp] != '<') pp++;

    // Found tag
    if (xbf[pp] == '<') {
      xbf[pp] = 0;
      char *start = &xbf[pp+1];
      while (pp < size && xbf[pp] != '>') pp++;

      if (xbf[pp] == '>') {
        xbf[pp] = 0;
      }
      if (*start=='!')
        continue; //Comment

      processTag(start, &xbf[pp-1]);
    }

    if (maxobj>0 && int(xmlinfo.size()) >= maxobj) {
      xbf[pp+1] = 0;
      return true;
    }
    pp++;
  }

  lastIndex = 0;
  return true;
}

bool xmlparser::processTag(char *start, char *end) {
  static char err[64];
  bool onlyAttrib = *end == '/';
  bool endTag = *start == '/';

  char *tag = start;

  if (endTag)
    tag++;

  while (start<=end && !isBlankSpace(*start))
    start++;

  *start = 0;

  if (!endTag && !onlyAttrib) {
    parseStack.push_back(xmlinfo.size());
    xmlinfo.push_back(xmldata(tag, end+2));
    int p = parseStack.size()-2;
    xmlinfo.back().parent = p>=0 ? parseStack[p] : -1;
  }
  else if (endTag) {
    if (!parseStack.empty()){
      xmldata &xd = xmlinfo[parseStack.back()];
      inplaceDecodeXML(xd.data);
      if (strcmp(tag, xd.tag)== 0) {
        parseStack.pop_back();
        xd.next = xmlinfo.size();
      }
      else {
        snprintf(err, sizeof(err), "Unmatched tag '%s', expected '%s'.", tag, xd.tag);
        throw std::runtime_error(err);
      }
    }
    else
    {
      snprintf(err, sizeof(err), "Unmatched tag '%s'.", tag);
      throw std::runtime_error(err);
    }
  }
  else if (onlyAttrib) {
    *end = 0;
    xmlinfo.push_back(xmldata(tag, 0));
    int p = parseStack.size() - 1;
    xmlinfo.back().parent = p>=0 ? parseStack[p] : -1;
    xmlinfo.back().next = xmlinfo.size();
  }
  return true;
}

char * xmlparser::ltrim(char *s)
{
  while(*s && isspace((unsigned char)(*s)))
    s++;

  return s;
}

const char * xmlparser::ltrim(const char *s)
{
  while(*s && isspace((unsigned char)(*s)))
    s++;

  return s;
}
/*
const char * xmlparser::getError()
{
  return errorMessage.c_str();
}*/

xmlobject xmlobject::getObject(const char *pname) const
{
  if (pname == 0)
    return *this;
  if (isnull())
    throw std::runtime_error("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;

  parser->access(index);

  unsigned child = index+1;
  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    if (strcmp(xmlinfo[child].tag, pname)==0)
      return xmlobject(parser, child);
    else
      child = xmlinfo[child].next;
  }
  return xmlobject(0);
}


void xmlobject::getObjects(xmlList &obj) const
{
  obj.clear();

  if (isnull())
    throw std::runtime_error("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;
  unsigned child = index+1;
  parser->access(index);

  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    obj.push_back(xmlobject(parser, child));
    child = xmlinfo[child].next;
  }
}

void xmlobject::getObjects(const char *tag, xmlList &obj) const
{
  obj.clear();

  if (isnull())
    throw std::runtime_error("Null pointer exception");

  vector<xmldata> &xmlinfo = parser->xmlinfo;
  unsigned child = index+1;
  parser->access(index);

  while (child < xmlinfo.size() && xmlinfo[child].parent == index) {
    if (strcmp(tag, xmlinfo[child].tag) == 0)
      obj.push_back(xmlobject(parser, child));
    child = xmlinfo[child].next;
  }
}


const xmlobject xmlparser::getObject(const char *pname) const
{
  if (xmlinfo.size()>0){
    if (pname == 0 || strcmp(xmlinfo[0].tag, pname) == 0)
      return xmlobject(const_cast<xmlparser *>(this), 0);
    else return xmlobject(const_cast<xmlparser *>(this), 0).getObject(pname);
  }
  else return xmlobject(0);
}

xmlattrib xmlobject::getAttrib(const char *pname) const
{
  if (pname != 0) {
    char *start = const_cast<char *>(parser->xmlinfo[index].tag);
    const char *end = parser->xmlinfo[index].data;

    if (end)
      end-=2;
    else {
      if (size_t(index + 1) < parser->xmlinfo.size())
        end = parser->xmlinfo[index+1].tag - 1;
      else
        end = &parser->xbf.back();
    }

    // Scan past tag.
    while (start<end && *start != 0)
      start++;
    start++;

    char *oldStart = start;
    while (start<=end) {
      while(start<=end && isBlankSpace(*start))
        start++;

      char *tag = start;

      while(start<=end && *start!='=' && *start!=0)
        start++;

      if (start<end && (start[1]=='"' || start[1] == 0)) {
        *start = 0;
        ++start;
        char *value = ++start;

        while(start<=end && (*start!='"' && *start != 0))
          start++;

        if (start<=end) {
          *start = 0;
          if (strcmp(pname, tag) == 0)
            return xmlattrib(tag, value, parser);
          start++;
        }
        else {//Error
        }
      }

      if (oldStart == start)
        break;
      else
        oldStart = start;
    }
  }
  return xmlattrib(0,0, parser);
}

static int unconverted = 0;

const wchar_t *xmlobject::getWPtr() const
{
  const char *ptr = getRawPtr();
  if (ptr == nullptr)
    return nullptr;
  static wchar_t buff[buff_pre_alloc];
  int len = min<int>(strlen(ptr), buff_pre_alloc - 1);
  if (parser->isUTF) {
    wstring w = meos::util::fromUTF8(string(ptr, len));
    wcsncpy(buff, w.c_str(), buff_pre_alloc - 1);
    buff[buff_pre_alloc - 1] = 0;
  }
  else {
    wstring w = meos::util::widen(string(ptr, len));
    wcsncpy(buff, w.c_str(), buff_pre_alloc - 1);
    buff[buff_pre_alloc - 1] = 0;
  }
  return buff;
}

const char *xmlobject::getPtr() const {
  const char *ptr = getRawPtr();
  if (ptr == nullptr)
    return nullptr;
  static char buff[buff_pre_alloc];
  if (parser->isUTF) {
    string s = meos::util::narrow(meos::util::fromUTF8(ptr));
    strncpy(buff, s.c_str(), buff_pre_alloc - 1);
    buff[buff_pre_alloc - 1] = 0;
  }
  else {
    return ptr;
  }
  return buff;
}

int xmlobject::getRelativeTime() const {
  const char *d = parser->xmlinfo[index].data;
  return parseRelativeTime(d);
}

void xmlparser::convertString(const char *in, char *out, int maxlen) const
{
  if (in == nullptr)
    throw std::runtime_error("Null pointer exception");

  if (!isUTF) {
    strncpy(out, in, maxlen - 1);
    out[maxlen - 1] = 0;
    return;
  }

  string s = meos::util::narrow(meos::util::fromUTF8(in));
  strncpy(out, s.c_str(), maxlen - 1);
  out[maxlen - 1] = 0;
}

void xmlparser::convertString(const char *in, wchar_t *out, int maxlen) const
{
  if (in==0)
    throw std::runtime_error("Null pointer exception");

  wstring w = !isUTF ? meos::util::widen(in) : meos::util::fromUTF8(in);
  wcsncpy(out, w.c_str(), maxlen - 1);
  out[maxlen - 1] = 0;
}


bool xmlobject::getObjectBool(const char *pname) const
{
  string tmp;
  getObjectString(pname, tmp);

  return tmp=="true" ||
         atoi(tmp.c_str()) > 0 ||
         strcasecmp(trim(tmp).c_str(), "true") == 0;
}

string &xmlobject::getObjectString(const char *pname, string &out) const
{
  xmlobject x=getObject(pname);
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
  }
  else
    out = "";

  return out;
}

wstring &xmlobject::getObjectString(const char *pname, wstring &out) const
{
  xmlobject x=getObject(pname);
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
  }
  else
    out = L"";

  return out;
}


char *xmlobject::getObjectString(const char *pname, char *out, int maxlen) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const char *bf = x.getRawPtr();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    else
      out[0] = 0;
  }
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      parser->convertString(xa.data, out, maxlen);
      inplaceDecodeXML(out);
    } else
       out[0] = 0;
  }
  return out;
}

wchar_t *xmlobject::getObjectString(const char *pname, wchar_t *out, int maxlen) const
{
  xmlobject x=getObject(pname);
  if (x) {
    const char *bf = x.getRawPtr();
    if (bf) {
      parser->convertString(bf, out, maxlen);
      return out;
    }
    else 
      out[0] = 0;
  }
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa && xa.data) {
      inplaceDecodeXML(xa.data); //WCS XXX  Only once!?
      parser->convertString(xa.data, out, maxlen);
    } else
       out[0] = 0;
  }
  return out;
}


const char *xmlattrib::getPtr() const
{
  if (data)
    return decodeXML(data);
  else
    return nullptr;
}

const wchar_t *xmlattrib::getWPtr() const
{
  if (data) {
    const char *dec = decodeXML(data);
    static wchar_t xbf[buff_pre_alloc];
    parser->convertString(dec, xbf, buff_pre_alloc);
    return xbf;
  }
  else
    return nullptr;
}

int64_t xmlobject::getObjectInt64(const char* pname) const {
  xmlobject x(getObject(pname));
  if (x)
    return x.getInt64();
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa)
      return xa.getInt64();
  }
  return 0;
}

uint64_t xmlobject::getObjectInt64u(const char* pname) const {
  xmlobject x(getObject(pname));
  if (x)
    return x.getInt64u();
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa)
      return xa.getInt64u();
  }
  return 0;
}

double xmlobject::getObjectDouble(const char* pname) const {
  xmlobject x(getObject(pname));
  if (x)
    return x.getDouble();
  else {
    xmlattrib xa(getAttrib(pname));
    if (xa)
      return xa.getDouble();
  }
  return 0;
}
