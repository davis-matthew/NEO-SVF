//
// Created by peiming on 5/9/19.
//

#ifndef SVF_ORIGIN_CTXSENSITIVE_H
#define SVF_ORIGIN_CTXSENSITIVE_H

#include "MemoryModel/PointerAnalysis.h"
#include "MemoryModel/OriginPAG.h"
#include "MemoryModel/CallSitePAG.h"

#include "WPA/Andersen.h"
#include "WPA/WPAStat.h"
#include "WPA/WPASolver.h"
#include "MemoryModel/ConsG.h"
#include "MemoryModel/CtxConsG.h"

#include <llvm/PassAnalysisSupport.h>	// analysis usage
#include <llvm/Support/Debug.h>		// DEBUG TYPE


#include "OriginGTraits.h"

// TODO : better use CondPTAImpl, use BVDataPTAImpl for simplicity now.
template <typename Ctx>
class CtxSensitive : public BVDataPTAImpl, public WPASolver<CtxConstraintGraph<Ctx> *>{
public:
    Size_t numOfProcessedAddr = 0;	/// Number of processed Addr edge
    Size_t numOfProcessedCopy = 0;	/// Number of processed Copy edge
    Size_t numOfProcessedGep = 0;	/// Number of processed Gep edge
    Size_t numOfProcessedLoad = 0;	/// Number of processed Load edge
    Size_t numOfProcessedStore = 0;	/// Number of processed Store edge
    Size_t numOfSCCDetection = 0;
    Size_t AveragePointsToSetSize = 0;
    Size_t MaxPointsToSetSize = 0;
    double timeOfSCCDetection = 0;
    double timeOfSCCMerges = 0;
    double timeOfCollapse = 0;
    double timeOfProcessCopyGep = 0;
    double timeOfProcessLoadStore = 0;
    double timeOfUpdateCallGraph = 0;

private:
    CtxPAG<Ctx> *ctxPAG;

    void mergeSccCycle();


protected:
    explicit CtxSensitive(PointerAnalysis::PTATY ptaTy) : BVDataPTAImpl(ptaTy) {}
    virtual CtxPAG<Ctx>* buildCtxPAG(SVFModule module, PTACallGraph *callGraph) = 0;

    void processNode(NodeID id) override;
    void processAllAddr();
    virtual void processAddr(const CtxAddrCGEdge<Ctx> *);
    virtual bool processLoad(NodeID node, const CtxConstraintEdge<Ctx>* load);
    virtual bool processStore(NodeID node, const CtxConstraintEdge<Ctx>* store);
    virtual bool processCopy(NodeID node, const CtxConstraintEdge<Ctx>* edge);
    virtual void processGep(NodeID node, const CtxGepCGEdge<Ctx>* edge);
    virtual void processGepPts(PointsTo& pts, const CtxGepCGEdge<Ctx>* edge);

    /// Add copy edge on constraint graph
    virtual inline bool addCopyEdge(NodeID src, NodeID dst) {
        return this->graph()->addCopyCGEdge(src, dst);
    }

    void mergeSccNodes(NodeID repNodeId, NodeBS & chanegdRepNodes);
    virtual void mergeNodeToRep(NodeID nodeId,NodeID newRepId);

    /// SCC methods
    inline NodeID sccRepNode(NodeID id) const override {
        return this->graph()->sccRepNode(id);
    }
    inline NodeBS& sccSubNodes(NodeID repId)  {
        return this->graph()->sccSubNodes(repId);
    }

    NodeStack& SCCDetect() override;

protected:
    bool reanalyze = false;
public:
    void analyze(SVFModule svfModule) override {
        auto andersen = new AndersenWaveDiff();
        andersen->analyze(svfModule);

        double timeStart = CLOCK_IN_MS();
        ctxPAG = buildCtxPAG(svfModule, andersen->getPTACallGraph());

        auto *consG = new CtxConstraintGraph<Ctx> (ctxPAG);
        this->setGraph(consG);
            // the initial pts can be computed
        processAllAddr();
        do {
            // we do not update Call graph on the fly.
            // so we do not need re-analyze
            reanalyze = false;
            this->solve();

        } while(reanalyze);

        double timeEnd = CLOCK_IN_MS();

        printf("solving time: %f\n", (timeEnd - timeStart) / 1000.0f);
        this->graph()->dump();
    }

    inline NodeID getBaseObjNode(NodeID id) {
        return ctxPAG->getBaseObjNode(id);
    }
    /*
     * Updates subnodes of its rep, and rep node of its subs
     */
    void updateNodeRepAndSubs(NodeID nodeId) {
        NodeID repId = this->graph()->sccRepNode(nodeId);
        NodeBS repSubs;
        /// update nodeToRepMap, for each subs of current node updates its rep to newRepId
        /// update nodeToSubsMap, union its subs with its rep Subs
        NodeBS& nodeSubs = this->graph()->sccSubNodes(nodeId);
        for(NodeBS::iterator sit = nodeSubs.begin(), esit = nodeSubs.end(); sit!=esit; ++sit) {
            NodeID subId = *sit;
            this->graph()->setRep(subId,repId);
        }
        repSubs |= nodeSubs;
        this->graph()->setSubs(repId,repSubs);
    }

};

template <typename Ctx>
void CtxSensitive<Ctx>::mergeSccCycle() {
    NodeBS changedRepNodes;

    NodeStack revTopoOrder;
    NodeStack & topoOrder = this->getSCCDetector()->topoNodeStack();
    while (!topoOrder.empty()) {
        NodeID repNodeId = topoOrder.top();
        topoOrder.pop();
        revTopoOrder.push(repNodeId);

        // merge sub nodes to rep node
        mergeSccNodes(repNodeId, changedRepNodes);
    }

    // update rep/sub relation in the constraint graph.
    // each node will have a rep node
    for(NodeBS::iterator it = changedRepNodes.begin(), eit = changedRepNodes.end(); it!=eit; ++it) {
        updateNodeRepAndSubs(*it);
    }

    // restore the topological order for later solving.
    while (!revTopoOrder.empty()) {
        NodeID nodeId = revTopoOrder.top();
        revTopoOrder.pop();
        topoOrder.push(nodeId);
    }
}

/*
 * Merge a node to its rep node
 */
template <typename Ctx>
void CtxSensitive<Ctx>::mergeNodeToRep(NodeID nodeId,NodeID newRepId) {
    if(nodeId==newRepId)
        return;

    /// union pts of node to rep
    unionPts(newRepId,nodeId);

    /// move the edges from node to rep, and remove the node
    CtxConstraintNode<Ctx>* node = this->graph()->getConstraintNode(nodeId);
    bool gepInsideScc = this->graph()->moveEdgesToRepNode(node, this->graph()->getConstraintNode(newRepId));
    /// 1. if find gep edges inside SCC cycle, the rep node will become a PWC node and
    /// its pts should be collapsed later.
    /// 2. if the node to be merged is already a PWC node, the rep node will also become
    /// a PWC node as it will have a self-cycle gep edge.
    if (gepInsideScc || node->isPWCNode())
        this->graph()->setPWCNode(newRepId);

    this->graph()->removeConstraintNode(node);

    /// set rep and sub relations
    this->graph()->setRep(node->getId(),newRepId);
    NodeBS& newSubs = this->graph()->sccSubNodes(newRepId);
    newSubs.set(node->getId());
}

template <typename Ctx>
void CtxSensitive<Ctx>::mergeSccNodes(NodeID repNodeId, NodeBS& chanegdRepNodes) {
    const NodeBS& subNodes = this->getSCCDetector()->subNodes(repNodeId);
    for (unsigned int subNodeId : subNodes) {
        if (subNodeId != repNodeId) {
            mergeNodeToRep(subNodeId, repNodeId);
            chanegdRepNodes.set(subNodeId);
        }
    }
}

template <typename Ctx>
NodeStack& CtxSensitive<Ctx>::SCCDetect() {
    numOfSCCDetection++;
    //double sccStart = stat->getClk();
    WPASolver<CtxConstraintGraph<Ctx> *>::SCCDetect();
    //double sccEnd = stat->getClk();
    //timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    //double mergeStart = stat->getClk();
    //merge SCC in constraint Graph
    mergeSccCycle();
    //double mergeEnd = stat->getClk();
    //timeOfSCCMerges +=  (mergeEnd - mergeStart)/TIMEINTERVAL;

    return this->getSCCDetector()->topoNodeStack();
}

template <typename Ctx>
void CtxSensitive<Ctx>::processAllAddr()  {
    for (auto nodeIt = this->graph()->begin(); nodeIt != this->graph()->end(); nodeIt++) {
        CtxConstraintNode<Ctx> * cgNode = nodeIt->second;
        for (auto it = cgNode->incomingAddrsBegin(); it != cgNode->incomingAddrsEnd(); ++it)
            processAddr(cast<CtxAddrCGEdge<Ctx>>(*it));
    }
}

template <typename Ctx>
void CtxSensitive<Ctx>::processAddr(const CtxAddrCGEdge<Ctx> *addr) {
    assert(addr);
    numOfProcessedAddr++;

    NodeID dst = addr->getDstID();
    NodeID src = addr->getSrcID();
    if(addPts(dst,src))
        // PTS changed
        this->pushIntoWorklist(dst);
}

template <typename Ctx>
bool CtxSensitive<Ctx>::processStore(NodeID node, const CtxConstraintEdge<Ctx> *store) {
    if (ctxPAG->isConstantObj(node) || ctxPAG->isNonPointerObj(node))
        return false;

    numOfProcessedStore++;

    NodeID src = store->getSrcID();
    return addCopyEdge(src, node);
}

template <typename Ctx>
bool CtxSensitive<Ctx>::processLoad(NodeID node, const CtxConstraintEdge<Ctx> *load) {
    if (ctxPAG->isConstantObj(node) || ctxPAG->isNonPointerObj(node))
        return false;

    numOfProcessedLoad++;

    NodeID dst = load->getDstID();
    return addCopyEdge(node, dst);
}

template <typename Ctx>
bool CtxSensitive<Ctx>::processCopy(NodeID node, const CtxConstraintEdge<Ctx>* edge) {
    numOfProcessedCopy++;

    assert((isa<CtxCopyCGEdge<Ctx>>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcPts = getPts(node);
    bool changed = unionPts(dst,srcPts);
    if (changed)
        this->pushIntoWorklist(dst);

    return changed;
}

template <typename Ctx>
void CtxSensitive<Ctx>::processGep(NodeID node, const CtxGepCGEdge<Ctx> *edge) {
    PointsTo& srcPts = this->getPts(edge->getSrcID());
    processGepPts(srcPts, edge);
}

template <typename Ctx>
void CtxSensitive<Ctx>::processGepPts(PointsTo &pts, const CtxGepCGEdge<Ctx> *edge) {

    numOfProcessedGep++;
    PointsTo tmpDstPts;
    for (PointsTo::iterator piter = pts.begin(); piter != pts.end(); ++piter) {
        /// get the object
        NodeID ptd = *piter;
        /// handle blackhole and constant
        if (ctxPAG->isBlkObjOrConstantObj(ptd)) {
            tmpDstPts.set(*piter);
        } else {
            /// handle variant gep edge
            /// If a pointer connected by a variant gep edge,
            /// then set this memory object to be field insensitive
            if (isa<CtxVariantGepCGEdge<Ctx>>(edge)) {
                if (!this->graph()->isFieldInsensitiveObj(ptd)) {
                    this->graph()->setObjFieldInsensitive(ptd);
                    //NOTE: only used for andersen Wave
                    //this->graph()->addNodeToBeCollapsed(consCG->getBaseObjNode(ptd));
                }
                // add the field-insensitive node into pts.
                // NodeID baseId = this->graph()->getFIObjNode(ptd);
                tmpDstPts.set(ptd);
            }
            /// Otherwise process invariant (normal) gep
            /// TODO: after the node is set to field insensitive, handling invaraint gep edge may lose precision
            /// because offset here are ignored, and it always return the base obj
            else if (const CtxNormalGepCGEdge<Ctx> *normalGepEdge = dyn_cast<CtxNormalGepCGEdge<Ctx>>(edge)) {
//                if (!matchType(edge->getSrcID(), ptd, normalGepEdge))
//                    continue;
                NodeID fieldSrcPtdNode = this->graph()->getGepObjNode(ptd, normalGepEdge->getLocationSet());
                tmpDstPts.set(fieldSrcPtdNode);

                // addTypeForGepObjNode(fieldSrcPtdNode, normalGepEdge);
                // Any points-to passed to an FIObj also pass to its first field
                if (normalGepEdge->getLocationSet().getOffset() == 0)
                    addCopyEdge(getBaseObjNode(fieldSrcPtdNode), fieldSrcPtdNode);
            }
            else {
                assert(false && "new gep edge?");
            }
        }
    }

    NodeID dstId = edge->getDstID();
    if (unionPts(dstId, tmpDstPts))
        this->pushIntoWorklist(dstId);
}

template <typename Ctx>
void CtxSensitive<Ctx>::processNode(NodeID nodeId) {
    numOfIteration++;

    CtxConstraintNode<Ctx>* node = this->graph()->getConstraintNode(nodeId);

    for (auto it = node->outgoingAddrsBegin(); it != node->outgoingAddrsEnd(); ++it) {
        processAddr(cast<CtxAddrCGEdge<Ctx>>(*it));
    }

    for (PointsTo::iterator piter = getPts(nodeId).begin(); piter != getPts(nodeId).end(); ++piter) {
        NodeID ptd = *piter;
        // handle load
        for (auto it = node->outgoingLoadsBegin(); it != node->outgoingLoadsEnd(); ++it) {
            if (processLoad(ptd, *it))
                this->pushIntoWorklist(ptd);
        }

        // handle store
        for (auto it = node->incomingStoresBegin(); it != node->incomingStoresEnd(); ++it) {
            if (processStore(ptd, *it))
                this->pushIntoWorklist((*it)->getSrcID());
        }
    }

    // handle copy, call, return, gep
    for (auto it = node->directOutEdgeBegin(); it != node->directOutEdgeEnd(); ++it) {
        if (auto* gepEdge = llvm::dyn_cast<CtxGepCGEdge<Ctx>>(*it))
            processGep(nodeId, gepEdge);
        else
            processCopy(nodeId, *it);
    }
}

/*
 * Wave propagation based Andersen Analysis
 */
template <typename Ctx>
class CtxSensitiveWave : public CtxSensitive<Ctx> {
private:
    static AndersenWave* waveAndersen; // static instance

public:
    CtxSensitiveWave() : CtxSensitive<Ctx>(PointerAnalysis::ORIGIN_PTA) {}

    void processNode(NodeID nodeId) override {
        if (this->sccRepNode(nodeId) != nodeId)
            return;

        CtxConstraintNode<Ctx> *node = this->graph()->getConstraintNode(nodeId);

        handleCopyGep(node);
    }

    void postProcessNode(NodeID nodeId) override {
        CtxConstraintNode<Ctx> *node = this->graph()->getConstraintNode(nodeId);
        // handle load
        for (auto it = node->outgoingLoadsBegin(), eit = node->outgoingLoadsEnd();
             it != eit; ++it) {
            if (handleLoad(nodeId, *it))
                this->reanalyze = true;
        }
        // handle store
        for (auto it = node->incomingStoresBegin(), eit =  node->incomingStoresEnd();
             it != eit; ++it) {
            if (handleStore(nodeId, *it))
                this->reanalyze = true;
        }
    }

    virtual void handleCopyGep(CtxConstraintNode<Ctx>* node) {
        NodeID nodeId = node->getId();
        for (auto it = node->directOutEdgeBegin(); it != node->directOutEdgeEnd(); ++it) {
            if(CtxCopyCGEdge<Ctx>* copyEdge = dyn_cast<CtxCopyCGEdge<Ctx>>(*it)) {
                this->processCopy(nodeId, copyEdge);
            } else if(CtxGepCGEdge<Ctx>* gepEdge = dyn_cast<CtxGepCGEdge<Ctx>>(*it)) {
                this->processGep(nodeId, gepEdge);
            }
        }
    }

    virtual bool handleLoad(NodeID id, const CtxConstraintEdge<Ctx>* load) {
        bool changed = false;
        for (auto piter = this->getPts(id).begin(), epiter = this->getPts(id).end();
             piter != epiter; ++piter) {
            if (this->processLoad(*piter, load)) {
                changed = true;
            }
        }
        return changed;
    }

    virtual bool handleStore(NodeID id, const CtxConstraintEdge<Ctx>* store) {
        bool changed = false;
        for (auto piter = this->getPts(id).begin(), epiter = this->getPts(id).end();
             piter != epiter; ++piter) {
            if (this->processStore(*piter, store)) {
                changed = true;
            }
        }
        return changed;
    }
};



/**
 * Wave propagation with diff points-to set.
 */
template <typename Ctx>
class CtxSensitiveWaveDiff : public CtxSensitiveWave<Ctx> {

private:
    PointsTo & getCachePts(const CtxConstraintEdge<Ctx>* edge) {
        EdgeID edgeId = edge->getEdgeID();
        return this->getDiffPTDataTy()->getCachePts(edgeId);
    }

    /// Handle diff points-to set.
    //@{
    virtual inline void computeDiffPts(NodeID id) {
        NodeID rep = this->sccRepNode(id);
        this->getDiffPTDataTy()->computeDiffPts(rep, this->getDiffPTDataTy()->getPts(rep));
    }
    virtual inline PointsTo& getDiffPts(NodeID id) {
        NodeID rep = this->sccRepNode(id);
        return this->getDiffPTDataTy()->getDiffPts(rep);
    }
    //@}

    /// Handle propagated points-to set.
    //@{
    inline void updatePropaPts(NodeID dst, NodeID src) {
        NodeID srcRep = this->sccRepNode(src);
        NodeID dstRep = this->sccRepNode(dst);
        this->getDiffPTDataTy()->updatePropaPtsMap(srcRep, dstRep);
    }
    inline void clearPropaPts(NodeID src) {
        NodeID rep = this->sccRepNode(src);
        this->getDiffPTDataTy()->clearPropaPts(rep);
    }
    //@}

public:
    CtxSensitiveWaveDiff(): CtxSensitiveWave<Ctx>() {}

    void handleCopyGep(CtxConstraintNode<Ctx> *node) override {
        this->computeDiffPts(node->getId());
        if (!getDiffPts(node->getId()).empty()) {
            CtxSensitiveWave<Ctx>::handleCopyGep(node);
        }
    }

    bool processCopy(NodeID node, const CtxConstraintEdge<Ctx> *edge) override {
        bool changed = false;
        assert((isa<CtxCopyCGEdge<Ctx>>(edge)) && "not copy/call/ret ??");

        NodeID dst = edge->getDstID();
        PointsTo& srcDiffPts = getDiffPts(node);
        //processCast(edge);

        if(this->unionPts(dst,srcDiffPts)) {
            changed = true;
            this->pushIntoWorklist(dst);
        }

        return changed;
    }

    void processGep(NodeID node, const CtxGepCGEdge<Ctx>* edge) override {
        PointsTo& srcDiffPts = getDiffPts(edge->getSrcID());
        this->processGepPts(srcDiffPts, edge);
    }

    bool handleLoad(NodeID id, const CtxConstraintEdge<Ctx>* load) override {
        /// calculate diff pts.
        PointsTo & cache = getCachePts(load);
        PointsTo & pts = this->getPts(id);
        PointsTo newPts;
        newPts.intersectWithComplement(pts, cache);
        cache |= newPts;

        bool changed = false;
        for (PointsTo::iterator piter = newPts.begin(), epiter = newPts.end(); piter != epiter; ++piter) {
            NodeID ptdId = *piter;
            if (this->processLoad(ptdId, load)) {
                changed = true;
            }
        }

        return changed;
    }

    bool handleStore(NodeID id, const CtxConstraintEdge<Ctx>* store) override {
        /// calculate diff pts.
        PointsTo & cache = getCachePts(store);
        PointsTo & pts = this->getPts(id);
        PointsTo newPts;
        newPts.intersectWithComplement(pts, cache);
        cache |= newPts;

        bool changed = false;
        for (PointsTo::iterator piter = newPts.begin(), epiter = newPts.end(); piter != epiter; ++piter) {
            NodeID ptdId = *piter;
            if (this->processStore(ptdId, store)) {
                changed = true;
            }
        }

        return changed;
    }

protected:
    void mergeNodeToRep(NodeID nodeId, NodeID newRepId) override {
        if(nodeId==newRepId)
            return;

        /// update rep's propagated points-to set
        updatePropaPts(newRepId, nodeId);
        CtxSensitive<Ctx>::mergeNodeToRep(nodeId, newRepId);
    }

    inline bool addCopyEdge(NodeID src, NodeID dst) override {
        if (CtxSensitive<Ctx>::addCopyEdge(src, dst)) {
            if (this->unionPts(this->sccRepNode(dst), this->sccRepNode(src)))
                this->pushIntoWorklist(this->sccRepNode(dst));
            return true;
        }
        else
            return false;
    }

    /// process "bitcast" CopyCGEdge
    virtual void processCast(const ConstraintEdge *edge) {
        return;
    }
};


class OriginSensitive : public CtxSensitiveWaveDiff<OriginID> {
protected:
    CtxPAG<OriginID>* buildCtxPAG(SVFModule module, PTACallGraph *callGraph) override {
        auto *oPAG = new OriginPAG(module, callGraph);
        oPAG->initFromPAG(Andersen::pag);

        printf("Insensitive:\n Number of Node: %ld\n Number of Edge: %ld\n",Andersen::pag->getTotalNodeNum(), Andersen::pag->getTotalEdgeNum());
        printf("Origin:\n Number of Node: %ld\n Object Node: %lld\n Pointer Node: %lld\n Number of Edge: %ld\n",
               oPAG->getTotalNodeNum(), oPAG->getObjNodeNum(), oPAG->getPtrNodeNum(), oPAG->getTotalEdgeNum());

        //oPAG->dump(oPAG->getGraphName());
        return oPAG;
    }

public:
    OriginSensitive() : CtxSensitiveWaveDiff<OriginID> () {}
};

class CallSiteSensitive : public CtxSensitiveWaveDiff<ctx::CallSite> {
protected:
    CtxPAG<ctx::CallSite>* buildCtxPAG(SVFModule module, PTACallGraph *callGraph) override {
        auto *csPAG = new CallSitePAG(module, callGraph);
        csPAG->initFromPAG(Andersen::pag);

        printf("Insensitive:\n Number of Node: %ld\n Number of Edge: %ld\n",Andersen::pag->getTotalNodeNum(), Andersen::pag->getTotalEdgeNum());
        printf("CallSite:\n Number of Node: %ld\n Object Node: %lld\n Pointer Node: %lld\n Number of Edge: %ld\n",
                csPAG->getTotalNodeNum(), csPAG->getObjNodeNum(), csPAG->getPtrNodeNum(), csPAG->getTotalEdgeNum());

        //csPAG->dump(csPAG->getGraphName());
        return csPAG;
    }

public:
    CallSiteSensitive() : CtxSensitiveWaveDiff<ctx::CallSite> () {}
};

#endif //SVF_ORIGIN_CTXSENSITIVE_H
