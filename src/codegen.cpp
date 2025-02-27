#include "./codegen.h"
#include "./datatype.h"
#include "./lexer.h"
#include "./BinopsData.h"
#include "./AST.h"
#include "./parser.h"
#include "./logging.h"
#include "./optimizations.h"
#include "../include/QuailJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
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
#include "llvm/Support/TargetSelect.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst*> NamedValues;
static std::unique_ptr<QuailJIT> TheJIT;
static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static ExitOnError ExitOnErr;

static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction,
        StringRef VarName, Type* dtype) {
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

Value *DoubleExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}

/// FIX AND TURN INTO FLOATING POINT INSTEAD OF DOUBLE.
Value *FloatExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *I64ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(64, Val));
}

Value *I32ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *I16ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(16, Val));
}

Value *I8ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(8, Val));
}

Value *U64ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(64, Val));
}

Value *U32ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *U16ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(16, Val));
}

Value *U8ExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(8, Val));
}

Value *BoolExprAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
    // Look this variable up in the funcion
    AllocaInst *A = NamedValues[Name];
    if (!A)
        LogErrorCompileV("Unknown variable name '" + Name + "'.\n" + 
                "Name did not exist in NamedValues table");

    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

namespace BinOps {

Value *toBool(Value* input) {
    return Builder->CreateFCmpONE(input,ConstantFP::get(*TheContext, APFloat(0.0)), "tobool");
}
Value* toDouble(Value* input) {
    return Builder->CreateUIToFP(input, Type::getDoubleTy(*TheContext), "tofloat");
};

Value* expandDataType(Value* input, DataType target, DataType prior){
    if (prior == target){
        return input;
    }
    if (target == type_double){
        if(prior != type_float){
            if (isSigned(prior)){
                return Builder->CreateSIToFP(input, getType(target));
            }
            else {
                return Builder->CreateUIToFP(input, getType(target));
            }
        } 
        else {
            return Builder->CreateFPExt(input, getType(target));
        }
    }
    else if (target == type_float) {
        if (isSigned(prior)){
            return Builder->CreateSIToFP(input, getType(target));
        }
        else {
            return Builder->CreateUIToFP(input, getType(target));
        }
    }
    else {
        if (isSigned(prior)){
            return Builder->CreateSExt(input, getType(target));
        }
        else{
            return Builder->CreateZExt(input, getType(target));
        }
    }
};

std::pair<Value*, Value*> expandOperation(DataType LHS, DataType RHS, Value* L, Value* R){
    DataType retType = getExpandType(LHS, RHS);
    if (retType == type_UNDECIDED){
        LogCompilerBug("Datatype expansion of type '" + dtypeToString(LHS) + "' and '" + 
                dtypeToString(RHS) + "' results in compile-time dataloss");
    }

    Value* LExt = expandDataType(L, retType, LHS);
    Value* RExt = expandDataType(R, retType, RHS);
    return std::make_pair(LExt, RExt);
};

/// 0: Or
/// 1: Xor
/// 2: And
Value* LogicGate(DataType LHS, DataType RHS, Value* L, Value* R, int gate) {
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);

    if (gate == 0)
        return Builder->CreateOr(parts.first, parts.second, "ortmp");
    else if (gate == 1)
        return Builder->CreateXor(parts.first, parts.second, "xortmp");
    else if (gate == 2)
        return Builder->CreateAnd(parts.first, parts.second, "andtmp");

    return LogCompilerBug("Attempted to build non-existant logic gate with ID #" + std::to_string(gate));
};

Value* EqualityCheck(DataType LHS, DataType RHS, Value* L, Value* R, int Op) {
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);
    if (isFP(retType)){
        if (Op == '<')
            return Builder->CreateFCmpULT(parts.first, parts.second, "tlttmp");
        else if (Op == '>')
            return Builder->CreateFCmpUGT(parts.first, parts.second, "tgttmp");
        else if (Op == op_eq)
            return Builder->CreateFCmpUEQ(parts.first, parts.second, "teqtmp");
        else if (Op == op_geq)
            return Builder->CreateFCmpUGE(parts.first, parts.second, "tgetmp");
        else if (Op == op_leq)
            return Builder->CreateFCmpULE(parts.first, parts.second, "tletmp");
        else if (Op == op_neq)
            return Builder->CreateFCmpUNE(parts.first, parts.second, "tnetmp");
    }
    else if (isSigned(retType)){
        if (Op == '<')
            return Builder->CreateICmpSLT(parts.first, parts.second, "tlttmp");
        else if (Op == '>')
            return Builder->CreateICmpSGT(parts.first, parts.second, "tgttmp");
        else if (Op == op_eq)
            return Builder->CreateICmpEQ(parts.first, parts.second, "teqtmp");
        else if (Op == op_geq)
            return Builder->CreateICmpSGE(parts.first, parts.second, "tgetmp");
        else if (Op == op_leq)
            return Builder->CreateICmpSLE(parts.first, parts.second, "tletmp");
        else if (Op == op_neq)
            return Builder->CreateICmpNE(parts.first, parts.second, "tnetmp");
    }
    else {
        if (Op == '<')
            return Builder->CreateICmpULT(parts.first, parts.second, "tlttmp");
        else if (Op == '>')
            return Builder->CreateICmpUGT(parts.first, parts.second, "tgttmp");
        else if (Op == op_eq)
            return Builder->CreateICmpEQ(parts.first, parts.second, "teqtmp");
        else if (Op == op_geq)
            return Builder->CreateICmpUGE(parts.first, parts.second, "tgetmp");
        else if (Op == op_leq)
            return Builder->CreateICmpULE(parts.first, parts.second, "tletmp");
        else if (Op == op_neq)
            return Builder->CreateICmpNE(parts.first, parts.second, "tnetmp");
    }


    return LogCompilerBug("Attempted to compare equality with '" + tokop(Op) + "' operator.");
};

Value* Add(DataType LHS, DataType RHS, Value* L, Value* R){
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);

    if (isFP(retType)){
        return Builder->CreateFAdd(parts.first, parts.second, "addtmp");
    }
    else{
        return Builder->CreateAdd(parts.first, parts.second, "addtmp");
    }
};
Value* Sub(DataType LHS, DataType RHS, Value* L, Value* R){
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);

    if (isFP(retType)){
        return Builder->CreateFSub(parts.first, parts.second, "subtmp");
    }
    else{
        return Builder->CreateSub(parts.first, parts.second, "subtmp");
    }
};

Value* Mul(DataType LHS, DataType RHS, Value* L, Value* R){
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);

    if (isFP(retType)) {
        return Builder->CreateFMul(parts.first, parts.second, "multmp");
    }
    else{
        return Builder->CreateMul(parts.first, parts.second, "multmp");
    }
};
Value* Div(DataType LHS, DataType RHS, Value* L, Value* R){
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);

    if (isFP(retType)) {
        return Builder->CreateFDiv(parts.first, parts.second, "divtmp");
    }
    else{
        if (isSigned(retType)){
            return Builder->CreateSDiv(parts.first, parts.second, "divtmp");
        } else {
            return Builder->CreateUDiv(parts.first, parts.second, "divtmp");
        }
    }
};

Value* Neg(DataType dtype, Value* input){
    if (isFP(dtype)) {
        return Builder->CreateFNeg(input, "negtmp");
    }
    else {
        return Builder->CreateNeg(input, "negtmp");
    }
};
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
        Value *Variable = NamedValues[LHSE->getName()];
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
        return BinOps::Add(LT, RT, L, R);
    case '-':
        return BinOps::Sub(LT, RT, L, R);
    case '*':
        return BinOps::Mul(LT, RT, L, R);
    case '/':
        return BinOps::Div(LT, RT, L, R);
    case '<':
    case '>':
    case op_eq:
    case op_geq:
    case op_leq:
    case op_neq:
        return BinOps::EqualityCheck(LT, RT, L, R, Op);
    case '|':
        return BinOps::LogicGate(LT, RT, L, R, 1);
    case op_or:
        return BinOps::LogicGate(LT, RT, L, R, 0);
    case '&':
        return BinOps::LogicGate(LT, RT, L, R, 2);
    default:
        break;
    }
    // If it wasn't a builtin binary operator, it must be a user defined one. Emit
    // a call to it.
    Function *F = getFunction(std::string("operator") + tokop(Op));
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
        return BinOps::Neg(DT, OperandV);
    case '!':
        return Builder->CreateNot(OperandV, "not");
    default:
        break;
    }

    Function *F = getFunction(std::string("operator") + tokop(Opcode));
    if (!F)
        return LogCompilerBug("Unknown unary operator '" + tokop(Opcode) + "' after all checks completed");

    return Builder->CreateCall(F, OperandV, "unop");
}

Value *CallExprAST::codegen() {
    //Look up the name in the global module table.
    Function *CalleeF = getFunction(Callee);
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

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

static std::stack<BlockAST*> BlockStack;
Value *BlockAST::codegen() {
    std::string name = "block" + std::to_string(BlockStack.size());
    // Add self to block stack, so that content code can access it
    BlockStack.push(this);

    // Create blocks
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *CurrentBlock = BasicBlock::Create(*TheContext, name, TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, name + "end");

    // Allow the flow to enter current block
    Builder->CreateBr(CurrentBlock);

    // Create a return value at every return point
    // Create block and fill with lines
    Builder->SetInsertPoint(CurrentBlock);

    Value *RetVal = UndefValue::get(Type::getVoidTy(*TheContext));
    bool hasImmediateReturn = false;
    for (unsigned i = 0, e = Lines.size(); i != e; i++) {
        Value *Line = Lines[i]->codegen();
        if (!Line)
            return nullptr;
        if (Lines[i]->getReturns()) {
            RetVal = Line;
            hasImmediateReturn = true;
            break; // Do not generate unreachable code
        }
    }
    CurrentBlock = Builder->GetInsertBlock();
    // Pass return value to PHI node

    // Exit block
    TheFunction->insert(TheFunction->end(), AfterBB);
    Builder->CreateBr(AfterBB);
    Builder->SetInsertPoint(AfterBB);

    // Remove self from block stack
    BlockStack.pop();

    // Pop all local variables from scope.
    for (unsigned i = 0, e = VarNames.size(); i != e; i++)
        NamedValues[VarNames[i].first] = LocalVarAlloca[i];

    if (hasImmediateReturn || ReturnFromPoints.size() > 0){

        // Get the return type
        Type* retType = RetVal->getType();
        if (!hasImmediateReturn) {
            retType = ReturnFromPoints[0].second->getType();
            RetVal = Constant::getNullValue(ReturnFromPoints[0].second->getType());
            if (retType == Type::getVoidTy(*TheContext)){
                RetVal = UndefValue::get(Type::getVoidTy(*TheContext));
            }
        }

        // Create the PHI node to store return values
        PHINode *PN = Builder->CreatePHI(retType, ReturnFromPoints.size() + 1, "retval");

        // Create a return value at every return point
        for (int i = 0; i < ReturnFromPoints.size(); i++) {
            Builder->SetInsertPoint(ReturnFromPoints[i].first);
            PN->addIncoming(ReturnFromPoints[i].second, ReturnFromPoints[i].first);
            Builder->CreateBr(AfterBB);
        }
        Builder->SetInsertPoint(AfterBB);
        PN->addIncoming(RetVal, CurrentBlock);

        return PN;
    }
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

Value *IfExprAST::codegen() {
    if (Cond->getDatatype() != type_bool) {
        return LogErrorCompileV("If condition should be a boolean value! Got '" + dtypeToString(Cond->getDatatype()) + "' instead.");
    }

    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases. Insert the then block at the
    // end of the function.
    BasicBlock *ThenBB =
        BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    if(Else){
        Builder->CreateCondBr(CondV, ThenBB, ElseBB);
    } else {
        Builder->CreateCondBr(CondV, ThenBB, MergeBB);
    }

    // Emit then value.
    Builder->SetInsertPoint(ThenBB);
    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;

    // Codegen of the 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder->GetInsertBlock();

    // If "then" block does not have a semicolon, then if it is called, it should trigger a block return
    if (Then->getReturns() && BlockStack.size() > 0) {
        BlockStack.top()->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ThenBB, ThenV));
    } else {
        Builder->CreateBr(MergeBB);
    }

    if (Else) {
        // Emit else block, if an else statement exists
        TheFunction->insert(TheFunction->end(), ElseBB);
        Builder->SetInsertPoint(ElseBB);

        Value *ElseV = Else->codegen();
        if (!ElseV)
            return nullptr;

        // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
        ElseBB = Builder->GetInsertBlock();

        // If "else" block does not have a semicolon, then if it is called, it should trigger a block return
        if (Else->getReturns() && BlockStack.size() > 0) {
            BlockStack.top()->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ElseBB, ElseV));
        } else {
            Builder->CreateBr(MergeBB);
        }
    }
    
    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);

    return UndefValue::get(Type::getVoidTy(*TheContext));
}

Value *ForExprAST::codegen() {
    if (End->getDatatype() != type_bool) {
        return LogErrorCompileV("For loop condition should be bool type. Got '" + dtypeToString(End->getDatatype()) + "' instead");
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    //Create an alloca for the variable in the entry block.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName, getType(VarType));

    // Emit the start code first, without 'variable' in scope.
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    //Store the value into the alloca.
    Builder->CreateStore(StartVal, Alloca);

    BasicBlock *Preloop = Builder->GetInsertBlock();

    // Make the new basic block for the loop header, inserting after current block
    BasicBlock *LoopBB =
        BasicBlock::Create(*TheContext, "loop", TheFunction);

    // Start insertion in LoopBB.
    Builder->SetInsertPoint(LoopBB);

    // Within the loop, the variable is defined equal to the PHI node. If it
    // shadows an existing variable, we have to restore it, so save it now.
    AllocaInst *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // Emit the body of the loop. This, like any other expr, can change the
    // current BB. Note that we ignore the value computed by the body, but don't
    // allow an error.
    Value* BodyV = Body->codegen();
    if (!BodyV)
        return nullptr;

    // Emit the step value.
    Value *StepVal = Step->codegen();
    if (!StepVal)
        return nullptr;

    BasicBlock *CondBB =
        BasicBlock::Create(*TheContext, "conditionblock", TheFunction);
    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    // Compute the end condition
    Value *EndCond = End->codegen();
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

    //Restore the unshadowed variable.
    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);

    return UndefValue::get(Type::getVoidTy(*TheContext));
}

Value *VarExprAST::codegen() {
    std::vector<AllocaInst *> OldBindings;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Save the first variable, in case it is needed latter
    const std::string &VarName = VarNames[0].first;
    ExprAST *Init = VarNames[0].second.get();

    Value *FirstInitVal;
    if (Init) {
        FirstInitVal = Init->codegen();
        if (!FirstInitVal)
            return nullptr;
    } else {
        // If not specified, throw an error.
        return LogCompilerBug("Did not get an initial value from variable expression for '" + VarName + "'");
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName, FirstInitVal->getType());
    Builder->CreateStore(FirstInitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;

    // Save all the other variables
    // Register all variables and emit their initializer.
    for (unsigned i = 1, e = VarNames.size(); i != e; i++) {
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        /// Emit the initializer before adding the variable to scope, this prevents
        /// the initializer from referencing the variable itself, and permits stuff
        /// like this:
        /// var a = 1 in
        /// var a = a in... # refers to outer 'a'.
        Value *InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal){
                return nullptr;
            }
        } else {
            InitVal = FirstInitVal;
        }

        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName, InitVal->getType());
        Builder->CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding.
        NamedValues[VarName] = Alloca;
    }

    // Feed deepest level current local variables
    BlockStack.top()->LocalVarAlloca.insert(std::end(BlockStack.top()->LocalVarAlloca),
                                            std::begin(OldBindings), std::end(OldBindings));
    for (int i = VarNames.size() - 1; i >= 0; i--) {
        BlockStack.top()->VarNames.push_back(std::move(VarNames[i]));
    }

    // Return nothing
    return UndefValue::get(Type::getVoidTy(*TheContext));
}

Function *PrototypeAST::codegen() {
    // Make the function type: double(double,double) etc.
    std::vector<Type*> TypeVector;
    for (int i = 0; i < Args.size(); i++){
        TypeVector.push_back(getType(Args[i].second));
    }

    FunctionType *FT =
        FunctionType::get(getType(ReturnType), TypeVector, false);

    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

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
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName());
    if (!TheFunction)
        return nullptr;

    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        // Create an alloca for this variable.
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), Arg.getType());

        // Store the initial value into the alloca.
        Builder->CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table.
        NamedValues[std::string(Arg.getName())] = Alloca;
    }

    if (Value *RetVal = Body->codegen()) {
        //Finish off the function.
        Builder->CreateRet(RetVal);

        //Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);

        //Optimize the function
        TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // Error reading body, remove function
    TheFunction->eraseFromParent();
    return nullptr;
}

std::unique_ptr<Module> getModule(){
    return std::move(TheModule);
}

void InitializeCodegen(){
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    TheJIT = ExitOnErr(QuailJIT::Create());
}

void InitializeModuleAndManagers() {
    //Open a new context and module
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("QuailJIT", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    //Create a builder for the module
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    // Create new pass and analysis manager
    TheFPM = std::make_unique<FunctionPassManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
            /*DebugLogging*/ true);
    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    TheFPM = Optimize(std::move(TheFPM));

    // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void HandleDefinitionJit() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Parsed a function definition.\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            ExitOnErr(TheJIT->addModule(
                          ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModuleAndManagers();
        }
    } 
}

void HandleDefinitionFile() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Parsed a function definition.\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
        }
    } 
}

void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Parsed an extern\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    } 
}

void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        DataType dtype = FnAST->getDataType();
        if (FnAST->codegen()) {
            // Create a Resource Tracker to track JIT'd memory allocated to our
            // anonymous expression -- that way we can free it after executing.
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();

            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            InitializeModuleAndManagers();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
            // Get the symbol's address and cast it into the right type (takes no
            // arguments, returns a double) so we can call it as a native function.
            
            if (dtype == type_double){
                double (*Function)() = ExprSymbol.getAddress().toPtr<double (*)()>();
                fprintf(stderr, "Evaluated to %f\n", Function());
            } else if (dtype == type_bool){
                // This might be a temporary workaround, depending on if the -1 value of true is supposed to happen.
                // First, we assume that our bool is actually an integer.
                int8_t (*Function)() = ExprSymbol.getAddress().toPtr<int8_t (*)()>();
                // Next, we take a bitwise and of 1, essentially only checking for the 1 relevant bit.
                if (Function() & 1)
                    fprintf(stderr, "Evaluated to True\n");
                else
                    fprintf(stderr, "Evaluated to False\n");
                if ((Function() & 1) != Function())
                    fprintf(stderr, "Bad bool %i was created\n", Function());
                // It looks like, besides the bad display value, everything works properly with the bools being -1 or -2. 
                // The ! operation still flips the relevant bit, and addition still adds 1 or 0, as does the other operations.
                // The bug looks difficult to fix, so only fix if it causes real problems
            } else if (dtype == type_float){
                float (*Function)() = ExprSymbol.getAddress().toPtr<float (*)()>();
                fprintf(stderr, "Evaluated to %f\n", Function());
            } else if (dtype == type_i8){
                int8_t (*Function)() = ExprSymbol.getAddress().toPtr<int8_t (*)()>();
                fprintf(stderr, "Evaluated to %i\n", Function());
            } else if (dtype == type_i16){
                int16_t (*Function)() = ExprSymbol.getAddress().toPtr<int16_t (*)()>();
                fprintf(stderr, "Evaluated to %i\n", Function());
            } else if (dtype == type_i32){
                int32_t (*Function)() = ExprSymbol.getAddress().toPtr<int32_t (*)()>();
                fprintf(stderr, "Evaluated to %i\n", Function());
            } else if (dtype == type_i64){
                int64_t (*Function)() = ExprSymbol.getAddress().toPtr<int64_t (*)()>();
                fprintf(stderr, "Evaluated to %ld\n", Function());
            } else if (dtype == type_u8){
                uint8_t (*Function)() = ExprSymbol.getAddress().toPtr<uint8_t (*)()>();
                fprintf(stderr, "Evaluated to %u\n", Function());
            } else if (dtype == type_u16){
                uint16_t (*Function)() = ExprSymbol.getAddress().toPtr<uint16_t (*)()>();
                fprintf(stderr, "Evaluated to %u\n", Function());
            } else if (dtype == type_u32){
                uint32_t (*Function)() = ExprSymbol.getAddress().toPtr<uint32_t (*)()>();
                fprintf(stderr, "Evaluated to %u\n", Function());
            } else if (dtype == type_u64){
                uint64_t (*Function)() = ExprSymbol.getAddress().toPtr<uint64_t (*)()>();
                fprintf(stderr, "Evaluated to %lu\n", Function());
            } else if (dtype == type_void){
                void (*Function)() = ExprSymbol.getAddress().toPtr<void (*)()>();
                Function();
            }
            
            // Delete the anonymous expression module from the JIT
            ExitOnErr(RT->remove());
        }
    } 
}
