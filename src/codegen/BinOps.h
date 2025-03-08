#ifndef CODEGEN_BINOPS
#define CODEGEN_BINOPS

#include "../datatype.h"
namespace llvm {
    class Value;
}

namespace CG {
namespace BinOps {

llvm::Value* LogicGate(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R, int gate);

llvm::Value* EqualityCheck(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R, int Op);

llvm::Value* Add(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R);
llvm::Value* Sub(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R);

llvm::Value* Mul(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R);
llvm::Value* Div(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R);
llvm::Value* Mod(DataType LHS, DataType RHS, llvm::Value* L, llvm::Value* R);

llvm::Value* Neg(DataType dtype, llvm::Value* input);

}
}

#endif
