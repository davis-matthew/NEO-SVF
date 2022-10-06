//
// Created by peiming on 7/30/19.
//

#ifndef SVF_ORIGIN_SHBGRAPH_H
#define SVF_ORIGIN_SHBGRAPH_H

#include <llvm/IR/Instruction.h>
#include <MemoryModel/PointerAnalysis.h>
#include "MemoryModel/GenericGraph.h"
#include "Util/SVFModule.h"


class SHBNode;

class SHBEdge : public GenericEdge<SHBNode> {
public:
    enum EdgeType {Fork, Join, PO, Call, Ret};

private:
    EdgeType edgeTy;
    static u64_t CURRENT_EDGE_ID;

public:
    SHBEdge::EdgeType getType() const {
        return this->edgeTy;
    }
    // FORK and JOIN are inter thread edges
    inline bool isInterThreadEdge() {
        return this->getType() == Fork || this->getType() == Join;
    }

    SHBEdge(SHBNode *src, SHBNode *dst, EdgeType ty) : GenericEdge<SHBNode>(src,dst, 0), edgeTy(ty) {}
};

class SHBNode : public GenericNode<SHBNode, SHBEdge> {
public:
    enum NodeType {Write, Read, Enter, Ret};

private:
    NodeType nodeTy;
    static u64_t CURRENT_NODE_ID;
    llvm::Instruction *inst;

private:
    SHBNode(NodeType ty, NodeID id, llvm::Instruction *inst) : GenericNode<SHBNode, SHBEdge>(id, 0), nodeTy(ty), inst(inst) {}

public:
    SHBNode() = delete;

    static SHBNode * createWriteNode(llvm::Instruction * inst) {
        return new SHBNode(NodeType::Write, CURRENT_NODE_ID++, inst);
    }

    static SHBNode * createReadNode(llvm::Instruction * inst) {
        return new SHBNode(NodeType::Read, CURRENT_NODE_ID++, inst);
    }

    static SHBNode * createEnterNode(llvm::Instruction * inst) {
        return new SHBNode(NodeType::Enter, CURRENT_NODE_ID++, inst);
    }

    static SHBNode * createRetNode(llvm::Instruction * inst) {
        return new SHBNode(NodeType::Ret, CURRENT_NODE_ID++, inst);
    }

    const llvm::Instruction *getInst() {
        return this->inst;
    }

    const llvm::Value *getPointerOperand() {
        if (nodeTy == Write) {
            auto store = llvm::dyn_cast<llvm::StoreInst>(this->getInst());
            assert(store);
            return store->getPointerOperand();
        } else if (nodeTy == Read){
            auto load = llvm::dyn_cast<llvm::LoadInst>(this->getInst());
            assert(load);
            return load->getPointerOperand();
        }

        return nullptr;
    }

    SHBNode::NodeType getType() const {
        return this->nodeTy;
    }
};

class SHBGraph : public GenericGraph<SHBNode, SHBEdge> {
private:
    using Inst2NodeMap = std::map<llvm::Instruction *, SHBNode *>;
    using Func2RetMap = std::map<llvm::Function *, SHBNode *>;
    using Func2EnterMap = std::map<llvm::Function *, SHBNode *>;
    using NodeSet = std::set<SHBNode *>;

private:
    Inst2NodeMap inst2NodeMap;
    Func2RetMap func2RetMap;
    Func2EnterMap func2EnterMap;

    NodeSet writeNodeSet;
    NodeSet readNodeSet;

    PointerAnalysis *PTA;

private:
    explicit SHBGraph(PointerAnalysis *PTA) : PTA(PTA) {}
    static void buildIntraProcNode(
            llvm::Module *module, SHBGraph *shb,
            std::set<llvm::Instruction *> *,
            std::set<llvm::Instruction *> *,
            std::set<llvm::Instruction *> *,
            std::map<llvm::Instruction *, NodeID> &);

    static void connectCallEdges(llvm::Module *, SHBGraph *, std::set<llvm::Instruction *> *, std::map<llvm::Instruction *, NodeID> *);
    static void connectForkEdges(llvm::Module *, SHBGraph *, std::set<llvm::Instruction *> *, std::map<llvm::Instruction *, NodeID> *);
    static void connectJoinEdges(llvm::Module *, SHBGraph *, std::set<llvm::Instruction *> *, std::map<llvm::Instruction *, NodeID> *);

public:
    SHBGraph() = delete;

    using ThreadSet = std::set<llvm::Value *>;
    using Thread2RountineMap = std::map<llvm::Value *, llvm::Function *>;

    ThreadSet threadSet;
    Thread2RountineMap thread2RountineMap;

    static SHBGraph *buildFromModule(llvm::Module *module, PointerAnalysis *PTA);

    bool reachable(SHBNode *n1, SHBNode *n2);

    void addThread(llvm::Value *threadHandle, llvm::Function *startRoutine) {
        // TODO: what about indirect calls? which may have multiple routines
        threadSet.insert(threadHandle);
        thread2RountineMap[threadHandle] = startRoutine;
    }

    const ThreadSet& getThreadSet() {
        return threadSet;
    }

    llvm::Function *getThreadStart(llvm::Value *thread) {
        auto iter = thread2RountineMap.find(thread);
        assert(iter != thread2RountineMap.end());
        return iter->second;
    }

    inline SHBNode *getFunctionEnterNode(llvm::Function *func) {
        auto iter = func2EnterMap.find(func);
        assert(iter != func2EnterMap.end());
        return iter->second;
    }

    inline SHBNode *getFunctionRetNode(llvm::Function *func) {
        auto iter = func2RetMap.find(func);
        assert(iter != func2RetMap.end());
        return iter->second;
    }

    void addEdge(NodeID src, NodeID dst, SHBEdge::EdgeType ty) {
        SHBNode *srcNode = getGNode(src);
        SHBNode *dstNode = getGNode(dst);

        this->addEdge(srcNode, dstNode, ty);
    }

    void addEdge(SHBNode *srcNode, SHBNode *dstNode, SHBEdge::EdgeType ty) {
        auto edge = new SHBEdge(srcNode, dstNode, ty);

        srcNode->addOutgoingEdge(edge);
        dstNode->addIncomingEdge(edge);
    }

    NodeID addReadNode(llvm::Instruction * inst) {
        auto iter = inst2NodeMap.find(inst);
        if (iter == inst2NodeMap.end()) {
            SHBNode *node = SHBNode::createReadNode(inst);
            this->addGNode(node->getId(), node);

            inst2NodeMap[inst] = node;
            readNodeSet.insert(node);
            return node->getId();
        } else {
            // already in the graph
            return (*iter).second->getId();
        }
    }

    NodeID addWriteNode(llvm::Instruction *inst) {
        auto iter = inst2NodeMap.find(inst);
        if (iter == inst2NodeMap.end()) {
            SHBNode *node = SHBNode::createWriteNode(inst);
            this->addGNode(node->getId(), node);

            inst2NodeMap[inst] = node;
            writeNodeSet.insert(node);
            return node->getId();
        } else {
            // already in the graph
            return (*iter).second->getId();
        }
    }

    NodeID addEnterNode(llvm::Function *func) {
        auto iter = func2EnterMap.find(func);
        if (iter == func2EnterMap.end()) {
            SHBNode *node = SHBNode::createEnterNode(nullptr);
            this->addGNode(node->getId(), node);

            func2EnterMap[func] = node;
            return node->getId();
        } else {
            // already in the graph
            return (*iter).second->getId();
        }
    }

    NodeID addRetNode(llvm::Function *inst) {
        auto iter = func2RetMap.find(inst);
        if (iter == func2RetMap.end()) {
            SHBNode *node = SHBNode::createRetNode(nullptr);
            this->addGNode(node->getId(), node);

            func2RetMap[inst] = node;
            return node->getId();
        } else {
            // already in the graph
            return (*iter).second->getId();
        }
    }

    void dumpDotGraph();
};

namespace llvm {
    /* !
     * GraphTraits specializations of PAG to be used for the generic graph algorithms.
     * Provide graph traits for tranversing from a PAG node using standard graph traversals.
     */
    template<>
    struct GraphTraits<SHBNode *> : public GraphTraits<GenericNode<SHBNode, SHBEdge>*> {};

    /// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
    template<>
    struct GraphTraits<Inverse<SHBNode *>> : public GraphTraits<Inverse<GenericNode<SHBNode, SHBEdge>*>> {};

    template<>
    struct GraphTraits<SHBGraph *> : public GraphTraits<GenericGraph<SHBNode, SHBEdge>*> {
        typedef SHBNode* NodeRef;
    };
}


#endif //SVF_ORIGIN_SHBGRAPH_H
