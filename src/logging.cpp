#include "./logging.h"
#include "./lexer.h"
#include "./AST.h"
#include <iostream>
#include <string>

using namespace AST;

std::string strLexPos() {
    location pos = getLexPos(); 
    return "Line: " + std::to_string(pos.line) + " Col: " + std::to_string(pos.col);
}

/// LogError* - These are little helper funcions for error handling.
std::unique_ptr<ExprAST> LogError(std::string Str) {
    std::cout << "Error: " << Str << "\n" << strLexPos() << "\n";
    throw CompileError();
    return nullptr;
}

std::unique_ptr<ExprAST> LogErrorParse(std::string Str) {
    std::cout << "Syntax Error: " << Str << "\n" << strLexPos() << "\n";
    throw CompileError();
    return nullptr;
}

std::unique_ptr<ExprAST> LogErrorCompile(std::string Str) {
    std::cout << "Compile Error: " << Str << "\n";
    throw CompileError();
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(std::string Str) {
    LogError(Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorParseP(std::string Str) {
    LogErrorParse(Str);
    return nullptr;
}

llvm::Value *LogErrorV(std::string Str) {
    LogError(Str);
    return nullptr;
}

llvm::Value *LogErrorCompileV(std::string Str) {
    LogErrorCompile(Str);
    return nullptr;
}
llvm::Value *LogCompilerBug(std::string Str) {
    std::cout << "Compiler Bug: " << Str << "\n\n";
    std::cout << "This should never happen.\n";
    std::cout << "Please submit a bugreport on github.\n";
    std::cout << "The link is: https://github.com/ThorGameDev/Quail/issues\n";
    std::cout << "If this is a fork, please inform the relevant maintainers.\n";
    abort();
    return nullptr;
}

void DebugLog(std::string Str){
     std::cerr << Str << std::endl;
}

void FileOutputError(std::string Str){
    std::cout << "File Output Error: " << Str << "\n";
    throw CompileError();
}

