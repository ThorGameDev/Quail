#include "./src/lexer.h"
#include "./src/codegen.h"
#include "./src/BinopsData.h"
#include "./src/logging.h"
#include <iostream>

/// top ::= definition | external | expression | ';'
void JitLine() {
    resetLexer();
    std::cout << ">>> ";
    initBuffer();
    getNextToken();
    try {
        while (true) {
            switch (CurTok) {
            case tok_eof:
                std::cout << "\n";
                return;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
            }
        }
    }
    catch (CompileError ce){
        DebugLog("Error Recovered\n");
    }
}

void MainLoop(){
    while (true){
        JitLine();
    }
}

int main() {
    InitializeCodegen();
    InitializeBinopPrecedence();

    // Prime the first token.

    // Make the module, which holds all the code
    InitializeModuleAndManagers();

    // Run the main "interpreter loop" now.
    MainLoop();
    return 0;
}
