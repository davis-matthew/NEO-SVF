//
// Created by peiming on 5/13/19.
//

#ifndef SVF_ORIGIN_ORIGINPAG_H
#define SVF_ORIGIN_ORIGINPAG_H

#include <Util/PTACallGraph.h>
#include "MemoryModel/CtxPAG.h"

class OriginID {
private:
    uint32_t id;
public:
    OriginID(uint32_t id) : id(id) {};

    static OriginID emptyCtx() {
        // using 0 for no ctx
        return OriginID{UINT32_MAX};
    }

    inline std::string toString() const {
        std::string str;
        llvm::raw_string_ostream rawstr(str);
        if (id == UINT32_MAX) {
            rawstr << "( no origin )";
        } else if (id == 0) {
            rawstr << "( origin: MAIN )";
        } else {
            rawstr << "( origin: " << id << " )";
        }
        return rawstr.str();
    }

    friend bool operator< (const OriginID& lhs, const OriginID& rhs) {
        return lhs.id < rhs.id;
    }

    friend bool operator== (const OriginID& lhs, const OriginID& rhs) {
        return lhs.id == rhs.id;
    }

    uint32_t getID() const {
        return id;
    }

    bool isEmptyCtx() {
        return id == UINT32_MAX;
    }
};

class OriginPAG : public CtxPAG<OriginID> {
private:
    typedef llvm::SparseBitVector<20> Origins;
    typedef std::map<const llvm::Function *, Origins> FuncToOriginsMap;

private:
    SVFModule &module;
    PTACallGraph *callGraph; // pre-built call graph
    FuncToOriginsMap funcToOrigins;

private:
    void markOrigin(llvm::Function*, u32_t);
    void markFuncOrigins();

    void addNodeInOrigins(PAGNode *node, Origins &origins);

    void addNodes() override;

    std::set<uint32_t> commonOrigins(const CtxPAG<OriginID>::CtxSenIDs&, const CtxPAG<OriginID>::CtxSenIDs&);

public:
    OriginPAG(SVFModule &module, PTACallGraph *callGraph) : CtxPAG<OriginID>(), module(module), callGraph(callGraph) {
        // TODO: not a good practice!! change it to singleton pattern later!
        OriginPAG::ctxPAG = this;
        markFuncOrigins();
    }

    std::string getGraphName() override;
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
    struct GraphTraits<CtxPAGNode<OriginID> *> : public GraphTraits<GenericNode<CtxPAGNode<OriginID>, CtxPAGEdge<OriginID>>* > {
    };

    /// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
    template<>
    struct GraphTraits<Inverse<CtxPAGNode<OriginID> *>> : public GraphTraits<Inverse<GenericNode<CtxPAGNode<OriginID>, CtxPAGEdge<OriginID>>*>> {
    };

    template<>
    struct GraphTraits<OriginPAG *> : public GraphTraits<GenericGraph<CtxPAGNode<OriginID>, CtxPAGEdge<OriginID>>* > {
        typedef CtxPAGNode<OriginID> *NodeRef;
    };
}

#endif //SVF_ORIGIN_ORIGINPAG_H
