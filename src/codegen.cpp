#include "./codegen/CG_internal.h"
#include "./codegen/passes.h"
#include "./codegen.h"
#include "./datatype.h"
#include "./AST.h"
#include "./parser.h"
#include "./logging.h"
#include "./codegen/optimizations.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/APFloat.h"
#include "../include/QuailJIT.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/TargetSelect.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <utility>

namespace CG { 

using namespace llvm;
using namespace llvm::orc;

std::unique_ptr<Module> TheModule;
std::map<std::string, AllocaInst*> NamedValues;
std::unique_ptr<QuailJIT> TheJIT;
std::map<std::string, std::unique_ptr<AST::PrototypeAST>> FunctionProtos;
ExitOnError ExitOnErr;
std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<IRBuilder<>> Builder;

Passes passes;

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
    passes.TheFPM = std::make_unique<FunctionPassManager>();
    passes.TheLAM = std::make_unique<LoopAnalysisManager>();
    passes.TheFAM = std::make_unique<FunctionAnalysisManager>();
    passes.TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    passes.TheMAM = std::make_unique<ModuleAnalysisManager>();
    passes.ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    passes.TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
            /*DebugLogging*/ true);
    passes.TheSI->registerCallbacks(*passes.ThePIC, passes.TheMAM.get());

    Optimize();

    // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*passes.TheMAM);
    PB.registerFunctionAnalyses(*passes.TheFAM);
    PB.crossRegisterProxies(*passes.TheLAM, *passes.TheFAM, *passes.TheCGAM, *passes.TheMAM);
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

void CloseCodegen() {
    TheModule.reset(); 
    TheJIT.reset();
    TheContext.reset();
    Builder.reset();
}

}
