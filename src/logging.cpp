#include "./logging.h"
#include <iostream>
#include <string>

/// LogError* - These are little helper funcions for error handling.
std::unique_ptr<ExprAST> LogError(std::string Str) {
    std::cout << "Error: " << Str << "\n";
    return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(std::string Str) {
    LogError(Str);
    return nullptr;
}

llvm::Value *LogErrorV(std::string Str) {
    LogError(Str);
    return nullptr;
}
