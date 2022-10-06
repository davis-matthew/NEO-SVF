//
// Created by peiming on 5/10/19.
//

#ifndef SVF_ORIGIN_CTXPAGNODE_H
#define SVF_ORIGIN_CTXPAGNODE_H

#include "MemoryModel/PAGNode.h"
#include "MemoryModel/MemModel.h"

template <class Cond>
class CtxPAGEdge;

/*
 * PAG node, but with context.
 */
template<class Cond>
class CtxPAGNode : public GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> {
private:
    // inner PAG node
    PAGNode *pagNode;
    // ID for the CxtPAGNode, differ from pagNode->nodeID
    NodeID id;
    // Ctx
    Cond ctx;
protected:
    typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy InEdgeKindToSetMap;
    typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy OutEdgeKindToSetMap;
public:
    CtxPAGNode(PAGNode *innerNode, NodeID nodeId, Cond ctx)
    : GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>>(nodeId, innerNode->getNodeKind()), pagNode(innerNode), id(nodeId), ctx(ctx) {
        assert(innerNode);
    }

    virtual ~CtxPAGNode() = default;

    Cond getContext() {
        return ctx;
    }

    NodeID getInnerID() {
        this->pagNode->getId();
    }

    PAGNode *getInnerNode() const{
        return this->pagNode;
    }

    /// Get name of the LLVM value
    virtual const std::string getValueName() {
        this->pagNode->getValueName();
    }

    /// Get/has methods of the components
    /// Delegate to inner PagNode
    inline const llvm::Value* getValue() const {
        this->pagNode->getValue();
    }

    /// Return type of the value
    inline virtual const llvm::Type* getType() const{
        this->pagNode->getType();
    }

    inline bool hasValue() const {
        return this->pagNode->hasValue();
    }

    /// Whether it is a pointer
    virtual inline bool isPointer() const {
        return this->pagNode->isPointer();
    }

    /// Whether it is a top-level pointer
    inline bool isTopLevelPtr() const {
        return this->pagNode->isTopLevelPtr();
    }

    /// Whether it is an address-taken pointer
    inline bool isAddressTakenPtr() const {
        return this->pagNode->isAddressTakenPtr();
    }

    /// Overloading operator << for dumping PAGNode value
    friend llvm::raw_ostream& operator<< (llvm::raw_ostream &o, const CtxPAGNode &node) {
        o << *(node.pagNode) << node.ctx.toString() << "\n";
        return o;
    }


    // EDGE-related functions
    /// Get incoming PAG edges
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy& getIncomingEdges(PAGEdge::PEDGEK kind) {
        return InEdgeKindToSetMap[kind];
    }

    /// Get outgoing PAG edges
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy& getOutgoingEdges(PAGEdge::PEDGEK kind) {
        return OutEdgeKindToSetMap[kind];
    }

    /// Has incoming PAG edges
    inline bool hasIncomingEdges(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        if (it != InEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Has incoming VariantGepEdges
    inline bool hasIncomingVariantGepEdge() const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(PAGEdge::VariantGep);
        if (it != InEdgeKindToSetMap.end()) {
            return (!it->second.empty());
        }
        return false;
    }

    /// Get incoming PAGEdge iterator
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy::iterator getIncomingEdgesBegin(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get incoming PAGEdge iterator
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy::iterator getIncomingEdgesEnd(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }

    /// Has outgoing PAG edges
    inline bool hasOutgoingEdges(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        if (it != OutEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Get outgoing PAGEdge iterator
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy::iterator getOutgoingEdgesBegin(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get outgoing PAGEdge iterator
    inline typename CtxPAGEdge<Cond>::PAGEdgeSetTy::iterator getOutgoingEdgesEnd(PAGEdge::PEDGEK kind) const {
        typename CtxPAGEdge<Cond>::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }
    //@}

    ///  add methods of the components
    //@{
    inline void addInEdge(CtxPAGEdge<Cond>* inEdge) {
        typename GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>>::GNodeK kind = inEdge->getEdgeKind();
        InEdgeKindToSetMap[kind].insert(inEdge);
        this->addIncomingEdge(inEdge);
    }

    inline void addOutEdge(CtxPAGEdge<Cond>* outEdge) {
        typename GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>>::GNodeK kind = outEdge->getEdgeKind();
        OutEdgeKindToSetMap[kind].insert(outEdge);
        this->addOutgoingEdge(outEdge);
    }
};

/*
 * Value(Pointer) node
 */
template<class Cond>
class CtxValPN: public CtxPAGNode<Cond> {
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxValPN<Cond>*) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::ValNode ||
               node->getNodeKind() == PAGNode::GepValNode ||
               node->getNodeKind() == PAGNode::DummyValNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::ValNode ||
               node->getNodeKind() == PAGNode::GepValNode ||
               node->getNodeKind() == PAGNode::DummyValNode;
    }

    /// Constructor
    CtxValPN(ValPN *valPn, NodeID id, Cond ctx) :CtxPAGNode<Cond>(valPn, id, ctx) {}
};

/*
 * Memory Object node
 */
template<class Cond>
class CtxObjPN: public CtxPAGNode<Cond> {

protected:
    /// Constructor
    CtxObjPN(ObjPN *objPn, NodeID id, Cond ctx) : CtxPAGNode<Cond>(objPn, id, ctx) {}
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxObjPN *) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::ObjNode ||
               node->getNodeKind() == PAGNode::GepObjNode ||
               node->getNodeKind() == PAGNode::FIObjNode ||
               node->getNodeKind() == PAGNode::DummyObjNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::ObjNode ||
               node->getNodeKind() == PAGNode::GepObjNode ||
               node->getNodeKind() == PAGNode::FIObjNode ||
               node->getNodeKind() == PAGNode::DummyObjNode;
    }

    /// Return memory object
    const MemObj* getMemObj() const {
        ObjPN *objPN = llvm::dyn_cast<ObjPN>(this->getInnerNode());
        assert(objPN && "type mismatch!");

        objPN->getMemObj();
    }

    /// Return type of the value
    inline virtual const llvm::Type* getType() const{
        return getMemObj()->getType();
    }
};


/*
 * Gep Value (Pointer) node, this node can be dynamic generated for field sensitive analysis
 * e.g. memcpy, temp gep value node needs to be created
 * Each Gep Value node is connected to base value node via gep edge
 */
template <class Cond>
class CtxGepValPN: public CtxValPN<Cond> {
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxGepValPN<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxValPN<Cond> * node) {
        return node->getNodeKind() == PAGNode::GepValNode;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::GepValNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::GepValNode;
    }

    /// Constructor
    CtxGepValPN(GepValPN* gepValPN, NodeID id, Cond ctx) : CtxValPN<Cond>(gepValPN, id, ctx) {}

    /// offset of the base value node
    inline u32_t getOffset() const {
        GepValPN *gepValPN = llvm::dyn_cast<GepValPN>(this->getInnerNode());
        assert(gepValPN && "type mismatch!");

        return this->getInnerNode()->getOffset();
    }

    inline const llvm::Type* getType() const {
        GepValPN *gepValPN = llvm::dyn_cast<GepValPN>(this->getInnerNode());
        assert(gepValPN && "type mismatch!");

        return gepValPN->getType();
    }

    u32_t getFieldIdx() const {
        GepValPN *gepValPN = llvm::dyn_cast<GepValPN>(this->getInnerNode());
        assert(gepValPN && "type mismatch!");

        return gepValPN->getFieldIdx();
    }
};


/*
 * Gep Obj node, this is dynamic generated for field sensitive analysis
 * Each gep obj node is one field of a MemObj (base)
 */
template <class Cond>
class CtxGepObjPN: public CtxObjPN<Cond> {
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxGepObjPN<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxObjPN<Cond> * node) {
        return node->getNodeKind() == PAGNode::GepObjNode;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::GepObjNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::GepObjNode;
    }

    /// Constructor
    CtxGepObjPN(GepObjPN *gepObjPn, NodeID id, Cond ctx) : CtxObjPN<Cond>(gepObjPn, id, ctx) {}

    /// offset of the mem object
    inline const LocationSet& getLocationSet() const {
        GepObjPN *gepValPN = llvm::dyn_cast<GepObjPN>(this->getInnerNode());
        assert(gepValPN && "type mismatch!");

        return gepValPN->getLocationSet();
    }

    /// Return the type of this gep object
    inline const llvm::Type* getType() {
        GepObjPN *gepValPN = llvm::dyn_cast<GepObjPN>(this->getInnerNode());
        assert(gepValPN && "type mismatch!");

        return gepValPN->getType();
    }
};

/*
 * Field-insensitive Gep Obj node, this is dynamic generated for field sensitive analysis
 * Each field-insensitive gep obj node represents all fields of a MemObj (base)
 */
template <class Cond>
class CtxFIObjPN: public CtxObjPN<Cond> {
public:

    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxFIObjPN *) {
        return true;
    }
    static inline bool classof(const CtxObjPN<Cond> * node) {
        return node->getNodeKind() == PAGNode::FIObjNode;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::FIObjNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::FIObjNode;
    }

    /// Constructor
    CtxFIObjPN(FIObjPN *fiObjPN, NodeID id, Cond ctx) : CtxObjPN<Cond>(fiObjPN, id, ctx) {}
};

/*
 * Unique Return node of a procedure
 */
template <class Cond>
class CtxRetPN: public CtxPAGNode<Cond> {

public:

    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxRetPN *) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::RetNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::RetNode;
    }

    /// Constructor
    CtxRetPN(RetPN *retPN, NodeID id, Cond ctx) : CtxPAGNode<Cond>(retPN, id, ctx) {}
};


/*
 * Unique vararg node of a procedure
 */
template <class Cond>
class CtxVarArgPN: public CtxPAGNode<Cond> {

public:

    //Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxVarArgPN *) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::VarargNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::VarargNode;
    }

    /// Constructor
    CtxVarArgPN(VarArgPN *varArgPn, NodeID id, Cond ctx) : CtxPAGNode<Cond>(varArgPn, id, ctx) {}
};



// PEIMING:
// DO we really need Context-sensitive DummyVal/ObjPN?

/*
 * Dummy node
 */
template <class Cond>
class CtxDummyValPN: public CtxValPN<Cond> {
public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxDummyValPN *) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::DummyValNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::DummyValNode;
    }
    //@}

    /// Constructor
    CtxDummyValPN(DummyValPN *dummyValPn, NodeID id, Cond ctx) : CtxValPN<Cond>(dummyValPn, id, ctx) {}
};


/*
 * Dummy node
 */
template <class Cond>
class CtxDummyObjPN: public CtxObjPN<Cond> {
public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxDummyObjPN *) {
        return true;
    }
    static inline bool classof(const CtxPAGNode<Cond> *node) {
        return node->getNodeKind() == PAGNode::DummyObjNode;
    }
    static inline bool classof(const GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> *node) {
        return node->getNodeKind() == PAGNode::DummyObjNode;
    }
    //@}

    /// Constructor
    CtxDummyObjPN(DummyObjPN *dummyObjPn, NodeID id, Cond ctx) : CtxObjPN<Cond>(dummyObjPn, id, ctx) {}
};

#endif //SVF_ORIGIN_CTXPAGNODE_H
