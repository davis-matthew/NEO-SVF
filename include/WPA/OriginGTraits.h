//
// Created by peiming on 5/20/19.
//

#ifndef SVF_ORIGIN_ORIGINGTRAITS_H
#define SVF_ORIGIN_ORIGINGTRAITS_H

namespace llvm {
    /* !
     * GraphTraits specializations of PAG to be used for the generic graph algorithms.
     * Provide graph traits for tranversing from a PAG node using standard graph traversals.
     */
    template<>
    struct GraphTraits<CtxConstraintNode<OriginID> *>
            : public GraphTraits<GenericNode<CtxConstraintNode<OriginID>, CtxConstraintEdge<OriginID>> *> {
    };

    /// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
    template<>
    struct GraphTraits<Inverse<CtxConstraintNode<OriginID> *>>
            : public GraphTraits<Inverse<GenericNode<CtxConstraintNode<OriginID>, CtxConstraintEdge<OriginID>> *>> {
    };

    template<>
    struct GraphTraits<CtxConstraintGraph<OriginID> *>
            : public GraphTraits<GenericGraph<CtxConstraintNode<OriginID>, CtxConstraintEdge<OriginID>> *> {
        typedef CtxConstraintNode<OriginID> *NodeRef;
    };

    template<>
    struct DOTGraphTraits<CtxConstraintGraph<OriginID> *> : public DefaultDOTGraphTraits {

        typedef CtxConstraintNode<OriginID> NodeType;

        explicit DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

        /// Return name of the graph
        static std::string getGraphName(CtxConstraintGraph<OriginID> *graph) {
            return "Origin_ConstraintG";
        }

        /// Return label of a VFG node with two display mode
        /// Either you can choose to display the name of the value or the whole instruction
        static std::string getNodeLabel(NodeType *n, CtxConstraintGraph<OriginID> *graph) {
            CtxPAGNode<OriginID> *node = CtxPAG<OriginID>::getCtxPAG()->getCtxPAGNode(n->getId());
            bool briefDisplay = false;
            bool nameDisplay = true;

            std::string str;
            raw_string_ostream rawstr(str);

            if (briefDisplay) {
                if (isa<CtxValPN<OriginID>>(node)) {
                    if (nameDisplay)
                        rawstr << node->getId() << ":" << node->getValueName();
                    else
                        rawstr << node->getId();
                } else
                    rawstr << node->getId();
            } else {
                // print the whole value
                if (!isa<CtxDummyValPN<OriginID>>(node) && !isa<CtxDummyObjPN<OriginID>>(node))
                    rawstr << *node->getValue();
                else
                    rawstr << "";

            }

            return rawstr.str();
        }

        static std::string getNodeAttributes(NodeType *n, CtxConstraintGraph<OriginID> *graph) {
            CtxPAGNode<OriginID> *node = CtxPAG<OriginID>::getCtxPAG()->getCtxPAGNode(n->getId());

            if (isa<CtxValPN<OriginID>>(node)) {
                if (isa<CtxGepValPN<OriginID>>(node))
                    return "shape=hexagon";
                else if (isa<CtxDummyValPN<OriginID>>(node))
                    return "shape=diamond";
                else
                    return "shape=circle";
            } else if (isa<CtxObjPN<OriginID>>(node)) {
                if (isa<CtxGepObjPN<OriginID>>(node))
                    return "shape=doubleoctagon";
                else if (isa<CtxFIObjPN<OriginID>>(node))
                    return "shape=septagon";
                else if (isa<CtxDummyObjPN<OriginID>>(node))
                    return "shape=Mcircle";
                else
                    return "shape=doublecircle";
            } else if (isa<CtxRetPN<OriginID>>(node)) {
                return "shape=Mrecord";
            } else if (isa<CtxVarArgPN<OriginID>>(node)) {
                return "shape=octagon";
            } else {
                assert(0 && "no such kind node!!");
            }
            return "";
        }

        template<class EdgeIter>
        static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, CtxConstraintGraph<OriginID> *pag) {
            CtxConstraintEdge<OriginID> *edge = *(EI.getCurrent());

            assert(edge && "No edge found!!");
            if (edge->getEdgeKind() == ConstraintEdge::Addr) {
                return "color=green";
            } else if (edge->getEdgeKind() == ConstraintEdge::Copy) {
                return "color=black";
            } else if (edge->getEdgeKind() == ConstraintEdge::NormalGep
                       || edge->getEdgeKind() == ConstraintEdge::VariantGep) {
                return "color=purple";
            } else if (edge->getEdgeKind() == ConstraintEdge::Store) {
                return "color=blue";
            } else if (edge->getEdgeKind() == ConstraintEdge::Load) {
                return "color=red";
            } else {
                assert(0 && "No such kind edge!!");
            }
            return "";
        }

        template<class EdgeIter>
        static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
            return "";
        }
    };


    /* !
  * GraphTraits specializations of PAG to be used for the generic graph algorithms.
  * Provide graph traits for tranversing from a PAG node using standard graph traversals.
  */
    template<>
    struct GraphTraits<CtxConstraintNode<ctx::CallSite> *>
            : public GraphTraits<GenericNode<CtxConstraintNode<ctx::CallSite>, CtxConstraintEdge<ctx::CallSite>> *> {
    };

    /// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
    template<>
    struct GraphTraits<Inverse<CtxConstraintNode<ctx::CallSite> *>>
            : public GraphTraits<Inverse<GenericNode<CtxConstraintNode<ctx::CallSite>, CtxConstraintEdge<ctx::CallSite>> *>> {
    };

    template<>
    struct GraphTraits<CtxConstraintGraph<ctx::CallSite> *>
            : public GraphTraits<GenericGraph<CtxConstraintNode<ctx::CallSite>, CtxConstraintEdge<ctx::CallSite>> *> {
        typedef CtxConstraintNode<ctx::CallSite> *NodeRef;
    };

    template<>
    struct DOTGraphTraits<CtxConstraintGraph<ctx::CallSite> *> : public DefaultDOTGraphTraits {

        typedef CtxConstraintNode<ctx::CallSite> NodeType;

        explicit DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

        /// Return name of the graph
        static std::string getGraphName(CtxConstraintGraph<ctx::CallSite> *graph) {
            return "Origin_ConstraintG";
        }

        /// Return label of a VFG node with two display mode
        /// Either you can choose to display the name of the value or the whole instruction
        static std::string getNodeLabel(NodeType *n, CtxConstraintGraph<ctx::CallSite> *graph) {
            CtxPAGNode<ctx::CallSite> *node = CtxPAG<ctx::CallSite>::getCtxPAG()->getCtxPAGNode(n->getId());
            bool briefDisplay = false;
            bool nameDisplay = true;

            std::string str;
            raw_string_ostream rawstr(str);

            if (briefDisplay) {
                if (isa<CtxValPN<ctx::CallSite>>(node)) {
                    if (nameDisplay)
                        rawstr << node->getId() << ":" << node->getValueName();
                    else
                        rawstr << node->getId();
                } else
                    rawstr << node->getId();
            } else {
                // print the whole value
                if (!isa<CtxDummyValPN<ctx::CallSite>>(node) && !isa<CtxDummyObjPN<ctx::CallSite>>(node))
                    rawstr << *node->getValue();
                else
                    rawstr << "";

            }

            return rawstr.str();
        }

        static std::string getNodeAttributes(NodeType *n, CtxConstraintGraph<ctx::CallSite> *graph) {
            CtxPAGNode<ctx::CallSite> *node = CtxPAG<ctx::CallSite>::getCtxPAG()->getCtxPAGNode(n->getId());

            if (isa<CtxValPN<ctx::CallSite>>(node)) {
                if (isa<CtxGepValPN<ctx::CallSite>>(node))
                    return "shape=hexagon";
                else if (isa<CtxDummyValPN<ctx::CallSite>>(node))
                    return "shape=diamond";
                else
                    return "shape=circle";
            } else if (isa<CtxObjPN<ctx::CallSite>>(node)) {
                if (isa<CtxGepObjPN<ctx::CallSite>>(node))
                    return "shape=doubleoctagon";
                else if (isa<CtxFIObjPN<ctx::CallSite>>(node))
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
        static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, CtxConstraintGraph<ctx::CallSite> *pag) {
            CtxConstraintEdge<ctx::CallSite> *edge = *(EI.getCurrent());

            assert(edge && "No edge found!!");
            if (edge->getEdgeKind() == ConstraintEdge::Addr) {
                return "color=green";
            } else if (edge->getEdgeKind() == ConstraintEdge::Copy) {
                return "color=black";
            } else if (edge->getEdgeKind() == ConstraintEdge::NormalGep
                       || edge->getEdgeKind() == ConstraintEdge::VariantGep) {
                return "color=purple";
            } else if (edge->getEdgeKind() == ConstraintEdge::Store) {
                return "color=blue";
            } else if (edge->getEdgeKind() == ConstraintEdge::Load) {
                return "color=red";
            } else {
                assert(0 && "No such kind edge!!");
            }
            return "";
        }

        template<class EdgeIter>
        static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
            return "";
        }
    };
}


#endif //SVF_ORIGIN_ORIGINGTRAITS_H
