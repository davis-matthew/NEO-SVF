//
// Created by peiming on 5/10/19.
//

#ifndef SVF_ORIGIN_CTXPAGEDGE_H
#define SVF_ORIGIN_CTXPAGEDGE_H

#include "MemoryModel/PAGEdge.h"
#include "MemoryModel/CtxPAGNode.h"

template<class Cond>
class CtxPAGEdge : public GenericEdge<CtxPAGNode<Cond>> {
private:
    const llvm::Value *value;    ///< LLVM value
    const llvm::BasicBlock *basicBlock;   ///< LLVM BasicBlock
    EdgeID edgeId;                    ///< Edge ID

    typedef GenericEdge<CtxPAGNode<Cond>> BaseEdgeTy;
    typedef GenericNode<CtxPAGNode<Cond>, CtxPAGEdge<Cond>> BaseNodeTy;

    typedef typename GenericEdge<CtxPAGNode<Cond>>::GEdgeFlag EdgeFlag;
    typedef typename GenericEdge<CtxPAGNode<Cond>>::GEdgeKind EdgeKind;

public:
    static Size_t totalEdgeNum;        ///< Total edge number

    CtxPAGEdge(CtxPAGNode<Cond>* s, CtxPAGNode<Cond>* d, EdgeFlag k)
        : GenericEdge<CtxPAGNode<Cond>>(s, d, k), value(nullptr), basicBlock(nullptr) {}

    /// Return Edge ID
    inline EdgeID getEdgeID() const {
        return edgeId;
    }

    /// Get/set methods for llvm instruction
    inline const llvm::Instruction *getInst() const {
        return llvm::dyn_cast<llvm::Instruction>(value);
    }

    inline void setValue(const llvm::Value *val) {
        value = val;
    }
    inline const llvm::Value *getValue() const {
        return value;
    }

    inline void setBB(const llvm::BasicBlock *bb) {
        basicBlock = bb;
    }

    inline const llvm::BasicBlock *getBB() const {
        return basicBlock;
    }

    static inline bool classof(const CtxPAGNode<Cond>* edge) {
        return true;
    }
    //reuse PAGEdge types
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>>* edge) {
        return edge->getEdgeKind() == PAGEdge::Addr ||
               edge->getEdgeKind() == PAGEdge::Copy ||
               edge->getEdgeKind() == PAGEdge::Store ||
               edge->getEdgeKind() == PAGEdge::Load ||
               edge->getEdgeKind() == PAGEdge::Call ||
               edge->getEdgeKind() == PAGEdge::Ret ||
               edge->getEdgeKind() == PAGEdge::NormalGep ||
               edge->getEdgeKind() == PAGEdge::VariantGep ||
               edge->getEdgeKind() == PAGEdge::ThreadFork ||
               edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }

    // TODO: maybe need to be extended to handle context here! check it out later!
    /// Compute the unique edgeFlag value from edge kind and call site Instruction.
    static inline EdgeKind makeEdgeFlagWithCallInst(EdgeKind k, const llvm::Instruction* cs) {
        Inst2LabelMap::const_iterator iter = inst2LabelMap.find(cs);
        u64_t label = (iter != inst2LabelMap.end()) ?
                      iter->second : callEdgeLabelCounter++;
        return (label << BaseEdgeTy::EdgeKindMaskBits) | k;
    }

    /// Compute the unique edgeFlag value from edge kind and store Instruction.
    /// Two store instructions may share the same StorePAGEdge
    static inline EdgeKind makeEdgeFlagWithStoreInst(EdgeKind k, const llvm::Value* store) {
        Inst2LabelMap::const_iterator iter = inst2LabelMap.find(store);
        u64_t label = (iter != inst2LabelMap.end()) ?
                      iter->second : storeEdgeLabelCounter++;
        return (label << BaseEdgeTy::EdgeKindMaskBits) | k;
    }

    typedef std::set<CtxPAGEdge<Cond>*, typename CtxPAGEdge<Cond>::equalGEdge> PAGEdgeSetTy;
    typedef llvm::DenseMap<EdgeID, PAGEdgeSetTy> PAGEdgeToSetMapTy;
    typedef PAGEdgeToSetMapTy PAGKindToEdgeSetMapTy;
private:
    typedef llvm::DenseMap<const llvm::Value*, u32_t> Inst2LabelMap;
    static Inst2LabelMap inst2LabelMap; ///< Call site Instruction to label map
    static u64_t callEdgeLabelCounter;  ///< Call site Instruction counter
    static u64_t storeEdgeLabelCounter;  ///< Store Instruction counter
};

/*!
 * Copy edge
 */
template <class Cond>
class CtxAddrPE: public CtxPAGEdge<Cond> {
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::Addr;

    static inline bool classof(const CtxAddrPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }

    CtxAddrPE(CtxPAGNode<Cond> *src, CtxPAGNode<Cond> *dest, PAGEdge *edge) : CtxPAGEdge<Cond>(src, dest, PAGEdge::Addr) {
        assert(llvm::isa<AddrPE>(edge));

        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};


/*!
 * Copy edge
 */
template <class Cond>
class CtxCopyPE: public CtxPAGEdge<Cond> {
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::Copy;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxCopyPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }

    CtxCopyPE(CtxPAGNode<Cond> *src, CtxPAGNode<Cond> *dest, PAGEdge *edge) : CtxPAGEdge<Cond>(src, dest, PAGEdge::Copy) {
        assert(llvm::isa<CopyPE>(edge));

        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};


/*!
 * Store edge
 */
template <class Cond>
class CtxStorePE: public CtxPAGEdge<Cond> {
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::Store;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxStorePE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Store;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Store;
    }

    CtxStorePE(CtxPAGNode<Cond> *src, CtxPAGNode<Cond> *dest, PAGEdge *edge) : CtxPAGEdge<Cond>(src, dest, PAGEdge::Store) {
        assert(llvm::isa<StorePE>(edge));

        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};


/*!
 * Load edge
 */
template<class Cond>
class CtxLoadPE: public CtxPAGEdge<Cond> {
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::Load;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxLoadPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Load;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Load;
    }

    CtxLoadPE(CtxPAGNode<Cond> *src, CtxPAGNode<Cond> *dest, PAGEdge *edge) : CtxPAGEdge<Cond>(src, dest, PAGEdge::Load) {
        assert(llvm::isa<LoadPE>(edge));

        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};


/*!
 * Gep edge
 */
template<class Cond>
class CtxGepPE: public CtxPAGEdge<Cond> {
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxGepPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return 	edge->getEdgeKind() == PAGEdge::NormalGep ||
                  edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return	edge->getEdgeKind() == PAGEdge::NormalGep ||
                  edge->getEdgeKind() == PAGEdge::VariantGep;
    }
protected:
    CtxGepPE(CtxPAGNode<Cond>* s, CtxPAGNode<Cond>* d, PAGEdge::PEDGEK k) : CtxPAGEdge<Cond>(s,d,k) {}
};


/*!
 * Gep edge with a variant offset
 */
template <class Cond>
class CtxVariantGepPE : public CtxGepPE<Cond> {
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::VariantGep;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxVariantGepPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxGepPE<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }

    CtxVariantGepPE(CtxPAGNode<Cond>* s, CtxPAGNode<Cond>* d, PAGEdge *edge) : CtxGepPE<Cond>(s,d,PAGEdge::VariantGep) {
        assert(llvm::isa<VariantGepPE>(edge));

        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};


/*!
 * Gep edge with a fixed offset
 */
template <class Cond>
class CtxNormalGepPE : public CtxGepPE<Cond> {
private:
    LocationSet ls;	///< location set of the gep edge
public:
    static const PAGEdge::PEDGEK TYPE = PAGEdge::NormalGep;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxNormalGepPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxGepPE<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }

    /// offset of the gep edge
    inline u32_t getOffset() const {
        return ls.getOffset();
    }
    inline const LocationSet& getLocationSet() const {
        return ls;
    }

    CtxNormalGepPE(CtxPAGNode<Cond> *s, CtxPAGNode<Cond> *d, PAGEdge *edge) : CtxGepPE<Cond>(s,d,PAGEdge::NormalGep) {
        auto *normalGepPE = llvm::dyn_cast<NormalGepPE>(edge);
        assert(edge);

        this->ls = normalGepPE->getLocationSet();
        this->setValue(edge->getValue());
        this->setBB(edge->getBB());
    }
};

/*!
 * Call edge
 */
template <class Cond>
class CtxCallPE: public CtxPAGEdge<Cond> {
private:
    const llvm::Instruction* inst;		///< llvm instruction for this call
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxCallPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Call;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Call;
    }

    CtxCallPE(CtxPAGNode<Cond>* s, CtxPAGNode<Cond>* d, CallPE *callPE) :
            CtxPAGEdge<Cond>(s,d, callPE->getUnmaskedFlag()), inst(callPE->getCallInst()) {

    }

    /// Get method for the call instruction
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
};


/*!
 * Return edge
 */
template <class Cond>
class CtxRetPE: public CtxPAGEdge<Cond> {
private:
    const llvm::Instruction* inst;		/// the callsite instruction return to
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxRetPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::Ret;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::Ret;
    }

    /// Get method for call instruction at caller
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }

    CtxRetPE(CtxPAGNode<Cond>* s, CtxPAGNode<Cond>* d, RetPE *retPE) :
            CtxPAGEdge<Cond>(s,d, retPE->getUnmaskedFlag()), inst(retPE->getCallInst()) {

    }
};


/*!
 * Thread Fork Edge
 */
template <class Cond>
class CtxTDForkPE: public CtxPAGEdge<Cond> {
private:
    const llvm::Instruction* inst;		///< llvm instruction at the fork site
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxTDForkPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }

    /// Get method for the instruction at the fork site
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
};



/*!
 * Thread Join Edge
 */
template <class Cond>
class CtxTDJoinPE: public CtxPAGEdge<Cond> {
private:
    const llvm::Instruction* inst;		/// the callsite instruction return to
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxTDJoinPE<Cond> *) {
        return true;
    }
    static inline bool classof(const CtxPAGEdge<Cond> *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    static inline bool classof(const GenericEdge<CtxPAGNode<Cond>> *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }

    /// Get method for the instruction at the join site
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
};

#endif //SVF_ORIGIN_CTXPAGEDGE_H
