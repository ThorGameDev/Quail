#include "../datatype.h"
#include "../AST.h"
#include "../logging.h"
#include "CG_internal.h"

namespace AST {

using namespace llvm;
using CG::Builder;

static std::vector<BlockAST*> BlockStack;
static int BS_index = -1;
Value *BlockAST::codegen() {
    std::string name = "block" + std::to_string(BlockStack.size());
    // Add self to block stack, so that content code can access it
    BlockStack.push_back(this);
    BS_index += 1;

    // Create blocks
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *CurrentBlock = BasicBlock::Create(*CG::TheContext, name, TheFunction);

    // Allow the flow to enter current block
    Builder->CreateBr(CurrentBlock);

    // Create a return value at every return point
    // Create block and fill with lines
    Builder->SetInsertPoint(CurrentBlock);

    Value *RetVal = UndefValue::get(Type::getVoidTy(*CG::TheContext));
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
        if (fleeFrom)
            break;
    }
    // Code gen may have changed the current block
    CurrentBlock = Builder->GetInsertBlock();

    // Exit the block

    // Remove self from block stack
    BlockStack.pop_back();
    BS_index -= 1;

    // Pop all local variables from scope.
    for (unsigned i = 0, e = VarNames.size(); i != e; i++)
        CG::NamedValues[VarNames[i].first] = LocalVarAlloca[i];

    // Get the return type
    Type* retType = RetVal->getType();
    if (retType != CG::getType(getDatatype())) {
        retType = ReturnFromPoints[0].second->getType();
    }

    // Create the end block
    BasicBlock *AfterBB = BasicBlock::Create(*CG::TheContext, name + "end", TheFunction);
    if (!fleeFrom) {
        Builder->CreateBr(AfterBB);
    }
    Builder->SetInsertPoint(AfterBB);

    // Allow other methods of entering AfterBB
    if (ReturnFromPoints.size() > 0) {
        // Create the PHI node to store return values
        PHINode *PN = Builder->CreatePHI(retType, ReturnFromPoints.size() + hasImmediateReturn, "retval");

        // Create a return value at every return point
        for (int i = 0; i < ReturnFromPoints.size(); i++) {
            Builder->SetInsertPoint(ReturnFromPoints[i].first);
            PN->addIncoming(ReturnFromPoints[i].second, ReturnFromPoints[i].first);
            Builder->CreateBr(AfterBB);
        }
        if (hasImmediateReturn)
            PN->addIncoming(RetVal, CurrentBlock);
        Builder->SetInsertPoint(AfterBB);

        RetVal = PN;
    }

    return RetVal;
}

Value *FleeAST::codegen() {
    BlockStack[BS_index]->fleeFrom = true;
    if(Body != nullptr){
        Value *body = Body->codegen();
        if (!body)
            return nullptr;

        BasicBlock *block = Builder->GetInsertBlock();

        BlockStack[Depth]->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(block, body));
    }
    else{
        Value* body = UndefValue::get(Type::getVoidTy(*CG::TheContext));
        BasicBlock *block = Builder->GetInsertBlock();

        BlockStack[Depth]->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(block, body));
    }
    return UndefValue::get(Type::getVoidTy(*CG::TheContext));
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
        BasicBlock::Create(*CG::TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*CG::TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*CG::TheContext, "ifcont");

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
    bool fleeFromThen = false;
    if (Then->getReturns() && BlockStack.size() > 0) {
        BlockStack[BS_index]->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ThenBB, ThenV));
    } else if (!BlockStack[BS_index]->fleeFrom) {
        Builder->CreateBr(MergeBB);
    } else {
        fleeFromThen = true;
        BlockStack[BS_index]->fleeFrom = false;
    }

    bool fleeFromElse = false;
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
            BlockStack[BS_index]->ReturnFromPoints.push_back(std::pair<BasicBlock*, Value*>(ElseBB, ElseV));
        } else if (!BlockStack[BS_index]->fleeFrom) {
            Builder->CreateBr(MergeBB);
        } else {
            BlockStack[BS_index]->fleeFrom = false;
            fleeFromElse = true;
        }
    }
    
    if (fleeFromElse && fleeFromThen) {
        BlockStack[BS_index]->fleeFrom = true; 
    }
    else {
        TheFunction->insert(TheFunction->end(), MergeBB);
        Builder->SetInsertPoint(MergeBB);
    }

    return UndefValue::get(Type::getVoidTy(*CG::TheContext));
}

Value *ForExprAST::codegen() {
    if (End->getDatatype() != type_bool) {
        return LogErrorCompileV("For loop condition should be bool type. Got '" + dtypeToString(End->getDatatype()) + "' instead");
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    //Create an alloca for the variable in the entry block.
    AllocaInst *Alloca = CG::CreateEntryBlockAlloca(TheFunction, VarName, CG::getType(VarType));

    // Emit the start code first, without 'variable' in scope.
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    //Store the value into the alloca.
    Builder->CreateStore(StartVal, Alloca);

    BasicBlock *Preloop = Builder->GetInsertBlock();

    // Make the new basic block for the loop header, inserting after current block
    BasicBlock *LoopBB =
        BasicBlock::Create(*CG::TheContext, "loop", TheFunction);

    // Start insertion in LoopBB.
    Builder->SetInsertPoint(LoopBB);

    // Within the loop, the variable is defined equal to the PHI node. If it
    // shadows an existing variable, we have to restore it, so save it now.
    AllocaInst *OldVal = CG::NamedValues[VarName];
    CG::NamedValues[VarName] = Alloca;

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
        BasicBlock::Create(*CG::TheContext, "conditionblock", TheFunction);
    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    // Compute the end condition
    Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;

    // Create the "after loop" block and insert it.
    BasicBlock *AfterBB =
        BasicBlock::Create(*CG::TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Allow acces into the loop, but make sure to enter the check branch.
    Builder->SetInsertPoint(Preloop);
    Builder->CreateBr(CondBB);

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    //Restore the unshadowed variable.
    if (OldVal)
        CG::NamedValues[VarName] = OldVal;
    else
        CG::NamedValues.erase(VarName);

    return UndefValue::get(Type::getVoidTy(*CG::TheContext));
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

    AllocaInst *Alloca = CG::CreateEntryBlockAlloca(TheFunction, VarName, FirstInitVal->getType());
    Builder->CreateStore(FirstInitVal, Alloca);

    OldBindings.push_back(CG::NamedValues[VarName]);
    CG::NamedValues[VarName] = Alloca;

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

        AllocaInst *Alloca = CG::CreateEntryBlockAlloca(TheFunction, VarName, InitVal->getType());
        Builder->CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(CG::NamedValues[VarName]);

        // Remember this binding.
        CG::NamedValues[VarName] = Alloca;
    }

    // Feed deepest level current local variables
    BlockStack[BS_index]->LocalVarAlloca.insert(std::end(BlockStack[BS_index]->LocalVarAlloca),
                                            std::begin(OldBindings), std::end(OldBindings));
    for (int i = VarNames.size() - 1; i >= 0; i--) {
        BlockStack[BS_index]->VarNames.push_back(std::move(VarNames[i]));
    }

    // Return nothing
    return UndefValue::get(Type::getVoidTy(*CG::TheContext));
}

}
