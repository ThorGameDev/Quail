#ifndef LOGGING
#define LOGGING

#include <memory>
#include <string>
namespace llvm {
    class Value;
};
namespace AST {
    class ExprAST;
    class PrototypeAST;
}


std::unique_ptr<AST::ExprAST> LogError(std::string Str);
std::unique_ptr<AST::PrototypeAST> LogErrorP(std::string Str);
llvm::Value *LogErrorV(std::string Str);
std::unique_ptr<AST::ExprAST> LogErrorParse(std::string Str);
std::unique_ptr<AST::PrototypeAST> LogErrorParseP(std::string Str);
std::unique_ptr<AST::ExprAST> LogErrorCompile(std::string Str);
llvm::Value *LogErrorCompileV(std::string Str);
llvm::Value *LogCompilerBug(std::string Str);

void DebugLog(std::string Str);
void FileOutputError(std::string Str);

class CompileError : public std::exception {

};

#endif
