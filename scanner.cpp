#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// SCANNER

enum Token {
  tok_eof = -1,
  tok_func = -2,
  tok_import = -3,
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal; // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  // Get to next non-space
  while (isspace(LastChar)) {
    LastChar = getchar();
  }

  // Handle identifiers
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar()))) {
      IdentifierStr += LastChar;
    }
    
    if (IdentifierStr == "func")
      return tok_func;
    else if (IdentifierStr == "import")
      return tok_import;
    return tok_identifier;
  }

  // Handle numbers
  if (isdigit(LastChar) || LastChar == '.') {
    std::string numberString;
    do {
      numberString += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');
    NumVal = strtod(numberString.c_str(), nullptr);
    return tok_number;
  }

  // Handle Comments
  if (LastChar == '#') {
    do {
      LastChar = getchar();
    } while (LastChar != '\n' && LastChar != '\r' && LastChar != EOF);
    if (LastChar != EOF)
      return gettok();
  }

  // End of file
  if (LastChar == EOF)
    return tok_eof;
  
  // Handle all other tokens: operators
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

// ABSTRACT SYNTAX TREE

class ExprAST {
  Token token;
  public:
    virtual ~ExprAST() = default;
};

class NumberAST: public ExprAST {
  double value;

  public:
    NumberAST(double value): value(value) {};
};

class BinOpAST: public ExprAST {
  char op_char;
  std::unique_ptr<ExprAST> lhs, rhs;
  public:
    BinOpAST(char op_char, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs): op_char(op_char), lhs(std::move(lhs)), rhs(std::move(rhs)) {};
};

class VariableAST: public ExprAST{
  std::string symbol;
  public:
    VariableAST(const std::string &symbol): symbol(symbol) {};
};

class FuncCallAST: public ExprAST {
  std::string callee;
  std::vector<std::unique_ptr<ExprAST>> args;

  public:
    FuncCallAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args): callee(callee), args(std::move(args)) {};
};

class FuncProtoAST: public ExprAST {
  std::vector<std::string> argNames;
  std::string name;
  public:
    FuncProtoAST(std::vector<std::string> argNames, const std::string &name): argNames(argNames), name(name) {};
};

class FunctionAST: public ExprAST {
  std::unique_ptr<FuncProtoAST> prototype;
  std::unique_ptr<ExprAST> expr;
  public:
    FunctionAST(std::unique_ptr<FuncProtoAST> prototype, std::unique_ptr<ExprAST> expr): prototype(std::move(prototype)), expr(std::move(expr)) {};
};

// PARSER

static int CurrToken;
static int GetNextToken() {
  return CurrToken = gettok();
};

static std::map<char, int> ORDER_OF_OPS; // What about when you want ops that are multiple characters?

static std::unique_ptr<ExprAST> RaiseError(const std::string &msg) {
  fprintf(stderr, "Error: %s\n", msg);
  return nullptr;
}

static std::unique_ptr<NumberAST> ParseNumber() {
  auto result = std::make_unique<NumberAST>(NumVal);
  GetNextToken();
  return result;
};

static std::unique_ptr<FuncProtoAST> ParseFuncProto() {
  GetNextToken(); // Eat 'func' keyword
  if (CurrToken != tok_identifier) {
    RaiseError("Expected identifier in function definition.");
    return;
  }

  std::string name = IdentifierStr;
  GetNextToken(); // Eat the identifier

  if (CurrToken != '(') {
    RaiseError("Expected '(' in function definition.");
    return;
  }
  GetNextToken();

  std::vector<std::string> args;
  do {
    if (CurrToken != tok_identifier) {
      RaiseError("Expected identifier in function definition.");
      return;
    }
    args.push_back(IdentifierStr);
  } while (GetNextToken() == ',');
  if (CurrToken != ')') {
    RaiseError("Expected ')' in function definition");
    return;
  }
  GetNextToken(); // Eat ')'

  return std::make_unique<FuncProtoAST>(FuncProtoAST(std::move(args), name));
}

static std::unique_ptr<FunctionAST> ParseFuncDef() {
  GetNextToken(); // Eat 'func' keyword.
  auto proto = ParseFuncProto();
  if (!proto)
    return nullptr;
  auto body = ParseExpr();
  if (!body)
    return nullptr;
  return std::make_unique<FunctionAST>(FunctionAST(std::move(proto), std::move(body)));
}

static std::unique_ptr<ExprAST> ParseParenExpr() {
    GetNextToken(); // Eat '('
    auto expr = ParseExpr();
    if (CurrToken != ')') {
      return RaiseError("Expected ')'.");
    }
    GetNextToken();
    return expr;
}

static std::unique_ptr<ExprAST> ParseIdentifier() {
  std::string name = IdentifierStr;
  GetNextToken(); // Eat identifier name

  // Variable
  if (CurrToken != '(')
    return std::make_unique<VariableAST>(VariableAST(name));
  
  // Function call
  std::vector<std::unique_ptr<ExprAST>> args;
  do {
    auto arg = ParseExpr();
    if (!arg)
      return nullptr;
    args.push_back(arg);
  } while (GetNextToken() == ',');
  if (CurrToken != ')') {
    RaiseError("Expected ')' in function call");
    return;
  }
  GetNextToken(); // Eat ')'

  return std::make_unique<FuncCallAST>(FuncCallAST(std::move(name), std::move(args)));
}

static std::unique_ptr<ExprAST> ParseExpr() {
  auto lhs = ParseLhs();
  if (!lhs)
    return nullptr;


  return ParseBinOpRhs(0, std::move(lhs));
};

static std::unique_ptr<ExprAST> ParseLhs() {
  switch (CurrToken) {
    case tok_identifier:
      return ParseIdentifier();
    case tok_number:
      return ParseNumber();
    case '(':
      return ParseParenExpr();
    default:
      return RaiseError("Invalid expression. Expected LHS.");
  }
}

static std::unique_ptr<ExprAST> ParseBinOpRhs(int ExprPrec, std::unique_ptr<ExprAST> lhs) {
  while (true) {
    int TokPrec = ORDER_OF_OPS[CurrToken]; // -1 if not in the map

    // If for example...
    if (TokPrec < ExprPrec)
      return lhs;

    // Save and eat the binary operation token
    int CurrOp = CurrToken;
    GetNextToken();

    auto rhs = ParseLhs();
    if (!rhs)
      return nullptr;
    
    int NextPrec = ORDER_OF_OPS[CurrToken];
    if (TokPrec < NextPrec) {
      rhs = ParseBinOpRhs(TokPrec + 1, std::move(rhs));
      if (!rhs)
        return nullptr;
    }

    lhs = std::make_unique<BinOpAST>(CurrOp, std::move(lhs), std::move(rhs));
  }
}

static std::unique_ptr<ExprAST> ParseTopLevelExpr() {
  auto expr = ParseExpr();
  if (!expr)
    return nullptr;
  
  auto proto = std::make_unique<FuncProtoAST>(FuncProtoAST(std::vector<std::string>(), "__anon_func__"));
  return std::make_unique<FunctionAST>(FunctionAST(std::move(proto), std::move(expr)));
}

static std::unique_ptr<ExprAST> ParseImport() {
  return RaiseError("Import not implemented.");
}

// ================================= MAIN LOOP ================================= //

static void HandleFuncDef() {
  if (ParseFuncDef) {
    // Handle function definition
  } else {
    // Try to recover
    GetNextToken();
  }
}

static void HandleImport() {
  if (ParseImport) {
    // Handle import
  } else {
    GetNextToken();
  }
}

static void HandleTopLevelExpr() {
  if (ParseTopLevelExpr()) {
    // Handle top level expression
  } else { 
    GetNextToken();
  }
}

static std::unique_ptr<ExprAST> TopLevelParse() {
    while (CurrToken != EOF) {
      switch (CurrToken) {
        case tok_func:
          HandleFuncDef();
          break;
        case tok_import:
          HandleImport();
          break;
        case ';': // Ignore semi-colon
          break;
        default:
          HandleTopLevelExpr();
          break;
      }
    }
};

int main() {
  // Set the binary operations precedence


  TopLevelParse();

  return 0;
}