//
// Created by peiming on 5/13/19.
//

#ifndef SVF_ORIGIN_CTXPAG_H
#define SVF_ORIGIN_CTXPAG_H

#include "MemoryModel/CtxPAGNode.h"
#include "MemoryModel/CtxPAGEdge.h"
#include "MemoryModel/PAG.h"

/*!
 * Program Assignment Graph for pointer analysis
 * Peiming: SymID and NodeID are not equal here (same numbering).
 */
template<class Ctx>
class CtxPAG : public GenericGraph<CtxPAGNode<Ctx>,CtxPAGEdge<Ctx>> {

protected:
    typedef std::pair<NodeID, Ctx> CtxInSensID;
    typedef std::map<CtxInSensID, NodeID> CtxInSensToSensIDMap;

    typedef std::set<NodeID> CtxSenIDs;
    typedef std::map<NodeID, CtxSenIDs> CtxInSensToSensIDSetMap;

    typedef std::tuple<NodeID, Ctx, LocationSet> NodeLocationSet;
    typedef std::map<NodeLocationSet,NodeID> NodeLocationSetMap;

protected:
    PAG *pag;

    u64_t totalNodeNum;
    CtxInSensToSensIDMap inSensToSensIDMap;
    CtxInSensToSensIDSetMap inSensToSensSetMap;
    NodeLocationSetMap gepObjNodeMap;

    u64_t objNodeNum;
    u64_t ptrNodeNum;

    typename CtxPAGEdge<Ctx>::PAGKindToEdgeSetMapTy PAGEdgeKindToSetMap;  // < PAG edge map
public:
    static CtxPAG<Ctx> *ctxPAG;
    CtxPAG() : pag(nullptr), totalNodeNum(0), objNodeNum(0), ptrNodeNum(0) {}

    //virtual void initFromPAG(PAG *) = 0;
    u64_t inline getObjNodeNum() {
        return objNodeNum;
    }

    u64_t inline getPtrNodeNum() {
        return ptrNodeNum;
    }

    virtual std::string getGraphName() = 0;
    virtual void addNodes() = 0;

    virtual void dump(std::string) = 0;

    static CtxPAG<Ctx> *getCtxPAG() {
        return ctxPAG;
    }

    inline bool isBlkPtr(NodeID id) const {
        return (SymbolTableInfo::isBlkPtr(id));
    }
    inline bool isNullPtr(NodeID id) const {
        return (SymbolTableInfo::isNullPtr(id));
    }
    inline bool isBlkObjOrConstantObj(NodeID id) const {
        return (isBlkObj(id) || isConstantObj(id));
    }
    inline bool isBlkObj(NodeID id) const {
        return SymbolTableInfo::isBlkObj(id);
    }
    inline bool isConstantObj(NodeID id) const {
        return SymbolTableInfo::isConstantObj(id);
    }
    inline bool isNonPointerObj(NodeID id) const {
        CtxPAGNode<Ctx>* node = this->getCtxPAGNode(id);
        if (auto *fiNode = llvm::dyn_cast<CtxFIObjPN<Ctx>>(node)) {
            return !fiNode->getMemObj()->hasPtrObj();
        }
        else if (auto *gepNode = llvm::dyn_cast<CtxGepObjPN<Ctx>>(node)) {
            return (gepNode->getMemObj()->isNonPtrFieldObj(gepNode->getLocationSet()));
        }
        else if (llvm::isa<CtxDummyObjPN<Ctx>>(node)) {
            return false;
        }
        else {
            assert(false && "expecting a object node");
            return false;
        }
    }

    /// Base and Offset methods for Value and Object node
    //@{
    /// Get a base pointer node given a field pointer
    NodeID getBaseValNode(NodeID nodeId);
    LocationSet getLocationSetFromBaseNode(NodeID nodeId);

    inline NodeID getBaseObjNode(NodeID id) const {
        CtxPAGNode<Ctx>* node = this->getCtxPAGNode(id);
        auto iter = inSensToSensIDMap.find(std::make_pair(getBaseObj(id)->getSymId(), node->getContext()));
        assert(iter != inSensToSensIDMap.end());
        return iter->second;
    }

    inline const MemObj* getBaseObj(NodeID id) const {
        const CtxPAGNode<Ctx>* node = this->getCtxPAGNode(id);
        assert(llvm::isa<CtxObjPN<Ctx>>(node) && "need an object node");
        const auto* obj = llvm::cast<CtxObjPN<Ctx>>(node);
        return obj->getMemObj();
    }

    inline NodeID getObjectNode(const MemObj* obj, Ctx ctx) const {
        auto iter = inSensToSensIDMap.find(std::make_pair(obj->getSymId(), ctx));
        assert(iter != inSensToSensIDMap.end());

        return iter->second;
    }

    inline NodeID getFIObjNode(const MemObj* obj, Ctx ctx) const {
        auto iter = inSensToSensIDMap.find(std::make_pair(obj->getSymId(), ctx));
        assert(iter != inSensToSensIDMap.end());

        return iter->second;
    }

    inline NodeID getFIObjNode(NodeID id) const {
        CtxPAGNode<Ctx>* node = this->getCtxPAGNode(id);
        assert(llvm::isa<CtxObjPN<Ctx>>(node) && "need an object node");

        CtxObjPN<Ctx> *obj = llvm::cast<CtxObjPN<Ctx>>(node);
        return getFIObjNode(obj->getMemObj(), node->getContext());
    }

    //@}

    // Dummy Nodes
    // Reuse the same Node ID, no context as well
    void addDummyNodes() {
        this->addDummyObjNode(pag->getGNode(pag->getBlackHoleNode())); // node 0
        this->addDummyObjNode(pag->getGNode(pag->getConstantNode())); // node 1

        this->addDummyValNode(pag->getGNode(pag->getBlkPtr())); // node 2
        this->addDummyValNode(pag->getGNode(pag->getNullPtr())); // node 3

        // SVF reserve id 0~3 to identify dummy nodes
        totalNodeNum = 4;
    }

    void addDummyObjNode(PAGNode *objNode) {
        auto *dummyObjNode = llvm::dyn_cast<DummyObjPN>(objNode);
        assert(dummyObjNode);

        addNodeInCtx(objNode, Ctx::emptyCtx(), dummyObjNode->getId());
    }

    void addDummyValNode(PAGNode *valNode) {
        auto *dummyValNode = llvm::dyn_cast<DummyValPN>(valNode);
        assert(dummyValNode);

        addNodeInCtx(valNode, Ctx::emptyCtx(), dummyValNode->getId());
    }

    void addNodeInCtx(PAGNode *node, Ctx ctx, NodeID id = UINT32_MAX) {
        CtxPAGNode<Ctx> *ctxNode = nullptr;

        if (id == UINT32_MAX) {
            id = totalNodeNum;
            totalNodeNum ++;
        }

        switch (node->getNodeKind()) {
            case PAGNode::ValNode:
                ctxNode = new CtxValPN<Ctx>(llvm::dyn_cast<ValPN>(node), id, ctx);
                break;
            case PAGNode::RetNode:
                ctxNode = new CtxRetPN<Ctx>(llvm::dyn_cast<RetPN>(node), id, ctx);
                break;
            case PAGNode::VarargNode:
                ctxNode = new CtxVarArgPN<Ctx>(llvm::dyn_cast<VarArgPN>(node), id, ctx);
                break;
            case PAGNode::GepValNode:
                ctxNode = new CtxGepValPN<Ctx>(llvm::dyn_cast<GepValPN>(node), id, ctx);
                break;
            case PAGNode::GepObjNode:
                ctxNode = new CtxGepObjPN<Ctx>(llvm::dyn_cast<GepObjPN>(node), id, ctx);
                break;
            case PAGNode::FIObjNode:
                ctxNode = new CtxFIObjPN<Ctx>(llvm::dyn_cast<FIObjPN>(node), id, ctx);
                break;
            case PAGNode::DummyValNode:
                ctxNode = new CtxDummyValPN<Ctx>(llvm::dyn_cast<DummyValPN>(node), id, ctx);
                break;
            case PAGNode::DummyObjNode:
                ctxNode = new CtxDummyObjPN<Ctx>(llvm::dyn_cast<DummyObjPN>(node), id, ctx);
                break;
            default:
                assert(false);
        }

        if (llvm::isa<CtxObjPN<Ctx>>(ctxNode)) {
            this->objNodeNum ++;
        } else {
            this->ptrNodeNum ++;
        }

        assert(ctxNode);
        // must be adding a new node
        if (inSensToSensIDMap.find(std::make_pair(node->getId(), ctx)) != inSensToSensIDMap.end()) {
            return;
        }

        inSensToSensIDMap[std::make_pair(node->getId(), ctx)] = id;
        inSensToSensSetMap[node->getId()].insert(id);

        if (llvm::isa<CtxGepObjPN<Ctx>>(ctxNode)) {
            auto *gepObj = llvm::dyn_cast<CtxGepObjPN<Ctx>>(ctxNode);
            gepObjNodeMap[std::make_tuple(gepObj->getMemObj()->getSymId(), ctx, gepObj->getLocationSet())] = gepObj->getId();
        }
        this->addGNode(id, ctxNode);
    }

    NodeID getGepObjNode(const MemObj* obj, Ctx ctx, const LocationSet& ls) {
        NodeID base = getObjectNode(obj, ctx);

        /// if this obj is field-insensitive, just return the field-insensitive node.
        if (obj->isFieldInsensitive())
            return getFIObjNode(obj, ctx);

        LocationSet newLS = SymbolTableInfo::Symbolnfo()->getModulusOffset(obj->getTypeInfo(),ls);

        auto iter = gepObjNodeMap.find(std::make_tuple(obj->getSymId(), ctx, newLS));

        assert(iter != gepObjNodeMap.end());
        return iter->second;
    }


    NodeID getGepObjNode(NodeID id, const LocationSet &ls) {
        CtxPAGNode<Ctx>* node = ctxPAG->getCtxPAGNode(id);

        if (auto *gepNode = llvm::dyn_cast<CtxGepObjPN<Ctx>>(node))
            return getGepObjNode(gepNode->getMemObj(), node->getContext(), gepNode->getLocationSet() + ls);
        else if (CtxFIObjPN<Ctx> *baseNode = llvm::dyn_cast<CtxFIObjPN<Ctx>>(node))
            return getGepObjNode(baseNode->getMemObj(), node->getContext(), ls);
        else {
            assert(false && "new gep obj node kind?");
            return id;
        }
    }

    bool hasIntraEdge(CtxPAGNode<Ctx>* src, CtxPAGNode<Ctx>* dst, PAGEdge::PEDGEK kind) {
        CtxPAGEdge<Ctx> edge(src,dst,kind);
        return PAGEdgeKindToSetMap[kind].find(&edge) != PAGEdgeKindToSetMap[kind].end();
    }

    bool hasInterEdge(CtxPAGNode<Ctx>* src, CtxPAGNode<Ctx>* dst, PAGEdge::PEDGEK kind, const llvm::Instruction* callInst) {
        CtxPAGEdge<Ctx> edge(src,dst,PAGEdge::makeEdgeFlagWithCallInst(kind,callInst));
        return PAGEdgeKindToSetMap[kind].find(&edge) != PAGEdgeKindToSetMap[kind].end();
    }

    bool addEdge(CtxPAGNode<Ctx>* src, CtxPAGNode<Ctx>* dst, CtxPAGEdge<Ctx>* edge) {
        src->addOutEdge(edge);
        dst->addInEdge(edge);

        bool added = PAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge).second;
        this->incEdgeNum();
        assert(added && "duplicated edge, not added!!!");

        return true;
    }

    inline typename CtxPAGEdge<Ctx>::PAGEdgeSetTy& getEdgeSet(PAGEdge::PEDGEK kind) {
        return PAGEdgeKindToSetMap[kind];
    }

    bool isEmptyCtxNode(NodeID id) {
        const auto &ctxSrcIDs = inSensToSensSetMap[id];
        if (ctxSrcIDs.size() == 1) {
            return this->getGNode(*ctxSrcIDs.begin())->getContext().isEmptyCtx();
        }
    }

    // duplicate insensitive edges to sensitive PAG
    // NodeID is the ID in context insensitive PAG!
    // TODO: need to handle Global Variables differently
    template <typename EdgeType>
    void dupIntraCtxEdge(PAGEdge *pagEdge) {
        NodeID src = pagEdge->getSrcID();
        NodeID dest = pagEdge->getDstID();
        const auto &ctxSrcIDs = inSensToSensSetMap[src];
        const auto &ctxDestIDs = inSensToSensSetMap[dest];

        if (isEmptyCtxNode(src) || isEmptyCtxNode(dest)) {
            //dummy node, no context
            for (NodeID ctxSrcID : ctxSrcIDs) {
                for (NodeID ctxDestID : ctxDestIDs) {
                    CtxPAGNode<Ctx> *ctxSrcNode = this->getGNode(ctxSrcID);
                    CtxPAGNode<Ctx> *ctxDestNode = this->getGNode(ctxDestID);

                    if (hasIntraEdge(ctxSrcNode, ctxDestNode, EdgeType::TYPE)) {
                        return;
                    }
                    addEdge(ctxSrcNode, ctxDestNode, new EdgeType(ctxSrcNode, ctxDestNode, pagEdge));
                }
            }
            return;
        }

        // intra-nodes in one functions should have same size of ctx.
        assert(ctxSrcIDs.size() == ctxDestIDs.size());

        for (NodeID ctxSrcID : ctxSrcIDs) {
            CtxPAGNode<Ctx> *ctxSrcNode = this->getGNode(ctxSrcID);
            auto destNode = inSensToSensIDMap.find(std::make_pair(dest, ctxSrcNode->getContext()));

            // we should have the corresponding node in the same context
            assert(destNode != inSensToSensIDMap.end());

            NodeID ctxDestId = (*destNode).second;
            CtxPAGNode<Ctx> *ctxDestNode = this->getGNode(ctxDestId);

            if (hasIntraEdge(ctxSrcNode, ctxDestNode, EdgeType::TYPE)) {
                // already have the edge
                return;
            }
            addEdge(ctxSrcNode, ctxDestNode, new EdgeType(ctxSrcNode, ctxDestNode, pagEdge));
        }
    }

    // The following two kinds of edges requires Context Switch (e.g., call into a different origins)
    virtual void dupCtxCallEdge(CallPE *callEdge) = 0;
    virtual void dupCtxRetEdge(RetPE *retEdge) = 0;

    // TODO: handle it later
    void dupCtxForkEdge(NodeID src, NodeID dest) {}
    void dupCtxJoinEdge(NodeID src, NodeID dest) {}

    void addIntraEdges() {
        PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
        for (auto edge : addrs) {
            dupIntraCtxEdge<CtxAddrPE<Ctx>>(edge);
        }

        PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
        for (auto edge : copys) {
            dupIntraCtxEdge<CtxCopyPE<Ctx>>(edge);
        }

        PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Load);
        for (auto edge : stores) {
            dupIntraCtxEdge<CtxLoadPE<Ctx>>(edge);
        }

        PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Store);
        for (auto edge : loads) {
            dupIntraCtxEdge<CtxStorePE<Ctx>>(edge);
        }

        PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
        for (auto edge : vgeps) {
            dupIntraCtxEdge<CtxVariantGepPE<Ctx>>(edge);
        }

        PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
        for (auto edge : ngeps) {
            dupIntraCtxEdge<CtxNormalGepPE<Ctx>>(edge);
        }
    }

    void addInterEdges() {
        // Call/Ret
        PAGEdge::PAGEdgeSetTy& calls = pag->getEdgeSet(PAGEdge::Call);
        for (auto edge : calls) {
            dupCtxCallEdge(llvm::dyn_cast<CallPE>(edge));
        }

        PAGEdge::PAGEdgeSetTy& rets = pag->getEdgeSet(PAGEdge::Ret);
        for (auto edge : rets) {
            dupCtxRetEdge(llvm::dyn_cast<RetPE>(edge));
        }

        // Fork/Join
        PAGEdge::PAGEdgeSetTy& tdfks = pag->getEdgeSet(PAGEdge::ThreadFork);
        for (auto edge : tdfks) {
            dupCtxForkEdge(edge->getSrcID(),edge->getDstID());
        }

        PAGEdge::PAGEdgeSetTy& tdjns = pag->getEdgeSet(PAGEdge::ThreadJoin);
        for (auto edge : tdjns) {
            dupCtxJoinEdge(edge->getSrcID(),edge->getDstID());
        }
    }

    virtual void addEdges() {
        addIntraEdges();
        addInterEdges();
    }

    void initFromPAG(PAG *pag) {
        this->pag = pag;
        this->addDummyNodes();

        // add nodes
        addNodes();

        // add edges
        addEdges();
    }

    // getters and setters
    CtxPAGNode<Ctx> *getCtxPAGNode(NodeID id) const {
        return this->getGNode(id);
    }
};


#endif //SVF_ORIGIN_CTXPAG_H
