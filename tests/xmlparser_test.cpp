#include <gtest/gtest.h>
#include "xmlparser.h"
#include "meos_util.h"
#include "meosexception.h"

#include <sstream>
#include <string>

using std::string;
using std::wstring;

// ---- Write → memory output → read back ----------------------------------

TEST(XmlWrite, WriteIntRoundTrip) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write("value", 42);
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("<value>42</value>"), string::npos);
}

TEST(XmlWrite, WriteWStringRoundTrip) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write("name", wstring(L"Anna"));
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("<name>Anna</name>"), string::npos);
}

TEST(XmlWrite, WriteBoolTrue) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.writeBool("flag", true);
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("<flag>true</flag>"), string::npos);
}

TEST(XmlWrite, WriteBoolFalse) {
  xmlparser p;
  p.openMemoryOutput(true /*cutMode*/);
  p.startTag("root");
  p.writeBool("flag", false); // cutMode: false values omitted
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_EQ(mem.find("<flag>"), string::npos);
}

TEST(XmlWrite, WriteDouble) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write("dist", 3.14);
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("<dist>3.14</dist>"), string::npos);
}

TEST(XmlWrite, WriteAttributePropValue) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.write("item", "id", string("123"));
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("id=\"123\""), string::npos);
}

TEST(XmlWrite, WriteXmlEscaping) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write("data", string("a & b < c > d"));
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("&amp;"), string::npos);
  EXPECT_NE(mem.find("&lt;"),  string::npos);
  EXPECT_NE(mem.find("&gt;"),  string::npos);
}

TEST(XmlWrite, CutModeSkipsZero) {
  xmlparser p;
  p.openMemoryOutput(true /*cutMode*/);
  p.startTag("root");
  p.write("zero", 0);
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_EQ(mem.find("<zero>"), string::npos);
}

TEST(XmlWrite, CutModeSkipsEmptyString) {
  xmlparser p;
  p.openMemoryOutput(true);
  p.startTag("root");
  p.write("empty", string(""));
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_EQ(mem.find("<empty>"), string::npos);
}

TEST(XmlWrite, Write64) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write64("big", int64_t(9876543210LL));
  p.endTag();
  string mem;
  p.getMemoryOutput(mem);
  EXPECT_NE(mem.find("9876543210"), string::npos);
}

// ---- Read from memory ---------------------------------------------------

static string makeXML(const string &body) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + body;
}

TEST(XmlRead, ReadIntValue) {
  string xml = makeXML("<root><age>25</age></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_EQ(root.getObjectInt("age"), 25);
}

TEST(XmlRead, ReadStringValue) {
  string xml = makeXML("<root><name>Alice</name></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  wstring name;
  root.getObjectString("name", name);
  EXPECT_EQ(name, L"Alice");
}

TEST(XmlRead, ReadAttribute) {
  string xml = makeXML("<root><item id=\"42\"/></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  xmlobject item = root.getObject("item");
  ASSERT_TRUE(item);
  EXPECT_EQ(item.getAttrib("id").getInt(), 42);
}

TEST(XmlRead, ReadBoolTrue) {
  string xml = makeXML("<root><flag>true</flag></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root.getObjectBool("flag"));
}

TEST(XmlRead, ReadBoolFalse) {
  string xml = makeXML("<root><flag>false</flag></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_FALSE(root.getObjectBool("flag"));
}

TEST(XmlRead, ReadNestedObjects) {
  string xml = makeXML(
      "<root><child><val>1</val></child><child><val>2</val></child></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  xmlList children;
  root.getObjects("child", children);
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0].getObjectInt("val"), 1);
  EXPECT_EQ(children[1].getObjectInt("val"), 2);
}

TEST(XmlRead, SkipsXmlDeclaration) {
  // Parser must skip <?xml...?> and parse root correctly.
  string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<data><x>99</x></data>";
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject data = p.getObject("data");
  ASSERT_TRUE(data);
  EXPECT_EQ(data.getObjectInt("x"), 99);
}

TEST(XmlRead, FragmentWithoutDeclaration) {
  // readMemory must handle fragments without <?xml?> declaration.
  string xml = "<config><key>hello</key></config>";
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject cfg = p.getObject("config");
  ASSERT_TRUE(cfg);
  wstring key;
  cfg.getObjectString("key", key);
  EXPECT_EQ(key, L"hello");
}

TEST(XmlRead, AttributeClosingQuote) {
  // Regression: closing quote must be handled (start <= end boundary).
  string xml = makeXML("<root><item name=\"test\" value=\"123\"/></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  xmlobject item = root.getObject("item");
  ASSERT_TRUE(item);
  xmlattrib name  = item.getAttrib("name");
  xmlattrib value = item.getAttrib("value");
  ASSERT_TRUE(name);
  ASSERT_TRUE(value);
  EXPECT_EQ(name.getStr(),  "test");
  EXPECT_EQ(value.getInt(), 123);
}

TEST(XmlRead, UTF8Swedish) {
  // Swedish characters should round-trip through UTF-8.
  xmlparser pw;
  pw.openMemoryOutput(false);
  pw.startTag("root");
  pw.write("city", wstring(L"\u00C5 \u00E4 \u00F6")); // Å ä ö
  pw.endTag();
  string mem;
  pw.getMemoryOutput(mem);

  xmlparser pr;
  pr.readMemory(mem, 0);
  xmlobject root = pr.getObject("root");
  ASSERT_TRUE(root);
  wstring city;
  root.getObjectString("city", city);
  EXPECT_EQ(city, wstring(L"\u00C5 \u00E4 \u00F6"));
}

TEST(XmlRead, ReadInt64) {
  string xml = makeXML("<root><big>9876543210</big></root>");
  xmlparser p;
  p.readMemory(xml, 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_EQ(root.getObjectInt64("big"), int64_t(9876543210LL));
}

TEST(XmlRead, TagDepthError) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("a");
  p.endTag();  // Pops "a" — succeeds
  EXPECT_THROW(p.endTag(), meosException); // Stack empty — must throw
}
