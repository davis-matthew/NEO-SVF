//
// Created by peiming on 7/26/19.
//

#ifndef SVF_ORIGIN_RACEDETECTORBASE_H
#define SVF_ORIGIN_RACEDETECTORBASE_H


#include "WPA/WPAPass.h"
#include "RaceDetectorBase/SHBGraph.h"

#include <iostream>

#include <llvm-c/Core.h> // for LLVMGetGlobalContext()
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
#include "RaceDetectorBase/InsensitiveLockSet.h"

using namespace llvm;
using namespace std;

class RaceDetectorBase {
private:
    using AccessSet = set<SHBNode *>;
    using ThreadAccessSetMap = map<Value *, AccessSet>;

    SHBGraph *shbGraph;
    Module *module;
    PointerAnalysis *PTA;
    InsensitiveLockSet *LS;
    ThreadAccessSetMap threadAccessSetMap;
private:
    void collectAccess();
    void intraThreadDFS(SHBNode *entry, Value *thread);

    void inputExistingLogs();
    vector<string> split(string s, string splitStr);

    void detectShared();

    bool checkNodes(SHBNode *n1, SHBNode *n2);

    void checkFiles();
    void checkMethods();
    void checkVariables();

    int output(bool sharedOutput,bool outputFiles, bool outputMethods,bool outputVariables);
public:
    int runOnModule(SVFModule module, bool sharedOutput,bool outputFiles, bool outputMethods,bool outputVariables);
};
#endif //SVF_ORIGIN_RACEDETECTORBASE_H
