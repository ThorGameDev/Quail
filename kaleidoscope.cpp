#include "./include/KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::orc;


//Lexer

// The lexer returns tokens [0-255] if it is an unknown character, otherwise
// one of these for known things. It returns tokens greater than 255 for
// multi-part operators
enum Token {
    // Operators CURRENTLY WRONG
    op_eq = 15677,
    op_or = 31868,
    op_neq = 15649,
    op_geq = 15678,
    op_leq = 15676,
    op_shl = 15420,
    op_shr = 15934,

    tok_eof = -1,

    //commands
    tok_def = -2,
    tok_extern = -3,

    //primary
    tok_identifier = -4,
    tok_number = -5,
    tok_true = -6,
    tok_false = -7,

    // control
    tok_if = -8,
    tok_else = -9,
    tok_for = -10,

    //operators
    tok_binary = -11,
    tok_unary = -12,

    // var definition
    tok_double = -13,
    tok_bool = -14,
};

static int optok(std::string op) {
    return (op[1] << 8) + op[0];
}
static std::string tokop(int op) {
    std::string ret;
    ret += (char)(op);
    char second = op >> 8;
    if (second != 0) {
        ret += second;
    }
    return ret;
}

static std::string IdentifierStr; //Filled in if tok_identifier
static double NumVal;             //Filled in if tok_number
static std::vector<int> longops;

// gettok - Return the next token from the standard input.
static int gettok() {
    static char LastChar = ' ';

    //Skip any white space
    while (isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "binary")
            return tok_binary;
        if (IdentifierStr == "unary")
            return tok_unary;
        if (IdentifierStr == "double")
            return tok_double;
        if (IdentifierStr == "bool")
            return tok_bool;
        if (IdentifierStr == "true")
            return tok_true;
        if (IdentifierStr == "false")
            return tok_false;

        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.') { //Fix 1.23.45.67
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#') {
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    // Check for end of file
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    char ThisChar = LastChar;
    LastChar = getchar();

    // But first, check to make sure it isnt a multipart operator
    int value = (LastChar << 8) + ThisChar;
    for (int val = 0; val < longops.size(); val++) {
        if (longops[val] == value) {
            LastChar = getchar();
            return value;
        }
    }

    return ThisChar;
}

//AST

enum DataType {
    type_ERROR = 999,
    type_double = 0,
    type_bool = 1,
};

/// ExprAST - Base class for all expression nodes
class ExprAST { //To add types other than doubles, this would have a type field
    DataType dtype;
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
    const DataType &getDatatype() const {
        return dtype;
    };
protected:
    ExprAST(DataType dtype): dtype(dtype) {};
};


class LineAST : public ExprAST {
    std::unique_ptr<ExprAST> Body;
    bool returns;

public:
    LineAST(std::unique_ptr<ExprAST> Body, bool returns)
        : Body(std::move(Body)), returns(returns), ExprAST(Body->getDatatype()) {}
    Value *codegen() override;
    const bool &getReturns() const {
        return returns;
    }
};

/// DoubleExprAST - Expression class for numeric literals like "1.0".
class DoubleExprAST : public ExprAST {
    double Val;

public:
    DoubleExprAST(double Val) : Val(Val), ExprAST(type_double) {}
    Value *codegen() override;
};

// BoolExprAST - Expression class for bools
class BoolExprAST : public ExprAST {
    bool Val;

public:
    BoolExprAST(bool Val) : Val(Val), ExprAST(type_bool) {}
    Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a"
/// USES TEMPORARY DATATYPE
class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(const std::string &Name) : Name(Name), ExprAST(type_double) {}
    Value *codegen() override;
    const std::string &getName() const {
        return Name;
    }
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    int Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS, DataType dtype)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)), ExprAST(dtype) {}
    Value *codegen() override;
};

/// UnaryExprAST - Expression class for a binary operator.
class UnaryExprAST : public ExprAST {
    int Opcode;
    std::unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(int Opcode, std::unique_ptr<ExprAST> Operand)
        : Opcode(Opcode), Operand(std::move(Operand)), ExprAST(Operand->getDatatype() ) {}
    Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
// USES TEMPORARY DTYPE
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee,
                std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)), ExprAST(type_double) {}
    Value *codegen() override;
};

class BlockAST : public ExprAST {
    std::vector<std::unique_ptr<LineAST>> Lines;

public:
    std::vector<AllocaInst *> LocalVarAlloca;
    std::vector<std::pair<BasicBlock*, Value*>> ReturnFromPoints;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    BlockAST(std::vector<std::unique_ptr<LineAST>> Lines, DataType dtype)
        : Lines(std::move(Lines)), ExprAST(dtype) {}
    Value *codegen() override;
};

// USES TEMPORARY DTYPE
class IfExprAST :
    public ExprAST {
    std::unique_ptr<ExprAST> Cond;
    std::unique_ptr<LineAST> Then, Else;

public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<LineAST> Then,
              std::unique_ptr<LineAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)),
          ExprAST(type_double) {}
    Value *codegen() override;
};

// USES TEMPORARY DTYPE
class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body)), ExprAST(type_double) {}

    Value *codegen() override;
};

// USES TEMPORARY DTYPE
class VarExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

public:
    VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames)
        : VarNames(std::move(VarNames)), ExprAST(type_double) {}

    Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
    bool IsOperator;
    unsigned Precedence; //Precedence if a binary op.
    int OperatorName;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args,
                 bool IsOperator = false, unsigned Prec = 0, int OperatorName = 0)
        : Name(Name), Args(std::move(Args)), IsOperator(IsOperator),
          Precedence(Prec), OperatorName(OperatorName) {}

    Function *codegen();
    const std::string &getName() const {
        return Name;
    }

    bool isUnaryOp() const {
        return IsOperator && Args.size() == 1;
    }
    bool isBinaryOp() const {
        return IsOperator && Args.size() == 2;
    }

    int getOperatorName() const {
        assert(isUnaryOp() || isBinaryOp());
        return OperatorName;
    }

    unsigned getBinaryPrecedence() const {
        return Precedence;
    }
};

// FunctionAST - This class represents a function definition itself
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
    Function *codegen();
};

// Parser


/// CurTok/getNextToken - Provide a simple token buffer. CurTok is the current
/// token the parser is looking at. getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}

/// LogError* - These are little helper funcions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<LineAST> ParseLine();

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseDoubleExpr() {
    auto Result = std::make_unique<DoubleExprAST>(NumVal);
    getNextToken(); // consume the number
    return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseBoolExpr() {
    auto Result = std::make_unique<BoolExprAST>(CurTok == tok_true);
    getNextToken(); // Consume the truth statement
    return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V)
        return nullptr;
    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); //eat ).
    return V;
}

/// identifierexpr
/// ::=identifier
/// ::=identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier.
    if (CurTok != '(') // Simple variable ref.
        return std::make_unique<VariableExprAST>(IdName);

    // Call.
    getNextToken(); //eat (
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expecetd ')' or ',' in argument list") ;
            getNextToken();
        }
    }

    // Eat the ')'
    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// USES PLACEHOLDER DTYPE
// these two global values are used exclusively inside of parse block, and parse if.
// Their purpose is so that any if statement contained within a block can effect the
// overall block's return type, or throw an error if the return type is different from
// expected.
static bool inBlock;
static DataType blockDtype;
static std::unique_ptr<ExprAST> ParseBlock() {
    getNextToken(); // Eat {
    std::vector<std::unique_ptr<LineAST>> lines;
    inBlock = true;
    blockDtype = type_ERROR;
    while (CurTok != '}') {
        std::unique_ptr<LineAST> line = ParseLine();
        if(line->getReturns()) {
            if (blockDtype == type_ERROR)
                blockDtype = line->getDatatype();
            if (blockDtype != line->getDatatype())
                return LogError("Block can not have multiple return types");
        }
        lines.push_back(std::move(line));
    }
    inBlock = false;
    getNextToken(); // Eat '}'
    if (blockDtype == type_ERROR)
        blockDtype = type_double;
    return std::make_unique<BlockAST>(std::move(lines), blockDtype);
}

static std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken(); // eat the if.

    if (CurTok != '(')
        return LogError("Expected '('");
    getNextToken(); // Eat the '('

    //condition.
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != ')')
        return LogError("Expected ')'");
    getNextToken(); // Eat the ')'

    std::unique_ptr<LineAST> Then = ParseLine();
    if (!Then)
        return nullptr;

    if(Then->getReturns() && inBlock) {
        if (blockDtype == type_ERROR)
            blockDtype = Then->getDatatype();
        else if (blockDtype != Then->getDatatype())
            return LogError("Block can not have multiple return types");
    }

    if (CurTok != tok_else)
        return LogError("Expected else");

    getNextToken();
    std::unique_ptr<LineAST> Else = ParseLine();
    if (!Else)
        return nullptr;

    if(Else->getReturns() && inBlock) {
        if (blockDtype == type_ERROR)
            blockDtype = Else->getDatatype();
        else if (blockDtype != Else->getDatatype())
            return LogError("Block can not have multiple return types");
    }

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

// forexper ::= 'for' identifier '=' expr ',' exper (',' expr)? 'in' expression
static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken(); // eat the for.

    if (CurTok != '(')
        return LogError("Expected '('");
    getNextToken(); // Eat the '('

    if (CurTok != tok_identifier)
        return LogError("expected identifier after for");

    std::string IdName = IdentifierStr;
    getNextToken(); //eat identifier.

    if (CurTok != '=')
        return LogError("expected = after for");
    getNextToken(); // eat '='.

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ';')
        return LogError("expected ';' after for start value");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // The step value is optional.
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ';') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }

    if (CurTok != ')')
        return LogError("Expected ')'");
    getNextToken(); // Eat the ')'

    std::unique_ptr<ExprAST> Body = ParseExpression();
    if (!Body)
        return nullptr;

    if (CurTok == ';')
        getNextToken();
    else
        return LogError("expected ';' at end of for loop");

    return std::make_unique<ForExprAST>(IdName, std::move(Start),
                                        std::move(End), std::move(Step),std::move(Body));
}

/// varexpr :: 'var' identifier ('=' expression)?
///                 (',' identifier ('=' expression)?)* 'in' expression
static std::unique_ptr<ExprAST> ParseVarExpr() {
    getNextToken(); // eat the var.

    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

    // At least one variable name is required
    if (CurTok != tok_identifier)
        return LogError("expected identifier after var");

    while (true) {
        std::string Name = IdentifierStr;
        getNextToken(); // eat identifier.

        // Read the optional initializer.
        std::unique_ptr<ExprAST> Init;
        if (CurTok == '=') {
            getNextToken(); // eat the '='.

            Init = ParseExpression();
            if (!Init) return nullptr;
        }

        VarNames.push_back(std::make_pair(Name, std::move(Init)));

        // End of var list, exit loop.
        if (CurTok != ',') break;
        getNextToken(); // eat the ','.

        if (CurTok != tok_identifier)
            return LogError("expected identifier list after var");
    }

    // Check and consume In omitted

    return std::make_unique<VarExprAST>(std::move(VarNames));
}

/// primary
///     ::= identifierexpr
///     ::= numberexpr
///     ::= parenexpr
///     ::= ifexpr
///     ::= forexpr
///     ::= varexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch(CurTok) {
    default:
        return LogError("Unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseDoubleExpr();
    case tok_true:
    case tok_false:
        return ParseBoolExpr();
    case '{':
        return ParseBlock();
    case '(':
        return ParseParenExpr();
    case tok_if:
        return ParseIfExpr();
    case tok_for:
        return ParseForExpr();
    case tok_double:
        return ParseVarExpr();
    }
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<int, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
    if (CurTok < 0)
        return -1;

    //Make sure it is a declared binop.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

/// unary
///     ::= primary
///     ::= '!' unary
static std::unique_ptr<ExprAST> ParseUnary() {
    // If the current token is not an operator, it must be a primary expr.
    if (CurTok < 0 || CurTok == '(' || CurTok == ',' || CurTok == '{')
        return ParsePrimary();

    // If this is a unary operator, read it.
    int Opc = CurTok;
    getNextToken();
    if (auto Operand = ParseUnary())
        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    return nullptr;
}

///binoprhs
///     ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
        std::unique_ptr<ExprAST> LHS) {
    // If this is a binop, find its precedence.
    while (true) {
        int TokPrec = GetTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec)
            return LHS;


        // Okay, we know this is a binop.
        int BinOp = CurTok;
        getNextToken(); // eat binop

        //Parse the unary expression after the binary operator.
        auto RHS = ParseUnary();
        if (!RHS)
            return nullptr;

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
            if(!RHS)
                return nullptr;
        }
        //Merge LHS/RHS.

        if(LHS->getDatatype() != RHS->getDatatype()) {
            return LogError("Both sides of the operation must be the same type.");
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                                              std::move(RHS), LHS->getDatatype());
    }
}


static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParseUnary();
    if (!LHS) {
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<LineAST> ParseLine() {
    bool returns = true;
    // Prevent double semicolon possibility
    if (CurTok == tok_def || CurTok == tok_for || CurTok == tok_if || CurTok == tok_extern) {
        returns = false;
    }
    auto body = ParseExpression();

    if (CurTok == ';') {
        getNextToken(); // Eat ;
        returns = false;
    }
    return std::make_unique<LineAST>(std::move(body), returns);
}

/// prototype
/// ::= id '(' id* ')'
/// ::= binary LETTER number? (id, id)
/// ::= unary LETTER (id)
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    std::string FnName;

    unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary.
    unsigned BinaryPrecedence = 30;
    std::string FnSufix;
    int OperatorName;
    switch (CurTok) {
    default:
        return LogErrorP("Expected function name in prototype");
    case tok_identifier:
        FnName = IdentifierStr;
        Kind = 0;
        getNextToken();
        break;
    case tok_unary:
        getNextToken();
        if (CurTok < 0)
            return LogErrorP("Expected unary operator");
        FnName = "unary";
        FnSufix = (char)CurTok;
        Kind = 1;
        getNextToken();
        if (isascii(CurTok) && CurTok != '(') {
            FnSufix += (char)CurTok;
            longops.push_back(optok(FnSufix));
            getNextToken();
        }
        FnName += FnSufix;
        OperatorName = optok(FnSufix);

        break;
    case tok_binary:
        getNextToken();
        if (CurTok < 0)
            return LogErrorP("Expected binary operator");
        FnName = "binary";
        FnSufix = (char)CurTok;
        Kind = 2;
        getNextToken();
        if (isascii(CurTok) && CurTok != '(') {
            FnSufix += (char)CurTok;
            longops.push_back(optok(FnSufix));
            getNextToken();
        }
        FnName += FnSufix;
        OperatorName = optok(FnSufix);


        // Read the precedence if present.
        if (CurTok == tok_number) {
            if (NumVal < 1 || NumVal > 100)
                return LogErrorP("Invalid precedence: must be 1..100");
            BinaryPrecedence = (unsigned)NumVal;
            getNextToken();
        }
        break;
    }

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    //success.
    getNextToken(); // eat ')'.

    // Verify right number of names for operator.
    if (Kind && ArgNames.size() != Kind)
        return LogErrorP("Invalid number of operands for operator");

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0, BinaryPrecedence, OperatorName);
}

/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto Body = ParseLine())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
    return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    std::unique_ptr<PrototypeAST> body = ParsePrototype();
    if (CurTok == ';')
        getNextToken();
    return std::move(body);
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseLine()) {
        // Make an anonymous proto.
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

// Code Generation


static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst*> NamedValues;
static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static ExitOnError ExitOnErr;

static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
        StringRef VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr,
                             VarName);
}

Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}

Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

Value *DoubleExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *BoolExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
    // Look this variable up in the funcion
    AllocaInst *A = NamedValues[Name];
    if (!A)
        LogErrorV("Unknown variable name");

    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

namespace BinOps {
Value *toBool(Value* input) {
    return Builder->CreateFCmpONE(input,ConstantFP::get(*TheContext, APFloat(0.0)), "tobool");
}
Value* toFloat(Value* input) {
    return Builder->CreateUIToFP(input, Type::getDoubleTy(*TheContext), "tofloat");
};

/// 0: Or
/// 1: Xor
/// 2: And
Value* LogicGate(DataType LHS, DataType RHS, Value* L, Value* R, int gate) {
    if (LHS != type_bool)
        L = toBool(L);
    Value* R_bool = R;
    if (RHS != type_bool)
        R_bool = toBool(R_bool);

    if (gate == 0)
        L = Builder->CreateOr(L, R_bool, "ortmp");
    else if (gate == 1)
        L = Builder->CreateXor(L, R_bool, "xortmp");
    else if (gate == 2)
        L = Builder->CreateAnd(L, R_bool, "andtmp");

    if (LHS == type_double)
        L = toFloat(L);

    return L;
};

Value* EqualityCheck(DataType LHS, DataType RHS, Value* L, Value* R, int Op) {
    if (LHS != type_double)
        L = toFloat(L);
    Value* R_float = R;
    if (RHS != type_double)
        R_float = toFloat(R);

    if (Op == '<')
        return Builder->CreateFCmpULT(L, R_float, "tlttmp");
    else if (Op == '>')
        return Builder->CreateFCmpUGT(L, R_float, "tgttmp");
    else if (Op == op_eq)
        return Builder->CreateFCmpUEQ(L, R_float, "teqtmp");
    else if (Op == op_geq)
        return Builder->CreateFCmpUGE(L, R_float, "tgetmp");
    else if (Op == op_leq)
        return Builder->CreateFCmpULE(L, R_float, "tletmp");
    else if (Op == op_neq)
        return Builder->CreateFCmpUNE(L, R_float, "tnetmp");


    return LogErrorV("Equality check did not exist");
}
}

Value *BinaryExprAST::codegen() {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (Op == '=') {
        // This assumes we're building without RTTI because LLVM builds that way by
        // default. If you build LLVM with RTTI this can be changed to a
        // dynamic_cast for automatic error checking.
        VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
        if (!LHSE)
            return LogErrorV("destination of '=' must be a variable");

        //Codegen the RHS.
        Value *Val = RHS->codegen();
        if (!Val)
            return nullptr;

        // Look up the name.
        Value *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
            return LogErrorV("Unknown variable name");

        Builder->CreateStore(Val, Variable);
        return Val;
    }
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    DataType LT = LHS->getDatatype();
    DataType RT = RHS->getDatatype();
    if(!L || !R)
        return nullptr;

    switch (Op) {
    case '+':
        return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder->CreateFSub(L, R, "subtmp");
    case '*':
        return Builder->CreateFMul(L, R, "multmp");
    case '/':
        return Builder->CreateFDiv(L, R, "divtmp");
    case '<':
    case '>':
    case op_eq:
    case op_geq:
    case op_leq:
    case op_neq:
        return BinOps::EqualityCheck(LT, RT, L, R, Op);
    case '|':
        return BinOps::LogicGate(LT, RT, L, R, 1);
    case op_or:
        return BinOps::LogicGate(LT, RT, L, R, 0);
    case '&':
        return BinOps::LogicGate(LT, RT, L, R, 2);
    default:
        break;
    }
    // If it wasn't a builtin binary operator, it must be a user defined one. Emit
    // a call to it.
    Function *F = getFunction(std::string("binary") + tokop(Op));
    assert(F && "binary operator not found!");

    Value *Ops[2] = { L, R };
    return Builder->CreateCall(F, Ops, "binop");
}

Value *UnaryExprAST::codegen() {
    Value *OperandV = Operand->codegen();
    if (!OperandV)
        return nullptr;

    switch(Opcode) {
    case '-':
        return Builder->CreateFNeg(OperandV, "negtmp");
        break;
    case '!':
        OperandV = Builder->CreateFCmpONE(
                       OperandV, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");
        OperandV = Builder->CreateNot(OperandV);
        return Builder->CreateUIToFP(OperandV, Type::getDoubleTy(*TheContext));
    default:
        break;
    }

    Function *F = getFunction(std::string("unary") + tokop(Opcode));
    if (!F)
        return LogErrorV("Unknown unary operator");

    return Builder->CreateCall(F, OperandV, "unop");
}

Value *CallExprAST::codegen() {
    //Look up the name in the global module table.
    Function *CalleeF = getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function refrenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; i++) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

static std::stack<BlockAST*> BlockStack;
Value *BlockAST::codegen() {
    std::string name = "block" + std::to_string(BlockStack.size());
    // Add self to block stack, so that content code can access it
    BlockStack.push(this);

    // Create blocks
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *CurrentBlock = BasicBlock::Create(*TheContext, name, TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, name + "end");

    // Allow the flow to enter current block
    Builder->CreateBr(CurrentBlock);

    // Create a return value at every return point
    // Create block and fill with lines
    Builder->SetInsertPoint(CurrentBlock);

    Value *RetVal = Constant::getNullValue(Type::getDoubleTy(*TheContext));
    bool hasImmediateReturn = false;
    for (unsigned i = 0, e = Lines.size(); i != e; i++) {
        Value *Line = Lines[i]->codegen();
        if (!Line)
            return nullptr;
        if (Lines[i]->getReturns()) {
            RetVal = Line;
            hasImmediateReturn = true;
            break; // Do not generate unreachable code
        }
    }
    CurrentBlock = Builder->GetInsertBlock();
    // Pass return value to PHI node

    // Exit block
    TheFunction->insert(TheFunction->end(), AfterBB);
    Builder->CreateBr(AfterBB);
    Builder->SetInsertPoint(AfterBB);

    // Get the return type
    Type* retType = RetVal->getType();
    if (!hasImmediateReturn && ReturnFromPoints.size() > 0) {
        retType = ReturnFromPoints[0].second->getType();
    }

    // Create the PHI node to store return values
    PHINode *PN = Builder->CreatePHI(retType, ReturnFromPoints.size(), "retval");

    // Create a return value at every return point
    for (int i = 0; i < ReturnFromPoints.size(); i++) {
        Builder->SetInsertPoint(ReturnFromPoints[i].first);
        PN->addIncoming(ReturnFromPoints[i].second, ReturnFromPoints[i].first);
        Builder->CreateBr(AfterBB);
    }
    Builder->SetInsertPoint(AfterBB);
    if (hasImmediateReturn)
        PN->addIncoming(RetVal, CurrentBlock);

    // Remove self from block stack
    BlockStack.pop();

    // Pop all local variables from scope.
    for (unsigned i = 0, e = VarNames.size(); i != e; i++)
        NamedValues[VarNames[i].first] = LocalVarAlloca[i];

    return PN;
}

Value *LineAST::codegen() {
    Value *body = Body->codegen();
    if (returns == true) {
        return body;
    } else {
        return Constant::getNullValue(Type::getDoubleTy(*TheContext));
    }
}

// USES TEMPORARY DTYPE
Value *IfExprAST::codegen() {
    if (Cond->getDatatype() != type_bool) {
        return LogErrorV("If condition should be a boolean value!");
    }
    Type* retType = Type::getDoubleTy(*TheContext);

    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases. Insert the then block at the
    // end of the function.
    BasicBlock *ThenBB =
        BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value.
    Builder->SetInsertPoint(ThenBB);
    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;

    // Codegen of the 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder->GetInsertBlock();

    // If "then" block does not have a semicolon, then if it is called, it should trigger a block return
    if (Then->getReturns() && BlockStack.size() > 0) {
        BlockStack.top()->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ThenBB, ThenV));
        retType = ThenV->getType();
    } else {
        Builder->CreateBr(MergeBB);
    }

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;

    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder->GetInsertBlock();

    // If "else" block does not have a semicolon, then if it is called, it should trigger a block return
    if (Else->getReturns() && BlockStack.size() > 0) {
        BlockStack.top()->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ElseBB, ElseV));
        retType = ElseV->getType();
    } else {
        Builder->CreateBr(MergeBB);
    }


    // Emit merge block.
    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
    PHINode *PN = Builder->CreatePHI(retType, 2, "iftmp");
    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value *ForExprAST::codegen() {
    if (End->getDatatype() != type_bool) {
        return LogErrorV("For loop condition should be bool type");
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    //Create an alloca for the variable in the entry block.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

    // Emit the start code first, without 'variable' in scope.
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    //Store the value into the alloca.
    Builder->CreateStore(StartVal, Alloca);

    // Make the new basic block for the loop header, inserting after current block
    BasicBlock *LoopBB =
        BasicBlock::Create(*TheContext, "loop", TheFunction);


    // Insert an explicit fall through from the current block to the LoopBB.
    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB.
    Builder->SetInsertPoint(LoopBB);

    // Within the loop, the variable is defined equal to the PHI node. If it
    // shadows an existing variable, we have to restore it, so save it now.
    AllocaInst *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // Emit the body of the loop. This, like any other expr, can change the
    // current BB. Note that we ignore the value computed by the body, but don't
    // allow an error.
    Value* BodyV = Body->codegen();
    if (!BodyV)
        return nullptr;


    // Emit the step value.
    Value *StepVal = nullptr;
    if (Step) {
        StepVal = Step->codegen();
        if(!StepVal)
            return nullptr;
    } else {
        // If not specified, use 1.0
        StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    }

    // Compute the end condition
    Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;

    // Reload, increment, and restore the alloca. This handles the case where
    // the body of the loop mutates the variable
    Value *CurVar =
        Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
    Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
    Builder->CreateStore(NextVar, Alloca);

    // Create the "after loop" block and insert it.
    BasicBlock *AfterBB =
        BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    //Restore the unshadowed variable.
    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);

    // for expr always returns 0.0.
    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value *VarExprAST::codegen() {
    if (BlockStack.size() == 0) {
        return LogErrorV("Variable must be contained in a block");
    }

    std::vector<AllocaInst *> OldBindings;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (unsigned i = 0, e = VarNames.size(); i != e; i++) {
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        /// Emit the initializer before adding the variable to scope, this prevents
        /// the initializer from refrenceing the variable itself, and permits stuff
        /// like this:
        /// var a = 1 in
        /// var a = a in... # refers to outer 'a'.
        Value *InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        } else {
            // If not specified, use 0.0
            InitVal = ConstantFP::get(*TheContext, APFloat(0.0));
        }

        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder->CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding.
        NamedValues[VarName] = Alloca;
    }

    // Feed deepest level current local variables
    BlockStack.top()->LocalVarAlloca.insert(std::end(BlockStack.top()->LocalVarAlloca),
                                            std::begin(OldBindings), std::end(OldBindings));
    for (int i = VarNames.size() - 1; i >= 0; i--) {
        BlockStack.top()->VarNames.push_back(std::move(VarNames[i]));
    }

    // Return nothing
    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Function *PrototypeAST::codegen() {
    // Make the function type: double(double,double) etc.
    std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext));

    FunctionType *FT =
        FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}

Function *FunctionAST::codegen() { // Might have an error, details are in the tutorial
    // Transfer ownership of the protype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName());
    if (!TheFunction)
        return nullptr;

    // If this is an operator, install it.
    if (P.isBinaryOp())
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        // Create an alloca for this variable.
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

        // Store the initial value into the alloca.
        Builder->CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table.
        NamedValues[std::string(Arg.getName())] = Alloca;
    }

    if (Value *RetVal = Body->codegen()) {
        //Finish off the function.
        Builder->CreateRet(RetVal);

        //Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);

        //Optimize the function
        TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // Error reading body, remove function
    TheFunction->eraseFromParent();
    return nullptr;
}

// Top level parsing

static void InitializeBinopPrecedence() {
    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence['='] = 2; // Assignment
    BinopPrecedence['|'] = 5; // Xor
    BinopPrecedence[optok("||")] = 5; // Or
    BinopPrecedence['&'] = 5; // And
    BinopPrecedence['>'] = 10; // Greater Than
    BinopPrecedence['<'] = 10; // Less Than
    BinopPrecedence[optok("==")] = 10; // Equal
    BinopPrecedence[optok("!=")] = 10; // Not Equal
    BinopPrecedence[optok(">=")] = 10; // Greater than or equal
    BinopPrecedence[optok("<=")] = 10; // Less than or equal
    BinopPrecedence['+'] = 20; // Add
    BinopPrecedence['-'] = 20; // Subtract
    BinopPrecedence['*'] = 40; // Multiply
    BinopPrecedence['/'] = 40; // Divide
    BinopPrecedence['^'] = 50; // Exponent
    //BinopPrecedence[optok("<<")] = 60; // Bitwise Shift
    //BinopPrecedence[optok(">>")] = 60; // Bitwise Shift

    longops.push_back(optok("||"));
    longops.push_back(optok("=="));
    longops.push_back(optok("!="));
    longops.push_back(optok(">="));
    longops.push_back(optok("<="));
    //longops.push_back(optok("<<"));
    //longops.push_back(optok(">>"));
}

static void InitializeModuleAndManagers() {
    //Open a new context and module
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("KaleidoscopeJIT", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    //Create a builder for the module
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    // Create new pass and analysis manager
    TheFPM = std::make_unique<FunctionPassManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
            /*DebugLogging*/ true);
    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    // Add transform passes
    // Promote allocas to registers
    TheFPM->addPass(PromotePass());
    // Do simple peephole optimizations and bit twiddling optimizations.
    TheFPM->addPass(InstCombinePass());
    // reassociate expressions.
    TheFPM->addPass(ReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks etc.)
    TheFPM->addPass(SimplifyCFGPass());

    // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Parsed a function definition.\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            ExitOnErr(TheJIT->addModule(
                          ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModuleAndManagers();
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Parsed an extern\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    } else {
        // Skip token for error recovery
        getNextToken();
    }
}

static void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        if (FnAST->codegen()) {
            // Create a Resource Tracker to track JIT'd memory allocated to our
            // anonymous expression -- that way we can free it after executing.
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();

            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            InitializeModuleAndManagers();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
            //assert(ExprSymbol && "Function not found");

            // Get the symbol's address and cast it into the right type (takes no
            // arguments, returns a double) so we can call it as a native function.

            double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
            bool (*TF)() = ExprSymbol.getAddress().toPtr<bool (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());
            fprintf(stderr, "Evaluated to %d\n", TF());

            // Delete the anonymous expression module from the JIT
            ExitOnErr(RT->remove());

        }
    } else {
        // Skip token for error recovery
        getNextToken();
    }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
    while (true) {
        fprintf(stderr, "\nready> ");
        switch (CurTok) {
        case tok_eof:
            return;
        case '_': //ignore placeholder exec-char
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            HandleTopLevelExpression();
            break;
        }
    }
}

// "Library" functions that can be "extern'd" from user code.

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
    fputc((char)X, stderr);
    return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
    fprintf(stderr, "%f\n", X);
    return 0;
}

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    InitializeBinopPrecedence();

    // Prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken();

    TheJIT = ExitOnErr(KaleidoscopeJIT::Create());

    // Make the module, which holds all the code
    InitializeModuleAndManagers();

    // Run the main "interpreter loop" now.
    MainLoop();
    return 0;
}
