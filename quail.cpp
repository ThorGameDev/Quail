#include "./src/lexer.h"
#include "./src/codegen.h"
#include "./src/BinopsData.h"
#include "./src/logging.h"

/// top ::= definition | external | expression | ';'
static void MainLoop() {
    while (true) {
        try {
            fprintf(stderr, "\nready> ");
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
        }
    }
}

int main() {
    InitializeCodegen();
    InitializeBinopPrecedence();

    // Prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken();

    // Make the module, which holds all the code
    InitializeModuleAndManagers();

    // Run the main "interpreter loop" now.
    MainLoop();
    return 0;
}
