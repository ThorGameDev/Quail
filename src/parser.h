#ifndef PARSER
#define PARSER

#include "./AST.h"

std::unique_ptr<FunctionAST> ParseTopLevelExpr();
std::unique_ptr<PrototypeAST> ParseExtern();
std::unique_ptr<FunctionAST> ParseDefinition();

#endif
