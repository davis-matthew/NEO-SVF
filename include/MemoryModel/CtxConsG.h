//
// Created by peiming on 5/19/19.
//

#ifndef SVF_ORIGIN_CTXCONSG_H
#define SVF_ORIGIN_CTXCONSG_H

#include "MemoryModel/CtxConsGEdge.h"
#include "MemoryModel/CtxConsGNode.h"
#include "Util/GraphUtil.h"

using namespace llvm;

template <typename Ctx>
class CtxConstraintGraph :  public GenericGraph<CtxConstraintNode<Ctx>, CtxConstraintEdge<Ctx>> {

private:
    CtxPAG<Ctx> *ctxPAG;
    EdgeID edgeIndex;

    typedef typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy EdgeSetTy;
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy addrCGEdgeSet;
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy directEdgeSet;
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy loadCGEdgeSet;
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy storeCGEdgeSet;

    typedef llvm::DenseMap<NodeID, NodeID> NodeToRepMap;
    typedef llvm::DenseMap<NodeID, NodeBS> NodeToSubsMap;

    NodeToRepMap nodeToRepMap;
    NodeToSubsMap nodeToSubsMap;


    void buildCG();
public:
    /// Constructor
    explicit CtxConstraintGraph(CtxPAG<Ctx>* p): ctxPAG(p), edgeIndex(0) {
        buildCG();
    }

    inline void addConstraintNode(CtxConstraintNode<Ctx>* node, NodeID id) {
        this->addGNode(id,node);
    }

    CtxConstraintNode<Ctx>* getConstraintNode(NodeID id) {
        id = sccRepNode(id);
        return this->getGNode(id);
    }

    inline bool hasConstraintNode(NodeID id) const {
        return this->hasGNode(id);
    }

    bool hasEdge(CtxConstraintNode<Ctx> *src, CtxConstraintNode<Ctx> *dst, ConstraintEdge::ConstraintEdgeK kind) {
        CtxConstraintEdge<Ctx> edge(src,dst,kind);

        if(kind == ConstraintEdge::Copy ||
           kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep)
            return directEdgeSet.find(&edge) != directEdgeSet.end();
        else if(kind == ConstraintEdge::Addr)
            return addrCGEdgeSet.find(&edge) != addrCGEdgeSet.end();
        else if(kind == ConstraintEdge::Store)
            return storeCGEdgeSet.find(&edge) != storeCGEdgeSet.end();
        else if(kind == ConstraintEdge::Load)
            return loadCGEdgeSet.find(&edge) != loadCGEdgeSet.end();
        else
            assert(false && "no other kind!");
        return false;
    }

    void dump() {
        GraphPrinter::WriteGraphToFile(llvm::outs(), "Ctx_ConsCG_final", this);
    }

    inline bool isFieldInsensitiveObj(NodeID id) const {
        const MemObj* mem = ctxPAG->getBaseObj(id);
        return mem->isFieldInsensitive();
    }

    inline void setObjFieldInsensitive(NodeID id) {
        auto* mem =  const_cast<MemObj*>(ctxPAG->getBaseObj(id));
        mem->setFieldInsensitive();
    }

    inline NodeID getGepObjNode(NodeID id, const LocationSet& ls) {
        NodeID gep =  ctxPAG->getGepObjNode(id,ls);
        /// Create a node when it is (1) not exist on graph and (2) not merged
        if(sccRepNode(gep)==gep && !hasConstraintNode(gep))
            addConstraintNode(new CtxConstraintNode<Ctx>(gep), gep);
        return gep;
    }
    /// Move incoming direct edges of a sub node which is outside the SCC to its rep node
    /// Remove incoming direct edges of a sub node which is inside the SCC from its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    bool moveInEdgesToRepNode(CtxConstraintNode<Ctx> *node, CtxConstraintNode<Ctx> *rep);

    /// Move outgoing direct edges of a sub node which is outside the SCC to its rep node
    /// Remove outgoing direct edges of sub node which is inside the SCC from its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    bool moveOutEdgesToRepNode(CtxConstraintNode<Ctx> *node, CtxConstraintNode<Ctx> *rep);

    /// Move incoming/outgoing direct edges of a sub node to its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    inline bool moveEdgesToRepNode(CtxConstraintNode<Ctx> *node, CtxConstraintNode<Ctx> *rep) {
        bool gepIn = moveInEdgesToRepNode(node, rep);
        bool gepOut = moveOutEdgesToRepNode(node, rep);
        return (gepIn || gepOut);
    }

    /*!
     * Add an address edge
     */
    bool addAddrCGEdge(NodeID src, NodeID dst) {
        CtxConstraintNode<Ctx>* srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx>* dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::Addr))
            return false;

        auto* edge = new CtxAddrCGEdge<Ctx>(srcNode, dstNode, edgeIndex++);
        bool added = addrCGEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingAddrEdge(edge);
        dstNode->addIncomingAddrEdge(edge);
        return added;
    }

    /*!
     * Add Copy edge
     */
    bool addCopyCGEdge(NodeID src, NodeID dst) {
        CtxConstraintNode<Ctx>* srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx>* dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::Copy) || srcNode == dstNode)
            return false;

        auto* edge = new CtxCopyCGEdge<Ctx>(srcNode, dstNode, edgeIndex++);
        bool added = directEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingCopyEdge(edge);
        dstNode->addIncomingCopyEdge(edge);
        return true;
    }

    /*!
     * Add normal gep edge
     */
    bool addNormalGepCGEdge(NodeID src, NodeID dst, const LocationSet& ls) {
        CtxConstraintNode<Ctx> *srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx> *dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::NormalGep))
            return false;

        auto *edge = new CtxNormalGepCGEdge<Ctx>(srcNode, dstNode,ls, edgeIndex++);
        bool added = directEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingGepEdge(edge);
        dstNode->addIncomingGepEdge(edge);
        return added;
    }

    /*!
     * Add variant gep edge
     */
    bool addVariantGepCGEdge(NodeID src, NodeID dst) {
        CtxConstraintNode<Ctx>* srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx>* dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::VariantGep))
            return false;

        auto* edge = new CtxVariantGepCGEdge<Ctx>(srcNode, dstNode, edgeIndex++);
        bool added = directEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingGepEdge(edge);
        dstNode->addIncomingGepEdge(edge);
        return added;
    }


    bool addLoadCGEdge(NodeID src, NodeID dst) {
        CtxConstraintNode<Ctx>* srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx>* dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::Load))
            return false;

        auto* edge = new CtxLoadCGEdge<Ctx>(srcNode, dstNode, edgeIndex++);
        bool added = loadCGEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingLoadEdge(edge);
        dstNode->addIncomingLoadEdge(edge);
        return added;
    }

    /*!
     * Add Store edge
     */
    bool addStoreCGEdge(NodeID src, NodeID dst) {
        CtxConstraintNode<Ctx>* srcNode = getConstraintNode(src);
        CtxConstraintNode<Ctx>* dstNode = getConstraintNode(dst);
        if(hasEdge(srcNode, dstNode, ConstraintEdge::Store))
            return false;

        auto* edge = new CtxStoreCGEdge<Ctx>(srcNode, dstNode, edgeIndex++);
        bool added = storeCGEdgeSet.insert(edge).second;
        assert(added && "not added??");
        srcNode->addOutgoingStoreEdge(edge);
        dstNode->addIncomingStoreEdge(edge);
        return added;
    }

    /// SCC rep/sub nodes methods
    //@{
    inline NodeID sccRepNode(NodeID id) const {
        NodeToRepMap::const_iterator it = nodeToRepMap.find(id);
        if(it==nodeToRepMap.end())
            return id;
        else
            return it->second;
    }
    inline NodeBS& sccSubNodes(NodeID id) {
        if(0==nodeToSubsMap.count(id))
            nodeToSubsMap[id].set(id);
        return nodeToSubsMap[id];
    }
    inline void setRep(NodeID node, NodeID rep) {
        nodeToRepMap[node] = rep;
    }
    inline void setSubs(NodeID node, NodeBS& subs) {
        nodeToSubsMap[node] |= subs;
    }
    //@}

    void removeAddrEdge(CtxAddrCGEdge<Ctx>* edge) {
        getConstraintNode(edge->getSrcID())->removeOutgoingAddrEdge(edge);
        getConstraintNode(edge->getDstID())->removeIncomingAddrEdge(edge);
        Size_t num = addrCGEdgeSet.erase(edge);
        delete edge;
        assert(num && "edge not in the set, can not remove!!!");
    }

    void removeLoadEdge(CtxLoadCGEdge<Ctx>* edge) {
        getConstraintNode(edge->getSrcID())->removeOutgoingLoadEdge(edge);
        getConstraintNode(edge->getDstID())->removeIncomingLoadEdge(edge);
        Size_t num = loadCGEdgeSet.erase(edge);
        delete edge;
        assert(num && "edge not in the set, can not remove!!!");
    }

    void removeStoreEdge(CtxStoreCGEdge<Ctx>* edge) {
        getConstraintNode(edge->getSrcID())->removeOutgoingStoreEdge(edge);
        getConstraintNode(edge->getDstID())->removeIncomingStoreEdge(edge);
        Size_t num = storeCGEdgeSet.erase(edge);
        delete edge;
        assert(num && "edge not in the set, can not remove!!!");
    }

    void removeDirectEdge(CtxConstraintEdge<Ctx>* edge) {

        getConstraintNode(edge->getSrcID())->removeOutgoingDirectEdge(edge);
        getConstraintNode(edge->getDstID())->removeIncomingDirectEdge(edge);
        Size_t num = directEdgeSet.erase(edge);

        assert(num && "edge not in the set, can not remove!!!");
        delete edge;
    }

    /// Check if a given edge is a NormalGepCGEdge with 0 offset.
    inline bool isZeroOffsettedGepCGEdge(CtxConstraintEdge<Ctx> *edge) const {
        if (CtxNormalGepCGEdge<Ctx> *normalGepCGEdge = llvm::dyn_cast<CtxNormalGepCGEdge<Ctx>>(edge))
            if (0 == normalGepCGEdge->getLocationSet().getOffset())
                return true;
        return false;
    }

    inline void removeConstraintNode(CtxConstraintNode<Ctx>* node) {
        this->removeGNode(node);
    }

    /// Check/Set PWC (positive weight cycle) flag
    //@{
    inline bool isPWCNode(NodeID nodeId) {
        return getConstraintNode(nodeId)->isPWCNode();
    }
    inline void setPWCNode(NodeID nodeId) {
        getConstraintNode(nodeId)->setPWCNode();
    }
    //@}


    void reTargetDstOfEdge(CtxConstraintEdge<Ctx>* edge, CtxConstraintNode<Ctx> *newDstNode) {
        NodeID newDstNodeID = newDstNode->getId();
        NodeID srcId = edge->getSrcID();

        if(CtxLoadCGEdge<Ctx>* load = dyn_cast<CtxLoadCGEdge<Ctx>>(edge)) {
            removeLoadEdge(load);
            addLoadCGEdge(srcId,newDstNodeID);
        }
        else if(CtxStoreCGEdge<Ctx>* store = dyn_cast<CtxStoreCGEdge<Ctx>>(edge)) {
            removeStoreEdge(store);
            addStoreCGEdge(srcId,newDstNodeID);
        }
        else if(CtxCopyCGEdge<Ctx>* copy = dyn_cast<CtxCopyCGEdge<Ctx>>(edge)) {
            removeDirectEdge(copy);
            addCopyCGEdge(srcId,newDstNodeID);
        }
        else if(CtxNormalGepCGEdge<Ctx>* gep = dyn_cast<CtxNormalGepCGEdge<Ctx>>(edge)) {
            const LocationSet ls = gep->getLocationSet();
            removeDirectEdge(gep);
            addNormalGepCGEdge(srcId,newDstNodeID,ls);
        }
        else if(CtxVariantGepCGEdge<Ctx>* vgep = dyn_cast<CtxVariantGepCGEdge<Ctx>>(edge)) {
            removeDirectEdge(vgep);
            addVariantGepCGEdge(srcId,newDstNodeID);
        }
        else if(CtxAddrCGEdge<Ctx>* addr = dyn_cast<CtxAddrCGEdge<Ctx>>(edge)) {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no other edge type!!");
    }

    void reTargetSrcOfEdge(CtxConstraintEdge<Ctx>* edge, CtxConstraintNode<Ctx>* newSrcNode) {
        NodeID newSrcNodeID = newSrcNode->getId();
        NodeID dstId = edge->getDstID();

        if(CtxLoadCGEdge<Ctx>* load = dyn_cast<CtxLoadCGEdge<Ctx>>(edge)) {
            removeLoadEdge(load);
            addLoadCGEdge(newSrcNodeID,dstId);
        }
        else if(CtxStoreCGEdge<Ctx>* store = dyn_cast<CtxStoreCGEdge<Ctx>>(edge)) {
            removeStoreEdge(store);
            addStoreCGEdge(newSrcNodeID,dstId);
        }
        else if(CtxCopyCGEdge<Ctx>* copy = dyn_cast<CtxCopyCGEdge<Ctx>>(edge)) {
            removeDirectEdge(copy);
            addCopyCGEdge(newSrcNodeID,dstId);
        }
        else if(CtxNormalGepCGEdge<Ctx>* gep = dyn_cast<CtxNormalGepCGEdge<Ctx>>(edge)) {
            const LocationSet ls = gep->getLocationSet();
            removeDirectEdge(gep);
            addNormalGepCGEdge(newSrcNodeID,dstId,ls);
        }
        else if(CtxVariantGepCGEdge<Ctx>* gep = dyn_cast<CtxVariantGepCGEdge<Ctx>>(edge)) {
            removeDirectEdge(gep);
            addVariantGepCGEdge(newSrcNodeID,dstId);
        }
        else if(CtxAddrCGEdge<Ctx>* addr = dyn_cast<CtxAddrCGEdge<Ctx>>(edge)) {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no other edge type!!");
    }
};


/*!
 * Move incoming direct edges of a sub node which is outside SCC to its rep node
 * Remove incoming direct edges of a sub node which is inside SCC from its rep node
 */
template <typename Ctx>
bool CtxConstraintGraph<Ctx>::moveInEdgesToRepNode(CtxConstraintNode<Ctx>* node, CtxConstraintNode<Ctx>* rep) {
    std::vector<CtxConstraintEdge<Ctx>*> sccEdges;
    std::vector<CtxConstraintEdge<Ctx>*> nonSccEdges;

    for (auto it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it) {
        CtxConstraintEdge<Ctx> *subInEdge = *it;
        if(sccRepNode(subInEdge->getSrcID()) != rep->getId())
            nonSccEdges.push_back(subInEdge);
        else {
            sccEdges.push_back(subInEdge);
        }
    }
    // if this edge is outside scc, then re-target edge dst to rep
    while(!nonSccEdges.empty()) {
        CtxConstraintEdge<Ctx> *edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetDstOfEdge(edge,rep);
    }

    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty()) {
        CtxConstraintEdge<Ctx> *edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(isa<CtxCopyCGEdge<Ctx>>(edge))
            removeDirectEdge(edge);
        else if (isa<CtxGepCGEdge<Ctx>>(edge)) {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge)) {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(isa<CtxLoadCGEdge<Ctx>>(edge) || isa<CtxStoreCGEdge<Ctx>>(edge))
            reTargetDstOfEdge(edge,rep);
        else if(auto *addr = dyn_cast<CtxAddrCGEdge<Ctx>>(edge)) {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no such edge");
    }
    return criticalGepInsideSCC;
}

/*!
 * Move outgoing direct edges of a sub node which is outside SCC to its rep node
 * Remove outgoing direct edges of a sub node which is inside SCC from its rep node
 */
template <typename Ctx>
bool CtxConstraintGraph<Ctx>::moveOutEdgesToRepNode(CtxConstraintNode<Ctx> *node, CtxConstraintNode<Ctx> *rep) {
    std::vector<CtxConstraintEdge<Ctx>*> sccEdges;
    std::vector<CtxConstraintEdge<Ctx>*> nonSccEdges;

    for (auto it = node->OutEdgeBegin(), eit = node->OutEdgeEnd(); it != eit; ++it) {
        CtxConstraintEdge<Ctx> *subOutEdge = *it;
        if(sccRepNode(subOutEdge->getDstID()) != rep->getId())
            nonSccEdges.push_back(subOutEdge);
        else {
            sccEdges.push_back(subOutEdge);
        }
    }
    // if this edge is outside scc, then re-target edge src to rep
    while(!nonSccEdges.empty()) {
        CtxConstraintEdge<Ctx> *edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetSrcOfEdge(edge,rep);
    }
    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty()) {
        CtxConstraintEdge<Ctx> *edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(isa<CtxCopyCGEdge<Ctx>>(edge))
            removeDirectEdge(edge);
        else if (isa<CtxGepCGEdge<Ctx>>(edge)) {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge)) {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(isa<CtxLoadCGEdge<Ctx>>(edge) || isa<CtxStoreCGEdge<Ctx>>(edge))
            reTargetSrcOfEdge(edge,rep);
        else if(auto *addr = dyn_cast<CtxAddrCGEdge<Ctx>>(edge)) {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no such edge");
    }
    return criticalGepInsideSCC;
}

template <typename Ctx>
void CtxConstraintGraph<Ctx>::buildCG() {

    // initialize nodes (Here, reuse the same ID)
    for(typename CtxPAG<Ctx>::iterator it = ctxPAG->begin(), eit = ctxPAG->end(); it!=eit; ++it) {
        addConstraintNode(new CtxConstraintNode<Ctx>(it->first), it->first);
    }

    // initialize edges
    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& addrs = ctxPAG->getEdgeSet(PAGEdge::Addr);
    for (CtxPAGEdge<Ctx>* edge : addrs) {
        addAddrCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& copys = ctxPAG->getEdgeSet(PAGEdge::Copy);
    for (CtxPAGEdge<Ctx> *edge : copys) {
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& calls = ctxPAG->getEdgeSet(PAGEdge::Call);
    for (CtxPAGEdge<Ctx> *edge : calls) {
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& rets = ctxPAG->getEdgeSet(PAGEdge::Ret);
    for (CtxPAGEdge<Ctx> *edge : rets) {
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& tdfks = ctxPAG->getEdgeSet(PAGEdge::ThreadFork);
    for (CtxPAGEdge<Ctx> *edge : tdfks) {
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& tdjns = ctxPAG->getEdgeSet(PAGEdge::ThreadJoin);
    for (CtxPAGEdge<Ctx> *edge : tdjns) {
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& ngeps = ctxPAG->getEdgeSet(PAGEdge::NormalGep);
    for (CtxPAGEdge<Ctx> *edge : ngeps) {
        auto *ngep = cast<CtxNormalGepPE<Ctx>>(edge);
        addNormalGepCGEdge(ngep->getSrcID(),ngep->getDstID(),ngep->getLocationSet());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& vgeps = ctxPAG->getEdgeSet(PAGEdge::VariantGep);
    for (CtxPAGEdge<Ctx> *edge : vgeps) {
        auto* vgep = cast<CtxVariantGepPE<Ctx>>(edge);
        addVariantGepCGEdge(vgep->getSrcID(),vgep->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& stores = ctxPAG->getEdgeSet(PAGEdge::Store);
    for (CtxPAGEdge<Ctx> *edge : stores) {
        addStoreCGEdge(edge->getSrcID(),edge->getDstID());
    }

    typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& loads = ctxPAG->getEdgeSet(PAGEdge::Load);
    for (CtxPAGEdge<Ctx> *edge : loads) {
        addLoadCGEdge(edge->getSrcID(),edge->getDstID());
    }
}

#endif //SVF_ORIGIN_CTXCONSG_H
