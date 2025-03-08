#include "passes.h"
#include "optimizations.h"
#include <memory>
#include <llvm/IR/PassManager.h>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
using namespace llvm;

int optimLevel;
void SetLevel(int level){
    optimLevel = level;
}

void Optimize() {
    if (optimLevel == 1){
        // Add transform passes
        // Promote allocas to registers
        CG::passes.TheFPM->addPass(PromotePass());
    }
    if (optimLevel == 2){
        // Add transform passes
        // Promote allocas to registers
        CG::passes.TheFPM->addPass(PromotePass());
        // Do simple peephole optimizations and bit twiddling optimizations.
        CG::passes.TheFPM->addPass(InstCombinePass());
        // reassociate expressions.
        CG::passes.TheFPM->addPass(ReassociatePass());
        // Eliminate Common SubExpressions.
        CG::passes.TheFPM->addPass(GVNPass());
        // Simplify the control flow graph (deleting unreachable blocks etc.)
        CG::passes.TheFPM->addPass(SimplifyCFGPass());
    }
}
