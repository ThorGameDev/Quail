#include "./BinopsData.h"
#include "./parser.h"
#include "./datatype.h"
#include "./logging.h"
#include "./lexer.h"
#include "./AST.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>


/// CurTok/getNextToken - Provide a simple token buffer. CurTok is the current
/// token the parser is looking at. getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static std::map<std::string, DataType> NamedValuesDatatype;
static std::map<std::string, std::pair<DataType, std::vector<DataType>>> FunctionDataTypes;


static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<LineAST> ParseLine();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
    if (TokenDataType == type_double){
        auto Result = std::make_unique<DoubleExprAST>(NumVal);
        getNextToken(); // consume the number
        return Result;
    } 
    if (TokenDataType == type_float){
        auto Result = std::make_unique<FloatExprAST>((float)NumVal);
        getNextToken(); // consume the number
        return Result;
    } 
    else if (TokenDataType == type_i64){
        auto Result = std::make_unique<I64ExprAST>(INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_i32){
        auto Result = std::make_unique<I32ExprAST>((int32_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_i16){
        auto Result = std::make_unique<I16ExprAST>((int16_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_i8){
        auto Result = std::make_unique<I8ExprAST>((int8_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }

    return LogError("Invalid datatype");
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

// USES PLACEHOLDER DTYPE
struct ParserBlockStackData {
    DataType blockDtype = type_UNDECIDED;
    std::map<std::string, DataType> outerVariables;
    std::vector<std::string> localVariables;
};
static std::stack<ParserBlockStackData*> ParseBlockStack;
static std::unique_ptr<ExprAST> ParseBlock() {
    getNextToken(); // Eat {
    std::vector<std::unique_ptr<LineAST>> lines;
    ParserBlockStackData data = ParserBlockStackData();
    ParseBlockStack.push(&data);
    while (CurTok != '}') {
        std::unique_ptr<LineAST> line = ParseLine();
        if(line->getReturns()) {
            if (data.blockDtype == type_UNDECIDED)
                data.blockDtype = line->getDatatype();
            else if (data.blockDtype != line->getDatatype())
                return LogError("Block can not have multiple return types");
        }
        lines.push_back(std::move(line));
    }
    ParseBlockStack.pop();
    getNextToken(); // Eat '}'
    // Remove all local variables from scope
    for (int i = 0; i < data.localVariables.size(); i++){
        if(data.outerVariables.count(data.localVariables[i]) != 0)
            NamedValuesDatatype[data.localVariables[i]] = data.outerVariables[data.localVariables[i]];
        else
            NamedValuesDatatype.erase(data.localVariables[i]);
    }
    if (data.blockDtype == type_UNDECIDED)
        data.blockDtype = type_double;
    return std::make_unique<BlockAST>(std::move(lines), data.blockDtype);
}

/// identifierexpr
/// ::=identifier
/// ::=identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier.
    if (CurTok != '('){ // Simple variable ref.
        if (NamedValuesDatatype.count(IdName) == 0){
            return LogError("Variable does not exist!");
        }
        return std::make_unique<VariableExprAST>(IdName, NamedValuesDatatype[IdName]);
    }

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

    if (FunctionDataTypes.count(IdName) == 0){
        return LogError("Function does not exist!");
    }
    std::vector<DataType> argsig;
    for (int i = 0; i < Args.size(); i++){
        argsig.push_back(Args[i]->getDatatype());
    }
    if (FunctionDataTypes[IdName].second != argsig){
        return LogError("Incorrect argument data types for function");
    }

    return std::make_unique<CallExprAST>(IdName, std::move(Args), FunctionDataTypes[IdName].first);
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

    if(Then->getReturns() && !ParseBlockStack.empty()) {
        if (ParseBlockStack.top()->blockDtype == type_UNDECIDED)
            ParseBlockStack.top()->blockDtype = Then->getDatatype();
        else if (ParseBlockStack.top()->blockDtype != Then->getDatatype())
            return LogError("Block can not have multiple return types");
    }

    if (CurTok != tok_else)
        return LogError("Expected else");

    getNextToken();
    std::unique_ptr<LineAST> Else = ParseLine();
    if (!Else)
        return nullptr;

    if(Else->getReturns() && !ParseBlockStack.empty()) {
        if (ParseBlockStack.top()->blockDtype == type_UNDECIDED)
            ParseBlockStack.top()->blockDtype = Else->getDatatype();
        else if (ParseBlockStack.top()->blockDtype != Else->getDatatype())
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
    if (ParseBlockStack.size() == 0) {
        return LogError("Variable must be contained in a block");
    }

    DataType dtype = type_UNDECIDED;
    if (CurTok == tok_dtype)
        dtype = TokenDataType;
    else
        return LogError("Invalid datatype passed to 'ParseVarExpr()'");
    getNextToken(); // eat the var.

    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

    // At least one variable name is required
    if (CurTok != tok_identifier)
        return LogError("expected identifier after var");

    while (true) {
        std::string Name = IdentifierStr;
        ParseBlockStack.top()->localVariables.push_back(Name);
        if (NamedValuesDatatype.count(Name) == 0){
            NamedValuesDatatype[Name] = dtype;
        }
        else {
            ParseBlockStack.top()->outerVariables[Name] = NamedValuesDatatype[Name];
        }
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

    return std::make_unique<VarExprAST>(std::move(VarNames), dtype);
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
        return ParseNumberExpr();
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
    case tok_dtype:
        return ParseVarExpr();
    }
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
    if (CurTok < 0)
        return -1;

    //Make sure it is a declared binop.
    if (BinopProperties.find(CurTok) == BinopProperties.end()) return -1;
    return BinopProperties[CurTok].Precedence;
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

        std::pair<DataType, DataType> OperationTyping = std::make_pair(LHS->getDatatype(), RHS->getDatatype());

        if(BinopProperties[BinOp].CompatibilityChart.count(OperationTyping) == 0) {
            return LogError("Can not perform operation between those types");
        }
        DataType returnType = BinopProperties[BinOp].CompatibilityChart[OperationTyping];

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS), returnType);
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
    DataType ReturnType;
    
    if(CurTok == tok_dtype)
        ReturnType = TokenDataType;
    else 
        return LogErrorP("No type specified in prototype");
    getNextToken(); // Eat datatype

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
            if (INumVal < 1 || INumVal > 100)
                return LogErrorP("Invalid precedence: must be 1..100");
            BinaryPrecedence = (unsigned)INumVal;
            getNextToken();
        }
        break;
    }


    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");
    getNextToken(); // Eat '('

    std::vector<std::pair<std::string, DataType>> Arguments;
    std::vector<DataType> argsig;
    while (true){
        DataType dtype;
        if (CurTok == tok_dtype)
            dtype = TokenDataType;
        else
            break;
        getNextToken(); // Eat datatype

        if (CurTok != tok_identifier){
            return LogErrorP("Expected name after variable datatype");
        }
        Arguments.push_back(std::make_pair(IdentifierStr, dtype));
        argsig.push_back(dtype);
        NamedValuesDatatype[IdentifierStr] = dtype;
        getNextToken(); // Eat name

        if (CurTok != ',')
            break;
        getNextToken();
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    //success.
    getNextToken(); // eat ')'.

    // Verify right number of names for operator.
    if (Kind && Arguments.size() != Kind)
        return LogErrorP("Invalid number of operands for operator");

    FunctionDataTypes[FnName] = std::make_pair(ReturnType, std::move(argsig));

    return std::make_unique<PrototypeAST>(FnName, std::move(Arguments), ReturnType, Kind != 0, BinaryPrecedence, OperatorName);
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto Body = ParseLine())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
    return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    std::unique_ptr<PrototypeAST> body = ParsePrototype();
    if (CurTok == ';')
        getNextToken();
    return std::move(body);
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseLine()) {
        // Make an anonymous proto.
        //FunctionDataTypes["__anon_expr"] = std::make_pair(E->getDatatype(), std::vector<DataType>());
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::pair<std::string, DataType>>(), E->getDatatype());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

