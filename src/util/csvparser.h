#pragma once

#include <fstream>
#include <list>
#include <map>
#include <string>
#include <vector>

using std::list;
using std::map;
using std::string;
using std::vector;
using std::wstring;

class CSVLineWrapper;

struct PunchInfo {
  int code;
  int card;
  int time;
  char date[28];
};

struct TeamLineup {
  struct TeamMember {
    wstring name;
    wstring club;
    int cardNo;
    wstring course;
    wstring cls;
  };
  wstring teamName;
  wstring teamClass;
  wstring teamClub;
  vector<TeamMember> members;
};

class csvparser {
protected:
  std::ofstream fout;
  std::ifstream fin;

  int LineNumber;
  string ErrorMessage;

  void parseUnicode(const wstring &file, list<vector<wstring>> &data);

public:
  static void convertUTF(const wstring &file);

  void parse(const wstring &file, list<vector<wstring>> &dataOutput);

  bool openOutput(const wstring &file, bool writeUTF = false);
  bool closeOutput();

  bool outputRow(const vector<string> &out);
  bool outputRow(const vector<wstring>& out);
  bool outputRow(const string &row);

  int nimport = 0;

  static int split(char *line, vector<char *> &split);
  static int split(wchar_t *line, vector<wchar_t *> &split);

  enum class CSV {
    NoCSV,
    Unknown,
    RAID,
    OE,
    OS,
  };

  static CSV iscsv(const wstring &file);

  csvparser();
  virtual ~csvparser();
};
