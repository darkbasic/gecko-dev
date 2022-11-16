/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc64le_Lowering_ppc64le_h
#define jit_ppc64le_Lowering_ppc64le_h

#include "jit/shared/Lowering-shared.h"

namespace js {
namespace jit {

class LIRGeneratorPPC64 : public LIRGeneratorShared
{
  protected:
    LIRGeneratorPPC64(MIRGenerator* gen, MIRGraph& graph, LIRGraph& lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph)
    { }

    // x86 has constraints on what registers can be formatted for 1-byte
    // stores and loads, but on Power all GPRs are okay.
    LAllocation useByteOpRegister(MDefinition* mir);
    LAllocation useByteOpRegisterAtStart(MDefinition* mir);
    LAllocation useByteOpRegisterOrNonDoubleConstant(MDefinition* mir);
    LDefinition tempByteOpRegister();

    bool needTempForPostBarrier() { return false; }

    void lowerForShift(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir, MDefinition* lhs,
                       MDefinition* rhs);
    void lowerUrshD(MUrsh* mir);

    void lowerForALU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir,
                     MDefinition* input);
    void lowerForALU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                     MDefinition* lhs, MDefinition* rhs);

    void lowerForALUInt64(LInstructionHelper<INT64_PIECES, INT64_PIECES, 0>* ins,
                          MDefinition* mir, MDefinition* input);
    void lowerForALUInt64(
        LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
        MDefinition* mir, MDefinition* lhs, MDefinition* rhs);

    void lowerForMulInt64(LMulI64* ins, MMul* mir, MDefinition* lhs, MDefinition* rhs);
    template<size_t Temps>
    void lowerForShiftInt64(LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
                            MDefinition* mir, MDefinition* lhs, MDefinition* rhs);
  void lowerForCompareI64AndBranch(MTest* mir, MCompare* comp, JSOp op,
                                   MDefinition* left, MDefinition* right,
                                   MBasicBlock* ifTrue, MBasicBlock* ifFalse);

    void lowerForFPU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir,
                     MDefinition* src);
    template<size_t Temps>
    void lowerForFPU(LInstructionHelper<1, 2, Temps>* ins, MDefinition* mir,
                     MDefinition* lhs, MDefinition* rhs);

#if 0
    void lowerForCompIx4(LSimdBinaryCompIx4* ins, MSimdBinaryComp* mir,
                         MDefinition* lhs, MDefinition* rhs)
    {
        return lowerForFPU(ins, mir, lhs, rhs);
    }
    void lowerForCompFx4(LSimdBinaryCompFx4* ins, MSimdBinaryComp* mir,
                         MDefinition* lhs, MDefinition* rhs)
    {
        return lowerForFPU(ins, mir, lhs, rhs);
    }
#endif

    void lowerForBitAndAndBranch(LBitAndAndBranch* baab, MInstruction* mir,
                                 MDefinition* lhs, MDefinition* rhs);
    void lowerDivI(MDiv* div);
    void lowerModI(MMod* mod);
    void lowerMulI(MMul* mul, MDefinition* lhs, MDefinition* rhs);
    void lowerPowOfTwoI(MPow* mir);
    void lowerUDiv(MDiv* div);
    void lowerUMod(MMod* mod);

    LTableSwitch* newLTableSwitch(const LAllocation& in, const LDefinition& inputCopy,
                                  MTableSwitch* ins);
    LTableSwitchV* newLTableSwitchV(MTableSwitch* ins);

    void lowerPhi(MPhi* phi);
    void lowerInt64PhiInput(MPhi*, uint32_t, LBlock*, size_t);
    void defineInt64Phi(MPhi*, size_t);

    // Returns a box allocation. reg2 is ignored since we're 64-bit.
    LBoxAllocation useBoxFixed(MDefinition* mir, Register reg1, Register reg2,
                               bool useAtStart = false);

    inline LDefinition tempToUnbox() {
        return temp();
    }

    void lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition, LBlock* block, size_t lirIndex);
    void defineUntypedPhi(MPhi* phi, size_t lirIndex);

    void lowerTruncateDToInt32(MTruncateToInt32* ins);
    void lowerTruncateFToInt32(MTruncateToInt32* ins);

    void lowerDivI64(MDiv* div);
    void lowerModI64(MMod* mod);
    void lowerUDivI64(MDiv* div);
    void lowerUModI64(MMod* mod);
    void lowerNegI(MInstruction* ins, MDefinition* input);
    void lowerNegI64(MInstruction* ins, MDefinition* input);

    void lowerAtomicLoad64(MLoadUnboxedScalar* ins);
    void lowerAtomicStore64(MStoreUnboxedScalar* ins);

    // BigInt
    void lowerBigIntDiv(MBigIntDiv *ins);
    void lowerBigIntMod(MBigIntMod *ins);
    void lowerBigIntLsh(MBigIntLsh *ins);
    void lowerBigIntRsh(MBigIntRsh *ins);

    // WASM bits
    void lowerWasmBuiltinTruncateToInt32(MWasmBuiltinTruncateToInt32 *ins);
    void lowerWasmBuiltinTruncateToInt64(MWasmBuiltinTruncateToInt64 *ins);
    void lowerWasmBuiltinDivI64(MWasmBuiltinDivI64 *ins);
    void lowerWasmBuiltinModI64(MWasmBuiltinModI64 *ins);
    void lowerWasmSelectI(MWasmSelect* select);
    void lowerWasmSelectI64(MWasmSelect* select);

    void lowerBuiltinInt64ToFloatingPoint(MBuiltinInt64ToFloatingPoint *ins);
};

typedef LIRGeneratorPPC64 LIRGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_ppc64le_Lowering_ppc64le_h */
