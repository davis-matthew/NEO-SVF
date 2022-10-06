//
// Created by peiming on 7/26/19.
//

#include "RaceDetectorBase/InsensitiveLockSet.h"

bool InsensitiveLockSet::isThreadCreate(Instruction *inst) {
    CallInst *called = dyn_cast<CallInst>(inst);
    if (!called) {
        return false;
    } else if (called->getCalledFunction()->getName().equals("pthread_create")) {
        return true;
    }
    return false;
}

bool InsensitiveLockSet::isThreadJoin(Instruction *inst) {
    CallInst *called = dyn_cast<CallInst>(inst);
    if (!called) {
        return false;
    } else if (called->getCalledFunction()->getName().equals("pthread_join")) {
        return true;
    }
    return false;
}

bool InsensitiveLockSet::isMutexLock(Instruction *inst) {
    auto *called = dyn_cast<CallInst>(inst);
    if (!called) {
        return false;
    } else if (called->getCalledFunction()->getName().equals("pthread_mutex_lock") ||
               called->getCalledFunction()->getName().equals("_Z15safe_mutex_lockP15pthread_mutex_t")) {
        return true;
    }
    return false;
}

bool InsensitiveLockSet::isMutexUnLock(Instruction *inst) {
    auto called = dyn_cast<CallInst>(inst);
    if (!called) {
        return false;
    } else if (called->getCalledFunction()->getName().equals("pthread_mutex_unlock") ||
               called->getCalledFunction()->getName().equals("_Z15safe_mutex_unlockP15pthread_mutex_t")) {
        return true;
    }
    return false;
}

bool InsensitiveLockSet::isRead(Instruction *inst) {
    return llvm::isa<LoadInst>(inst);
}

bool InsensitiveLockSet::isWrite(Instruction *inst) {
    return llvm::isa<StoreInst>(inst);
}

bool InsensitiveLockSet::isReadORWrite(Instruction * inst) {
    return isRead(inst) || isWrite(inst);
}

bool InsensitiveLockSet::isExtFunction(Function *func) {
    return func->isDeclaration() ||
           func->isIntrinsic() ||
           func->getName().equals("pthread_create") ||
           func->getName().equals("pthread_mutex_lock") ||
           func->getName().equals("_Z15safe_mutex_lockP15pthread_mutex_t") ||
           func->getName().equals("pthread_mutex_unlock") ||
           func->getName().equals("_Z15safe_mutex_unlockP15pthread_mutex_t");
}

bool InsensitiveLockSet::protectedByCommonLock(const Instruction *i1, const Instruction *i2) {
    if (i1 == i2) return true;

    auto it1 = outLockSetMap->find(i1);
    auto it2 = outLockSetMap->find(i2);

    // not protected by any lock
    if (it1 == outLockSetMap->end() || it2 == outLockSetMap->end()) {
        return false;
    }

    LockSet *set1 = it1->second;
    LockSet *set2 = it2->second;

    for (Value *lock1 : *set1) {
        for (Value *lock2 : *set2) {
            if (this->PTA->alias(lock1, lock2)==MayAlias) {
                return true;
            }
        }
    }
    return false;
}

void InsensitiveLockSet::analyze(llvm::Module *module) {
    inLockSetMap = new StmtLockSetMap();
    outLockSetMap = new StmtLockSetMap();
    func2CallSitesMap = new Func2CallSites();
    func2ReturnMap = new Func2CallSites();

    auto *worklist = new vector<Instruction *>();

    for (auto &func : *module) {
        if (isExtFunction(&func)) {
            continue;
        }

        for (auto &bb : func) {
            for (auto &inst : bb) {
                worklist->push_back(&inst);
                inLockSetMap->insert(make_pair(&inst, new LockSet()));
                outLockSetMap->insert(make_pair(&inst, new LockSet()));

                // TODO: handle invoke
                if (isa<CallInst>(inst)) {
                    auto callinst = dyn_cast<CallInst>(&inst);
                    (*func2CallSitesMap)[callinst->getCalledFunction()].insert(callinst);
                } else if (isa<ReturnInst>(inst)) {
                    (*func2ReturnMap)[inst.getFunction()].insert(&inst);
                }
            }
        }
    }

    while (!worklist->empty()) {
        Instruction *inst = worklist->back();
        worklist->pop_back();

        BasicBlock *instBB = inst->getParent();
        Function *instFunc = instBB->getParent();

        LockSet inLS;
        LockSet outLS;

        // first instruction in BB;
        if (&instBB->front() == inst) {
            if (instBB == &instFunc->getEntryBlock()) {
                // func entry block's first instruction, predecessor are callsites;
                for (auto callsite : (*func2CallSitesMap)[instFunc]) {
                    for (Value *v : *outLockSetMap->find(callsite)->second){
                        inLS.insert(v);
                    }
                }
            } else {
                for (BasicBlock *pred : predecessors(inst->getParent())) {
                    for (Value *v : *outLockSetMap->find(pred->getTerminator())->second){
                        inLS.insert(v);
                    }
                }
            }
        } else if (isa<CallInst>(inst->getPrevNode())) {
            auto callInst = dyn_cast<CallInst>(inst->getPrevNode());
            // previous call edges
            if (callInst->getCalledFunction()) {
                if (isExtFunction(callInst->getCalledFunction())) {
                    for (Value *v : *outLockSetMap->find(inst->getPrevNode())->second){
                        inLS.insert(v);
                    }
                } else {
                    for (auto retInst : (*func2ReturnMap)[callInst->getCalledFunction()]) {
                        for (Value *v : *outLockSetMap->find(retInst)->second){
                            inLS.insert(v);
                        }
                    }
                }
            } else {
                continue;
            }
        } else {
            // normal cases
            for (Value *v : *outLockSetMap->find(inst->getPrevNode())->second){
                inLS.insert(v);
            }
        }

        // we get inLS now, calculate the outLS
        outLS = inLS;
        if (isMutexLock(inst)) {
            auto callInst = dyn_cast<CallInst>(inst);
            outLS.insert(callInst->getArgOperand(0));
        } else if (isMutexUnLock(inst)) {
            auto callInst = dyn_cast<CallInst>(inst);
            outLS.erase(callInst->getArgOperand(0));
        }

        // if outLS != inLS, add successors into worklist
        if (*(*outLockSetMap)[inst]!=outLS) {
            *(*outLockSetMap)[inst] = outLS;

            if (isa<ReturnInst>(inst)) {
                for (auto callSite : (*func2CallSitesMap)[inst->getFunction()]) {
                    worklist->push_back(callSite->getNextNode());
                }
            } else if (isa<TerminatorInst>(inst)) {
                auto termInst = dyn_cast<TerminatorInst>(inst);
                for (auto bb : termInst->successors())  {
                    worklist->push_back(&bb->front());
                }
            } else if (isa<CallInst>(inst)) {
                auto callInst = dyn_cast<CallInst>(inst);
                if (callInst->getFunction() == nullptr) {
                    // indirect call?
                    worklist->push_back(inst->getNextNode());
                } else if (isExtFunction(callInst->getCalledFunction())) {
                    worklist->push_back(inst->getNextNode());
                } else {
                    worklist->push_back(&callInst->getCalledFunction()->getEntryBlock().front());
                }
            } else {
                worklist->push_back(inst->getNextNode());
            }
        }
    }
}