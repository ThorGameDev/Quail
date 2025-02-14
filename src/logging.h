#include "./AST.h"
#include <llvm/IR/Value.h>
#include <string>

std::unique_ptr<ExprAST> LogError(std::string Str);
std::unique_ptr<PrototypeAST> LogErrorP(std::string Str);
llvm::Value *LogErrorV(std::string Str);
std::unique_ptr<ExprAST> LogErrorParse(std::string Str);
std::unique_ptr<PrototypeAST> LogErrorParseP(std::string Str);
std::unique_ptr<ExprAST> LogErrorCompile(std::string Str);
llvm::Value *LogErrorCompileV(std::string Str);
llvm::Value *LogCompilerBug(std::string Str);

void DebugLog(std::string Str);
