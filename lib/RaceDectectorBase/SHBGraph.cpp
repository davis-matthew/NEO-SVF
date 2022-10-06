//
// Created by peiming on 7/30/19.
//

#include "RaceDetectorBase/SHBGraph.h"
#include "RaceDetectorBase/InsensitiveLockSet.h"
#include "Util/GraphUtil.h"

#include <llvm/Support/DOTGraphTraits.h>	// for dot graph traits

u64_t SHBNode::CURRENT_NODE_ID = 1;
u64_t SHBEdge::CURRENT_EDGE_ID = 1;
using namespace llvm;

void SHBGraph::dumpDotGraph() {
    GraphPrinter::WriteGraphToFile(llvm::outs(), "SHB", this);
}

SHBGraph* SHBGraph:: buildFromModule(llvm::Module *module, PointerAnalysis *PTA) {
    auto *graph = new SHBGraph(PTA);

    auto forkSite = new std::set<Instruction *>();
    auto joinSite = new std::set<Instruction *>();
    auto callSite = new std::set<Instruction *>();
    auto sites2Node = new std::map<Instruction *, NodeID>();

    buildIntraProcNode(module, graph, forkSite, joinSite, callSite, *sites2Node);

    connectCallEdges(module, graph, callSite, sites2Node);
    connectForkEdges(module, graph, forkSite, sites2Node);
    connectJoinEdges(module, graph, joinSite, sites2Node);

    return graph;
}

// A BFS-based reachable algorithm
bool SHBGraph::reachable(SHBNode *n1, SHBNode *n2) {
    if (n1 == n2) {
        return true;
    }

    NodeBS visited;
    std::list<SHBNode *> queue;

    visited.set(n1->getId());
    queue.push_back(n1);

    while (!queue.empty()) {
        SHBNode *n = queue.front();
        queue.pop_front();

        for (auto it = n->directOutEdgeBegin(); it != n->directOutEdgeEnd(); it ++) {
            SHBEdge *edge = *it;
            if (n2->getId() == edge->getDstID()) {
                return true;
            }

            if (visited.test_and_set(edge->getDstID())) {
                queue.push_back(edge->getDstNode());
            }
        }
    }

    return false;
}

void SHBGraph::buildIntraProcNode(
        llvm::Module *module, SHBGraph *graph,
        std::set<Instruction *> *forkSite,
        std::set<Instruction *> *joinSite,
        std::set<Instruction *> *callSite,
        std::map<Instruction *, NodeID> &map) {

    u64_t prevID = 0;
    for (Function &func : *module) {
        if (InsensitiveLockSet::isExtFunction(&func)) {
            continue;
        }

        prevID = graph->addEnterNode(&func);
        for (BasicBlock &BB : func) {
            for (Instruction &inst : BB) {
                if (InsensitiveLockSet::isReadORWrite(&inst)) {
                    int curID;
                    if (InsensitiveLockSet::isRead(&inst)) {
                        curID = graph->addReadNode(&inst);
                    } else {
                        curID = graph->addWriteNode(&inst);
                    }
                    if (prevID > 0) {
                        graph->addEdge(prevID, curID, SHBEdge::PO);
                    }
                    prevID = curID;
                } else if (InsensitiveLockSet::isThreadCreate(&inst)) {
                    forkSite->insert(&inst);
                    map[&inst] = prevID;
                } else if (InsensitiveLockSet::isThreadJoin(&inst)) {
                    joinSite->insert(&inst);
                    map[&inst] = prevID;
                } else if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
                    callSite->insert(&inst);
                    map[&inst] = prevID;
                }
            }
        }
        graph->addEdge(prevID, graph->addRetNode(&func), SHBEdge::PO);
    }
}

void SHBGraph::connectCallEdges(llvm::Module *module, SHBGraph *graph,
        std::set<llvm::Instruction *> *callSite,
        std::map<Instruction *, NodeID> *map) {

    for (Instruction *inst : *callSite) {
        auto *call = new CallSite(inst);
        if (!call->isIndirectCall()) {
            // find function enter and exit node
            Function *callee = call->getCalledFunction();
            if (InsensitiveLockSet::isExtFunction(callee)) {
                continue;
            }

            auto enterNode = graph->getFunctionEnterNode(callee);
            auto retNode = graph->getFunctionRetNode(callee);

            //connect the previous node to enter node
            auto iter = map->find(inst);
            assert(iter != map->end());
            SHBNode *preNode = graph->getGNode(iter->second);
            graph->addEdge(preNode, enterNode, SHBEdge::Call);

            //connect the next node to return node
            SHBNode *nextNode = nullptr;
            for (auto it = preNode->directOutEdgeBegin(); it != preNode->directOutEdgeEnd(); it ++) {
                if ((*it)->getType() == SHBEdge::PO) {
                    nextNode = (*it)->getDstNode();
                    break;
                }
            }
            assert(nextNode);
            graph->addEdge(retNode, nextNode, SHBEdge::Ret);
        } else {
            //TODO: indirect call. using result from PTA
        }
    }
}

void SHBGraph::connectForkEdges(llvm::Module *, SHBGraph *graph,
        std::set<llvm::Instruction *> *forkSites, std::map<Instruction *, NodeID> *map) {
    for (Instruction *inst : *forkSites) {
        // TODO: ONLY SUPPORT PTHREAD_CREATE();
        auto forkSite = new CallSite(inst);
        assert(forkSite->getCalledFunction()->getName().equals("pthread_create"));

        Value *startFunc = forkSite->getArgOperand(2);
        Value *threadHandle = forkSite->getArgOperand(0);

        auto funcStart = dyn_cast<Function>(startFunc);
        assert(funcStart);

        graph->addThread(threadHandle, funcStart);

        auto enterNode = graph->getFunctionEnterNode(funcStart);
        auto retNode = graph->getFunctionRetNode(funcStart);

        auto iter = map->find(inst);
        assert(iter != map->end());
        SHBNode *preNode = graph->getGNode(iter->second);
        graph->addEdge(preNode, enterNode, SHBEdge::Fork);
    }
}

void SHBGraph::connectJoinEdges(llvm::Module *, SHBGraph *graph,
        std::set<llvm::Instruction *> *joinSites, std::map<Instruction *, NodeID> *map) {

    for  (Instruction *inst : *joinSites) {
        // TODO: ONLY SUPPORT PTHREAD_JOIN();
        auto joinSite = new CallSite(inst);
        assert(joinSite->getCalledFunction()->getName().equals("pthread_join"));\

        Value *threadHandle = joinSite->getArgOperand(0);
        // FIXME!!! pthread_join does not take a pointer to identify the thread! so it is skipped by PTA
        // only handle the following special case for now
        // %thread_t = alloca i64,
        // %x = load i64, i64* thread_t,
        // pthread_join(%x),

        assert(isa<LoadInst>(threadHandle));
        auto load = dyn_cast<LoadInst>(threadHandle);
        auto handle = load->getPointerOperand();

        for (llvm::Value *thread : graph->getThreadSet()) {
            NodeID threadInPAG = graph->PTA->getPAG()->getValueNode(thread);
            for(auto node : graph->PTA->getPts(threadInPAG)) {
                if (graph->PTA->getPAG()->getObject(node)->getRefVal() == handle) {
                    // can be the same thread!
                    // connect exit to join
                    Function *func = graph->getThreadStart(thread);
                    SHBNode *retNode = graph->getFunctionRetNode(func);

                    //connect the previous node to enter node
                    auto iter = map->find(inst);
                    assert(iter != map->end());
                    SHBNode *preNode = graph->getGNode(iter->second);
                    //connect the next node to return node
                    SHBNode *nextNode = nullptr;
                    for (auto it = preNode->directOutEdgeBegin(); it != preNode->directOutEdgeEnd(); it ++) {
                        if ((*it)->getType() == SHBEdge::PO) {
                            nextNode = (*it)->getDstNode();
                            break;
                        }
                    }

                    assert(nextNode);
                    graph->addEdge(retNode, nextNode, SHBEdge::Join);
                }
            }
        }
    }

}


namespace llvm {
    /*!
     * Write value flow graph into dot file for debugging
     */
    template<>
    struct DOTGraphTraits<SHBGraph *> : public DefaultDOTGraphTraits {

        typedef SHBNode NodeType;
        typedef NodeType::iterator ChildIteratorType;
        DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

        /// Return name of the graph
        static std::string getGraphName(SHBGraph *graph) {
            return "SHB";
        }

        /// Return label of a VFG node with two display mode
        /// Either you can choose to display the name of the value or the whole instruction
        static std::string getNodeLabel(SHBNode *node, SHBGraph *graph) {
            if (node->getType() == SHBNode::Ret) {
                return "exit";
            } else if (node->getType() == SHBNode::Enter) {
                return "enter";
            } else {
                std::string str;
                raw_string_ostream rawstr(str);

                rawstr << *node->getInst();
                return rawstr.str();
            }
        }

        static std::string getNodeAttributes(SHBNode *node, SHBGraph *pag) {
            switch (node->getType()) {
                case SHBNode::Read:
                    return "shape=rect";
                case SHBNode::Write:
                    return "shape=parallelogram";
                case SHBNode::Enter:
                    return "shape=diamond";
                case SHBNode::Ret:
                    return "shape=diamond";
            }
        }

        template<class EdgeIter>
        static std::string getEdgeAttributes(SHBNode *node, EdgeIter EI, SHBGraph *graph) {
            const SHBEdge* edge = *(EI.getCurrent());
            assert(edge && "No edge found!!");
            switch (edge->getType()) {
                case SHBEdge::Fork:
                    return "color=green";
                case SHBEdge::Join:
                    return "color=red";
                case SHBEdge::PO:
                    return "color=black";
                case SHBEdge::Call:
                    return "style=dotted";
                case SHBEdge::Ret:
                    return "style=dotted";
            }
        }

        template<class EdgeIter>
        static std::string getEdgeSourceLabel(SHBNode *node, EdgeIter EI) {
            return "";
        }
    };
}