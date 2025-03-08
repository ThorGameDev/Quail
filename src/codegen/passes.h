#ifndef CODEGEN_PASS_STORAGE
#define CODEGEN_PASS_STORAGE

#include <llvm/IR/PassManager.h>
#include "llvm/Passes/PassBuilder.h"

namespace llvm {
    class StandardInstrumentations;
}

namespace CG {

struct Passes{
    std::unique_ptr<llvm::FunctionPassManager> TheFPM;
    std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
    std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
    std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
    std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
    std::unique_ptr<llvm::StandardInstrumentations> TheSI;
};

extern Passes passes;

}

#endif
