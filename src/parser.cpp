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
#include <utility>
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
    else if (TokenDataType == type_float){
        auto Result = std::make_unique<FloatExprAST>((float)NumVal);
        getNextToken(); // consume the number
        return Result;
    } 
    else if (TokenDataType == type_i64){
        auto Result = std::make_unique<I64ExprAST>((int64_t)INumVal);
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
    else if (TokenDataType == type_u64){
        auto Result = std::make_unique<U64ExprAST>((uint64_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_u32){
        auto Result = std::make_unique<U32ExprAST>((uint32_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_u16){
        auto Result = std::make_unique<U16ExprAST>((uint16_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }
    else if (TokenDataType == type_u8){
        auto Result = std::make_unique<U8ExprAST>((uint8_t)INumVal);
        getNextToken(); // consume the number
        return Result;
    }

    return LogErrorParse("Invalid datatype: " + std::to_string(TokenDataType));
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
        return LogErrorParse("expected ')'. Got '" + tokop(CurTok) + "'");
    getNextToken(); //eat ).
    return V;
}

struct ParserBlockStackData {
    DataType blockDtype = type_UNDECIDED;
    std::map<std::string, DataType> outerVariables;
    std::vector<std::string> localVariables;
};
static std::vector<ParserBlockStackData*> ParseBlockStack;
static std::unique_ptr<BlockAST> ParseBlock() {
    if (CurTok != '{'){
        LogErrorParse("expected '{'. Got '" + tokop(CurTok) + "'");
        return nullptr;
    }
    getNextToken(); // Eat {

    std::vector<std::unique_ptr<LineAST>> lines;
    ParserBlockStackData data = ParserBlockStackData();
    ParseBlockStack.push_back(&data);
    while (CurTok != '}') {
        std::unique_ptr<LineAST> line = ParseLine();
        if(line->getReturns()) {
            if (data.blockDtype == type_UNDECIDED)
                data.blockDtype = line->getDatatype();
            else if (data.blockDtype != line->getDatatype()){
                LogErrorParse("Block can not have multiple return types. " +
                        dtypeToString(data.blockDtype) + " and " + 
                        dtypeToString(line->getDatatype()) + " are both returned");
                return nullptr;
            }
        }
        lines.push_back(std::move(line));
    }
    ParseBlockStack.pop_back();
    getNextToken(); // Eat '}'
    // Remove all local variables from scope
    for (int i = 0; i < data.localVariables.size(); i++){
        if(data.outerVariables.count(data.localVariables[i]) != 0)
            NamedValuesDatatype[data.localVariables[i]] = data.outerVariables[data.localVariables[i]];
        else
            NamedValuesDatatype.erase(data.localVariables[i]);
    }
    if (data.blockDtype == type_UNDECIDED)
        data.blockDtype = type_void;
    return std::make_unique<BlockAST>(std::move(lines), data.blockDtype);
}

static std::unique_ptr<ExprAST> ParseFleeExpr() {
    getNextToken(); // Eat flee
    uint8_t fleeAmmount = ParseBlockStack.size();
    if (CurTok == '('){
        getNextToken();
        if (CurTok != tok_dtype)
            LogErrorParse("expected integer constant within flee. Got '" + tokop(CurTok) + "'");
        if (!isInt(TokenDataType))
            LogErrorParse("expected integer constant within flee. Got '" + dtypeToString(TokenDataType) + "' type instead");
        if( INumVal > fleeAmmount || INumVal < 0){
            LogErrorParse("Flee distance should be within range (0 ~ " + std::to_string(fleeAmmount) + 
                    "). Got '" + std::to_string(INumVal) + "' type instead");
        }
        fleeAmmount = (uint8_t)INumVal;
        getNextToken();

        if (CurTok != ')')
            LogErrorParse("expected ')'. Got '" + tokop(CurTok) + "'");
        getNextToken();
    }
    if(CurTok == ';'){
        return std::make_unique<FleeAST>(nullptr, fleeAmmount, type_void);
    }

    std::unique_ptr<ExprAST> body = ParseExpression();

    /*
    if(!ParseBlockStack.empty()) {
        if (ParseBlockStack[fleeAmmount]->blockDtype == type_UNDECIDED)
            ParseBlockStack[fleeAmmount]->blockDtype = Then->getDatatype();
        else if (ParseBlockStack[fleeAmmount]->blockDtype != Then->getDatatype())
            return LogErrorParse("If statement's return type '" + dtypeToString(Then->getDatatype()) + "' " + 
                    "differs from the current block return type of '" + dtypeToString(ParseBlockStack.top()->blockDtype) + "'");
    }
    */
    return std::make_unique<FleeAST>(std::move(body), fleeAmmount, type_void);
}

/// identifierexpr
/// ::=identifier
/// ::=identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier.
    if (CurTok != '('){ // Simple variable ref.
        if (NamedValuesDatatype.count(IdName) == 0){
            return LogErrorParse("Variable '" + IdName + "' does not exist!");
        }
        return std::make_unique<VariableExprAST>(IdName, NamedValuesDatatype[IdName]);
    }

    // Call.
    getNextToken(); //eat (

    if (FunctionDataTypes.count(IdName) == 0){
        return LogErrorParse("Function '" + IdName + "' does not exist!");
    }
    std::vector<DataType> argDtypes = FunctionDataTypes[IdName].second;

    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        for (int i = 0; i <= argDtypes.size(); i++) {
            if (auto Arg = ParseExpression()){
                if (Arg->getDatatype() != argDtypes[i]){
                    return LogErrorParse("Function '" + IdName + "' contains a type mismatch.\n" + 
                            "Argument #" + std::to_string(i) + " requires type '" + dtypeToString(argDtypes[i]) + "'. " +
                            "Got '" + dtypeToString(Arg->getDatatype()) + "' instead");
                }
                Args.push_back(std::move(Arg));
            }
            else {
                return nullptr;
            }

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogErrorParse("Expecetd ')' or ',' in argument list. Got '" + tokop(CurTok) + "'");
            getNextToken();
        }
    }

    // Eat the ')'
    if (CurTok != ')'){ // Simple variable ref.
        return LogErrorParse("expected ')'. Got '" + tokop(CurTok) + "'. \n" + 
                    "Function '" + IdName + "' likely contains too many arguments.");

    }
    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args), FunctionDataTypes[IdName].first);
}

static std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken(); // eat the if.

    if (CurTok != '(')
        return LogErrorParse("expected '('. Got '" + tokop(CurTok) + "'");
    getNextToken(); // Eat the '('

    //condition.
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != ')')
        return LogErrorParse("expected ')'. Got '" + tokop(CurTok) + "'");
    getNextToken(); // Eat the ')'

    std::unique_ptr<LineAST> Then = ParseLine();
    if (!Then)
        return nullptr;

    if(Then->getReturns() && !ParseBlockStack.empty()) {
        if (ParseBlockStack[0]->blockDtype == type_UNDECIDED)
            ParseBlockStack[0]->blockDtype = Then->getDatatype();
        else if (ParseBlockStack[0]->blockDtype != Then->getDatatype())
            return LogErrorParse("If statement's return type '" + dtypeToString(Then->getDatatype()) + "' " + 
                    "differs from the current block return type of '" + dtypeToString(ParseBlockStack[0]->blockDtype) + "'");
    }

    if (CurTok == tok_else){
        getNextToken();
        std::unique_ptr<LineAST> Else = ParseLine();
        if (!Else)
            return nullptr;

        if(Else->getReturns() && !ParseBlockStack.empty()) {
            if (ParseBlockStack[0]->blockDtype == type_UNDECIDED)
                ParseBlockStack[0]->blockDtype = Else->getDatatype();
            else if (ParseBlockStack[0]->blockDtype != Else->getDatatype())
                return LogErrorParse("If statement's return type '" + dtypeToString(Else->getDatatype()) + "' " + 
                        "differs from the current block return type of '" + dtypeToString(ParseBlockStack[0]->blockDtype) + "'");
        }

        return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
    }
    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(nullptr));
}

// forexper ::= 'for' identifier '=' expr ',' exper (',' expr)? 'in' expression
static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken(); // eat the for.

    if (CurTok != '(')
        return LogErrorParse("expected '('. Got '" + tokop(CurTok) + "'");
    getNextToken(); // Eat the '('

    DataType indexDtype = type_UNDECIDED;
    if (CurTok == tok_dtype)
        indexDtype = TokenDataType;
    else
        return LogErrorParse("Expected datatype token. Instead got '" + tokop(CurTok) + "'");
    getNextToken(); // Eat the DataType

    if (CurTok != tok_identifier)
        return LogErrorParse("expected identifier after for. Got '" + tokop(CurTok) + "'");

    std::string IdName = IdentifierStr;
    getNextToken(); //eat identifier.

    DataType outerDtype = type_UNDECIDED;
    if (NamedValuesDatatype.count(IdName) != 0){
        outerDtype = NamedValuesDatatype[IdName];
    }
    NamedValuesDatatype[IdName] = indexDtype;

    if (CurTok != '=')
        return LogErrorParse("expected '='. Got '" + tokop(CurTok) + "'");
    getNextToken(); // eat '='.

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ';')
        return LogErrorParse("expected ';'. Got '" + tokop(CurTok) + "'");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;
    if (End->getDatatype() != type_bool){
        return LogErrorParse("For loop condition should be 'bool' rather than '" + dtypeToString(End->getDatatype()) + "'");
    }

    std::unique_ptr<ExprAST> Step;
    if (CurTok != ';') {
        return LogErrorParse("expected ';'. Got '" + tokop(CurTok) + "'");
    }
    getNextToken(); // Eat ;

    Step = ParseExpression();
    if (!Step)
        return nullptr;

    if (CurTok != ')')
        return LogErrorParse("expected ')'. Got '" + tokop(CurTok) + "'");
    getNextToken(); // Eat the ')'


    std::unique_ptr<ExprAST> Body = ParseExpression();
    if (!Body)
        return nullptr;

    if (CurTok == ';')
        getNextToken();
    else
        return LogErrorParse("expected ';'. Got '" + tokop(CurTok) + "'\n" + 
                "for loop statement must end with a ';', because return types are impossible.");

    // Restore old bindings to 'i' variable.
    if (outerDtype != type_UNDECIDED)
        NamedValuesDatatype[IdName] = outerDtype;
    else
        NamedValuesDatatype.erase(IdName);

    return std::make_unique<ForExprAST>(IdName, indexDtype, std::move(Start),
                        std::move(End), std::move(Step),std::move(Body));
}

/// varexpr :: 'var' identifier ('=' expression)?
///                 (',' identifier ('=' expression)?)* 'in' expression
static std::unique_ptr<ExprAST> ParseVarExpr() {
    if (ParseBlockStack.size() == 0) {
        return LogErrorParse("Variable must be contained in a block");
    }

    DataType dtype = type_UNDECIDED;
    if (CurTok == tok_dtype)
        dtype = TokenDataType;
    else
        return LogErrorParse("Expected datatype token. Instead got '" + tokop(CurTok) + "'");
    getNextToken(); // eat the var.

    std::vector<std::string> VarNames;
    std::vector<std::unique_ptr<ExprAST>> VarValues;

    // At least one variable name is required
    if (CurTok != tok_identifier)
        return LogErrorParse("expected identifier after '" + dtypeToString(dtype) + "' declaration");

    // Store each name
    while (true) {
        std::string Name = IdentifierStr;
        ParseBlockStack[0]->localVariables.push_back(Name);
        if (NamedValuesDatatype.count(Name) == 0){
            NamedValuesDatatype[Name] = dtype;
        }
        else {
            ParseBlockStack[0]->outerVariables[Name] = NamedValuesDatatype[Name];
        }
        VarNames.push_back(Name);
        getNextToken(); // eat identifier

        // if end of var list, exit loop.
        if (CurTok != ',') break;
        getNextToken(); // eat the ','.

        if (CurTok != tok_identifier)
            return LogErrorParse("expected identifier list to continue after '" + dtypeToString(dtype) + "' declaration due to ',' token.");
    }

    if (CurTok != '=') 
        return LogErrorParse("expected '=' after '" + dtypeToString(dtype) + "' declaration");
    getNextToken(); // eat the '='.

    // Store each value
    while (true) {
        std::unique_ptr<ExprAST> Init;
        Init = ParseExpression();
        if (!Init) return nullptr;

        if (Init->getDatatype() != dtype)
            return LogErrorParse("Variable of type '"+dtypeToString(Init->getDatatype())+"' is used as an initilizer within a '"
                    + dtypeToString(dtype) + "' variable expression.");
        
        VarValues.push_back(std::move(Init));

        // End of var list, exit loop.
        if (CurTok != ',') break;
        getNextToken(); // eat the ','.
    }

    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarPairs;
    if(VarNames.size() == VarValues.size()){
        for(int i = 0; i < VarValues.size(); i++){
            VarPairs.push_back(std::make_pair(VarNames[i], std::move(VarValues[i])));
        }
    }
    else if (VarValues.size() == 1){
        VarPairs.push_back(std::make_pair(VarNames[0], std::move(VarValues[0])));
        for(int i = 1; i < VarNames.size(); i++){
            VarPairs.push_back(std::make_pair(VarNames[i], nullptr));
        }
    }
    else{
        return LogErrorParse("'"+ dtypeToString(dtype) + "' declaration should have the number of names(" +
                std::to_string(VarNames.size()) + ") equal values(" + std::to_string(VarValues.size()) + ") or 1. ");
    }

    return std::make_unique<VarExprAST>(std::move(VarPairs), dtype);
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
        return LogErrorParse("Unknown token '" + tokop(CurTok) + "' when expecting an expression");
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
    case tok_flee:
        return ParseFleeExpr();
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
    if(UnopProperties.count(Opc) == 0){
        return LogErrorParse("Unknown unary operator '" + tokop(Opc) + "'");
    }

    getNextToken();
    if (auto Operand = ParseUnary()){
        DataType inputType = Operand->getDatatype();
        if (UnopProperties[Opc].count(inputType) == 0){
            return LogErrorParse("Can not perform unary operator '" + tokop(Opc) +
                    "' with type '" + dtypeToString(inputType) + "'");
        }

        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand), UnopProperties[Opc][inputType]);
    }
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
            return LogErrorParse("Can not perform '" + tokop(BinOp) + "' operation between '" +
                    dtypeToString(LHS->getDatatype()) + "' and '" + 
                    dtypeToString(RHS->getDatatype()) + "'.");
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
        return LogErrorParseP("No type specified in prototype");
    getNextToken(); // Eat datatype

    bool isOperator = false;
    int BinaryPrecedence = -1;
    const int defaultBinPrecedence = 30;
    std::string FnSufix;
    int OperatorName;
    switch (CurTok) {
    default:
        return LogErrorParseP("Expected function name in '" + tokop(ReturnType) + "' type prototype");
    case tok_identifier:
        FnName = IdentifierStr;
        isOperator = false;
        getNextToken();
        break;
    case tok_operator:
        getNextToken();
        if (CurTok < 0)
            return LogErrorParseP("Expected binary operator. Got '" + tokop(CurTok) + "' instead");
        FnName = "operator";
        FnSufix = (char)CurTok;
        isOperator = true;
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
            if (INumVal < 1 || INumVal > 1000)
                return LogErrorParseP("Invalid precedence '" + std::to_string(INumVal) + "': must be 1..1000");
            BinaryPrecedence = (unsigned)INumVal;
            getNextToken();
        }
        break;
    }


    if (CurTok != '(')
        return LogErrorParseP("Expected '(' in prototype. Got '" + tokop(CurTok) + "'");
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
            return LogErrorParseP("Expected name after variable datatype '"+dtypeToString(dtype)+"' declaration");
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
        return LogErrorParseP("Expected ')' in prototype. Got '" + tokop(CurTok) + "'");

    //success.
    getNextToken(); // eat ')'.

    // Verify right number of names for operator.
    if (isOperator && Arguments.size() != 1 && Arguments.size() != 2) {
        return LogErrorParseP("Expected 1 or 2 argument for operator. Got '" + std::to_string(Arguments.size()) + "'");
    }

    if (isOperator && Arguments.size() == 1 && BinaryPrecedence != -1) {
        return LogErrorParseP("Can not accept operator precidence for unary operator '" + tokop(OperatorName) + "'");
    }
    if (BinaryPrecedence == -1) {
        BinaryPrecedence = defaultBinPrecedence;
    }
    // If this is a Binop, install it.
    if(isOperator && Arguments.size() == 2){
        if (BinopProperties.count(OperatorName) == 0){
            BinopProperties[OperatorName] =
                {(int)BinaryPrecedence, std::map<std::pair<DataType, DataType>, DataType>()};
        }
        auto sig = std::make_pair(Arguments[0].second, Arguments[1].second);
        BinopProperties[OperatorName].CompatibilityChart[sig] = ReturnType;
    }
    // If this is a Unop, install it
    if(isOperator && Arguments.size() == 1){
        if (UnopProperties.count(OperatorName) == 0){
            UnopProperties[OperatorName] = std::map<DataType, DataType>();
        }
        UnopProperties[OperatorName][Arguments[0].second] = ReturnType;
    }

    FunctionDataTypes[FnName] = std::make_pair(ReturnType, std::move(argsig));

    return std::make_unique<PrototypeAST>(FnName, std::move(Arguments), ReturnType, isOperator, BinaryPrecedence);
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto Body = ParseBlock()){
        if (CurTok == ';' && Proto->getDataType() != type_void) {
            LogErrorParse("Non null function block can not have ';'");
            return nullptr;
        }
        if (CurTok == ';'){
            getNextToken(); // eat ;.
        }
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
    }

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

