/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/ppc64/Assembler-ppc64.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;

LAllocation
LIRGeneratorPPC64::useByteOpRegister(MDefinition* mir)
{
    return useRegister(mir);
}

LAllocation
LIRGeneratorPPC64::useByteOpRegisterAtStart(MDefinition* mir)
{
    return useRegisterAtStart(mir);
}

LAllocation
LIRGeneratorPPC64::useByteOpRegisterOrNonDoubleConstant(MDefinition* mir)
{
    return useRegisterOrNonDoubleConstant(mir);
}

LDefinition
LIRGeneratorPPC64::tempByteOpRegister()
{
    return temp();
}

// x = !y
void
LIRGeneratorPPC64::lowerForALU(LInstructionHelper<1, 1, 0>* ins,
                                    MDefinition* mir, MDefinition* input)
{
    ins->setOperand(0, useRegister(input));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

// z = x+y
void
LIRGeneratorPPC64::lowerForALU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                                    MDefinition* lhs, MDefinition* rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegisterOrConstant(rhs));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

void LIRGeneratorPPC64::lowerForALUInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES, 0>* ins, MDefinition* mir,
    MDefinition* input) {
  ins->setInt64Operand(0, useInt64RegisterAtStart(input));
  defineInt64ReuseInput(ins, mir, 0);
}

void LIRGeneratorPPC64::lowerForALUInt64(
    LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES, willHaveDifferentLIRNodes(lhs, rhs)
                                         ? useInt64OrConstant(rhs)
                                         : useInt64OrConstantAtStart(rhs));
  defineInt64ReuseInput(ins, mir, 0);
}

void
LIRGeneratorPPC64::lowerForMulInt64(LMulI64* ins, MMul* mir, MDefinition* lhs, MDefinition* rhs)
{
// XXX: simplify this because these are all static conditions
    bool needsTemp = false;
    bool cannotAliasRhs = false;
    bool reuseInput = true;

/*
    ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
    ins->setInt64Operand(INT64_PIECES,
                         (lhs != rhs || cannotAliasRhs) ? useInt64OrConstant(rhs) : useInt64OrConstantAtStart(rhs));
*/
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES,
                       (willHaveDifferentLIRNodes(lhs, rhs) || cannotAliasRhs)
                           ? useInt64OrConstant(rhs)
                           : useInt64OrConstantAtStart(rhs));

    if (needsTemp)
        ins->setTemp(0, temp());
    if(reuseInput)
        defineInt64ReuseInput(ins, mir, 0);
    else
        defineInt64(ins, mir);
}

template<size_t Temps>
void
LIRGeneratorPPC64::lowerForShiftInt64(LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
                                           MDefinition* mir, MDefinition* lhs, MDefinition* rhs)
{
    ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));

    static_assert(LShiftI64::Rhs == INT64_PIECES, "Assume Rhs is located at INT64_PIECES.");
    static_assert(LRotateI64::Count == INT64_PIECES, "Assume Count is located at INT64_PIECES.");

    ins->setOperand(INT64_PIECES, useRegisterOrConstant(rhs));

    defineInt64ReuseInput(ins, mir, 0);
}

template void LIRGeneratorPPC64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES+1, 0>* ins, MDefinition* mir,
    MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorPPC64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES+1, 1>* ins, MDefinition* mir,
    MDefinition* lhs, MDefinition* rhs);

void LIRGeneratorPPC64::lowerForCompareI64AndBranch(MTest* mir, MCompare* comp,
                                                    JSOp op, MDefinition* left,
                                                    MDefinition* right,
                                                    MBasicBlock* ifTrue,
                                                    MBasicBlock* ifFalse) {
  auto* lir = new (alloc())
      LCompareI64AndBranch(comp, op, useInt64Register(left),
                           useInt64RegisterOrConstant(right), ifTrue, ifFalse);
  add(lir, mir);
}

void
LIRGeneratorPPC64::lowerForFPU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir,
                                    MDefinition* input)
{
    ins->setOperand(0, useRegister(input));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template<size_t Temps>
void
LIRGeneratorPPC64::lowerForFPU(LInstructionHelper<1, 2, Temps>* ins, MDefinition* mir,
                                    MDefinition* lhs, MDefinition* rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegister(rhs));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template void LIRGeneratorPPC64::lowerForFPU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                                                  MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorPPC64::lowerForFPU(LInstructionHelper<1, 2, 1>* ins, MDefinition* mir,
                                                  MDefinition* lhs, MDefinition* rhs);

void
LIRGeneratorPPC64::lowerForBitAndAndBranch(LBitAndAndBranch* baab, MInstruction* mir,
                                                MDefinition* lhs, MDefinition* rhs)
{
    baab->setOperand(0, useRegisterAtStart(lhs));
    baab->setOperand(1, useRegisterOrConstantAtStart(rhs));
    add(baab, mir);
}

void LIRGeneratorPPC64::lowerWasmBuiltinTruncateToInt32(
    MWasmBuiltinTruncateToInt32* ins) {
/*
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

  if (opd->type() == MIRType::Double) {
    define(new (alloc()) LWasmBuiltinTruncateDToInt32(
               useRegister(opd), useFixed(ins->tls(), WasmTlsReg),
               LDefinition::BogusTemp()),
           ins);
    return;
  }

  define(new (alloc()) LWasmBuiltinTruncateFToInt32(
             useRegister(opd), useFixed(ins->tls(), WasmTlsReg),
             LDefinition::BogusTemp()),
         ins);
*/
  MOZ_CRASH("NYI");
}

void
LIRGeneratorPPC64::lowerForShift(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                                      MDefinition* lhs, MDefinition* rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegisterOrConstant(rhs));
    define(ins, mir);
}

void
LIRGeneratorPPC64::lowerDivI(MDiv* div)
{
    if (div->isUnsigned()) {
        lowerUDiv(div);
        return;
    }

    // Division instructions are slow. Division by constant denominators can be
    // rewritten to use other instructions.
    if (div->rhs()->isConstant()) {
        int32_t rhs = div->rhs()->toConstant()->toInt32();
        // Check for division by a positive power of two, which is an easy and
        // important case to optimize. Note that other optimizations are also
        // possible; division by negative powers of two can be optimized in a
        // similar manner as positive powers of two, and division by other
        // constants can be optimized by a reciprocal multiplication technique.
        int32_t shift = FloorLog2(rhs);
        if (rhs > 0 && 1 << shift == rhs) {
            LDivPowTwoI* lir = new(alloc()) LDivPowTwoI(useRegister(div->lhs()), shift, temp());
            if (div->fallible())
                assignSnapshot(lir, div->bailoutKind());
            define(lir, div);
            return;
        }
    }

    LDivI* lir = new(alloc()) LDivI(useRegister(div->lhs()), useRegister(div->rhs()), temp());
    if (div->fallible())
        assignSnapshot(lir, div->bailoutKind());
    define(lir, div);
}

void LIRGeneratorPPC64::lowerNegI(MInstruction* ins, MDefinition* input) {
  define(new (alloc()) LNegI(useRegisterAtStart(input)), ins);
}

void LIRGeneratorPPC64::lowerNegI64(MInstruction* ins, MDefinition* input) {
  defineInt64(new (alloc()) LNegI64(useInt64RegisterAtStart(input)), ins);
}

void LIRGenerator::visitAbs(MAbs* ins) {
  define(allocateAbs(ins, useRegisterAtStart(ins->input())), ins);
}

void
LIRGeneratorPPC64::lowerMulI(MMul* mul, MDefinition* lhs, MDefinition* rhs)
{
    // Automatically reduced by xs_sr_mulli.
    LMulI* lir = new(alloc()) LMulI;
    if (mul->fallible())
        assignSnapshot(lir, mul->bailoutKind());

    lowerForALU(lir, mul, lhs, rhs);
}

void
LIRGeneratorPPC64::lowerModI(MMod* mod)
{
    if (mod->isUnsigned()) {
        lowerUMod(mod);
        return;
    }

    // POWER9 has hardware modulo, so we don't need to lower it there.
    if (!HasPPCISA3() && mod->rhs()->isConstant()) {
        int32_t rhs = mod->rhs()->toConstant()->toInt32();
        int32_t shift = FloorLog2(rhs);
        if (rhs > 0 && 1 << shift == rhs) {
            LModPowTwoI* lir = new(alloc()) LModPowTwoI(useRegister(mod->lhs()), shift);
            if (mod->fallible())
                assignSnapshot(lir, mod->bailoutKind());
            define(lir, mod);
            return;
        } else if (shift < 31 && (1 << (shift + 1)) - 1 == rhs) {
            LModMaskI* lir = new(alloc()) LModMaskI(useRegister(mod->lhs()),
                                                    temp(LDefinition::GENERAL),
                                                    temp(LDefinition::GENERAL),
                                                    shift + 1);
            if (mod->fallible())
                assignSnapshot(lir, mod->bailoutKind());
            define(lir, mod);
            return;
        }
    }
    LModI* lir = new(alloc()) LModI(useRegister(mod->lhs()), useRegister(mod->rhs()),
                           temp(LDefinition::GENERAL));

    if (mod->fallible())
        assignSnapshot(lir, mod->bailoutKind());
    define(lir, mod);
}

void
LIRGenerator::visitPowHalf(MPowHalf* ins)
{
    MDefinition* input = ins->input();
    MOZ_ASSERT(input->type() == MIRType::Double);
    LPowHalfD* lir = new(alloc()) LPowHalfD(useRegisterAtStart(input));
    defineReuseInput(lir, ins, 0);
}

void LIRGeneratorPPC64::lowerWasmSelectI(MWasmSelect* select) {
/*
  auto* lir = new (alloc())
      LWasmSelect(useRegisterAtStart(select->trueExpr()),
                  useAny(select->falseExpr()), useRegister(select->condExpr()));
  defineReuseInput(lir, select, LWasmSelect::TrueExprIndex);
*/
  MOZ_CRASH("NYI");
}

void LIRGeneratorPPC64::lowerWasmSelectI64(MWasmSelect* select) {
/*
  auto* lir = new (alloc()) LWasmSelectI64(
      useInt64RegisterAtStart(select->trueExpr()),
      useInt64(select->falseExpr()), useRegister(select->condExpr()));
  defineInt64ReuseInput(lir, select, LWasmSelectI64::TrueExprIndex);
*/
  MOZ_CRASH("NYI");
}

LTableSwitch*
LIRGeneratorPPC64::newLTableSwitch(const LAllocation& in, const LDefinition& inputCopy,
                                        MTableSwitch* tableswitch)
{
    return new(alloc()) LTableSwitch(in, inputCopy, temp(), tableswitch);
}

LTableSwitchV*
LIRGeneratorPPC64::newLTableSwitchV(MTableSwitch* tableswitch)
{
    return new(alloc()) LTableSwitchV(useBox(tableswitch->getOperand(0)),
                                      temp(), tempDouble(), temp(), tableswitch);
}

void
LIRGeneratorPPC64::lowerUrshD(MUrsh* mir)
{
    MDefinition* lhs = mir->lhs();
    MDefinition* rhs = mir->rhs();

    MOZ_ASSERT(lhs->type() == MIRType::Int32);
    MOZ_ASSERT(rhs->type() == MIRType::Int32);

    LUrshD* lir = new(alloc()) LUrshD(useRegister(lhs), useRegisterOrConstant(rhs), temp());
    define(lir, mir);
}

void LIRGeneratorPPC64::lowerPowOfTwoI(MPow* mir) {
  int32_t base = mir->input()->toConstant()->toInt32();
  MDefinition* power = mir->power();

  auto* lir = new (alloc()) LPowOfTwoI(useRegister(power), base);
  assignSnapshot(lir, mir->bailoutKind());
  define(lir, mir);
}

void LIRGeneratorPPC64::lowerBigIntLsh(MBigIntLsh* ins) {
  auto* lir = new (alloc()) LBigIntLsh(
      useRegister(ins->lhs()), useRegister(ins->rhs()), temp(), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGeneratorPPC64::lowerBigIntRsh(MBigIntRsh* ins) {
  auto* lir = new (alloc()) LBigIntRsh(
      useRegister(ins->lhs()), useRegister(ins->rhs()), temp(), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void
LIRGenerator::visitWasmNeg(MWasmNeg* ins)
{
    if (ins->type() == MIRType::Int32) {
        define(new(alloc()) LNegI(useRegisterAtStart(ins->input())), ins);
    } else if (ins->type() == MIRType::Float32) {
        define(new(alloc()) LNegF(useRegisterAtStart(ins->input())), ins);
    } else {
        MOZ_ASSERT(ins->type() == MIRType::Double);
        define(new(alloc()) LNegD(useRegisterAtStart(ins->input())), ins);
    }
}

void LIRGenerator::visitWasmHeapBase(MWasmHeapBase* ins) {
/*
  auto* lir = new (alloc()) LWasmHeapBase(LAllocation());
  define(lir, ins);
*/
     MOZ_CRASH("NYI");
}

void
LIRGenerator::visitWasmLoad(MWasmLoad* ins)
{
    MDefinition* base = ins->base();
  // |base| is a GPR but may be of either type. If it is 32-bit, it is
  // sign-extended on 64-bit Power and we should explicitly promote it to
  // 64-bit when used as an index register in memory accesses.
  MOZ_ASSERT(base->type() == MIRType::Int32 || base->type() == MIRType::Int64);

    LAllocation ptr = useRegisterAtStart(base);

    if (IsUnaligned(ins->access())) {
        if (ins->type() == MIRType::Int64) {
            auto* lir = new(alloc()) LWasmUnalignedLoadI64(ptr, temp());
            if (ins->access().offset())
                lir->setTemp(0, tempCopy(base, 0));

            defineInt64(lir, ins);
            return;
        }

        auto* lir = new(alloc()) LWasmUnalignedLoad(ptr, temp());
        if (ins->access().offset())
            lir->setTemp(0, tempCopy(base, 0));

        define(lir, ins);
        return;
    }

    if (ins->type() == MIRType::Int64) {
        auto* lir = new(alloc()) LWasmLoadI64(ptr);
        if (ins->access().offset())
            lir->setTemp(0, tempCopy(base, 0));

        defineInt64(lir, ins);
        return;
    }

    auto* lir = new(alloc()) LWasmLoad(ptr);
    if (ins->access().offset())
        lir->setTemp(0, tempCopy(base, 0));

    define(lir, ins);
}

void
LIRGenerator::visitWasmStore(MWasmStore* ins)
{
    MDefinition* base = ins->base();
    // All about the two kinds of bases, no treble. Or trouble, maybe.
    // How's that line go again?
  MOZ_ASSERT(base->type() == MIRType::Int32 || base->type() == MIRType::Int64);

    MDefinition* value = ins->value();

    if (IsUnaligned(ins->access())) {
        LAllocation baseAlloc = useRegisterAtStart(base);
        if (ins->access().type() == Scalar::Int64) {
            LInt64Allocation valueAlloc = useInt64RegisterAtStart(value);
            auto* lir = new(alloc()) LWasmUnalignedStoreI64(baseAlloc, valueAlloc, temp());
            if (ins->access().offset())
                lir->setTemp(0, tempCopy(base, 0));

            add(lir, ins);
            return;
        }

        LAllocation valueAlloc = useRegisterAtStart(value);
        auto* lir = new(alloc()) LWasmUnalignedStore(baseAlloc, valueAlloc, temp());
        if (ins->access().offset())
            lir->setTemp(0, tempCopy(base, 0));

        add(lir, ins);
        return;
    }

    if (ins->access().type() == Scalar::Int64) {
        LAllocation baseAlloc = useRegisterAtStart(base);
        LInt64Allocation valueAlloc = useInt64RegisterAtStart(value);
        auto* lir = new(alloc()) LWasmStoreI64(baseAlloc, valueAlloc);
        if (ins->access().offset())
            lir->setTemp(0, tempCopy(base, 0));

        add(lir, ins);
        return;
    }

    LAllocation baseAlloc = useRegisterAtStart(base);
    LAllocation valueAlloc = useRegisterAtStart(value);
    auto* lir = new(alloc()) LWasmStore(baseAlloc, valueAlloc);
    if (ins->access().offset())
        lir->setTemp(0, tempCopy(base, 0));

    add(lir, ins);
}

void
LIRGeneratorPPC64::lowerUDiv(MDiv* div)
{
    MDefinition* lhs = div->getOperand(0);
    MDefinition* rhs = div->getOperand(1);

    LUDivOrMod* lir = new(alloc()) LUDivOrMod;
    lir->setOperand(0, useRegister(lhs));
    lir->setOperand(1, useRegister(rhs));
    if (div->fallible())
        assignSnapshot(lir, div->bailoutKind());

    define(lir, div);
}

void
LIRGeneratorPPC64::lowerUMod(MMod* mod)
{
    MDefinition* lhs = mod->getOperand(0);
    MDefinition* rhs = mod->getOperand(1);

    LUDivOrMod* lir = new(alloc()) LUDivOrMod;
    lir->setOperand(0, useRegister(lhs));
    lir->setOperand(1, useRegister(rhs));
    if (mod->fallible())
        assignSnapshot(lir, mod->bailoutKind());

    define(lir, mod);
}

void
LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble* ins)
{
    MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
    LWasmUint32ToDouble* lir = new(alloc()) LWasmUint32ToDouble(useRegisterAtStart(ins->input()));
    define(lir, ins);
}

void
LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32* ins)
{
    MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
    LWasmUint32ToFloat32* lir = new(alloc()) LWasmUint32ToFloat32(useRegisterAtStart(ins->input()));
    define(lir, ins);
}

void
LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins)
{
    MOZ_ASSERT(ins->access().offset() == 0);

    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);
    LAllocation baseAlloc;
    LAllocation limitAlloc;
    // We'd prefer base in a register or this will be slow.
    if (base->isConstant() && !ins->needsBoundsCheck()) {
        // A bounds check is only skipped for a positive index.
        MOZ_ASSERT(base->toConstant()->toInt32() >= 0);
        baseAlloc = LAllocation(base->toConstant());
    } else {
        baseAlloc = useRegisterAtStart(base);
        if (ins->needsBoundsCheck()) {
            MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
            MOZ_ASSERT(boundsCheckLimit->type() == MIRType::Int32);
            limitAlloc = useRegisterAtStart(boundsCheckLimit);
        }
    }

    define(new(alloc()) LAsmJSLoadHeap(baseAlloc, limitAlloc, LAllocation()), ins);
}

void
LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins)
{
    MOZ_ASSERT(ins->access().offset() == 0);

    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);
    LAllocation baseAlloc;
    LAllocation limitAlloc;
    if (base->isConstant() && !ins->needsBoundsCheck()) {
        MOZ_ASSERT(base->toConstant()->toInt32() >= 0);
        baseAlloc = LAllocation(base->toConstant());
    } else{
        baseAlloc = useRegisterAtStart(base);
        if (ins->needsBoundsCheck()) {
            MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
            MOZ_ASSERT(boundsCheckLimit->type() == MIRType::Int32);
            limitAlloc = useRegisterAtStart(boundsCheckLimit);
        }
    }

    add(new(alloc()) LAsmJSStoreHeap(baseAlloc, useRegisterAtStart(ins->value()), limitAlloc, LAllocation()), ins);
}

void
LIRGenerator::visitSubstr(MSubstr* ins)
{
    LSubstr* lir = new (alloc()) LSubstr(useRegister(ins->string()),
                                         useRegister(ins->begin()),
                                         useRegister(ins->length()),
                                         temp(),
                                         temp(),
                                         tempByteOpRegister());
    define(lir, ins);
    assignSafepoint(lir, ins);
}

void LIRGenerator::visitCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement* ins) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());

  const LAllocation newval = useRegister(ins->newval());
  const LAllocation oldval = useRegister(ins->oldval());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LInt64Definition temp2 = tempInt64();

    auto* lir = new (alloc()) LCompareExchangeTypedArrayElement64(
        elements, index, oldval, newval, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  // If the target is an FPR then we need a temp at the
  // CodeGenerator level for creating the result. XXX: we do?

  LDefinition outTemp = LDefinition::BogusTemp();
  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    outTemp = temp();
  }

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LCompareExchangeTypedArrayElement* lir = new (alloc())
      LCompareExchangeTypedArrayElement(elements, index, oldval, newval,
                                        outTemp, valueTemp, offsetTemp,
                                        maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement* ins) {
  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());

  const LAllocation value = useRegister(ins->value());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LDefinition temp2 = temp();

    auto* lir = new (alloc()) LAtomicExchangeTypedArrayElement64(
        elements, index, value, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  // Same thing.
  // XXX: same thing?

  MOZ_ASSERT(ins->arrayType() <= Scalar::Uint32);

  LDefinition outTemp = LDefinition::BogusTemp();
  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32) {
    MOZ_ASSERT(ins->type() == MIRType::Double);
    outTemp = temp();
  }

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LAtomicExchangeTypedArrayElement* lir =
      new (alloc()) LAtomicExchangeTypedArrayElement(
          elements, index, value, outTemp, valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void
LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap* ins)
{
  MDefinition* base = ins->base();
  // Again with the bases?
  MOZ_ASSERT(base->type() == MIRType::Int32 || base->type() == MIRType::Int64);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc()) LWasmCompareExchangeI64(
        useRegister(base), useInt64Register(ins->oldValue()),
        useInt64Register(ins->newValue()));
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LWasmCompareExchangeHeap* lir = new (alloc()) LWasmCompareExchangeHeap(
      useRegister(base), useRegister(ins->oldValue()),
      useRegister(ins->newValue()), valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void
LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap* ins)
{
  MDefinition* base = ins->base();
  // See comment in visitWasmLoad re the type of 'base'.
  MOZ_ASSERT(base->type() == MIRType::Int32 || base->type() == MIRType::Int64);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc()) LWasmAtomicExchangeI64(
        useRegister(base), useInt64Register(ins->value()));
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LWasmAtomicExchangeHeap* lir = new (alloc())
      LWasmAtomicExchangeHeap(useRegister(base), useRegister(ins->value()),
                              valueTemp, offsetTemp, maskTemp);
  define(lir, ins);
}

void
LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap* ins)
{
  MDefinition* base = ins->base();
  // See comment in visitWasmLoad re the type of 'base'.
  MOZ_ASSERT(base->type() == MIRType::Int32 || base->type() == MIRType::Int64);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc())
        LWasmAtomicBinopI64(useRegister(base), useInt64Register(ins->value()));
    lir->setTemp(0, temp());
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  if (!ins->hasUses()) {
    LWasmAtomicBinopHeapForEffect* lir = new (alloc())
        LWasmAtomicBinopHeapForEffect(useRegister(base),
                                      useRegister(ins->value()), valueTemp,
                                      offsetTemp, maskTemp);
    add(lir, ins);
    return;
  }

  LWasmAtomicBinopHeap* lir = new (alloc())
      LWasmAtomicBinopHeap(useRegister(base), useRegister(ins->value()),
                           valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop* ins) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Uint8Clamped);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());
  const LAllocation value = useRegister(ins->value());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LInt64Definition temp2 = tempInt64();

    // Case 1: the result of the operation is not used.
    //
    // We can omit allocating the result BigInt.

    if (ins->isForEffect()) {
      auto* lir = new (alloc()) LAtomicTypedArrayElementBinopForEffect64(
          elements, index, value, temp1, temp2);
      add(lir, ins);
      return;
    }

    // Case 2: the result of the operation is used.

    auto* lir = new (alloc())
        LAtomicTypedArrayElementBinop64(elements, index, value, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  if (ins->isForEffect()) {
    LAtomicTypedArrayElementBinopForEffect* lir =
        new (alloc()) LAtomicTypedArrayElementBinopForEffect(
            elements, index, value, valueTemp, offsetTemp, maskTemp);
    add(lir, ins);
    return;
  }

  // For a Uint32Array with a known double result we need a temp for
  // the intermediate output.

  LDefinition outTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    outTemp = temp();
  }

  LAtomicTypedArrayElementBinop* lir =
      new (alloc()) LAtomicTypedArrayElementBinop(
          elements, index, value, outTemp, valueTemp, offsetTemp, maskTemp);
  define(lir, ins);
}

void
LIRGenerator::visitCopySign(MCopySign* ins)
{
    MDefinition* lhs = ins->lhs();
    MDefinition* rhs = ins->rhs();

    MOZ_ASSERT(IsFloatingPointType(lhs->type()));
    MOZ_ASSERT(lhs->type() == rhs->type());
    MOZ_ASSERT(lhs->type() == ins->type());

    LInstructionHelper<1, 2, 2>* lir;
    if (lhs->type() == MIRType::Double)
        lir = new(alloc()) LCopySignD();
    else
        lir = new(alloc()) LCopySignF();

    lir->setTemp(0, temp());
    lir->setTemp(1, temp());

    lir->setOperand(0, useRegisterAtStart(lhs));
    lir->setOperand(1, useRegister(rhs));
    defineReuseInput(lir, ins, 0);
}

void
LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64* ins)
{
    defineInt64(new(alloc()) LExtendInt32ToInt64(useRegisterAtStart(ins->input())), ins);
}

void
LIRGenerator::visitSignExtendInt64(MSignExtendInt64* ins)
{
    defineInt64(new(alloc()) LSignExtendInt64(useInt64RegisterAtStart(ins->input())), ins);
}

bool LIRGeneratorShared::canSpecializeWasmCompareAndSelect(
    MCompare::CompareType compTy, MIRType insTy) {
        // XXX: for now
        // Should be able to implement this for (u)int32 compare/select
        // with isel, and probably floats/doubles with fsel.
/*
  return insTy == MIRType::Int32 && (compTy == MCompare::Compare_Int32 ||
                                     compTy == MCompare::Compare_UInt32);
*/
        return false;
}

void LIRGeneratorShared::lowerWasmCompareAndSelect(MWasmSelect* ins,
                                                   MDefinition* lhs,
                                                   MDefinition* rhs,
                                                   MCompare::CompareType compTy,
                                                   JSOp jsop) {
  MOZ_ASSERT(canSpecializeWasmCompareAndSelect(compTy, ins->type()));
/*
  auto* lir = new (alloc()) LWasmCompareAndSelect(
      useRegister(lhs), useRegister(rhs), compTy, jsop,
      useRegisterAtStart(ins->trueExpr()), useRegister(ins->falseExpr()));
  defineReuseInput(lir, ins, LWasmCompareAndSelect::IfTrueExprIndex);
*/
  MOZ_CRASH("that was unexpected");
}

void LIRGenerator::visitWasmTernarySimd128(MWasmTernarySimd128* ins) {
  MOZ_CRASH("ternary SIMD NYI");
}

void LIRGenerator::visitWasmBinarySimd128(MWasmBinarySimd128* ins) {
  MOZ_CRASH("binary SIMD NYI");
}

bool MWasmBinarySimd128::specializeForConstantRhs() {
  // Probably many we want to do here
  return false;
}

void LIRGenerator::visitWasmBinarySimd128WithConstant(
    MWasmBinarySimd128WithConstant* ins) {
  MOZ_CRASH("binary SIMD with constant NYI");
}

void LIRGenerator::visitWasmShiftSimd128(MWasmShiftSimd128* ins) {
  MOZ_CRASH("shift SIMD NYI");
}

void LIRGenerator::visitWasmShuffleSimd128(MWasmShuffleSimd128* ins) {
  MOZ_CRASH("shuffle SIMD NYI");
}

void LIRGenerator::visitWasmReplaceLaneSimd128(MWasmReplaceLaneSimd128* ins) {
  MOZ_CRASH("replace-lane SIMD NYI");
}

void LIRGenerator::visitWasmScalarToSimd128(MWasmScalarToSimd128* ins) {
  MOZ_CRASH("scalar-to-SIMD NYI");
}

void LIRGenerator::visitWasmUnarySimd128(MWasmUnarySimd128* ins) {
  MOZ_CRASH("unary SIMD NYI");
}

void LIRGenerator::visitWasmReduceSimd128(MWasmReduceSimd128* ins) {
  MOZ_CRASH("reduce-SIMD NYI");
}

void LIRGenerator::visitWasmLoadLaneSimd128(MWasmLoadLaneSimd128* ins) {
  MOZ_CRASH("load-lane SIMD NYI");
}

void LIRGenerator::visitWasmStoreLaneSimd128(MWasmStoreLaneSimd128* ins) {
  MOZ_CRASH("store-lane SIMD NYI");
}

// XXX: end mips-shared

void
LIRGeneratorPPC64::defineInt64Phi(MPhi* phi, size_t lirIndex)
{
    defineTypedPhi(phi, lirIndex);
}

void
LIRGeneratorPPC64::lowerInt64PhiInput(MPhi* phi, uint32_t inputPosition,
                                       LBlock* block, size_t lirIndex)
{
    lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

LBoxAllocation
LIRGeneratorPPC64::useBoxFixed(MDefinition* mir, Register reg1, Register reg2, bool useAtStart)
{
    MOZ_ASSERT(mir->type() == MIRType::Value);

    ensureDefined(mir);
    return LBoxAllocation(LUse(reg1, mir->virtualRegister(), useAtStart));
}

void
LIRGeneratorPPC64::lowerDivI64(MDiv* div)
{
    if (div->isUnsigned()) {
        lowerUDivI64(div);
        return;
    }

    LDivOrModI64* lir = new(alloc()) LDivOrModI64(useRegister(div->lhs()), useRegister(div->rhs()),
                                                  temp());
    defineInt64(lir, div);
}

void LIRGeneratorPPC64::lowerWasmBuiltinDivI64(MWasmBuiltinDivI64* div) {
// XXX: we don't?
  MOZ_CRASH("We don't use runtime div for this architecture");
}

void
LIRGeneratorPPC64::lowerModI64(MMod* mod)
{
    if (mod->isUnsigned()) {
        lowerUModI64(mod);
        return;
    }

    LDivOrModI64* lir = new(alloc()) LDivOrModI64(useRegister(mod->lhs()), useRegister(mod->rhs()),
                                                  temp());
    defineInt64(lir, mod);
}

void LIRGeneratorPPC64::lowerWasmBuiltinModI64(MWasmBuiltinModI64* mod) {
// XXX: we don't?
  MOZ_CRASH("We don't use runtime mod for this architecture");
}

void
LIRGeneratorPPC64::lowerUDivI64(MDiv* div)
{
    LUDivOrModI64* lir = new(alloc()) LUDivOrModI64(useRegister(div->lhs()),
                                                    useRegister(div->rhs()),
                                                    temp());
    defineInt64(lir, div);
}

void
LIRGeneratorPPC64::lowerUModI64(MMod* mod)
{
    LUDivOrModI64* lir = new(alloc()) LUDivOrModI64(useRegister(mod->lhs()),
                                                    useRegister(mod->rhs()),
                                                    temp());
    defineInt64(lir, mod);
}

void LIRGeneratorPPC64::lowerBigIntDiv(MBigIntDiv* ins) {
  auto* lir = new (alloc()) LBigIntDiv(useRegister(ins->lhs()),
                                       useRegister(ins->rhs()), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGeneratorPPC64::lowerBigIntMod(MBigIntMod* ins) {
  auto* lir = new (alloc()) LBigIntMod(useRegister(ins->lhs()),
                                       useRegister(ins->rhs()), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGeneratorPPC64::lowerAtomicLoad64(MLoadUnboxedScalar* ins) {
  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->storageType());

  auto* lir = new (alloc()) LAtomicLoad64(elements, index, temp(), tempInt64());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGeneratorPPC64::lowerAtomicStore64(MStoreUnboxedScalar* ins) {
  LUse elements = useRegister(ins->elements());
  LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->writeType());
  LAllocation value = useRegister(ins->value());

  add(new (alloc()) LAtomicStore64(elements, index, value, tempInt64()), ins);
}

void
LIRGenerator::visitBox(MBox* box)
{
    MDefinition* opd = box->getOperand(0);

    // If the operand is a constant, emit near its uses.
    if (opd->isConstant() && box->canEmitAtUses()) {
        emitAtUses(box);
        return;
    }

    if (opd->isConstant()) {
        define(new(alloc()) LValue(opd->toConstant()->toJSValue()), box, LDefinition(LDefinition::BOX));
    } else {
        LBox* ins = new(alloc()) LBox(useRegister(opd), opd->type());
        define(ins, box, LDefinition(LDefinition::BOX));
    }
}

void
LIRGenerator::visitUnbox(MUnbox* unbox)
{
    MDefinition* box = unbox->getOperand(0);

/*
    if (box->type() == MIRType::ObjectOrNull) {
        LUnboxObjectOrNull* lir = new(alloc()) LUnboxObjectOrNull(useRegisterAtStart(box));
        if (unbox->fallible())
            assignSnapshot(lir, unbox->bailoutKind());
        defineReuseInput(lir, unbox, 0);
        return;
    }
*/

    MOZ_ASSERT(box->type() == MIRType::Value);

    LUnbox* lir;
    if (IsFloatingPointType(unbox->type())) {
        lir = new(alloc()) LUnboxFloatingPoint(useRegisterAtStart(box), unbox->type());
    } else if (unbox->fallible()) {
        // If the unbox is fallible, load the Value in a register first to
        // avoid multiple loads.
        lir = new(alloc()) LUnbox(useRegisterAtStart(box));
    } else {
        lir = new(alloc()) LUnbox(useAtStart(box));
    }

    if (unbox->fallible())
        assignSnapshot(lir, unbox->bailoutKind());

    define(lir, unbox);
}

void
LIRGenerator::visitReturnImpl(MDefinition* opd, bool isGenerator)
{
    MOZ_ASSERT(opd->type() == MIRType::Value);

    LReturn* ins = new(alloc()) LReturn(isGenerator);
    ins->setOperand(0, useFixed(opd, JSReturnReg));
    add(ins);
}

void
LIRGeneratorPPC64::defineUntypedPhi(MPhi* phi, size_t lirIndex)
{
    defineTypedPhi(phi, lirIndex);
}

void
LIRGeneratorPPC64::lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition,
                                         LBlock* block, size_t lirIndex)
{
    lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

void
LIRGeneratorPPC64::lowerTruncateDToInt32(MTruncateToInt32* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Double);

    define(new(alloc())
           LTruncateDToInt32(useRegister(opd), tempDouble()), ins);
}

void
LIRGeneratorPPC64::lowerTruncateFToInt32(MTruncateToInt32* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Float32);

    define(new(alloc())
           LTruncateFToInt32(useRegister(opd), tempFloat32()), ins);
}

void
LIRGenerator::visitWasmTruncateToInt64(MWasmTruncateToInt64* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

    defineInt64(new(alloc()) LWasmTruncateToInt64(useRegister(opd)), ins);
}

void LIRGeneratorPPC64::lowerWasmBuiltinTruncateToInt64(
    MWasmBuiltinTruncateToInt64* ins) {
  MOZ_CRASH("We don't use it for this architecture");
}

void
LIRGenerator::visitInt64ToFloatingPoint(MInt64ToFloatingPoint* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Int64);
    MOZ_ASSERT(IsFloatingPointType(ins->type()));

    define(new(alloc()) LInt64ToFloatingPoint(useInt64Register(opd)), ins);
}

void
LIRGeneratorPPC64::lowerBuiltinInt64ToFloatingPoint(MBuiltinInt64ToFloatingPoint *)
{
    MOZ_CRASH("NYI");
}

#if 0
// A beautiful AltiVec and VSX world awaits me if I could only stop procrastinating.
void
LIRGenerator::visitSimdInsertElement(MSimdInsertElement*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdExtractElement(MSimdExtractElement*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdBinaryArith(MSimdBinaryArith*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSelect(MSimdSelect*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSplat(MSimdSplat*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdValueX4(MSimdValueX4*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdBinarySaturating(MSimdBinarySaturating*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSwizzle(MSimdSwizzle*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdShuffle(MSimdShuffle*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdGeneralShuffle(MSimdGeneralShuffle*)
{
    MOZ_CRASH("NYI");
}
#endif

