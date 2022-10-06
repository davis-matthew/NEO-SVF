//
// Created by peiming on 6/6/19.
//

#include "MemoryModel/CallSitePAG.h"
#include <llvm/Support/DOTGraphTraits.h>
#include "MemoryModel/PAG.h"

#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"
#include "Util/GraphUtil.h"

template<>
CtxPAG<ctx::CallSite>* CtxPAG<ctx::CallSite>::ctxPAG = nullptr;

void CallSitePAG::recAddNodes(PAGNode *node, PTACallGraphNode *callGraphNode, ctx::CallSite &ctx, int depth) {
    if (depth == 0 || !callGraphNode->hasIncomingEdge()) {
        addNodeInCtx(node, ctx);
        return;
    }

    for (auto inEdge = callGraphNode->InEdgeBegin(); inEdge != callGraphNode->InEdgeEnd(); inEdge ++) {
        PTACallGraphNode *caller = (*inEdge)->getSrcNode();
        ctx.push(caller);

        recAddNodes(node, caller, ctx, depth - 1);
        ctx.pop();
    }
}

void CallSitePAG::addNodeWithCallSite(PAGNode *node, const llvm::Function *func) {
    PTACallGraphNode *callGraphNode = this->callGraph->getCallGraphNode(func);

    ctx::CallSite callChain;
    //recursively add node until reach k-limiting
    recAddNodes(node, callGraphNode, callChain, K_LIMIT);
}

void CallSitePAG::addNodes() {
    for (auto it = pag->begin(); it != pag->end(); it++) {
        // iterate over pag node, and duplicated them when they can be in different call site
        PAGNode *pagNode = it->second;

        if (pagNode->hasValue()) {
            const llvm::Value *value = pagNode->getValue();

            if (llvm::isa<llvm::Instruction>(value)) {
                auto *inst = llvm::dyn_cast<llvm::Instruction>(value);
                auto func = inst->getParent()->getParent();

                addNodeWithCallSite(pagNode, func);
            } else if (llvm::isa<llvm::Function>(value)) {
                // function pointer?
                if (pagNode->getNodeKind() != PAGNode::RetNode && pagNode->getNodeKind() != PAGNode::VarargNode) {
                    addNodeInCtx(pagNode, ctx::CallSite::emptyCtx());
                    continue;
                }

                //return node or varargs Node
                auto func = llvm::dyn_cast<llvm::Function>(value);
                addNodeWithCallSite(pagNode, func);
            } else if (llvm::isa<llvm::Argument>(value)) {
                auto *arg = llvm::dyn_cast<llvm::Argument>(value);
                auto func = arg->getParent();

                addNodeWithCallSite(pagNode, func);
            } else if (llvm::isa<llvm::Constant>(value)) {
                auto *cons = llvm::dyn_cast<llvm::Constant>(value);
                // TODO: may need to be handled later, GLOBAL VARIABLES?
                addNodeInCtx(pagNode, ctx::CallSite::emptyCtx());
            } else {
                //INLINE ASM
                addNodeInCtx(pagNode, ctx::CallSite::emptyCtx());
            }
        } else {
            // Dummy node
            assert(pagNode->getNodeKind() == PAGNode::DummyObjNode||
                   pagNode->getNodeKind() == PAGNode::DummyValNode);

            if (pagNode->getId() > 3) {
                addNodeInCtx(pagNode, ctx::CallSite::emptyCtx());
            }
        }
    }
}

void CallSitePAG::recAddCtxCallEdges(PAGNode *src, PAGNode *dst, PTACallGraphNode *callGraphNode,
                                     PTACallGraphNode *curFunc, ctx::CallSite &ctx, CallPE *callEdge, int depth) {
    if (depth == 0 || !callGraphNode->hasIncomingEdge()) {
        ctx::CallSite calleeCtx = ctx;
        if (calleeCtx.getCallChain()->size() < K_LIMIT) {
            calleeCtx.addNodeInFront(curFunc);
        } else {
            assert(calleeCtx.getCallChain()->size() == K_LIMIT);
            calleeCtx.addNodeInFrontAndPopBack(curFunc);
        }

        auto ctxCallerNode = inSensToSensIDMap.find(std::make_pair(src->getId(), ctx));
        assert(ctxCallerNode != inSensToSensIDMap.end());
        auto ctxCalleeNode = inSensToSensIDMap.find(std::make_pair(dst->getId(), calleeCtx));
        assert(ctxCalleeNode != inSensToSensIDMap.end());

        auto *srcNode = getGNode(ctxCallerNode->second);
        auto *dstNode = getGNode(ctxCalleeNode->second);

        addEdge(srcNode, dstNode, new CtxCallPE<ctx::CallSite>(srcNode, dstNode, callEdge));
        return;
    }

    for (auto inEdge = callGraphNode->InEdgeBegin(); inEdge != callGraphNode->InEdgeEnd(); inEdge ++) {
        PTACallGraphNode *caller = (*inEdge)->getSrcNode();
        ctx.push(caller);

        recAddCtxCallEdges(src, dst, caller, curFunc, ctx, callEdge, depth - 1);
        ctx.pop();
    }

}
void CallSitePAG::addCtxCallEdges(PAGNode *src, PAGNode *dst, CallPE *callEdge,const llvm::Function *func) {
    PTACallGraphNode *callGraphNode = this->callGraph->getCallGraphNode(func);

    ctx::CallSite callChain;
    //recursively add node until reach k-limiting
    recAddCtxCallEdges(src, dst, callGraphNode, callGraphNode, callChain, callEdge, K_LIMIT);
}

void CallSitePAG::dupCtxCallEdge(CallPE *callEdge) {
    PAGNode * srcNode = callEdge->getSrcNode();
    PAGNode * dstNode = callEdge->getDstNode();

    if (!srcNode->hasValue() || !dstNode->hasValue()) {
        return;
    }
    // arguments || var-args
    assert(llvm::isa<llvm::Argument>(dstNode->getValue()) || llvm::isa<llvm::Function>(dstNode->getValue()));
    if(auto *srcInst = llvm::dyn_cast<llvm::Instruction>(srcNode->getValue())) {
        // local variables have contexts
        const llvm::Function *func = srcInst->getParent()->getParent();
        addCtxCallEdges(srcNode, dstNode, callEdge, func);
    } else {
        // TODO: global variables as parameters: (is it ever gonna happen?)
        return;
    }
}

void CallSitePAG::recAddCtxRetEdges(PAGNode *src, PAGNode *dst, PTACallGraphNode *callGraphNode,
                                     PTACallGraphNode *curFunc, ctx::CallSite &ctx, RetPE *retEdge, int depth) {
    if (depth == 0 || !callGraphNode->hasIncomingEdge()) {
        ctx::CallSite calleeCtx = ctx;
        if (calleeCtx.getCallChain()->size() < K_LIMIT) {
            calleeCtx.addNodeInFront(curFunc);
        } else {
            assert(calleeCtx.getCallChain()->size() == K_LIMIT);
            calleeCtx.addNodeInFrontAndPopBack(curFunc);
        }

        auto ctxCallerNode = inSensToSensIDMap.find(std::make_pair(dst->getId(), ctx));
        assert(ctxCallerNode != inSensToSensIDMap.end());
        auto ctxCalleeNode = inSensToSensIDMap.find(std::make_pair(src->getId(), calleeCtx));
        assert(ctxCalleeNode != inSensToSensIDMap.end());

        auto *dstNode = getGNode(ctxCallerNode->second);
        auto *srcNode = getGNode(ctxCalleeNode->second);

        addEdge(srcNode, dstNode, new CtxRetPE<ctx::CallSite>(srcNode, dstNode, retEdge));
        return;
    }

    for (auto inEdge = callGraphNode->InEdgeBegin(); inEdge != callGraphNode->InEdgeEnd(); inEdge ++) {
        PTACallGraphNode *caller = (*inEdge)->getSrcNode();
        ctx.push(caller);

        recAddCtxRetEdges(src, dst, caller, curFunc, ctx, retEdge, depth - 1);
        ctx.pop();
    }

}
void CallSitePAG::addCtxRetEdges(PAGNode *src, PAGNode *dst, RetPE *retEdge,const llvm::Function *func) {
    PTACallGraphNode *callGraphNode = this->callGraph->getCallGraphNode(func);

    ctx::CallSite callChain;
    //recursively add node until reach k-limiting
    recAddCtxRetEdges(src, dst, callGraphNode, callGraphNode, callChain, retEdge, K_LIMIT);
}

void CallSitePAG::dupCtxRetEdge(RetPE *retEdge) {
    PAGNode *srcNode = retEdge->getSrcNode();
    PAGNode *dstNode = retEdge->getDstNode();

    if (!srcNode->hasValue() || !dstNode->hasValue()) {
        return;
    }

    assert(llvm::isa<llvm::Function>(srcNode->getValue()));
    assert(llvm::isa<llvm::Instruction>(dstNode->getValue()));

    addCtxRetEdges(srcNode, dstNode, retEdge, llvm::dyn_cast<llvm::Instruction>(dstNode->getValue())->getParent()->getParent());
}

void CallSitePAG::dump(std::string name) {
    llvm::GraphPrinter::WriteGraphToFile(llvm::outs(), name, this);
}


namespace llvm {
/*!
 * Write value flow graph into dot file for debugging
 */
    template<>
    struct DOTGraphTraits<CallSitePAG*> : public DefaultDOTGraphTraits {

        typedef CtxPAGNode<ctx::CallSite> NodeType;
        typedef NodeType::iterator ChildIteratorType;

        explicit DOTGraphTraits(bool isSimple = false) :
                DefaultDOTGraphTraits(isSimple) {
        }

        /// Return name of the graph
        static std::string getGraphName(CallSitePAG *graph) {
            return graph->getGraphName();
        }

        /// Return label of a VFG node with two display mode
        /// Either you can choose to display the name of the value or the whole instruction
        static std::string getNodeLabel(CtxPAGNode<ctx::CallSite> *node, CallSitePAG *graph) {
            bool briefDisplay = false;
            bool nameDisplay = true;
            std::string str;
            raw_string_ostream rawstr(str);

            if (briefDisplay) {
                if (isa<CtxValPN<ctx::CallSite>>(node)) {
                    if (nameDisplay)
                        rawstr << node->getId() << node->getContext().toString() << ":" << node->getValueName();
                    else
                        rawstr << node->getId();
                } else
                    rawstr << node->getId();
            } else {
                // print the whole value
                if (!isa<CtxDummyValPN<ctx::CallSite>>(node) && !isa<CtxDummyObjPN<ctx::CallSite>>(node))
                    rawstr << node->getContext().toString() << "\n" << *node->getValue();
                else
                    rawstr << "";

            }

            return rawstr.str();

        }

        static std::string getNodeAttributes(CtxPAGNode<ctx::CallSite> *node, CallSitePAG *pag) {
            if (isa<CtxValPN<ctx::CallSite>>(node)) {
                if(isa<CtxGepValPN<ctx::CallSite>>(node))
                    return "shape=hexagon";
                else if (isa<CtxDummyValPN<ctx::CallSite>>(node))
                    return "shape=diamond";
                else
                    return "shape=circle";
            } else if (isa<CtxObjPN<ctx::CallSite>>(node)) {
                if(isa<CtxGepObjPN<ctx::CallSite>>(node))
                    return "shape=doubleoctagon";
                else if(isa<CtxFIObjPN<ctx::CallSite>>(node))
                    return "shape=septagon";
                else if (isa<CtxDummyObjPN<ctx::CallSite>>(node))
                    return "shape=Mcircle";
                else
                    return "shape=doublecircle";
            } else if (isa<CtxRetPN<ctx::CallSite>>(node)) {
                return "shape=Mrecord";
            } else if (isa<CtxVarArgPN<ctx::CallSite>>(node)) {
                return "shape=octagon";
            } else {
                assert(0 && "no such kind node!!");
            }
            return "";
        }

        template<class EdgeIter>
        static std::string getEdgeAttributes(CtxPAGNode<ctx::CallSite> *node, EdgeIter EI, CallSitePAG *pag) {
            const CtxPAGEdge<ctx::CallSite>* edge = *(EI.getCurrent());
            assert(edge && "No edge found!!");
            if (isa<CtxAddrPE<ctx::CallSite>>(edge)) {
                return "color=green";
            } else if (isa<CtxCopyPE<ctx::CallSite>>(edge)) {
                return "color=black";
            } else if (isa<CtxGepPE<ctx::CallSite>>(edge)) {
                return "color=purple";
            } else if (isa<CtxStorePE<ctx::CallSite>>(edge)) {
                return "color=blue";
            } else if (isa<CtxLoadPE<ctx::CallSite>>(edge)) {
                return "color=red";
            } else if (isa<CtxTDForkPE<ctx::CallSite>>(edge)) {
                return "color=Turquoise";
            } else if (isa<CtxTDJoinPE<ctx::CallSite>>(edge)) {
                return "color=Turquoise";
            } else if (isa<CtxCallPE<ctx::CallSite>>(edge)) {
                return "color=black,style=dashed";
            } else if (isa<CtxRetPE<ctx::CallSite>>(edge)) {
                return "color=black,style=dotted";
            } else {
                assert(0 && "No such kind edge!!");
            }
        }

        template<class EdgeIter>
        static std::string getEdgeSourceLabel(CtxPAGNode<ctx::CallSite> *node, EdgeIter EI) {
            const CtxPAGEdge<ctx::CallSite>* edge = *(EI.getCurrent());
            assert(edge && "No edge found!!");
            if(const auto* calledge = dyn_cast<CtxCallPE<ctx::CallSite>>(edge)) {
                const llvm::Instruction* callInst= calledge->getCallInst();
                return analysisUtil::getSourceLoc(callInst);
            }
            else if(const auto* retedge = dyn_cast<CtxRetPE<ctx::CallSite>>(edge)) {
                const llvm::Instruction* callInst= retedge->getCallInst();
                return analysisUtil::getSourceLoc(callInst);
            }
            return "";
        }
    };
}
