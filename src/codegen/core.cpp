#include "./CG_internal.h"
#include "../datatype.h"
#include "../AST.h"
#include "../logging.h"
#include "CG_internal.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Target/TargetMachine.h"
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace CG {

using namespace llvm;
using namespace llvm::orc;

AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, StringRef VarName, Type* dtype) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(dtype, nullptr,
                             VarName);
}

Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

Type *getType(DataType dtype) {
    if (dtype == type_double) {
        return Type::getDoubleTy(*TheContext);
    }
    else if (dtype == type_float) {
        return Type::getFloatTy(*TheContext);
    }
    else if (dtype == type_bool) {
        return Type::getInt1Ty(*TheContext);
    }
    else if (dtype == type_i8) {
        return Type::getInt8Ty(*TheContext);
    }
    else if (dtype == type_i16) {
        return Type::getInt16Ty(*TheContext);
    }
    else if (dtype == type_i32) {
        return Type::getInt32Ty(*TheContext);
    }
    else if (dtype == type_i64) {
        return Type::getInt64Ty(*TheContext);
    }
    else if (dtype == type_u8) {
        return Type::getInt8Ty(*TheContext);
    }
    else if (dtype == type_u16) {
        return Type::getInt16Ty(*TheContext);
    }
    else if (dtype == type_u32) {
        return Type::getInt32Ty(*TheContext);
    }
    else if (dtype == type_u64) {
        return Type::getInt64Ty(*TheContext);
    }
    else if (dtype == type_void) {
        return Type::getVoidTy(*TheContext);
    }
    LogErrorCompile("Failed to get LLVM type from type '" + dtypeToString(dtype) + "'");
    abort();
    return nullptr;
}

}
