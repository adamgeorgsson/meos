// parser.h — Cross-platform expression parser for DynamicResult scripting.
// Ported from code/parser.h — stripped Win32/gdioutput dependencies.
#pragma once

#include <map>
#include <string>
#include <vector>

class Parser;

class ParseNode {
public:
  virtual bool isVariable() const { return false; }
  virtual int evaluate(const Parser &parser) const = 0;
  virtual void assign(const Parser &parser, int value) const;
  virtual void assignVector(const Parser &parser, const std::vector<int> &value) const;
  virtual ~ParseNode() = 0;
};

class Parser {
  enum Operator {
    OpNone,
    OpPlus,
    OpMinus,
    OpTimes,
    OpDivide,
    OpMod,
    OpMax,
    OpMin,
    OpEquals,
    OpNotEquals,
    OpLess,
    OpMore,
    OpLessEquals,
    OpMoreEquals,
    OpAnd,
    OpOr,
    OpNot,
    OpAssign,
    OpLeftP,
    OpRightP,
    OpInc,
    OpDec,
    OpIncPost,
    OpDecPost,
    OpIncPre,
    OpDecPre,
    OpReturn,
    OpSize,
    OpSizeBase,
    OpSizeSub,
    OpBreak,
    OpSortArray,
  };

  static const int levelMax = 3;
  static int getLevel(Operator op);
  static void eatWhite(const std::string &expr, size_t &pos);

  ParseNode *parseFunction(const std::string &name, const std::string &expr, size_t &pos);
  static Operator parseOperator(const std::string &expr, size_t &pos, int level);
  static Operator parseOperatorAux(const std::string &expr, size_t &pos);

  struct Symbol {
    std::string desc;
    bool isVector;
    bool isMatrix;
    bool deprecated = false;
    std::vector<std::vector<int>> value;
  };

  std::map<std::string, Symbol> symb;
  mutable std::map<std::string, std::vector<int>> var;

  mutable int breakMode;
  mutable bool returnMode;
  mutable bool ignoreValue;

  ParseNode *parseStatement(const std::string &expr, bool primary);
  ParseNode *parseStatement(ParseNode *left, const std::string &expr, size_t &pos, int level, bool changeSign);

  class StatementNode : public ParseNode {
    ParseNode *node;
    StatementNode *next;
    StatementNode(const StatementNode &) = delete;
    StatementNode &operator=(const StatementNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    StatementNode();
    virtual ~StatementNode();
    friend class Parser;
  };

  class ValueNode : public ParseNode {
    std::string value;
    ValueNode(const ValueNode &) = delete;
    ValueNode &operator=(const ValueNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    bool isVariable() const;
    void assign(const Parser &parser, int value) const;
    void assignVector(const Parser &parser, const std::vector<int> &value) const;
    ValueNode();
    virtual ~ValueNode();
    friend class Parser;
  };

  class ArrayValueNode : public ParseNode {
    std::string expr;
    ParseNode *index;
    ParseNode *index2;
    ArrayValueNode(const ArrayValueNode &) = delete;
    ArrayValueNode &operator=(const ArrayValueNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    void assign(const Parser &parser, int value) const;
    void assignVector(const Parser &parser, const std::vector<int> &value) const;
    bool isVariable() const;
    ArrayValueNode();
    virtual ~ArrayValueNode();
    friend class Parser;
  };

  class UnaryOperatorNode : public ParseNode {
    Operator op;
    ParseNode *right;
    UnaryOperatorNode(const UnaryOperatorNode &) = delete;
    UnaryOperatorNode &operator=(const UnaryOperatorNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    UnaryOperatorNode();
    virtual ~UnaryOperatorNode();
    friend class Parser;
  };

  class BinaryOperatorNode : public ParseNode {
    Operator op;
    ParseNode *left;
    ParseNode *right;
    BinaryOperatorNode(const BinaryOperatorNode &) = delete;
    BinaryOperatorNode &operator=(const BinaryOperatorNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    BinaryOperatorNode();
    virtual ~BinaryOperatorNode();
    friend class Parser;
  };

  class IfNode : public ParseNode {
    ParseNode *condition;
    ParseNode *iftrue;
    ParseNode *iffalse;
    IfNode(const IfNode &) = delete;
    IfNode &operator=(const IfNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    IfNode();
    virtual ~IfNode();
    friend class Parser;
  };

  class WhileNode : public ParseNode {
    ParseNode *condition;
    ParseNode *body;
    WhileNode(const WhileNode &) = delete;
    WhileNode &operator=(const WhileNode &) = delete;
  public:
    int evaluate(const Parser &parser) const;
    WhileNode();
    virtual ~WhileNode();
    friend class Parser;
  };

  class ForNode : public ParseNode {
    ParseNode *start;
    ParseNode *condition;
    ParseNode *update;
    ParseNode *body;
    ForNode(const ForNode &);
    ForNode &operator=(const ForNode &);
  public:
    int evaluate(const Parser &parser) const;
    ForNode();
    virtual ~ForNode();
    friend class Parser;
  };

  int evaluate(const std::string &input) const;
  int evaluate(const std::string &input, int index, int index2) const;
  int evaluateSize(const std::string &input, int index) const;
  void sortArray(const std::string &input) const;

  void storeVariable(const std::string &input, const std::vector<int> &value) const;
  void storeVariable(const std::string &input, int value) const;
  void storeVariable(const std::string &input, int index, int value) const;

  UnaryOperatorNode *getUnary();
  BinaryOperatorNode *getBinary();
  ValueNode *getValue();
  ArrayValueNode *getArrayValue();
  StatementNode *getStatement();
  IfNode *getif();
  WhileNode *getWhile();
  ForNode *getFor();

  ParseNode *parseif(const std::string &expr, size_t &pos);
  ParseNode *parseReturn(const std::string &expr, size_t &pos);
  ParseNode *parseValue(const std::string &word, const std::string &expr, size_t &pos);
  ParseNode *parseWhile(const std::string &expr, size_t &pos);
  ParseNode *parseFor(const std::string &expr, size_t &pos);

  std::string parseMethod(const std::string &expr, size_t &pos);

  std::vector<ParseNode *> nodes;
  bool isMatrix(const std::string &symbol) const;
  bool isVector(const std::string &symbol) const;

  const std::vector<int> &getVector(const std::string &symbol, int index) const;

public:
  ParseNode *parse(const std::string &expr);
  static void test();

  Parser();
  ~Parser();
  void clear();
  void addSymbol(const char *name, const std::string &value);
  void addSymbol(const char *name, int value);
  void addSymbol(const char *name, const std::vector<std::string> &value);
  void addSymbol(const char *name, const std::vector<int> &value);
  void addSymbol(const char *name, std::vector<std::vector<int>> &value);
  void removeSymbol(const char *name);
  void declareSymbol(const char *name, const std::string &desc, bool isVector,
                     bool isMatrix = false, bool deprecated = false);
  void clearSymbols();
  void clearVariables() const;

  void takeVariable(const char *name, std::vector<int> &val) const;

  void getSymbols(std::vector<std::pair<std::wstring, size_t>> &symb) const;
  void getSymbolInfo(int ix, std::wstring &name, std::wstring &desc) const;

  // GUI dump methods — no-ops in cross-platform build
  // (legacy: void dumpVariables(gdioutput &, int, int) const)
  // (legacy: void dumpSymbols(gdioutput &, int, int) const)
};
