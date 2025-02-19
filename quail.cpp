#include "./src/lexer.h"
#include "./src/codegen.h"
#include "./src/BinopsData.h"
#include "./src/logging.h"
#include <iostream>

/// top ::= definition | external | expression | ';'
static void MainLoop() {
    while (true) {
        try {
            std::cout << ">>> ";
            switch (CurTok) {
            case tok_eof:
                return;
            case '_': //ignore placeholder exec-char
                getNextToken();
                break;
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
        catch (CompileError ce){
            DebugLog("Error Recovered");
            clearTok();
            std::cout << ">>> ";
            getNextToken();
        }
    }
}

int main() {
    InitializeCodegen();
    InitializeBinopPrecedence();

    // Prime the first token.
    std::cout << ">>> ";
    getNextToken();

    // Make the module, which holds all the code
    InitializeModuleAndManagers();

    // Run the main "interpreter loop" now.
    MainLoop();
    return 0;
}
