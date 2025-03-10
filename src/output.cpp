#include "./output.h"
#include "./logging.h"
#include "./codegen/CG_internal.h"

#include <filesystem>

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;

void SaveToObjectFile(std::string filename) { 
    auto TargetTriple = sys::getDefaultTargetTriple();

    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
        FileOutputError(Error);
    }

    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, Reloc::PIC_);

    CG::TheModule->setDataLayout(TargetMachine->createDataLayout());
    CG::TheModule->setTargetTriple(TargetTriple);

    std::error_code EC;
    raw_fd_ostream dest(filename, EC, sys::fs::OF_None);

    if (EC) {
        FileOutputError("Could not open file: " + EC.message());
    }

    legacy::PassManager pass;
    auto FileType = CodeGenFileType::ObjectFile;

    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        FileOutputError("TargetMachine can't emit a file of this type");
    }

    pass.run(*CG::TheModule);
    dest.flush();
    DebugLog(std::string("'") + filename + "' compiled succesfully");
}

void SaveToIRFile(std::string filename) { 
    std::error_code EC;
    raw_fd_ostream dest(filename, EC, sys::fs::OF_None);
    if (EC) {
        FileOutputError("Could not open file: " + EC.message());
    }
    CG::TheModule->print(dest, nullptr);
    dest.flush();
    DebugLog(std::string("'") + filename + "' compiled succesfully");
}

std::string getFileExtension(std::string filePath){
    std::filesystem::path Path(filePath);
    return Path.extension().string();
}

void SaveToFile(std::string filename){
    std::string extension = getFileExtension(filename);

    if (extension == ".o"){
        SaveToObjectFile(filename);
    }
    if (extension == ".ll"){
        SaveToIRFile(filename);
    }
}
