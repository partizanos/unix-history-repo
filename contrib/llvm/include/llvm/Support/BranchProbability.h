//===- BranchProbability.h - Branch Probability Analysis --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Definition of BranchProbability shared by IR and Machine Instructions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_BRANCHPROBABILITY_H
#define LLVM_SUPPORT_BRANCHPROBABILITY_H

#include "llvm/Support/DataTypes.h"

namespace llvm {

class raw_ostream;
class BranchProbabilityInfo;
class MachineBranchProbabilityInfo;
class MachineBasicBlock;

// This class represents Branch Probability as a non-negative fraction.
class BranchProbability {
  friend class BranchProbabilityInfo;
  friend class MachineBranchProbabilityInfo;
  friend class MachineBasicBlock;

  // Numerator
  uint32_t N;

  // Denominator
  uint32_t D;

  BranchProbability(uint32_t n, uint32_t d);

public:
  raw_ostream &print(raw_ostream &OS) const;

  void dump() const;
};

raw_ostream &operator<<(raw_ostream &OS, const BranchProbability &Prob);

}

#endif
