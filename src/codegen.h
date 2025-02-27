#ifndef CODEGEN
#define CODEGEN

#include <llvm/IR/Module.h>
#include <memory>
void InitializeCodegen();
void InitializeModuleAndManagers();
void HandleDefinitionJit();
void HandleDefinitionFile();
void HandleExtern();
void HandleTopLevelExpression();

std::unique_ptr<llvm::Module> getModule();

#endif
