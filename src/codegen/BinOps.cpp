#include "./CG_internal.h"
#include "./BinOps.h"
#include "../datatype.h"
#include "../lexer.h"
#include "../logging.h"

namespace CG {

using namespace llvm;

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
Value* Mod(DataType LHS, DataType RHS, Value* L, Value* R){
    std::pair<Value*, Value*> parts = expandOperation(LHS, RHS, L, R);
    DataType retType = getExpandType(LHS, RHS);

    if (isFP(retType)) {
        return Builder->CreateFRem(parts.first, parts.second, "modtmp");
    }
    else{
        if (isSigned(retType)){
            return Builder->CreateSRem(parts.first, parts.second, "modtmp");
        } else {
            return Builder->CreateURem(parts.first, parts.second, "modtmp");
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

}
