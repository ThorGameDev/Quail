#include "CG_internal.h"
#include "../AST.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>

namespace AST {

using namespace llvm;
using CG::TheContext;

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

}
