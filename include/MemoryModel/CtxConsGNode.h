//
// Created by peiming on 5/19/19.
//

#ifndef SVF_ORIGIN_CTXCONSGNODE_H
#define SVF_ORIGIN_CTXCONSGNODE_H

template <typename Ctx>
class CtxConstraintNode : public GenericNode<CtxConstraintNode<Ctx>,CtxConstraintEdge<Ctx>> {

public:
    typedef typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy::iterator iterator;
    typedef typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy::const_iterator const_iterator;
private:
    bool _isPWCNode;

    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy loadInEdges; ///< all incoming load edge of this node
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy loadOutEdges; ///< all outgoing load edge of this node

    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy storeInEdges; ///< all incoming store edge of this node
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy storeOutEdges; ///< all outgoing store edge of this node

    /// Copy/call/ret/gep incoming edge of this node,
    /// To be noted: this set is only used when SCC detection, and node merges
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy directInEdges;
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy directOutEdges;

    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy addressInEdges; ///< all incoming address edge of this node
    typename CtxConstraintEdge<Ctx>::ConstraintEdgeSetTy addressOutEdges; ///< all outgoing address edge of this node

public:

    CtxConstraintNode(NodeID i)
            : GenericNode<CtxConstraintNode<Ctx>,CtxConstraintEdge<Ctx>> (i,0), _isPWCNode(false) {}

    /// Whether a node involves in PWC, if so, all its points-to elements should become field-insensitive.
    //@{
    inline bool isPWCNode() const {
        return _isPWCNode;
    }
    inline void setPWCNode() {
        _isPWCNode = true;
    }
    //@}

    /// Direct and Indirect PAG edges
    inline bool isdirectEdge(ConstraintEdge::ConstraintEdgeK kind) {
        return (kind == ConstraintEdge::Copy || kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep );
    }
    inline bool isIndirectEdge(ConstraintEdge::ConstraintEdgeK kind) {
        return (kind == ConstraintEdge::Load || kind == ConstraintEdge::Store);
    }

    ///  Iterators
    //@{
    inline iterator directOutEdgeBegin() {
        return directOutEdges.begin();
    }
    inline iterator directOutEdgeEnd() {
        return directOutEdges.end();
    }
    inline iterator directInEdgeBegin() {
        return directInEdges.begin();
    }
    inline iterator directInEdgeEnd() {
        return directInEdges.end();
    }

    inline const_iterator directOutEdgeBegin() const {
        return directOutEdges.begin();
    }
    inline const_iterator directOutEdgeEnd() const {
        return directOutEdges.end();
    }
    inline const_iterator directInEdgeBegin() const {
        return directInEdges.begin();
    }
    inline const_iterator directInEdgeEnd() const {
        return directInEdges.end();
    }

    ConstraintEdge::ConstraintEdgeSetTy& incomingAddrEdges() {
        return addressInEdges;
    }
    ConstraintEdge::ConstraintEdgeSetTy& outgoingAddrEdges() {
        return addressOutEdges;
    }

    inline const_iterator outgoingAddrsBegin() const {
        return addressOutEdges.begin();
    }
    inline const_iterator outgoingAddrsEnd() const {
        return addressOutEdges.end();
    }
    inline const_iterator incomingAddrsBegin() const {
        return addressInEdges.begin();
    }
    inline const_iterator incomingAddrsEnd() const {
        return addressInEdges.end();
    }

    inline const_iterator outgoingLoadsBegin() const {
        return loadOutEdges.begin();
    }
    inline const_iterator outgoingLoadsEnd() const {
        return loadOutEdges.end();
    }
    inline const_iterator incomingLoadsBegin() const {
        return loadInEdges.begin();
    }
    inline const_iterator incomingLoadsEnd() const {
        return loadInEdges.end();
    }

    inline const_iterator outgoingStoresBegin() const {
        return storeOutEdges.begin();
    }
    inline const_iterator outgoingStoresEnd() const {
        return storeOutEdges.end();
    }
    inline const_iterator incomingStoresBegin() const {
        return storeInEdges.begin();
    }
    inline const_iterator incomingStoresEnd() const {
        return storeInEdges.end();
    }
    //@}

    ///  Add constraint graph edges
    //@{
    inline void addIncomingCopyEdge(CtxCopyCGEdge<Ctx> *inEdge) {
        addIncomingDirectEdge(inEdge);
    }
    inline void addIncomingGepEdge(CtxGepCGEdge<Ctx> *inEdge) {
        addIncomingDirectEdge(inEdge);
    }
    inline void addOutgoingCopyEdge(CtxCopyCGEdge<Ctx> *outEdge) {
        addOutgoingDirectEdge(outEdge);
    }
    inline void addOutgoingGepEdge(CtxGepCGEdge<Ctx> *outEdge) {
        addOutgoingDirectEdge(outEdge);
    }
    inline void addIncomingAddrEdge(CtxAddrCGEdge<Ctx>* inEdge) {
        addressInEdges.insert(inEdge);
        this->addIncomingEdge(inEdge);
    }
    inline void addIncomingLoadEdge(CtxLoadCGEdge<Ctx>* inEdge) {
        loadInEdges.insert(inEdge);
        this->addIncomingEdge(inEdge);
    }
    inline void addIncomingStoreEdge(CtxStoreCGEdge<Ctx>* inEdge) {
        storeInEdges.insert(inEdge);
        this->addIncomingEdge(inEdge);
    }
    inline void addIncomingDirectEdge(CtxConstraintEdge<Ctx>* inEdge) {
        assert(inEdge->getDstID() == this->getId());
        bool added1 = directInEdges.insert(inEdge).second;
        bool added2 = this->addIncomingEdge(inEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingAddrEdge(CtxAddrCGEdge<Ctx>* outEdge) {
        addressOutEdges.insert(outEdge);
        this->addOutgoingEdge(outEdge);
    }
    inline void addOutgoingLoadEdge(CtxLoadCGEdge<Ctx>* outEdge) {
        bool added1 = loadOutEdges.insert(outEdge).second;
        bool added2 = this->addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingStoreEdge(CtxStoreCGEdge<Ctx>* outEdge) {
        bool added1 = storeOutEdges.insert(outEdge).second;
        bool added2 = this->addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingDirectEdge(CtxConstraintEdge<Ctx>* outEdge) {
        assert(outEdge->getSrcID() == this->getId());
        bool added1 = directOutEdges.insert(outEdge).second;
        bool added2 = this->addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    //@}

    /// Remove constraint graph edges
    //{@
    inline void removeOutgoingAddrEdge(CtxAddrCGEdge<Ctx>* outEdge) {
        Size_t num1 = addressOutEdges.erase(outEdge);
        Size_t num2 = this->removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingAddrEdge(CtxAddrCGEdge<Ctx>* inEdge) {
        Size_t num1 = addressInEdges.erase(inEdge);
        Size_t num2 = this->removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingDirectEdge(CtxConstraintEdge<Ctx>* outEdge) {
        Size_t num1 = directOutEdges.erase(outEdge);
        Size_t num2 = this->removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingDirectEdge(CtxConstraintEdge<Ctx>* inEdge) {
        Size_t num1 = directInEdges.erase(inEdge);
        Size_t num2 = this->removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingLoadEdge(CtxLoadCGEdge<Ctx>* outEdge) {
        Size_t num1 = loadOutEdges.erase(outEdge);
        Size_t num2 = this->removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingLoadEdge(CtxLoadCGEdge<Ctx>* inEdge) {
        Size_t num1 = loadInEdges.erase(inEdge);
        Size_t num2 = this->removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingStoreEdge(CtxStoreCGEdge<Ctx>* outEdge) {
        Size_t num1 = storeOutEdges.erase(outEdge);
        Size_t num2 = this->removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingStoreEdge(CtxStoreCGEdge<Ctx>* inEdge) {
        Size_t num1 = storeInEdges.erase(inEdge);
        Size_t num2 = this->removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }
    //@}


};
#endif //SVF_ORIGIN_CTXCONSGNODE_H
