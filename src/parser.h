#ifndef PARSER
#define PARSER

#include <memory>
namespace AST {
    class FunctionAST;
    class PrototypeAST;
}

std::unique_ptr<AST::FunctionAST> ParseTopLevelExpr();
std::unique_ptr<AST::PrototypeAST> ParseExtern();
std::unique_ptr<AST::FunctionAST> ParseDefinition();

#endif
