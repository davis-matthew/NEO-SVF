//
// Created by peiming on 5/10/19.
//
#include "MemoryModel/OriginPAG.h"
#include <llvm/Support/DOTGraphTraits.h>
#include "MemoryModel/PAG.h"

#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"
#include "Util/GraphUtil.h"

//#define ORIGINS "gc_collect"
//#define ORIGINS "u_undo", "change_indent"
#define ORIGINS "commit_tree" , "merge_trees"

template<>
CtxPAG<OriginID>* CtxPAG<OriginID>::ctxPAG = nullptr;

void OriginPAG::markOrigin(llvm::Function *root, u32_t id) {
    funcToOrigins[root].set(id);
    std::stack<const PTACallGraphNode*> nodeStack;
    auto rootNode = callGraph->getCallGraphNode(root);

    NodeBS visitedNodes;
    nodeStack.push(rootNode);
    visitedNodes.set(rootNode->getId());

    while (!nodeStack.empty()) {
        auto *node = const_cast<PTACallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        for (auto it = node->OutEdgeBegin(), eit = node->OutEdgeEnd(); it != eit; ++it) {
            PTACallGraphEdge* edge = *it;

            if (visitedNodes.test_and_set(edge->getDstID())) {
                nodeStack.push(edge->getDstNode());

                funcToOrigins[edge->getDstNode()->getFunction()].set(id);
            }
        }
    }
}

void OriginPAG::markFuncOrigins() {
    // categorize functions into different origins.
    std::vector<std::string> origins = {ORIGINS};
    std::vector<std::pair<llvm::Function *, uint>> entryFunctions;

    for (auto func : this->module) {
        // default origin, can be accessed from main
        markOrigin(func, 0);

        int inOriginNum = -1;
        for (int i = 0; i < origins.size(); i++) {
            if (func->getName().equals(origins[i])) {
                inOriginNum = i + 1;
                entryFunctions.push_back(std::make_pair(func, i));
                break;
            }
        }

        if (inOriginNum > 0) {
            markOrigin(func, inOriginNum);
        }
    }

    assert(entryFunctions.size() == origins.size());

    for (const std::pair<llvm::Function *, uint> &entry: entryFunctions) {
        funcToOrigins[entry.first].clear();
        funcToOrigins[entry.first].set(entry.second);
    }
}

void OriginPAG::addNodeInOrigins(PAGNode *node, OriginPAG::Origins &origins) {
    if (origins.empty()) {
        addNodeInCtx(node, OriginID::emptyCtx());
    }

    for (u32_t origin : origins) {
        addNodeInCtx(node, origin);
    }
}

void OriginPAG::addNodes() {
    std::string str;
    llvm::raw_string_ostream rawstr(str);

    for (auto it = pag->begin(); it != pag->end(); it++) {
        // iterate over pag node, and duplicated them when they can be in different origins
        PAGNode *pagNode = it->second;

        if (pagNode->hasValue()) {
            const llvm::Value *value = pagNode->getValue();

            if (llvm::isa<llvm::Instruction>(value)) {
                auto *inst = llvm::dyn_cast<llvm::Instruction>(value);

                auto func = inst->getParent()->getParent();
                auto origins = funcToOrigins[func];
                addNodeInOrigins(pagNode, origins);
            } else if (llvm::isa<llvm::Function>(value)) {
                // function pointer?
                if (pagNode->getNodeKind() != PAGNode::RetNode && pagNode->getNodeKind() != PAGNode::VarargNode) {
                    addNodeInCtx(pagNode, OriginID::emptyCtx());
                    continue;
                }

                //return node
                auto func = llvm::dyn_cast<llvm::Function>(value);
                auto origins = funcToOrigins[func];

                addNodeInOrigins(pagNode, origins);
            } else if (llvm::isa<llvm::Argument>(value)) {
                auto *arg = llvm::dyn_cast<llvm::Argument>(value);

                auto func = arg->getParent();
                auto origins = funcToOrigins[func];
                addNodeInOrigins(pagNode, origins);
            } else if (llvm::isa<llvm::Constant>(value)) {
                auto *cons = llvm::dyn_cast<llvm::Constant>(value);
                // TODO: may need to be handled later, GLOBAL VARIABLES?
                // assert(!pagNode->isPointer());
                // assert(llvm::isa<llvm::GlobalValue>(value));
                addNodeInCtx(pagNode, OriginID::emptyCtx());
            } else {
                rawstr << *pagNode->getValue() << "\n\n\n";
                addNodeInCtx(pagNode, OriginID::emptyCtx());
                //assert(false && "what else could it be?");
            }
        } else {
            // Dummy node
            assert(pagNode->getNodeKind() == PAGNode::DummyObjNode||
                   pagNode->getNodeKind() == PAGNode::DummyValNode);

            if (pagNode->getId() > 3) {
                addNodeInCtx(pagNode, OriginID::emptyCtx());
            }
        }
    }
    printf(rawstr.str().c_str());
}


void OriginPAG::dupCtxRetEdge(RetPE *retEdge) {
    assert(retEdge);

    NodeID src = retEdge->getSrcID();
    NodeID dest = retEdge->getDstID();
    const auto &ctxSrcIDs = inSensToSensSetMap[src];
    const auto &ctxDestIDs = inSensToSensSetMap[dest];

    auto commonSet = commonOrigins(ctxSrcIDs, ctxDestIDs);
    if (commonSet.empty()) {
        // origin switch
        for (NodeID srcID : ctxSrcIDs) {
            for (NodeID destID : ctxDestIDs) {
                auto *srcNode = getGNode(srcID);
                auto *destNode = getGNode(destID);
                addEdge(srcNode, destNode, new CtxRetPE<OriginID>(srcNode, destNode, retEdge));
            }
        }
    } else {
        //invoke in the same origin
        for (OriginID id : commonSet) {
            auto srcIt = inSensToSensIDMap.find(std::make_pair(src, id));
            auto destIt = inSensToSensIDMap.find(std::make_pair(dest, id));
            assert(srcIt != inSensToSensIDMap.end() && destIt != inSensToSensIDMap.end());

            NodeID srcID = (*srcIt).second;
            NodeID destID = (*destIt).second;

            auto *srcNode = getGNode(srcID);
            auto *destNode = getGNode(destID);
            addEdge(srcNode, destNode, new CtxRetPE<OriginID>(srcNode, destNode, retEdge));
        }
    }

}

std::set<uint32_t> OriginPAG::commonOrigins(const CtxPAG<OriginID>::CtxSenIDs &ctxSrcIDs, const CtxPAG<OriginID>::CtxSenIDs &ctxDestIDs) {
    std::set<uint32_t> srcOrigins, destOrigins, commonOrigins;

    for (NodeID id : ctxSrcIDs) {
        srcOrigins.insert(getGNode(id)->getContext().getID());
    }
    for (NodeID id : ctxDestIDs) {
        destOrigins.insert(getGNode(id)->getContext().getID());
    }

    std::set_intersection(srcOrigins.begin(), srcOrigins.end(), destOrigins.begin(), destOrigins.end(),
            std::inserter(commonOrigins,commonOrigins.begin()));
    return commonOrigins;
}

void OriginPAG::dupCtxCallEdge(CallPE *callEdge) {
    assert(callEdge);
    NodeID src = callEdge->getSrcID();
    NodeID dest = callEdge->getDstID();
    const auto &ctxSrcIDs = inSensToSensSetMap[src];
    const auto &ctxDestIDs = inSensToSensSetMap[dest];

    auto commonSet = commonOrigins(ctxSrcIDs, ctxDestIDs);
    if (commonSet.empty()) {
        // origin switch
        for (NodeID srcID : ctxSrcIDs) {
            for (NodeID destID : ctxDestIDs) {
                auto *srcNode = getGNode(srcID);
                auto *destNode = getGNode(destID);
                addEdge(srcNode, destNode, new CtxCallPE<OriginID>(srcNode, destNode, callEdge));
            }
        }
    } else {
        //invoke in the same origin
        for (OriginID id : commonSet) {
            auto srcIt = inSensToSensIDMap.find(std::make_pair(src, id));
            auto destIt = inSensToSensIDMap.find(std::make_pair(dest, id));
            assert(srcIt != inSensToSensIDMap.end() && destIt != inSensToSensIDMap.end());

            NodeID srcID = (*srcIt).second;
            NodeID destID = (*destIt).second;

            auto *srcNode = getGNode(srcID);
            auto *destNode = getGNode(destID);
            addEdge(srcNode, destNode, new CtxCallPE<OriginID>(srcNode, destNode, callEdge));
        }
    }
}

std::string OriginPAG::getGraphName() {
    return "Origin_PAG";
}

void OriginPAG::dump(std::string name) {
    llvm::GraphPrinter::WriteGraphToFile(llvm::outs(), name, this);
}

namespace llvm {
/*!
 * Write value flow graph into dot file for debugging
 */
    template<>
    struct DOTGraphTraits<OriginPAG*> : public DefaultDOTGraphTraits {

        typedef CtxPAGNode<OriginID> NodeType;
        typedef NodeType::iterator ChildIteratorType;

        explicit DOTGraphTraits(bool isSimple = false) :
                DefaultDOTGraphTraits(isSimple) {
        }

        /// Return name of the graph
        static std::string getGraphName(OriginPAG *graph) {
            return graph->getGraphName();
        }

        /// Return label of a VFG node with two display mode
        /// Either you can choose to display the name of the value or the whole instruction
        static std::string getNodeLabel(CtxPAGNode<OriginID> *node, OriginPAG *graph) {
            bool briefDisplay = false;
            bool nameDisplay = true;
            std::string str;
            raw_string_ostream rawstr(str);

            if (briefDisplay) {
                if (isa<CtxValPN<OriginID>>(node)) {
                    if (nameDisplay)
                        rawstr << node->getId() << node->getContext().toString() << ":" << node->getValueName();
                    else
                        rawstr << node->getId();
                } else
                    rawstr << node->getId();
            } else {
                // print the whole value
                if (!isa<CtxDummyValPN<OriginID>>(node) && !isa<CtxDummyObjPN<OriginID>>(node))
                    rawstr << node->getContext().toString() << "\n" << *node->getValue();
                else
                    rawstr << "";

            }

            return rawstr.str();

        }

        static std::string getNodeAttributes(CtxPAGNode<OriginID> *node, OriginPAG *pag) {
            if (isa<CtxValPN<OriginID>>(node)) {
                if(isa<CtxGepValPN<OriginID>>(node))
                    return "shape=hexagon";
                else if (isa<CtxDummyValPN<OriginID>>(node))
                    return "shape=diamond";
                else
                    return "shape=circle";
            } else if (isa<CtxObjPN<OriginID>>(node)) {
                if(isa<CtxGepObjPN<OriginID>>(node))
                    return "shape=doubleoctagon";
                else if(isa<CtxFIObjPN<OriginID>>(node))
                    return "shape=septagon";
                else if (isa<CtxDummyObjPN<OriginID>>(node))
                    return "shape=Mcircle";
                else
                    return "shape=doublecircle";
            } else if (isa<CtxRetPN<OriginID>>(node)) {
                return "shape=Mrecord";
            } else if (isa<CtxVarArgPN<OriginID>>(node)) {
                return "shape=octagon";
            } else {
                assert(0 && "no such kind node!!");
            }
            return "";
        }

        template<class EdgeIter>
        static std::string getEdgeAttributes(CtxPAGNode<OriginID> *node, EdgeIter EI, OriginPAG *pag) {
            const CtxPAGEdge<OriginID>* edge = *(EI.getCurrent());
            assert(edge && "No edge found!!");
            if (isa<CtxAddrPE<OriginID>>(edge)) {
                return "color=green";
            } else if (isa<CtxCopyPE<OriginID>>(edge)) {
                return "color=black";
            } else if (isa<CtxGepPE<OriginID>>(edge)) {
                return "color=purple";
            } else if (isa<CtxStorePE<OriginID>>(edge)) {
                return "color=blue";
            } else if (isa<CtxLoadPE<OriginID>>(edge)) {
                return "color=red";
            } else if (isa<CtxTDForkPE<OriginID>>(edge)) {
                return "color=Turquoise";
            } else if (isa<CtxTDJoinPE<OriginID>>(edge)) {
                return "color=Turquoise";
            } else if (isa<CtxCallPE<OriginID>>(edge)) {
                return "color=black,style=dashed";
            } else if (isa<CtxRetPE<OriginID>>(edge)) {
                return "color=black,style=dotted";
            } else {
                assert(0 && "No such kind edge!!");
            }
        }

        template<class EdgeIter>
        static std::string getEdgeSourceLabel(CtxPAGNode<OriginID> *node, EdgeIter EI) {
            const CtxPAGEdge<OriginID>* edge = *(EI.getCurrent());
            assert(edge && "No edge found!!");
            if(const auto* calledge = dyn_cast<CtxCallPE<OriginID>>(edge)) {
                const llvm::Instruction* callInst= calledge->getCallInst();
                return analysisUtil::getSourceLoc(callInst);
            }
            else if(const auto* retedge = dyn_cast<CtxRetPE<OriginID>>(edge)) {
                const llvm::Instruction* callInst= retedge->getCallInst();
                return analysisUtil::getSourceLoc(callInst);
            }
            return "";
        }
    };
}















