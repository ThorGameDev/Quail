#include <llvm/IR/PassManager.h>
#include <memory>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
using namespace llvm;

std::unique_ptr<FunctionPassManager> Optimize(std::unique_ptr<FunctionPassManager> target) {
    // Add transform passes
    // Promote allocas to registers
    target->addPass(PromotePass());
    // Do simple peephole optimizations and bit twiddling optimizations.
    target->addPass(InstCombinePass());
    // reassociate expressions.
    target->addPass(ReassociatePass());
    // Eliminate Common SubExpressions.
    target->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks etc.)
    target->addPass(SimplifyCFGPass());
    return std::move(target);
}
