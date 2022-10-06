//===- wpa.cpp -- Whole program analysis -------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // Whole Program Pointer Analysis
 //
 // Author: Yulei Sui,
 */

#include "WPA/WPAPass.h"
#include "RaceDetectorBase/RaceDetectorBase.h"
#include "Util/AnalysisUtil.h" //for source loc

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

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

using namespace llvm;
using namespace std;

static bool sharedOutput=false, outputFiles = true, outputMethods = true, outputVariables = false, debug = false;
static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

int config(){
    string line;
    ifstream myFile;
    vector<string> config;
    myFile.open("../../TSAConfig.txt");
    if (myFile.is_open()) {while (getline(myFile, line)) {config.push_back(line);} myFile.close();}
    for(std::vector<string>::iterator index = config.begin();index!=config.end();index++) {
        if(index->find("//")==string::npos &&*index!="") {
            if (*index == "-PotentiallySharedOutput" || *index == "-potentiallysharedoutput") { sharedOutput = true; }
            else if (*index == "-NoFileOutput" || *index == "-nofileoutput") { outputFiles = false; }
            else if (*index == "-NoMethodOutput" || *index == "-nomethodoutput") { outputMethods = false; }
            else if (*index == "-OutputVariables" || *index == "-outputvariables") { outputVariables = true; }
            else if (*index == "-Debug" || *index == "-debug") { debug = true; }
            else if (*index == "-Help" || *index == "-help") {
                cout << "---------------------------------------------------------------------------------\n";
                cout << "| -PotentiallySharedOutput\t\tOutputs data that is not determined to be local\t|\n";
                cout << "| -NoFileOutput\t\t\t\t\tPrevents output from containing full files\t\t|\n";
                cout << "| -NoMethodOutput\t\t\t\tPrevents output from containing full methods\t|\n";
                cout << "| -OutputVariables\t\t\t\tAdds individual variable output\t\t\t\t\t|\n";
                cout << "| -Debug\t\t\t\t\t\tEnables debugging console output\t\t\t\t|\n";
                cout << "| -Help\t\t\t\t\t\t\tReturns this information\t\t\t\t\t\t|\n";
                cout << "---------------------------------------------------------------------------------";
                return 0;
            }
            else {
                cout << "Error detected in flag " << *index << " please consult -help for more info.";
                return 1;
            }
        }
    }
    cout << "--Current Configuration Settings:--\n";
    cout << "\tOutput Style:\t\t"; if(sharedOutput){cout << "Potentially Shared";}else{cout << "Unshared";} cout << "\n";
    cout << "\tFile Checks:\t\t"; if(outputFiles){cout << "Enabled";}else{cout << "Disabled";} cout << "\n";
    cout << "\tMethod Checks:\t\t"; if(outputMethods){cout << "Enabled";}else{cout << "Disabled";} cout << "\n";
    cout << "\tVariable Checks:\t"; if(outputVariables){cout << "Enabled";}else{cout << "Disabled";} cout << "\n";
    cout << "\tDebug Mode:\t\t\t"; if(debug){cout << "Enabled";}else{cout << "Disabled";} cout<<"\n\n";
    return -1;
}

int main(int argc, char ** argv) {

    //Exit Codes:
    //  1 - Error in runtime configuration
    //  2 - Error generating blacklist


    //Configuration & Runtime Flags
        int arg_num = 0;
        char **arg_value = new char*[argc];
        std::vector<std::string> moduleNameVec;
        analysisUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
        cl::ParseCommandLineOptions(arg_num, arg_value,
                                    "Whole Program Points-to Analysis\n");

        int c = config(); if (c!=-1) { return c; } //-1 is no error code, 1 is error, 0 is help

    //Analysis
        SVFModule svfModule(moduleNameVec);
        auto detector = new RaceDetectorBase();
        return detector->runOnModule(svfModule,sharedOutput,outputFiles,outputMethods,outputVariables,debug);
}
