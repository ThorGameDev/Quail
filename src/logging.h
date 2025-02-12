#include "./AST.h"
#include <llvm/IR/Value.h>
#include <string>

std::unique_ptr<ExprAST> LogError(std::string Str);
std::unique_ptr<PrototypeAST> LogErrorP(std::string Str);
llvm::Value *LogErrorV(std::string Str);
