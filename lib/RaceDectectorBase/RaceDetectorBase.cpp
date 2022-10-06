//
// Created by peiming on 7/26/19.
// Adapted to TSA by matthew on 8/4/19
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include "RaceDetectorBase/RaceDetectorBase.h"
#include "RaceDetectorBase/InsensitiveLockSet.h"
#include "RaceDetectorBase/SHBGraph.h"
#include "Util/GraphUtil.h"
#include "WPA/Andersen.h"

//#include "MTA/TCT.h"
//#include "MTA/LockAnalysis.h"

// using nullptr for MAIN_THREAD
#define MAIN_THREAD nullptr

vector<string> unsharedVarLocs;
vector<string> sharedVarLocs;
vector<string> unsharedStrings;
vector<string> sharedStrings;
bool debug;

vector<string> RaceDetectorBase::split(string s, string splitStr){
    size_t pos = 0;
    size_t subtracted =0;
    vector<string> temp;
    string scopy = s;
    while ((pos = scopy.find(splitStr)) != std::string::npos) {
        pos+=splitStr.length();
        temp.push_back(scopy.substr(0, pos));
        subtracted+=pos;
        scopy.erase(0, pos + splitStr.length());
    }
    temp.push_back(s.substr(subtracted)); //Gets the remaining str
    return temp;
}

int RaceDetectorBase::runOnModule(SVFModule svfModule,bool sharedOutput,bool outputFiles, bool outputMethods,bool outputVariables,bool debug) {
    this->debug = debug;
    this->inputExistingLogs();

    cout<<"Running pointer analysis\n";this->PTA = AndersenWaveDiff::createAndersenWaveDiff(module); //HOTCODE
    this->PTA->getPAG()->dump("PAG");
    this->module = svfModule.getModule(0);

    // Basic LockSet algorithm
    this->LS = new InsensitiveLockSet(this->PTA);
    LS->analyze(this->module);

    // Construct SHBGRAPH
    this->shbGraph = SHBGraph::buildFromModule(this->module, PTA);
    this->shbGraph->dumpDotGraph();

    this->collectAccess();
    this->detectShared(); //HOTCODE

    return this->output(sharedOutput,outputFiles,outputMethods,outputVariables);

}

void RaceDetectorBase::inputExistingLogs(){
    cout << "Reading in pre-existing logged TSA data\n";
    string line;
    ifstream myFile;
    myFile.open("../../UnsharedOutput.txt"); //Adds pre-existing unshared logged data
    if (myFile.is_open()) {while (getline(myFile, line)) {unsharedStrings.push_back(line);} myFile.close();}
    myFile.open("../../PotentiallySharedOutput.txt");//Adds pre-existing shared logged data
    if (myFile.is_open()) {while (getline(myFile, line)) {sharedStrings.push_back(line);} myFile.close();}
}

// HOTCODE
void RaceDetectorBase::detectShared() {
    cout<< "Detecting Shared/Unshared Data\n";
    const SHBGraph::ThreadSet &set = shbGraph->getThreadSet();
    SHBGraph::ThreadSet setContainsMain(set);
    setContainsMain.insert(MAIN_THREAD);
    //for every thread
    //  for every variable in that thread
    //      for every other thread
    //          for every variable in that other thread
    //--------------------------------------------------
    //for every variable in every thread, check sharing to every variable in every other thread
    for (auto outer = setContainsMain.begin(); outer != setContainsMain.end(); outer ++) {
        Value *t1 = *outer;
        for (SHBNode *a1 : threadAccessSetMap[t1]) {
            bool shared = false;
            string loc = analysisUtil::getSourceLoc(a1->getPointerOperand());
            if (loc!= "") {
                for (auto inner = setContainsMain.begin(); inner != setContainsMain.end(); inner++) {
                    if (inner != outer) { // no thread share check for the same thread
                        Value *t2 = *inner;

                        for (SHBNode *a2 : threadAccessSetMap[t2]) {
                            if (this->checkNodes(a1, a2)) {
                                sharedVarLocs.push_back(loc);
                                shared = true;
                            }
                        }
                    }
                }
                if (!shared) {unsharedVarLocs.push_back(loc);}
            }
        }
    }
}

bool RaceDetectorBase::checkNodes(SHBNode *n1, SHBNode *n2) {
    if (!shbGraph->reachable(n1, n2)) {
        if (PTA->alias(n1->getPointerOperand(), n2->getPointerOperand())==MayAlias) {
            return true;
        }
    }
}

void RaceDetectorBase::collectAccess() {
    cout << "\nCollecting Memory Access Operations\n";
    for (auto thread : shbGraph->getThreadSet()) {
        SHBNode *entryNode = shbGraph->getFunctionEnterNode(shbGraph->getThreadStart(thread));
        // DFS from entry node to collect all the read and write operations
        intraThreadDFS(entryNode, thread);
    }

    //collect main thread
    SHBNode *mainEntry = shbGraph->getFunctionEnterNode(this->module->getFunction("main"));
    intraThreadDFS(mainEntry, MAIN_THREAD);
}

void RaceDetectorBase::intraThreadDFS(SHBNode *entry, Value *thread) {
    std::stack<SHBNode *> nodeStack;
    NodeBS visitedNode;

    nodeStack.push(entry);
    visitedNode.set(entry->getId());

    while (!nodeStack.empty()) {
        SHBNode *node = nodeStack.top();
        nodeStack.pop();
        // collect read and write operation
        if (node->getType() == SHBNode::Read || node->getType() == SHBNode::Write) {
            this->threadAccessSetMap[thread].insert(node);
        }
        for (auto it = node->directOutEdgeBegin(); it != node->directOutEdgeEnd(); it ++) {
            SHBEdge *edge = (*it);
            // only collect read and write within the same thread
            if (!edge->isInterThreadEdge()) {
                if (edge->getType() != SHBEdge::Ret) {
                    SHBNode *dst = edge->getDstNode();
                    if (visitedNode.test_and_set(dst->getId())) {
                        nodeStack.push(dst);
                    }
                }
            }
        }
    }
}

void RaceDetectorBase::checkFiles(bool shared)
{
    vector<string> srcloc;
    string file;
    if(!shared)
    {
        cout << "Unshared File Check: ";
        int un =0;
        for(int index = 0;index<unsharedVarLocs.size();index++){
            if(debug) {cout << "\nStep # [" << index+1 << "/" << unsharedVarLocs.size() << "]";}
            string unsnode = unsharedVarLocs[index];
            srcloc = split(unsnode, "fl: ");
            if(srcloc.size()>1){file = srcloc[1];} else{continue;}
            if(true) {
                //If already identified as unshared
                for (string unshare : unsharedStrings) {if (unshare == "src:" + file) {goto cancel;}}
                //If already identified as shared
                for (string share : sharedStrings) {if (share == "src:" + file) {goto cancel;}}
                //If the file contains shared elements
                vector<string> srcloc2;
                string file2;
                for (string snode : sharedVarLocs) {
                    srcloc2 = split(snode, "fl: ");
                    if(srcloc2.size()>1){file2 = srcloc2[1];} else{continue;}
                    if (file == file2) {
                        sharedStrings.push_back("src:" + file);
                        goto cancel;
                    }
                }
                //Add file to blacklist
                un++;
                unsharedStrings.push_back("src:" + file);

                //TODO: could remove unshared nodes after use?
                //unsharedVarLocs.erase(std::remove(unsharedVarLocs.begin(), unsharedVarLocs.end(), unsnode), unsharedVarLocs.end());
                //Using above may skip a node when the values are shifted back
            }cancel:;
        }
        if(debug){cout<<"\n";}
        cout << "Found " << un << " new unshared file";
        if(un!=1){cout <<"s";}
        cout<<"\n";
    }
    else {
        int sh=0;
        cout << "Shared File Check: ";
        for (int index=0;index<sharedVarLocs.size();index++) {
            if(debug) {cout << "\nStep # [" << index+1 << "/" << sharedVarLocs.size() << "]";}
            string snode = sharedVarLocs[index];
            srcloc = split(snode, "fl: ");
            if (srcloc.size() > 1) { file = srcloc[1]; } else {continue;}
            if (true) {
                //If already identified as shared
                for (string share : sharedStrings) { if (share == "src:" + file) { goto cancel2; }}
                //Add file to whitelist
                sh++;
                sharedStrings.push_back("src:" + file);
            }
            cancel2:;
        }
        if(debug){cout << "\n";}
        cout << "Found " << sh << " new shared file";
        if(sh!=1){cout <<"s";}
        cout<<"\n";
    }
} //TODO: Potential Improvements
void RaceDetectorBase::checkMethods(bool shared){
    vector<string> srcloc;
    string file;
    string method;
    if(!shared)
    {
        cout << "Unshared Method Check: ";
        int un =0;
        for(int index=0;index<unsharedVarLocs.size();index++){
            if(debug) {cout << "\nStep # [" << index+1 << "/" << unsharedVarLocs.size() << "]";}
            string unsnode = unsharedVarLocs[index];
            srcloc = split(unsnode, "fl: ");
            if(srcloc.size()>1){file = srcloc[1];} else{continue;}
            if(true) {
                //If already identified as unshared
                for (string unshare : unsharedStrings) {if (unshare == "src:" + file || unshare == "fun:"+method) {goto cancel;}}
                //If already identified as shared
                for (string share : sharedStrings) {if (share == "src:" + file || share == "fun:"+method) {goto cancel;}}
                //If the method contains shared elements
                for (string snode : sharedVarLocs) {
                    string method2 = "";//FIXME: ^^
                    if (method == method2) {
                        sharedStrings.push_back("fun:" + method);
                        goto cancel;
                    }
                }
                //Add file to blacklist
                unsharedStrings.push_back("fun:" + method);
                //TODO: could remove unshared nodes after use?
                //unsharedVarLocs.erase(std::remove(unsharedVarLocs.begin(), unsharedVarLocs.end(), unsnode), unsharedVarLocs.end());
                //Using above may skip a node when the values are shifted back
            }cancel:;
        }
        if(debug){cout << "\n";}
        cout << "Found " << un << " new unshared method";
        if(un!=1){cout <<"s";}
        cout<<"\n";
    }
    else {
        cout << "Shared Method Check: ";
        int sh = 0;
        for(int index=0;index<sharedVarLocs.size();index++){
            if(debug) {cout << "\nStep # [" << index+1 << "/" << sharedVarLocs.size() << "]";}
            string snode = sharedVarLocs[index];
            srcloc = split(snode, "fl: ");
            if(srcloc.size()>1){file = srcloc[1];} else{continue;}
            if (true) {
                //If already identified as shared
                for (string share : sharedStrings) {
                    if (share == "src:" + file || share == "fun:" + method) { goto cancel2; }
                }
                //Add method to whitelist
                sh++;
                sharedStrings.push_back("fun:" + method);
            }
            cancel2:;
        }
        if(debug){cout << "\n";}
        cout << "Found " << sh << " new shared method";
        if(sh!=1){cout <<"s";}
        cout<<"\n";
    }
}//FIXME: METHODS??
void RaceDetectorBase::checkVariables(bool shared){
    vector<string> srcloc;
    string file;
    string method;
    string var;
    if(!shared) {
        int un=0;
        cout << "Unshared Var Check: ";
        for(int index=0;index<unsharedVarLocs.size();index++){
            if(debug) {cout << "\nStep # [" << index+1 << "/" << unsharedVarLocs.size() << "]";}
            string unsnode = unsharedVarLocs[index];
            if(unsnode.find("Glob ")!=string::npos){unsnode = split(unsnode, "Glob ")[1];}
            srcloc = split(unsnode, "fl: ");
            if (srcloc.size() > 1) { file = srcloc[1]; } else { continue; }
            method = ""; //FIXME: Find method name
            if (true) {
                //If already identified as unshared
                for (string unshare : unsharedStrings) {
                    if (unshare == "src:" + file || unshare == "fun:" + method || unshare == "var:" + unsnode) { goto cancel; }
                }

                //Add variable to blacklist
                un++;
                unsharedStrings.push_back("var:" + unsnode);
                //TODO: could remove unshared nodes after use?
                //unsharedVarLocs.erase(std::remove(unsharedVarLocs.begin(), unsharedVarLocs.end(), unsnode), unsharedVarLocs.end());
                //Using above may skip a node when the values are shifted back
            }
            cancel:;
        }
        if(debug){cout<<"\n";}
        cout << "Found " << un << " new unshared var";
        if(un!=1){cout <<"s";}
        cout<<"\n";
    }
    else
    {
        int sh=0;
        for(int index=0;index<sharedVarLocs.size();index++){
            if(debug) {cout << "\nStep # [" << index+1 << "/" << sharedVarLocs.size() << "]";}
            string snode = sharedVarLocs[index];
            if(snode.find("Glob ")!=string::npos){snode = split(snode, "Glob ")[1];}
            srcloc = split(snode, "fl: ");
            if(srcloc.size()>1){file = srcloc[1];} else{continue;}
            method = "";//FIXME:^^
            if(true) {
                //If already identified as shared
                for (string share : sharedStrings) {if (share == "src:" + file || share == "fun:"+method||share == snode) {goto cancel2;}}
                //Add variable to whitelist
                sh++;
                sharedStrings.push_back("var:" + snode);
            }cancel2:;
        }
        cout << "\nFound " << sh << " new shared var";
        if(sh!=1){cout <<"s";}
        cout<<"\n";
    }
}//FIXME: METHODS??

int RaceDetectorBase::output(bool sharedOutput,bool outputFiles, bool outputMethods,bool outputVariables){
    vector<int> found;
    //Generate strings for output
        if (outputFiles) {checkFiles(sharedOutput);}//Find unshared files to exclude from analysis
        if (outputMethods) {checkMethods(sharedOutput);} //Find unshared methods in files that have not been excluded yet
        if (outputVariables) {checkVariables(sharedOutput);}//Find unshared variables in files/methods that have not been fully excluded (not TSan supported by default)

    //Generate analysis logs {sharedOutput: false = log unshared,true = log potentially shared}
        ofstream myFile;
        if (!sharedOutput) {
            cout<<"Writing Unshared Output (UnsharedOutput.txt)";
            myFile.open("../../UnsharedOutput.txt");
            if (myFile.is_open()) {for (string s : unsharedStrings) {myFile << s + "\n";}}
            else{cout<<"Could Not Log Output";return 2;}
        }
        else {
            cout<<"Writing Potentially Shared Output (PotentiallySharedOutput.txt)";
            myFile.open("../../PotentiallySharedOutput.txt");
            if (myFile.is_open()) {for (string s : sharedStrings) {myFile << s + "\n";}}
            else{cout<<"Could Not Log Output";return 2;}
        }
        myFile.close();
        return 0;
}