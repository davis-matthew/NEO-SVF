//
// Created by peiming on 7/26/19.
//

#ifndef SVF_ORIGIN_INSENSITIVELOCKSET_H
#define SVF_ORIGIN_INSENSITIVELOCKSET_H


#include "WPA/WPAPass.h"

#include <llvm/IR/Instruction.h>
#include <llvm/Support/CommandLine.h>	// for cl
#include <llvm/Support/FileSystem.h>	// for sys::fs::F_None
#include <llvm/Bitcode/BitcodeWriterPass.h>  // for bitcode write
#include <llvm/IR/LegacyPassManager.h>		// pass manager
#include <llvm/Support/Signals.h>	// singal for command line
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Support/ToolOutputFile.h> // for tool output file
#include <llvm/Support/PrettyStackTrace.h> // for pass list
#include <llvm/IR/LLVMContext.h>		// for llvm LLVMContext
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriterPass.h>		// for createBitcodeWriterPass

using namespace std;
using namespace llvm;

class InsensitiveLockSet {
private:
    using LockSet = set<Value *>;
    using StmtLockSetMap = map<const Instruction *, LockSet *>;
    using Func2CallSites =  map<Function *, set<Instruction *>>;

    StmtLockSetMap *inLockSetMap;
    StmtLockSetMap *outLockSetMap;
    Func2CallSites *func2CallSitesMap;
    Func2CallSites *func2ReturnMap;

    PointerAnalysis *PTA;
public:
    // TODO: better place into different file
    static bool isThreadCreate(Instruction *inst);
    static bool isThreadJoin(Instruction *inst);
    static bool isMutexLock(Instruction *inst);
    static bool isMutexUnLock(Instruction *inst);
    static bool isRead(Instruction *);
    static bool isWrite(Instruction *);
    static bool isReadORWrite(Instruction *);
    static bool isExtFunction(Function *);

    bool protectedByCommonLock(const Instruction *i1, const Instruction *i2);

    InsensitiveLockSet(PointerAnalysis *PTA) : PTA(PTA) {}
public:
    void analyze(Module *module);
};


#endif //SVF_ORIGIN_INSENSITIVELOCKSET_H
