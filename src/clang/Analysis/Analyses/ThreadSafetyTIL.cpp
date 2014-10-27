//===- ThreadSafetyTIL.cpp -------------------------------------*- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT in the llvm repository for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/ThreadSafetyTIL.h"
#include "clang/Analysis/Analyses/ThreadSafetyTraverse.h"
#include "clang/Analysis/Analyses/ThreadSafetyPrint.h"

namespace clang {
namespace threadSafety {
namespace til {


StringRef getOpcodeString(TIL_Opcode Op) {
  switch (Op) {
#define TIL_OPCODE_DEF(X)                                                   \
  case COP_##X:                                                             \
    return #X;
#include "ThreadSafetyOps.def"
#undef TIL_OPCODE_DEF
  }
  return "";
}


StringRef getUnaryOpcodeString(TIL_UnaryOpcode Op) {
  switch (Op) {
    case UOP_Minus:    return "-";
    case UOP_BitNot:   return "~";
    case UOP_LogicNot: return "!";
  }
  return "";
}


StringRef getBinaryOpcodeString(TIL_BinaryOpcode Op) {
  switch (Op) {
    case BOP_Mul:      return "*";
    case BOP_Div:      return "/";
    case BOP_Rem:      return "%";
    case BOP_Add:      return "+";
    case BOP_Sub:      return "-";
    case BOP_Shl:      return "<<";
    case BOP_Shr:      return ">>";
    case BOP_BitAnd:   return "&";
    case BOP_BitXor:   return "^";
    case BOP_BitOr:    return "|";
    case BOP_Eq:       return "==";
    case BOP_Neq:      return "!=";
    case BOP_Lt:       return "<";
    case BOP_Leq:      return "<=";
    case BOP_LogicAnd: return "&&";
    case BOP_LogicOr:  return "||";
  }
  return "";
}


Slot* Record::findSlot(StringRef S) {
  // FIXME -- look this up in a hash table, please.
  for (auto* Slt : slots()) {
    if (Slt->name() == S)
      return Slt;
  }
  return nullptr;
}



unsigned BasicBlock::addPredecessor(BasicBlock *Pred) {
  unsigned Idx = Predecessors.size();
  Predecessors.reserveCheck(1, Arena);
  Predecessors.push_back(Pred);
  for (SExpr *E : Args) {
    if (Phi* Ph = dyn_cast<Phi>(E)) {
      Ph->values().reserveCheck(1, Arena);
      Ph->values().push_back(nullptr);
    }
  }
  return Idx;
}


void BasicBlock::reservePredecessors(unsigned NumPreds) {
  Predecessors.reserve(NumPreds, Arena);
  for (SExpr *E : Args) {
    if (Phi* Ph = dyn_cast<Phi>(E)) {
      Ph->values().reserve(NumPreds, Arena);
    }
  }
}



// Renumbers the arguments and instructions to have unique, sequential IDs.
unsigned BasicBlock::renumber(unsigned ID) {
  for (auto *Arg : Args) {
    if (!Arg)
      continue;
    Arg->setBlock(this);
    Arg->setInstrID(ID++);
  }
  for (auto *Instr : Instrs) {
    if (!Instr)
      continue;
    Instr->setBlock(this);
    Instr->setInstrID(ID++);
  }
  if (TermInstr)
    TermInstr->setInstrID(ID++);
  return ID;
}

// Renumber instructions in all blocks
void SCFG::renumber() {
  unsigned InstrID = 1;    // ID of 0 means unnumbered.
  unsigned BlockID = 0;
  for (auto *Block : Blocks) {
    InstrID = Block->renumber(InstrID);
    Block->setBlockID(BlockID++);
  }
  NumInstructions = InstrID;
}


// Sorts blocks in topological order, by following successors.
// If post-dominators have been computed, it takes that into account.
// Each block will be written into the Blocks array in order, and its BlockID
// will be set to the index in the array.  Sorting should start from the entry
// block, and ID should be the total number of blocks.
int BasicBlock::topologicalSort(SimpleArray<BasicBlock*>& Blocks, int ID) {
  if (Visited) return ID;
  Visited = true;

  // First sort the post-dominator, if it exists.
  // This gives us a topological order where post-dominators always come last.
  if (PostDominatorNode.Parent)
    ID = PostDominatorNode.Parent->topologicalSort(Blocks, ID);

  for (auto *Block : successors())
    ID = Block->topologicalSort(Blocks, ID);

  // set ID and update block array in place.
  // We may lose pointers to unreachable blocks.
  assert(ID > 0);
  BlockID = --ID;
  Blocks[BlockID] = this;
  return ID;
}


// Sorts blocks in post-topological order, by following predecessors.
// Each block will be written into the Blocks array in order, and PostBlockID
// will be set to the index in the array.  Sorting should start from the exit
// block, and ID should be the total number of blocks.
int BasicBlock::postTopologicalSort(SimpleArray<BasicBlock*>& Blocks, int ID) {
  if (Visited) return ID;
  Visited = true;

  // First sort the dominator, if it exists.
  // This gives us a topological order where post-dominators always come last.
  if (DominatorNode.Parent)
    ID = DominatorNode.Parent->postTopologicalSort(Blocks, ID);

  for (auto *Block : predecessors())
    ID = Block->postTopologicalSort(Blocks, ID);

  // set ID and update block array in place.
  // We may lose pointers to unreachable blocks.
  assert(ID > 0);
  PostBlockID = --ID;
  Blocks[PostBlockID] = this;
  return ID;
}


// Computes the immediate dominator of the current block.  Assumes that all of
// its predecessors have already computed their dominators.  This is achieved
// by visiting the nodes in topological order.
void BasicBlock::computeDominator() {
  BasicBlock *Candidate = nullptr;
  // Walk backwards from each predecessor to find the common dominator node.
  for (auto *Pred : predecessors()) {
    // Skip back-edges
    if (Pred->BlockID >= BlockID) continue;
    // If we don't yet have a candidate for dominator yet, take this one.
    if (Candidate == nullptr) {
      Candidate = Pred;
      continue;
    }
    // Walk the alternate and current candidate back to find a common ancestor.
    auto *Alternate = Pred;
    while (Alternate != Candidate) {
      if (Candidate->BlockID > Alternate->BlockID)
        Candidate = Candidate->DominatorNode.Parent;
      else
        Alternate = Alternate->DominatorNode.Parent;
    }
  }
  DominatorNode.Parent = Candidate;
  DominatorNode.SizeOfSubTree = 1;
}


// Computes the immediate post-dominator of the current block.  Assumes that all
// of its successors have already computed their post-dominators.  This is
// achieved visiting the nodes in reverse topological order.
void BasicBlock::computePostDominator() {
  BasicBlock *Candidate = nullptr;
  // Walk forward from each successor to find the common post-dominator node.
  for (auto *Succ : successors()) {
    // Skip back-edges
    if (Succ->PostBlockID >= PostBlockID) continue;
    // If we don't yet have a candidate for post-dominator yet, take this one.
    if (Candidate == nullptr) {
      Candidate = Succ;
      continue;
    }
    // Walk the alternate and current candidate back to find a common ancestor.
    auto *Alternate = Succ;
    while (Alternate != Candidate) {
      if (Candidate->PostBlockID > Alternate->PostBlockID)
        Candidate = Candidate->PostDominatorNode.Parent;
      else
        Alternate = Alternate->PostDominatorNode.Parent;
    }
  }
  PostDominatorNode.Parent = Candidate;
  PostDominatorNode.SizeOfSubTree = 1;
}


static inline void computeNodeSize(BasicBlock *B,
                                   BasicBlock::TopologyNode BasicBlock::*TN) {
  BasicBlock::TopologyNode *N = &(B->*TN);
  if (N->Parent) {
    BasicBlock::TopologyNode *P = &(N->Parent->*TN);
    // Initially set ID relative to the (as yet uncomputed) parent ID
    N->NodeID = P->SizeOfSubTree;
    P->SizeOfSubTree += N->SizeOfSubTree;
  }
}

static inline void computeNodeID(BasicBlock *B,
                                 BasicBlock::TopologyNode BasicBlock::*TN) {
  BasicBlock::TopologyNode *N = &(B->*TN);
  if (N->Parent) {
    BasicBlock::TopologyNode *P = &(N->Parent->*TN);
    N->NodeID += P->NodeID;    // Fix NodeIDs relative to starting node.
  }
}


// Normalizes a CFG.  Normalization has a few major components:
// 1) Removing unreachable blocks.
// 2) Computing dominators and post-dominators
// 3) Topologically sorting the blocks into the "Blocks" array.
void SCFG::computeNormalForm() {
  // Sort the blocks in post-topological order, starting from the exit.
  int NumUnreachableBlocks = Exit->postTopologicalSort(Blocks, Blocks.size());
  assert(NumUnreachableBlocks == 0);

  // Compute post-dominators, which improves the topological sort.
  for (auto *Block : Blocks) {
    Block->computePostDominator();
    Block->Visited = false;
  }

  // Now re-sort the blocks in topological order, starting from the entry.
  NumUnreachableBlocks = Entry->topologicalSort(Blocks, Blocks.size());
  assert(NumUnreachableBlocks == 0);

  // Renumber blocks and instructions now that we have a final sort.
  renumber();

  // Calculate dominators.
  // Compute sizes and IDs for the (post)dominator trees.
  for (auto *Block : Blocks) {
    Block->computeDominator();
    computeNodeSize(Block, &BasicBlock::PostDominatorNode);
  }
  for (auto *Block : Blocks.reverse()) {
    computeNodeSize(Block, &BasicBlock::DominatorNode);
    computeNodeID(Block, &BasicBlock::PostDominatorNode);
  }
  for (auto *Block : Blocks) {
    computeNodeID(Block, &BasicBlock::DominatorNode);
  }
}

}  // end namespace til
}  // end namespace threadSafety
}  // end namespace clang
