#include "./CG_internal.h"
#include "../datatype.h"
#include "../lexer.h"
#include "../BinopsData.h"
#include "../AST.h"
#include "../logging.h"
#include "./BinOps.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
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
#include <vector>

namespace AST {

using namespace llvm;
using namespace llvm::orc;
using CG::Builder;
using CG::TheContext;

Value *VariableExprAST::codegen() {
    // Look this variable up in the funcion
    AllocaInst *A = CG::NamedValues[Name];
    if (!A)
        LogErrorCompileV("Unknown variable name '" + Name + "'.\n" + 
                "Name did not exist in NamedValues table");

    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

Value *BinaryExprAST::codegen() {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (Op == '=') {
        // This assumes we're building without RTTI because LLVM builds that way by
        // default. If you build LLVM with RTTI this can be changed to a
        // dynamic_cast for automatic error checking.
        VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
        if (!LHSE)
            return LogErrorCompileV("destination of '=' must be a variable");

        //Codegen the RHS.
        Value *Val = RHS->codegen();
        if (!Val)
            return nullptr;

        // Look up the name.
        Value *Variable = CG::NamedValues[LHSE->getName()];
        if (!Variable)
            return LogErrorCompileV("Assignment to unknown variable '" + LHSE->getName() + "'");

        Builder->CreateStore(Val, Variable);
        return Val;
    }
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    DataType LT = LHS->getDatatype();
    DataType RT = RHS->getDatatype();
    if(!L || !R)
        return nullptr;

    switch (Op) {
    case '+':
        return CG::BinOps::Add(LT, RT, L, R);
    case '-':
        return CG::BinOps::Sub(LT, RT, L, R);
    case '*':
        return CG::BinOps::Mul(LT, RT, L, R);
    case '/':
        return CG::BinOps::Div(LT, RT, L, R);
    case '%':
        return CG::BinOps::Mod(LT, RT, L, R);
    case '<':
    case '>':
    case op_eq:
    case op_geq:
    case op_leq:
    case op_neq:
        return CG::BinOps::EqualityCheck(LT, RT, L, R, Op);
    case '|':
        return CG::BinOps::LogicGate(LT, RT, L, R, 1);
    case op_or:
        return CG::BinOps::LogicGate(LT, RT, L, R, 0);
    case '&':
        return CG::BinOps::LogicGate(LT, RT, L, R, 2);
    default:
        break;
    }
    // If it wasn't a builtin binary operator, it must be a user defined one. Emit
    // a call to it.
    Function *F = CG::getFunction(std::string("operator") + tokop(Op));
    if (!F)
        LogCompilerBug("binary operator not found! '" + std::string("operator") + tokop(Op) + "' does not exist");

    Value *Ops[2] = { L, R };
    return Builder->CreateCall(F, Ops, "binop");
}

Value *UnaryExprAST::codegen() {
    Value *OperandV = Operand->codegen();
    DataType DT = Operand->getDatatype();
    if (!OperandV)
        return nullptr;

    if (UnopProperties.count(Opcode) == 0){
        return LogCompilerBug("Unknown unary operator '" + tokop(Opcode) + "'");
    }
    if (UnopProperties[Opcode].count(DT) == 0){
        return LogCompilerBug("Can not perform unary operator '" + tokop(Opcode) + "' with type '" + dtypeToString(DT) + "'");
    }

    switch(Opcode) {
    case '-':
        return CG::BinOps::Neg(DT, OperandV);
    case '!':
        return Builder->CreateNot(OperandV, "not");
    default:
        break;
    }

    Function *F = CG::getFunction(std::string("operator") + tokop(Opcode));
    if (!F)
        return LogCompilerBug("Unknown unary operator '" + tokop(Opcode) + "' after all checks completed");

    return Builder->CreateCall(F, OperandV, "unop");
}

Value *CallExprAST::codegen() {
    //Look up the name in the global module table.
    Function *CalleeF = CG::getFunction(Callee);
    if (!CalleeF)
        return LogErrorCompileV("Unknown function refrenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogCompilerBug("Expected " + std::to_string(CalleeF->arg_size()) + "arguments, but got " +
                std::to_string(Args.size()) + "instead.");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; i++) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    if (getDatatype() == type_void)
        return Builder->CreateCall(CalleeF, ArgsV);
    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *WhileExprAST::codegen() {
    if (Condition->getDatatype() != type_bool) {
        return LogErrorCompileV("For loop condition should be bool type. Got '"
                + dtypeToString(Condition->getDatatype()) + "' instead");
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *Preloop = Builder->GetInsertBlock();

    // Make the new basic block for the loop header, inserting after current block
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

    // Start insertion in LoopBB.
    Builder->SetInsertPoint(LoopBB);

    // Emit the body of the loop. This, like any other expr, can change the
    // current BB. Note that we ignore the value computed by the body, but don't
    // allow an error.
    Value* BodyV = Body->codegen();
    if (!BodyV)
        return nullptr;

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "conditionblock", TheFunction);
    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    // Compute the end condition
    Value *EndCond = Condition->codegen();
    if (!EndCond)
        return nullptr;

    // Create the "after loop" block and insert it.
    BasicBlock *AfterBB =
        BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Allow acces into the loop, but make sure to enter the check branch.
    Builder->SetInsertPoint(Preloop);
    Builder->CreateBr(CondBB);

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    return UndefValue::get(Type::getVoidTy(*TheContext));
}

Value *LineAST::codegen() {
    Value *body = Body->codegen();
    if (returns == true) {
        return body;
    } else {
        return UndefValue::get(Type::getVoidTy(*TheContext));
    }
}

}
