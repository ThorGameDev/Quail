#ifndef AST_H
#define AST_H

namespace llvm {
    class Value;
    class AllocaInst;
    class BasicBlock;
    class Function;
}

#include "datatype.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AST { 

/// ExprAST - Base class for all expression nodes
class ExprAST { //To add types other than doubles, this would have a type field
    DataType dtype;
public:
    virtual ~ExprAST() = default;
    virtual llvm::Value *codegen() = 0;
    const DataType &getDatatype() const { return dtype; };
protected:
    ExprAST(DataType dtype): dtype(dtype) {};
};

class LineAST : public ExprAST {
    std::unique_ptr<ExprAST> Body;
    bool returns;

public:
    LineAST(std::unique_ptr<ExprAST> Body, bool returns)
        : Body(std::move(Body)), returns(returns), ExprAST(Body->getDatatype()) {}
    llvm::Value *codegen() override;
    const bool &getReturns() const {
        return returns;
    }
};

/// DoubleExprAST - Expression class for numeric literals like "1.0".
class DoubleExprAST : public ExprAST {
    double Val;

public:
    DoubleExprAST(double Val) : Val(Val), ExprAST(type_double) {}
    llvm::Value *codegen() override;
};

/// FloatExprAST - Expression class for numeric literals like "1.0".
class FloatExprAST : public ExprAST {
    float Val;

public:
    FloatExprAST(double Val) : Val(Val), ExprAST(type_float) {}
    llvm::Value *codegen() override;
};

/// I64ExprAST - Expression class for 32 bit integers
class I64ExprAST : public ExprAST {
    int64_t Val;

public:
    I64ExprAST(int64_t Val) : Val(Val), ExprAST(type_i64) {}
    llvm::Value *codegen() override;
};

/// I32ExprAST - Expression class for 32 bit integers
class I32ExprAST : public ExprAST {
    int32_t Val;

public:
    I32ExprAST(int32_t Val) : Val(Val), ExprAST(type_i32) {}
    llvm::Value *codegen() override;
};

/// I16ExprAST - Expression class for 16 bit integers
class I16ExprAST : public ExprAST {
    int16_t Val;

public:
    I16ExprAST(int16_t Val) : Val(Val), ExprAST(type_i16) {}
    llvm::Value *codegen() override;
};

/// I8ExprAST - Expression class for 8 bit integers
class I8ExprAST : public ExprAST {
    int8_t Val;

public:
    I8ExprAST(int8_t Val) : Val(Val), ExprAST(type_i8) {}
    llvm::Value *codegen() override;
};

/// U64ExprAST - Expression class for 32 bit unsigned integers
class U64ExprAST : public ExprAST {
    uint64_t Val;

public:
    U64ExprAST(uint64_t Val) : Val(Val), ExprAST(type_u64) {}
    llvm::Value *codegen() override;
};

/// U32ExprAST - Expression class for 32 bit unsigned integers
class U32ExprAST : public ExprAST {
    uint32_t Val;

public:
    U32ExprAST(uint32_t Val) : Val(Val), ExprAST(type_u32) {}
    llvm::Value *codegen() override;
};

/// U16ExprAST - Expression class for 16 bit unsigned integers
class U16ExprAST : public ExprAST {
    uint16_t Val;

public:
    U16ExprAST(uint16_t Val) : Val(Val), ExprAST(type_u16) {}
    llvm::Value *codegen() override;
};

/// U8ExprAST - Expression class for 8 bit unsigned integers
class U8ExprAST : public ExprAST {
    uint8_t Val;

public:
    U8ExprAST(uint8_t Val) : Val(Val), ExprAST(type_u8) {}
    llvm::Value *codegen() override;
};

/// BoolExprAST - Expression class for bools
class BoolExprAST : public ExprAST {
    bool Val;

public:
    BoolExprAST(bool Val) : Val(Val), ExprAST(type_bool) {}
    llvm::Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a"
class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(const std::string &Name, DataType dtype) : Name(Name), ExprAST(dtype) {}
    llvm::Value *codegen() override;
    const std::string &getName() const {
        return Name;
    }
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    int Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS, DataType dtype)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)), ExprAST(dtype) {}
    llvm::Value *codegen() override;
};

/// UnaryExprAST - Expression class for a binary operator.
class UnaryExprAST : public ExprAST {
    int Opcode;
    std::unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(int Opcode, std::unique_ptr<ExprAST> Operand, DataType dtype)
        : Opcode(Opcode), Operand(std::move(Operand)), ExprAST(dtype) {}
    llvm::Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee,
                std::vector<std::unique_ptr<ExprAST>> Args, DataType dtype)
        : Callee(Callee), Args(std::move(Args)), ExprAST(dtype) {}
    llvm::Value *codegen() override;
};

class BlockAST : public ExprAST {
    std::vector<std::unique_ptr<LineAST>> Lines;

public:
    std::vector<llvm::AllocaInst *> LocalVarAlloca;
    std::vector<std::pair<llvm::BasicBlock*, llvm::Value*>> ReturnFromPoints;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    bool fleeFrom = false;
    BlockAST(std::vector<std::unique_ptr<LineAST>> Lines, DataType dtype)
        : Lines(std::move(Lines)), ExprAST(dtype) {}
    llvm::Value *codegen() override;
};

class FleeAST : public ExprAST {
    std::unique_ptr<ExprAST> Body;
    int Depth;
public:
    FleeAST(std::unique_ptr<ExprAST> Body, int Depth) :
        Body(std::move(Body)), Depth(Depth), ExprAST(type_void) {}
    llvm::Value *codegen() override;
};

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond;
    std::unique_ptr<LineAST> Then, Else;

public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<LineAST> Then,
              std::unique_ptr<LineAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)),
          ExprAST(type_void) {}
    llvm::Value *codegen() override;
};

class ForExprAST : public ExprAST {
    std::string VarName;
    DataType VarType;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
    ForExprAST(const std::string &VarName, DataType VarType, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), VarType(VarType), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body)), ExprAST(type_void) {}

    llvm::Value *codegen() override;
};

class WhileExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Condition, Body;

public:
    WhileExprAST(std::unique_ptr<ExprAST> Condition, std::unique_ptr<ExprAST> Body)
        : Condition(std::move(Condition)), Body(std::move(Body)), ExprAST(type_void) {}

    llvm::Value *codegen() override;
};

class VarExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

public:
    VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames, DataType dtype)
        : VarNames(std::move(VarNames)), ExprAST(dtype) {}

    llvm::Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
    std::string Name;
    DataType ReturnType;
    std::vector<std::pair<std::string, DataType>> Args;
    bool IsOperator;
    unsigned Precedence; //Precedence if a binary op.

public:
    PrototypeAST(const std::string &Name, std::vector<std::pair<std::string, DataType>> Args,
                 DataType returnType, bool IsOperator = false, unsigned Prec = 0,
                 int OperatorName = 0)
        : Name(Name), Args(std::move(Args)), IsOperator(IsOperator),
          Precedence(Prec), ReturnType(returnType) {}

    llvm::Function *codegen();
    const std::string &getName() const {
        return Name;
    }

    bool isUnaryOp() const {
        return IsOperator && Args.size() == 1;
    }

    bool isBinaryOp() const {
        return IsOperator && Args.size() == 2;
    }

    unsigned getBinaryPrecedence() const {
        return Precedence;
    }

    DataType getDataType() const {
        return ReturnType;
    }

    std::pair<DataType, DataType> getBinopSig() const {
        return std::make_pair(Args[0].second, Args[1].second);
    }

    DataType getUnopSig() const {
        return Args[0].second;
    }
};

// FunctionAST - This class represents a function definition itself
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
    llvm::Function *codegen();

    DataType getDataType() const {
        return Proto->getDataType();
    }
};

}
#endif
