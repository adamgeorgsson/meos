#include <gtest/gtest.h>

#include <string>

#include "xmlparser.h"

TEST(XmlParserTest, WriteReadRoundTrip) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.write("name", std::wstring(L"Åke & <Jön>"));
  p.write("value", 42);
  p.endTag();

  std::string out;
  p.getMemoryOutput(out);
  ASSERT_NE(out.find("&amp;"), std::string::npos);
  ASSERT_NE(out.find("&lt;"), std::string::npos);

  xmlparser p2;
  p2.readMemory(out, 0);
  xmlobject root = p2.getObject("root");
  ASSERT_TRUE(root);
  std::wstring name;
  root.getObjectString("name", name);
  ASSERT_EQ(name, L"Åke & <Jön>");
  EXPECT_EQ(root.getObjectInt("value"), 42);
}

TEST(XmlParserTest, ReadMemoryHandlesFragment) {
  xmlparser p;
  std::string frag = "<root><item>1</item></root>";
  ASSERT_NO_THROW(p.readMemory(frag, 0));
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_EQ(root.getObjectInt("item"), 1);
}

TEST(XmlParserTest, WriteTimeRoundTrip) {
  xmlparser p;
  p.openMemoryOutput(false);
  p.startTag("root");
  p.writeTime("time", 123);
  p.endTag();
  std::string out;
  p.getMemoryOutput(out);

  xmlparser p2;
  p2.readMemory(out, 0);
  xmlobject root = p2.getObject("root");
  ASSERT_TRUE(root);
  xmlobject time = root.getObject("time");
  ASSERT_TRUE(time);
  EXPECT_EQ(time.getRelativeTime(), 123);
}

TEST(XmlParserTest, DecodeEntitiesOnRead) {
  xmlparser p;
  p.readMemory("<root><text>A&amp;B&lt;C&gt;&quot;D&quot;&apos;E&apos;</text></root>", 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  std::string text;
  root.getObjectString("text", text);
  EXPECT_EQ(text, "A&B<C>\"D\"'E'");
}

TEST(XmlParserTest, GetObjectBoolParsesValues) {
  xmlparser p;
  p.readMemory("<root><flag>true</flag><numeric>1</numeric><off>false</off></root>", 0);
  xmlobject root = p.getObject("root");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root.getObjectBool("flag"));
  EXPECT_TRUE(root.getObjectBool("numeric"));
  EXPECT_FALSE(root.getObjectBool("off"));
}
