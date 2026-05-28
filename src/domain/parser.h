#pragma once
// Stub parser for the domain migration.
// DynamicResult uses Parser to evaluate custom scoring expressions. The full
// expression-evaluation engine lives in the legacy codebase and will be
// migrated in a later story. This stub satisfies the interface so
// generalresult.h/cpp compile; all evaluation returns 0.

#include <string>
#include <vector>
#include <utility>

class Parser;

class ParseNode {
public:
  virtual bool isVariable() const { return false; }
  virtual int evaluate(const Parser& /*parser*/) const { return 0; }
  virtual void assign(const Parser& /*parser*/, int /*value*/) const {}
  virtual void assignVector(const Parser& /*parser*/, const std::vector<int>& /*value*/) const {}
  virtual ~ParseNode() = default;
};

class Parser {
public:
  Parser() = default;
  ~Parser() = default;

  ParseNode* parse(const std::string& /*expr*/) { return nullptr; }
  void clear() {}

  void addSymbol(const char* /*name*/, const std::string& /*value*/) {}
  void addSymbol(const char* /*name*/, int /*value*/) {}
  void addSymbol(const char* /*name*/, const std::vector<int>& /*value*/) {}
  void addSymbol(const char* /*name*/, const std::vector<std::string>& /*value*/) {}
  void addSymbol(const char* /*name*/, std::vector<std::vector<int>>& /*value*/) {}
  void removeSymbol(const char* /*name*/) {}
  void clearSymbols() {}
  void clearVariables() const {}
  void declareSymbol(const char* /*name*/, const std::string& /*desc*/,
                     bool /*isVector*/, bool /*isMatrix*/ = false,
                     bool /*deprecated*/ = false) {}

  void takeVariable(const char* /*name*/, std::vector<int>& val) const { val.clear(); }

  void getSymbols(std::vector<std::pair<std::wstring, size_t>>& symb) const { symb.clear(); }
  void getSymbolInfo(int /*ix*/, std::wstring& name, std::wstring& desc) const {
    name.clear(); desc.clear();
  }
};
