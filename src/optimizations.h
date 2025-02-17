#ifndef OPTIMIZATIONS
#define OPTIMIZATIONS

#include <llvm/IR/PassManager.h>

std::unique_ptr<llvm::FunctionPassManager> Optimize(std::unique_ptr<llvm::FunctionPassManager> target);

#endif
