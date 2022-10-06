//
// Created by peiming on 5/19/19.
//

#ifndef SVF_ORIGIN_CTXCONSGEDGE_H
#define SVF_ORIGIN_CTXCONSGEDGE_H

#include "MemoryModel/ConsG.h"
#include "MemoryModel/CtxPAG.h"

#include "Util/WorkList.h"

template <typename Ctx>
class CtxConstraintNode;

template <typename Ctx>
class CtxConstraintEdge : public GenericEdge<CtxConstraintNode<Ctx>> {
private:
    EdgeID edgeId;
public:
    /// Constructor
    CtxConstraintEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, ConstraintEdge::ConstraintEdgeK k, EdgeID id = 0)
            : GenericEdge<CtxConstraintNode<Ctx>>(s,d,k),edgeId(id) {}
    /// Destructor
    ~CtxConstraintEdge() {}

    /// Return edge ID
    inline EdgeID getEdgeID() const {
        return edgeId;
    }
    /// ClassOf
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Addr ||
               edge->getEdgeKind() == ConstraintEdge::Copy ||
               edge->getEdgeKind() == ConstraintEdge::Store ||
               edge->getEdgeKind() == ConstraintEdge::Load ||
               edge->getEdgeKind() == ConstraintEdge::NormalGep ||
               edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }
    /// Constraint edge type

    typedef std::set<CtxConstraintEdge<Ctx>*, typename CtxConstraintEdge<Ctx>::equalGEdge> ConstraintEdgeSetTy;
    //typedef typename GenericNode<CtxConstraintNode<Ctx>, CtxConstraintEdge<Ctx>>::GEdgeSetTy ConstraintEdgeSetTy;

};


/*!
 * Copy edge
 */
template <typename Ctx>
class CtxAddrCGEdge: public CtxConstraintEdge<Ctx> {
public:
    CtxAddrCGEdge() = delete;                      ///< place holder
    CtxAddrCGEdge(const CtxAddrCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxAddrCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::Addr;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxAddrCGEdge<Ctx> *) {
        return true;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Addr;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Addr;
    }

    /// constructor
    CtxAddrCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, EdgeID id)
            : CtxConstraintEdge<Ctx>(s, d, ConstraintEdge::Addr, id) {

        CtxPAGNode<Ctx>* node = CtxPAG<Ctx>::getCtxPAG()->getCtxPAGNode(s->getId());
        assert(!llvm::isa<CtxDummyValPN<Ctx>>(node) && "a dummy node??");
    }
};


/*!
 * Copy edge
 */
template <typename Ctx>
class CtxCopyCGEdge: public CtxConstraintEdge<Ctx> {
public:
    CtxCopyCGEdge() = delete;                      ///< place holder
    CtxCopyCGEdge(const CtxCopyCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxCopyCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::Copy;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxCopyCGEdge<Ctx> *) {
        return true;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Copy;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Copy;
    }

    /// constructor
    CtxCopyCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, EdgeID id)
            : CtxConstraintEdge<Ctx>(s, d, ConstraintEdge::Copy, id) {}
};


/*!
 * Store edge
 */
template <typename Ctx>
class CtxStoreCGEdge: public CtxConstraintEdge<Ctx> {
public:
    CtxStoreCGEdge() = delete;                      ///< place holder
    CtxStoreCGEdge(const CtxStoreCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxStoreCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::Store;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxStoreCGEdge<Ctx> *) {
        return true;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Store;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Store;
    }

    /// constructor
    CtxStoreCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, EdgeID id)
            : CtxConstraintEdge<Ctx>(s, d, ConstraintEdge::Store, id) {}
};


/*!
 * Load edge
 */
template <typename Ctx>
class CtxLoadCGEdge: public CtxConstraintEdge<Ctx> {
public:
    CtxLoadCGEdge() = delete;                      ///< place holder
    CtxLoadCGEdge(const CtxLoadCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxLoadCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::Load;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxLoadCGEdge<Ctx> *) {
        return true;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Load;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::Load;
    }

    /// Constructor
    CtxLoadCGEdge(CtxConstraintNode<Ctx>* s, CtxConstraintNode<Ctx>* d, EdgeID id)
            : CtxConstraintEdge<Ctx>(s, d, ConstraintEdge::Load, id) {}
};


/*!
 * Gep edge
 */
template <typename Ctx>
class CtxGepCGEdge: public CtxConstraintEdge<Ctx> {
public:
    CtxGepCGEdge() = delete;                      ///< place holder
    CtxGepCGEdge(const CtxGepCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxGepCGEdge<Ctx> &) = delete; ///< place holder

protected:
    /// Constructor
    CtxGepCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, ConstraintEdge::ConstraintEdgeK k, EdgeID id)
            : CtxConstraintEdge<Ctx>(s,d,k,id) {}
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxGepCGEdge *) {
        return true;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::NormalGep ||
               edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::NormalGep ||
               edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }
};

/*!
 * Gep edge with fixed offset size
 */
template <typename Ctx>
class CtxNormalGepCGEdge : public CtxGepCGEdge<Ctx> {
public:
    CtxNormalGepCGEdge() = delete;                      ///< place holder
    CtxNormalGepCGEdge(const CtxNormalGepCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxNormalGepCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::NormalGep;

    LocationSet ls;	///< location set of the gep edge
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxNormalGepCGEdge *) {
        return true;
    }
    static inline bool classof(const CtxGepCGEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::NormalGep;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::NormalGep;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::NormalGep;
    }

    /// Constructor
    CtxNormalGepCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, const LocationSet& l, EdgeID id)
            : CtxGepCGEdge<Ctx>(s, d, ConstraintEdge::NormalGep, id), ls(l) {}

    /// Get location set of the gep edge
    inline const LocationSet& getLocationSet() const {
        return ls;
    }

    /// Get location set of the gep edge
    inline const u32_t getOffset() const {
        return ls.getOffset();
    }
};

/*!
 * Gep edge with variant offset size
 */
template <typename Ctx>
class CtxVariantGepCGEdge : public CtxGepCGEdge<Ctx> {
public:
    CtxVariantGepCGEdge() = delete;                      ///< place holder
    CtxVariantGepCGEdge(const CtxVariantGepCGEdge<Ctx> &) = delete;  ///< place holder
    void operator=(const CtxVariantGepCGEdge<Ctx> &) = delete; ///< place holder

    static const ConstraintEdge::ConstraintEdgeK TYPE = ConstraintEdge::VariantGep;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CtxVariantGepCGEdge *) {
        return true;
    }
    static inline bool classof(const CtxGepCGEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }
    static inline bool classof(const CtxConstraintEdge<Ctx> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }
    static inline bool classof(const GenericEdge<CtxConstraintNode<Ctx>> *edge) {
        return edge->getEdgeKind() == ConstraintEdge::VariantGep;
    }

    /// Constructor
    CtxVariantGepCGEdge(CtxConstraintNode<Ctx> *s, CtxConstraintNode<Ctx> *d, EdgeID id)
            : CtxGepCGEdge<Ctx>(s, d, ConstraintEdge::VariantGep, id)
    {}
};


#endif //SVF_ORIGIN_CTXCONSGEDGE_H
