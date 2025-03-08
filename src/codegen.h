#ifndef CODEGEN
#define CODEGEN

namespace CG {

void InitializeCodegen();
void InitializeModuleAndManagers();
void HandleDefinitionJit();
void HandleDefinitionFile();
void HandleExtern();
void HandleTopLevelExpression();

}

#endif
