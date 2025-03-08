#include "./CG_internal.h"
#include "./passes.h"
#include "../datatype.h"
#include "../parser.h"
#include "../AST.h"
#include "../logging.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
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
#include <vector>

namespace AST {

using namespace llvm;
using CG::Builder;
using CG::TheContext;

Function *PrototypeAST::codegen() {
    // Make the function type: double(double,double) etc.
    std::vector<Type*> TypeVector;
    for (int i = 0; i < Args.size(); i++){
        TypeVector.push_back(CG::getType(Args[i].second));
    }

    FunctionType *FT =
        FunctionType::get(CG::getType(ReturnType), TypeVector, false);

    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, CG::TheModule.get());

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++].first);

    return F;
}

// NEEDS SOME WORK!!!
Function *FunctionAST::codegen() {
    // Might have an error, details are in the tutorial
    // Transfer ownership of the protype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto &P = *Proto;
    CG::FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = CG::getFunction(P.getName());
    if (!TheFunction)
        return nullptr;

    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    CG::NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        // Create an alloca for this variable.
        AllocaInst *Alloca = CG::CreateEntryBlockAlloca(TheFunction, Arg.getName(), Arg.getType());

        // Store the initial value into the alloca.
        Builder->CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table.
        CG::NamedValues[std::string(Arg.getName())] = Alloca;
    }

    if (Value *RetVal = Body->codegen()) {
        //Finish off the function.
        if (P.getDataType() == type_void)
            Builder->CreateRetVoid();
        else
            Builder->CreateRet(RetVal);

        //Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);

        //Optimize the function
        CG::passes.TheFPM->run(*TheFunction, *CG::passes.TheFAM);

        return TheFunction;
    }

    // Error reading body, remove function
    TheFunction->eraseFromParent();
    return nullptr;
}

}
