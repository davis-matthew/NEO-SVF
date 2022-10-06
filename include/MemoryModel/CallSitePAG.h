//
// Created by peiming on 6/6/19.
//

#ifndef SVF_ORIGIN_CALLSITEPAG_H
#define SVF_ORIGIN_CALLSITEPAG_H

#include <Util/PTACallGraph.h>
#include "MemoryModel/CtxPAG.h"

#define K_LIMIT 2

namespace ctx {
    class CallSite {
    public:
        typedef std::vector<const PTACallGraphNode *> CallChain;

    private:
        CallChain callSiteChain;

    public:
        CallSite() = default;

        static CallSite emptyCtx() {
            return CallSite();
        }

        inline void push(PTACallGraphNode *callSite) {
            this->callSiteChain.push_back(callSite);
        }

        inline void pop() {
            callSiteChain.pop_back();
        }

        inline void addNodeInFrontAndPopBack(PTACallGraphNode *node) {
            callSiteChain.pop_back();
            callSiteChain.insert(callSiteChain.begin(), node);
        }

        inline void addNodeInFront(PTACallGraphNode *node) {
            callSiteChain.insert(callSiteChain.begin(), node);
        }

        std::string toString() const {    
            std::string str;
            llvm::raw_string_ostream rawstr(str);
            if (callSiteChain.empty()) {
                rawstr << "( empty )";
            } else {
                rawstr << "{ " << callSiteChain.back()->getFunction()->getName();
                for (int i = callSiteChain.size() - 2; i >= 0; i --) {
                    rawstr << "->" << callSiteChain[i]->getFunction()->getName();
                }
                rawstr << "}";
            }
            return rawstr.str();
        }

        friend bool operator<(const CallSite &lhs, const CallSite &rhs) {
            return lhs.toString() < rhs.toString();
        }

        friend bool operator==(const CallSite &lhs, const CallSite &rhs) {
            if (lhs.callSiteChain.size() == rhs.callSiteChain.size()) {
                for (int i = 0; i < lhs.callSiteChain.size(); i++) {
                    if (lhs.callSiteChain[i] != rhs.callSiteChain[i]) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        const CallChain *getCallChain() const {
            return &this->callSiteChain;
        }

        bool isEmptyCtx() {
            return callSiteChain.empty();
        }
    };
}

class CallSitePAG : public CtxPAG<ctx::CallSite> {
private:
    typedef llvm::SparseBitVector<20> Origins;
    typedef std::map<const llvm::Function *, Origins> FuncToOriginsMap;

private:
    SVFModule &module;
    PTACallGraph *callGraph; // pre-built call graph
    FuncToOriginsMap funcToOrigins;

private:
    void addNodes() override;

    void addNodeWithCallSite(PAGNode *node, const llvm::Function *func);
    void recAddNodes(PAGNode *node, PTACallGraphNode *callGraphNode, ctx::CallSite &ctx, int depth);

    void addCtxCallEdges(PAGNode *src, PAGNode *dst, CallPE *callEdge, const llvm::Function *func);
    void recAddCtxCallEdges(PAGNode *src, PAGNode *dst, PTACallGraphNode *callGraphNode, PTACallGraphNode *curNode, ctx::CallSite &ctx, CallPE *callEdge, int depth);

    void addCtxRetEdges(PAGNode *src, PAGNode *dst, RetPE *callEdge, const llvm::Function *func);
    void recAddCtxRetEdges(PAGNode *src, PAGNode *dst, PTACallGraphNode *callGraphNode, PTACallGraphNode *curNode, ctx::CallSite &ctx, RetPE *callEdge, int depth);

public:
    CallSitePAG(SVFModule &module, PTACallGraph *callGraph) : CtxPAG<ctx::CallSite>(), module(module), callGraph(callGraph) {
        // TODO: not a good practice!! change it to singleton pattern later!
        CallSitePAG::ctxPAG = this;
    }

    std::string getGraphName() override {
        return "CallSite PAG";
    }
    void dump(std::string name) override;

    void dupCtxCallEdge(CallPE *callEdge) override;
    void dupCtxRetEdge(RetPE *retEdge) override;
};

namespace llvm {
    /* !
     * GraphTraits specializations of PAG to be used for the generic graph algorithms.
     * Provide graph traits for tranversing from a PAG node using standard graph traversals.
     */
    template<>
    struct GraphTraits<CtxPAGNode<ctx::CallSite> *> : public GraphTraits<GenericNode<CtxPAGNode<ctx::CallSite>, CtxPAGEdge<ctx::CallSite>>* > {
    };

    /// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
    template<>
    struct GraphTraits<Inverse<CtxPAGNode<ctx::CallSite> *>> : public GraphTraits<Inverse<GenericNode<CtxPAGNode<ctx::CallSite>, CtxPAGEdge<ctx::CallSite>>*>> {
    };

    template<>
    struct GraphTraits<CallSitePAG *> : public GraphTraits<GenericGraph<CtxPAGNode<ctx::CallSite>, CtxPAGEdge<ctx::CallSite>>* > {
        typedef CtxPAGNode<ctx::CallSite> *NodeRef;
    };
}



#endif //SVF_ORIGIN_CALLSITEPAG_H
