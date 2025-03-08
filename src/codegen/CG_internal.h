#ifndef CODEGEN_INTERNAL
#define CODEGEN_INTERNAL

namespace AST {
    class PrototypeAST; 
}
namespace llvm {
    class Module;
    class AllocaInst;
    class LLVMContext;
    class Function;
    class StringRef;
    class Type;
    namespace orc {
        class QuailJIT;
    }
}

#include <memory>
#include <map>
#include <string>
#include "../datatype.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Error.h>

namespace CG {

extern std::unique_ptr<llvm::Module> TheModule;
extern std::map<std::string, llvm::AllocaInst*> NamedValues;
extern std::unique_ptr<llvm::orc::QuailJIT> TheJIT;
extern std::map<std::string, std::unique_ptr<AST::PrototypeAST>> FunctionProtos;
extern llvm::ExitOnError ExitOnErr;
extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::IRBuilder<>> Builder;

llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* TheFunction, llvm::StringRef VarName, llvm::Type* dtype);
llvm::Function* getFunction(std::string Name);
llvm::Type* getType(DataType dtype);

}

#endif
