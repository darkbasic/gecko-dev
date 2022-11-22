/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc64le_MacroAssembler_ppc64le_inl_h
#define jit_ppc64le_MacroAssembler_ppc64le_inl_h

#include "jit/ppc64/MacroAssembler-ppc64.h"
#include "jit/FlushICache.h"

namespace js {
namespace jit {

//{{{ check_macroassembler_style

void
MacroAssembler::move64(Register64 src, Register64 dest)
{
    movePtr(src.reg, dest.reg);
}

void
MacroAssembler::move64(Imm64 imm, Register64 dest)
{
    movePtr(ImmWord(imm.value), dest.reg);
}

void
MacroAssembler::moveDoubleToGPR64(FloatRegister src, Register64 dest)
{
    moveFromDouble(src, dest.reg);
}

void
MacroAssembler::moveGPR64ToDouble(Register64 src, FloatRegister dest)
{
    moveToDouble(src.reg, dest);
}

void
MacroAssembler::move64To32(Register64 src, Register dest)
{
    // Registers are registers, so why should it be
    // 32 bits are treated differently?
    // (with apologies to Depeche Mode)
    as_srawi(dest, src.reg, 0); // clear upper word and sign extend
}

void
MacroAssembler::move32To64ZeroExtend(Register src, Register64 dest)
{
    as_rldicl(dest.reg, src, 0, 32); // "clrldi"
}

void
MacroAssembler::move8To64SignExtend(Register src, Register64 dest)
{
    move32To64SignExtend(src, dest);
    move8SignExtend(dest.reg, dest.reg);
}

void
MacroAssembler::move16To64SignExtend(Register src, Register64 dest)
{
    move32To64SignExtend(src, dest);
    move16SignExtend(dest.reg, dest.reg);
}

void
MacroAssembler::move32To64SignExtend(Register src, Register64 dest)
{
    as_srawi(dest.reg, src, 0);
}

void
MacroAssembler::move32SignExtendToPtr(Register src, Register dest)
{
    as_srawi(dest, src, 0);
}

void
MacroAssembler::move32ZeroExtendToPtr(Register src, Register dest)
{
    as_rldicl(dest, src, 0, 32); // "clrldi"
}


// ===============================================================
// Logical instructions

void
MacroAssembler::andPtr(Register src, Register dest)
{
    ma_and(dest, src);
}

void
MacroAssembler::andPtr(Imm32 imm, Register dest)
{
    ma_and(dest, imm);
}

void
MacroAssembler::and64(Imm64 imm, Register64 dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_and(dest.reg, ScratchRegister);
}

void
MacroAssembler::and64(Register64 src, Register64 dest)
{
    ma_and(dest.reg, src.reg);
}

void
MacroAssembler::and64(const Operand& src, Register64 dest)
{
    if (src.getTag() == Operand::MEM) {
        MOZ_ASSERT(dest.reg != ScratchRegister);
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        and64(scratch, dest);
    } else {
        and64(Register64(src.toReg()), dest);
    }
}

void
MacroAssembler::or64(Imm64 imm, Register64 dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_or(dest.reg, ScratchRegister);
}

void
MacroAssembler::xor64(Imm64 imm, Register64 dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    ma_li(ScratchRegister, ImmWord(imm.value));
    ma_xor(dest.reg, ScratchRegister);
}

void
MacroAssembler::orPtr(Register src, Register dest)
{
    ma_or(dest, src);
}

void
MacroAssembler::orPtr(Imm32 imm, Register dest)
{
    ma_or(dest, imm);
}

void
MacroAssembler::or64(Register64 src, Register64 dest)
{
    ma_or(dest.reg, src.reg);
}

void
MacroAssembler::or64(const Operand& src, Register64 dest)
{
    if (src.getTag() == Operand::MEM) {
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        or64(scratch, dest);
    } else {
        or64(Register64(src.toReg()), dest);
    }
}

void
MacroAssembler::xor64(Register64 src, Register64 dest)
{
    ma_xor(dest.reg, src.reg);
}

void
MacroAssembler::xor64(const Operand& src, Register64 dest)
{
    if (src.getTag() == Operand::MEM) {
        MOZ_ASSERT(dest.reg != ScratchRegister);
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        xor64(scratch, dest);
    } else {
        xor64(Register64(src.toReg()), dest);
    }
}

void
MacroAssembler::xorPtr(Register src, Register dest)
{
    ma_xor(dest, src);
}

void
MacroAssembler::xorPtr(Imm32 imm, Register dest)
{
    ma_xor(dest, imm);
}

// ===============================================================
// Arithmetic functions

void
MacroAssembler::addPtr(Register src, Register dest)
{
    ma_add(dest, src);
}

void
MacroAssembler::addPtr(Imm32 imm, Register dest)
{
    ma_add(dest, imm);
}

void
MacroAssembler::addPtr(ImmWord imm, Register dest)
{
    movePtr(imm, ScratchRegister);
    addPtr(ScratchRegister, dest);
}

void
MacroAssembler::add64(Register64 src, Register64 dest)
{
    addPtr(src.reg, dest.reg);
}

void
MacroAssembler::add64(const Operand& src, Register64 dest)
{
    if (src.getTag() == Operand::MEM) {
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        add64(scratch, dest);
    } else {
        add64(Register64(src.toReg()), dest);
    }
}

void
MacroAssembler::add64(Imm32 imm, Register64 dest)
{
    ma_add(dest.reg, imm);
}

void
MacroAssembler::add64(Imm64 imm, Register64 dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    mov(ImmWord(imm.value), ScratchRegister);
    ma_add(dest.reg, ScratchRegister);
}

void
MacroAssembler::notPtr(Register reg)
{
    as_nor(reg, reg, reg);
}

CodeOffset
MacroAssembler::sub32FromStackPtrWithPatch(Register dest)
{
    CodeOffset offset = CodeOffset(currentOffset());
    MacroAssemblerPPC64::ma_liPatchable(dest, Imm32(0));
    as_subf(dest, dest, StackPointer); // T = B - A
    return offset;
}

void
MacroAssembler::patchSub32FromStackPtr(CodeOffset offset, Imm32 imm)
{
    Instruction* lis = (Instruction*) m_buffer.getInst(BufferOffset(offset.offset()));
    MacroAssemblerPPC64::UpdateLisOriValue(lis, lis->next(), imm.value);
    FlushICache(lis, 2 * sizeof(uint32_t));
}

void
MacroAssembler::subPtr(Register src, Register dest)
{
    as_subf(dest, src, dest); // T = B - A
}

void
MacroAssembler::subPtr(Imm32 imm, Register dest)
{
    ma_dsubu(dest, dest, imm); // inverted at MacroAssembler level
}

void
MacroAssembler::sub64(Register64 src, Register64 dest)
{
    as_subf(dest.reg, src.reg, dest.reg);
}

void
MacroAssembler::sub64(const Operand& src, Register64 dest)
{
    if (src.getTag() == Operand::MEM) {
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        sub64(scratch, dest);
    } else {
        sub64(Register64(src.toReg()), dest);
    }
}

void
MacroAssembler::sub64(Imm64 imm, Register64 dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    mov(ImmWord(imm.value), ScratchRegister);
    as_subf(dest.reg, ScratchRegister, dest.reg); // T = B - A
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest)
{
    MOZ_ASSERT(dest.reg != ScratchRegister);
    mov(ImmWord(imm.value), ScratchRegister);
    as_mulld(dest.reg, ScratchRegister, dest.reg); // low order word
}

void
MacroAssembler::mul64(Imm64 imm, const Register64& dest, const Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    mul64(imm, dest);
}

void
MacroAssembler::mul64(const Register64& src, const Register64& dest, const Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    as_mulld(dest.reg, src.reg, dest.reg); // low order word
}

void
MacroAssembler::mul64(const Operand& src, const Register64& dest, const Register temp)
{
    if (src.getTag() == Operand::MEM) {
        Register64 scratch(ScratchRegister);

        load64(src.toAddress(), scratch);
        mul64(scratch, dest, temp);
    } else {
        mul64(Register64(src.toReg()), dest, temp);
    }
}

void MacroAssembler::mulPtr(Register rhs, Register srcDest) {
    as_mulld(srcDest, srcDest, rhs); // low order word
}

void
MacroAssembler::mulBy3(Register src, Register dest)
{
    // I guess this *is* better than mulli.
    MOZ_ASSERT(src != ScratchRegister);
    as_add(ScratchRegister, src, src);
    as_add(dest, ScratchRegister, src);
}

void
MacroAssembler::inc64(AbsoluteAddress dest)
{
    ma_li(SecondScratchReg, ImmWord(uintptr_t(dest.addr)));
    as_ld(ThirdScratchReg, SecondScratchReg, 0);
    as_addi(ScratchRegister, ThirdScratchReg, 1);
    as_std(ScratchRegister, SecondScratchReg, 0);
}

void
MacroAssembler::negPtr(Register reg)
{
    as_neg(reg, reg);
}

void
MacroAssembler::neg64(Register64 reg)
{
    negPtr(reg.reg);
}

void
MacroAssembler::quotient32(Register rhs, Register srcDest, bool isUnsigned)
{
    if (isUnsigned) {
        as_divwu(srcDest, srcDest, rhs);
    } else {
        as_divw(srcDest, srcDest, rhs);
    }
}

// byte swaps
void
MacroAssembler::byteSwap16SignExtend(Register reg)
{
    byteSwap16ZeroExtend(reg);
    as_extsh(reg, reg);
}

void
MacroAssembler::byteSwap16ZeroExtend(Register reg)
{
    MOZ_ASSERT(reg != ScratchRegister);
    xs_mr(ScratchRegister, reg);
    
    as_rlwinm(reg, ScratchRegister, 8, 16, 23);
    as_rlwimi(reg, ScratchRegister, 24, 24, 31);
}

void
MacroAssembler::byteSwap32(Register reg)
{
    // cribbed off gcc __builtin_swap32
    // POWER10 has/will have brw, but POWER10 is naughty and has non-free
    // RAM firmware. Naughty OMI! Spank spank!
    MOZ_ASSERT(reg != ScratchRegister);
    as_rlwinm(ScratchRegister, reg, 8, 0, 31); // "rotlwi"
    as_rlwimi(ScratchRegister, reg, 24, 0, 7);
    as_rlwimi(ScratchRegister, reg, 24, 16, 23);
    xs_mr(reg, ScratchRegister);
}

void
MacroAssembler::byteSwap64(Register64 reg)
{
    Register r = reg.reg;
    // Use VSX for this because the 64-bit swap with rld* is just hideous.
    // POWER10 has/will have brd, but, more electric spanking of war babies!
    if (HasPPCISA3()) {
        as_mtvsrd(ScratchDoubleReg, r);
        as_xxbrd(ScratchDoubleReg, ScratchDoubleReg);
        as_mfvsrd(r, ScratchDoubleReg);
    } else {
        MOZ_CRASH("NYI for pre-POWER9");
    }
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshiftPtr(Imm32 imm, Register dest)
{
    MOZ_ASSERT(imm.value >= 0);

    if (imm.value == 0) {
        // No-op
    } else {
        as_rldicr(dest, dest, imm.value % 64, 63 - (imm.value % 64)); // "sldi"
    }
}

void
MacroAssembler::lshiftPtr(Register shift, Register dest)
{
    // sld will zero out any shift amount greater than 64, but JavaScript
    // expects this to act like a modulo, so ...
    MOZ_ASSERT(shift != ScratchRegister);
    MOZ_ASSERT(dest != ScratchRegister);

    as_andi_rc(ScratchRegister, shift, 63);
    as_sld(dest, dest, ScratchRegister);
}

void
MacroAssembler::lshift64(Imm32 imm, Register64 dest)
{
    lshiftPtr(imm, dest.reg);
}

void
MacroAssembler::lshift64(Register shift, Register64 dest)
{
    lshiftPtr(shift, dest.reg);
}

void
MacroAssembler::rshiftPtr(Imm32 imm, Register dest)
{
    MOZ_ASSERT(imm.value >= 0);

    // Same deal as rshift32, same twist.
    if (!(imm.value % 64)) {
        if (imm.value == 0) {
            // No-op
        } else {
            // 64 bits right-shifted 64 bits is zero.
            xs_li(dest, 0);
        }
    } else {
        x_srdi(dest, dest, imm.value % 64);
    }
}

void
MacroAssembler::rshiftPtr(Register shift, Register dest)
{
    // JavaScript expects the shift to act like a modulo.
    MOZ_ASSERT(shift != ScratchRegister);
    as_andi_rc(ScratchRegister, shift, 63);
    as_srd(dest, dest, ScratchRegister);
}

void
MacroAssembler::rshift64(Imm32 imm, Register64 dest)
{
    rshiftPtr(imm, dest.reg);
}

void
MacroAssembler::rshift64(Register shift, Register64 dest)
{
    rshiftPtr(shift, dest.reg);
}

void
MacroAssembler::rshiftPtrArithmetic(Imm32 imm, Register dest)
{
    MOZ_ASSERT(0 <= imm.value);
    as_sradi(dest, dest, imm.value % 64);
}

void
MacroAssembler::rshift64Arithmetic(Imm32 imm, Register64 dest)
{
    rshiftPtrArithmetic(imm, dest.reg);
}

void
MacroAssembler::rshift64Arithmetic(Register shift, Register64 dest)
{
    // JavaScript expects the shift to act like a modulo.
    MOZ_ASSERT(shift != ScratchRegister);
    as_andi_rc(ScratchRegister, shift, 63);
    as_srad(dest.reg, dest.reg, ScratchRegister);
}

// ===============================================================
// Rotation functions

void
MacroAssembler::rotateLeft64(Imm32 count, Register64 src, Register64 dest, Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    MOZ_ASSERT(count.value >= 0);

    if (!(count.value % 64)) {
        if (src.reg != dest.reg)
            xs_mr(dest.reg, src.reg);
    } else {
        as_rldicl(dest.reg, src.reg, (count.value % 64), 0); // "rotldi"
    }
}

void
MacroAssembler::rotateLeft64(Register count, Register64 src, Register64 dest, Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    as_rldcl(dest.reg, src.reg, count, 0); // "rotld"
}

void
MacroAssembler::rotateRight64(Imm32 count, Register64 src, Register64 dest, Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    MOZ_ASSERT(count.value >= 0);

    if (!(count.value % 64)) {
        if (src.reg != dest.reg)
            xs_mr(dest.reg, src.reg);
    } else {
        as_rldicl(dest.reg, src.reg, 64 - (count.value % 64), 0); // "rotrdi"
    }
}

void
MacroAssembler::rotateRight64(Register count, Register64 src, Register64 dest, Register temp)
{
    MOZ_ASSERT(temp == InvalidReg);
    MOZ_ASSERT(count != ScratchRegister);
    MOZ_ASSERT(count != SecondScratchReg);
    MOZ_ASSERT(src.reg != ScratchRegister);
    MOZ_ASSERT(src.reg != SecondScratchReg);

    xs_li(ScratchRegister, 64);
    as_andi_rc(SecondScratchReg, count, 63);
    as_subf(ScratchRegister, SecondScratchReg, ScratchRegister); // T = B - A
    as_rldcl(dest.reg, src.reg, ScratchRegister, 0); // "rotrd"
}

// ===============================================================
// Condition functions

template <typename T1, typename T2>
void
MacroAssembler::cmpPtrSet(Condition cond, T1 lhs, T2 rhs, Register dest)
{
    ma_cmp_set(dest, lhs, rhs, cond, /* useCmpw */ false);
}

// Also see below for specializations of cmpPtrSet.

template <typename T1, typename T2>
void
MacroAssembler::cmp32Set(Condition cond, T1 lhs, T2 rhs, Register dest)
{
    ma_cmp_set(dest, lhs, rhs, cond, /* useCmpw */ true);
}

void
MacroAssembler::cmp64Set(Condition cond, Address lhs, Imm64 rhs, Register dest)
{
    ma_cmp_set(dest, lhs, rhs, cond, /* useCmpw */ false);
}

template <typename T>
void
MacroAssembler::testStringSet(Condition cond, const T& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_STRING), cond);
}

template <typename T>
void
MacroAssembler::testBooleanSet(Condition cond, const T& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_BOOLEAN), cond);
}

template <typename T>
void
MacroAssembler::testSymbolSet(Condition cond, const T& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_SYMBOL), cond);
}

template <typename T>
void
MacroAssembler::testBigIntSet(Condition cond, const T& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_BIGINT), cond);
}

template <typename T>
void
MacroAssembler::testNumberSet(Condition cond, const T& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg,
             ImmTag(JS::detail::ValueUpperInclNumberTag),
             cond == Equal ? BelowOrEqual : Above);
}


// ===============================================================
// Bit counting functions

void
MacroAssembler::clz64(Register64 src, Register dest)
{
    as_cntlzd(dest, src.reg);
}

void
MacroAssembler::ctz64(Register64 src, Register dest)
{
    if (HasPPCISA3()) {
        as_cnttzd(dest, src.reg);
    } else {
        MOZ_CRASH("NYI for pre-POWER9");
    }
}

void
MacroAssembler::popcnt64(Register64 input, Register64 output, Register tmp)
{
#if (1)
    as_popcntd(output.reg, input.reg);
#else
    // Prior to ISA 2.06. Not tested.
    ma_move(output.reg, input.reg);
    as_sradi(tmp, input.reg, Imm32(1));
    ma_li(ScratchRegister, ImmWord(0x5555555555555555UL));
    ma_and(tmp, ScratchRegister);
    ma_dsubu(output.reg, tmp);
    as_sradi(tmp, output.reg, Imm32(2));
    ma_li(ScratchRegister, ImmWord(0x3333333333333333UL));
    ma_and(output.reg, ScratchRegister);
    ma_and(tmp, ScratchRegister);
    ma_add(output.reg, tmp);
    ma_dsrl(tmp, output.reg, Imm32(4));
    ma_add(output.reg, tmp);
    ma_li(ScratchRegister, ImmWord(0xF0F0F0F0F0F0F0FUL));
    ma_and(output.reg, ScratchRegister);
    ma_dsll(tmp, output.reg, Imm32(8));
    ma_add(output.reg, tmp);
    ma_dsll(tmp, output.reg, Imm32(16));
    ma_add(output.reg, tmp);
    ma_dsll(tmp, output.reg, Imm32(32));
    ma_add(output.reg, tmp);
    as_sradi(output.reg, output.reg, Imm32(56));
#endif
}

// ===============================================================
// Branch functions

void
MacroAssembler::branch64(Condition cond, Register64 lhs, Imm64 val, Label* success, Label* fail)
{
    MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal ||
               cond == Assembler::LessThan || cond == Assembler::LessThanOrEqual ||
               cond == Assembler::GreaterThan || cond == Assembler::GreaterThanOrEqual ||
               cond == Assembler::Below || cond == Assembler::BelowOrEqual ||
               cond == Assembler::Above || cond == Assembler::AboveOrEqual,
               "other condition codes not supported");

    branchPtr(cond, lhs.reg, ImmWord(val.value), success);
    if (fail)
        jump(fail);
}

void
MacroAssembler::branch64(Condition cond, Register64 lhs, Register64 rhs, Label* success, Label* fail)
{
    MOZ_ASSERT(cond == Assembler::NotEqual || cond == Assembler::Equal ||
               cond == Assembler::LessThan || cond == Assembler::LessThanOrEqual ||
               cond == Assembler::GreaterThan || cond == Assembler::GreaterThanOrEqual ||
               cond == Assembler::Below || cond == Assembler::BelowOrEqual ||
               cond == Assembler::Above || cond == Assembler::AboveOrEqual,
               "other condition codes not supported");

    branchPtr(cond, lhs.reg, rhs.reg, success);
    if (fail)
        jump(fail);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, Imm64 val, Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");

    branchPtr(cond, lhs, ImmWord(val.value), label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, Register64 reg, Label* label)
{
    MOZ_ASSERT(reg.reg != SecondScratchReg);
    MOZ_ASSERT(reg.reg != ScratchRegister);

    loadPtr(lhs, ScratchRegister);
    branchPtr(cond, ScratchRegister, reg.reg, label);
}

void
MacroAssembler::branch64(Condition cond, const Address& lhs, const Address& rhs, Register scratch,
                         Label* label)
{
    MOZ_ASSERT(cond == Assembler::NotEqual,
               "other condition codes not supported");
    MOZ_ASSERT(lhs.base != scratch);
    MOZ_ASSERT(rhs.base != scratch);

    loadPtr(rhs, scratch);
    branchPtr(cond, lhs, scratch, label);
}

void MacroAssembler::branchNeg32(Condition cond, Register reg, Label* label) {
  MOZ_ASSERT(cond == Overflow);
  ma_negTestOverflow(reg, label);
}

void
MacroAssembler::branchPrivatePtr(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    if (rhs != ScratchRegister)
        movePtr(rhs, ScratchRegister);
    // Instead of unboxing lhs, box rhs and do direct comparison with lhs.
    rshiftPtr(Imm32(1), ScratchRegister);
    branchPtr(cond, lhs, ScratchRegister, label);
}

template <class L>
void
MacroAssembler::branchTest64(Condition cond, Register64 lhs, Register64 rhs, Register temp,
                             L label)
{
    branchTestPtr(cond, lhs.reg, rhs.reg, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32Truthy(bool b, const ValueOperand& value, Label* label)
{
    ScratchRegisterScope scratch(*this);
    unboxBoolean(value, scratch);
    ma_bc(scratch, scratch, label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestDouble(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    Condition actual = (cond == Equal) ? BelowOrEqual : Above;
    ma_bc(tag, ImmTag(JSVAL_TAG_MAX_DOUBLE), label, actual);
}

void
MacroAssembler::branchTestDouble(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestNumber(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestNumber(cond, scratch2, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestBooleanTruthy(bool b, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    unboxBoolean(value, scratch2);
    ma_bc(scratch2, scratch2, label, b ? NonZero : Zero);
}

void
MacroAssembler::branchTestString(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestString(cond, scratch2, label);
}

void
MacroAssembler::branchTestStringTruthy(bool b, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    unboxString(value, scratch2);
    ma_load(scratch2, Address(scratch2, JSString::offsetOfLength()), SizeWord, ZeroExtend);
    ma_bc(scratch2, Imm32(0), label, b ? NotEqual : Equal);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestSymbol(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, const ValueOperand& value, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestPrimitive(cond, scratch2, label);
}

template <class L>
void
MacroAssembler::branchTestMagic(Condition cond, const ValueOperand& value, L label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    ma_bc(scratch2, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& valaddr, JSWhyMagic why, Label* label)
{
    uint64_t magic = MagicValue(why).asRawBits();
    SecondScratchRegisterScope scratch(*this);
    loadPtr(valaddr, scratch);
    ma_bc(scratch, ImmWord(magic), label, cond);
}

void
MacroAssembler::branchTestBigInt(Condition cond, const BaseIndex& address,
                                 Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    computeEffectiveAddress(address, scratch2);
    splitTag(scratch2, scratch2);
    branchTestBigInt(cond, scratch2, label);
}

void
MacroAssembler::branchTestBigInt(Condition cond, const ValueOperand& value,
                                 Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    splitTag(value, scratch2);
    branchTestBigInt(cond, scratch2, label);
}

void
MacroAssembler::branchTestBigInt(Condition cond, Register tag,
                                 Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_BIGINT), label, cond);
}

void
MacroAssembler::branchTestBigIntTruthy(bool b, const ValueOperand& value,
                                       Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    unboxBigInt(value, scratch2);
    ma_load(scratch2, Address(scratch2, BigInt::offsetOfDigitLength()), SizeWord, ZeroExtend);
    ma_bc(scratch2, Imm32(0), label, b ? NotEqual : Equal);
}

void
MacroAssembler::branchTruncateDoubleMaybeModUint32(FloatRegister src, Register dest, Label* fail)
{
    MOZ_ASSERT(src != ScratchDoubleReg);

    // If the conversion is invalid (see Power ISA v3.1, page 173), fail.
    as_mtfsb0(23); // whack VXCVI
    as_fctiwz(ScratchDoubleReg, src);
    as_mcrfs(cr0, 5); // reserved - VXSOFT - VXSQRT - VXCVI
    moveFromDouble(ScratchDoubleReg, dest);
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    ma_bc(Assembler::SOBit, fail);
}

void
MacroAssembler::branchTruncateFloat32MaybeModUint32(FloatRegister src, Register dest, Label* fail)
{
    branchTruncateDoubleMaybeModUint32(src, dest, fail);
}

//}}} check_macroassembler_style
// ===============================================================

// The specializations for cmpPtrSet are outside the braces because check_macroassembler_style can't yet
// deal with specializations.

template<>
inline void
MacroAssembler::cmpPtrSet(Assembler::Condition cond, Address lhs, ImmPtr rhs,
                          Register dest)
{
    loadPtr(lhs, SecondScratchReg);
    cmpPtrSet(cond, SecondScratchReg, rhs, dest);
}

template<>
inline void
MacroAssembler::cmpPtrSet(Assembler::Condition cond, Register lhs, Address rhs,
                          Register dest)
{
    loadPtr(rhs, ScratchRegister);
    cmpPtrSet(cond, lhs, ScratchRegister, dest);
}

template<>
inline void
MacroAssembler::cmp32Set(Assembler::Condition cond, Register lhs, Address rhs,
                         Register dest)
{
    load32(rhs, ScratchRegister);
    cmp32Set(cond, lhs, ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::incrementInt32Value(const Address& addr)
{
    asMasm().add32(Imm32(1), addr);
}

/*
void
MacroAssemblerPPC64Compat::computeEffectiveAddress(const BaseIndex& address, Register dest)
{
    computeScaledAddress(address, dest);
    if (address.offset)
        asMasm().addPtr(Imm32(address.offset), dest);
}
*/

void
MacroAssemblerPPC64Compat::retn(Imm32 n)
{
    // pc <- [sp]; sp += n
    loadPtr(Address(StackPointer, 0), ScratchRegister);
    asMasm().addPtr(n, StackPointer);
    xs_mtlr(ScratchRegister);
    as_blr();
}

void MacroAssembler::load32SignExtendToPtr(const Address& src, Register dest)
{
    load32(src, dest);
}

void
MacroAssembler::moveFloat32ToGPR(FloatRegister src, Register dest)
{
    moveFromFloat32(src, dest);
}

void
MacroAssembler::moveGPRToFloat32(Register src, FloatRegister dest)
{
    moveToFloat32(src, dest);
}

void
MacroAssembler::move8SignExtend(Register src, Register dest)
{
    as_extsb(dest, src);
}

void
MacroAssembler::move16SignExtend(Register src, Register dest)
{
    as_extsh(dest, src);
}

void
MacroAssembler::loadAbiReturnAddress(Register dest)
{
    xs_mflr(dest);
}

// ===============================================================
// Logical instructions

void
MacroAssembler::not32(Register reg)
{
    as_nor(reg, reg, reg);
}

void
MacroAssembler::and32(Register src, Register dest)
{
    as_and(dest, dest, src);
}

void
MacroAssembler::and32(Imm32 imm, Register dest)
{
    ma_and(dest, imm);
}

void
MacroAssembler::and32(Imm32 imm, const Address& dest)
{
    ma_load(SecondScratchReg, dest, SizeWord, ZeroExtend);
    ma_and(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::and32(const Address& src, Register dest)
{
    ma_load(SecondScratchReg, src, SizeWord, ZeroExtend);
    ma_and(dest, SecondScratchReg);
}

void
MacroAssembler::or32(Register src, Register dest)
{
    ma_or(dest, src);
}

void
MacroAssembler::or32(Imm32 imm, Register dest)
{
    ma_or(dest, imm);
}

void
MacroAssembler::or32(Imm32 imm, const Address& dest)
{
    ma_load(SecondScratchReg, dest, SizeWord, ZeroExtend);
    ma_or(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::xor32(Register src, Register dest)
{
    ma_xor(dest, src);
}

void
MacroAssembler::xor32(Imm32 imm, Register dest)
{
    ma_xor(dest, imm);
}

void
MacroAssembler::xor32(Imm32 imm, const Address &dest)
{
    ma_load(SecondScratchReg, dest, SizeWord, ZeroExtend);
    ma_xor(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::xor32(const Address& src, Register dest)
{
    ma_load(SecondScratchReg, src, SizeWord, ZeroExtend);
    as_xor(dest, dest, SecondScratchReg);
}

// ===============================================================
// Arithmetic instructions

void
MacroAssembler::add32(Register src, Register dest)
{
    as_add(dest, dest, src);
}

void
MacroAssembler::add32(Imm32 imm, Register dest)
{
    ma_add(dest, dest, imm);
}

void
MacroAssembler::add32(Imm32 imm, const Address& dest)
{
    load32(dest, SecondScratchReg);
    ma_add(SecondScratchReg, imm);
    store32(SecondScratchReg, dest);
}

void
MacroAssembler::addPtr(Imm32 imm, const Address& dest)
{
    MOZ_ASSERT(dest.base != SecondScratchReg);
    loadPtr(dest, SecondScratchReg);
    addPtr(imm, SecondScratchReg);
    storePtr(SecondScratchReg, dest);
}

void
MacroAssembler::addPtr(const Address& src, Register dest)
{
    loadPtr(src, ScratchRegister);
    addPtr(ScratchRegister, dest);
}

void
MacroAssembler::addDouble(FloatRegister src, FloatRegister dest)
{
    as_fadd(dest, dest, src);
}

void
MacroAssembler::addFloat32(FloatRegister src, FloatRegister dest)
{
    as_fadds(dest, dest, src);
}

void
MacroAssembler::sub32(Register src, Register dest)
{
    as_subf(dest, src, dest); // T = B - A
}

void
MacroAssembler::sub32(Imm32 imm, Register dest)
{
    ma_subu(dest, dest, imm); // switched at MA level
}

void
MacroAssembler::sub32(const Address& src, Register dest)
{
    load32(src, SecondScratchReg);
    as_subf(dest, SecondScratchReg, dest); // T = B - A
}

void
MacroAssembler::subPtr(Register src, const Address& dest)
{
    loadPtr(dest, SecondScratchReg);
    subPtr(src, SecondScratchReg);
    storePtr(SecondScratchReg, dest);
}

void
MacroAssembler::subPtr(const Address& addr, Register dest)
{
    loadPtr(addr, SecondScratchReg);
    subPtr(SecondScratchReg, dest);
}

void
MacroAssembler::subDouble(FloatRegister src, FloatRegister dest)
{
    as_fsub(dest, dest, src); // T = A - B
}

void
MacroAssembler::subFloat32(FloatRegister src, FloatRegister dest)
{
    as_fsubs(dest, dest, src); // T = A - B
}

void
MacroAssembler::mul32(Imm32 rhs, Register srcDest)
{
    if (Imm16::IsInSignedRange(rhs.value)) {
        // See also ::mulBy3
        xs_sr_mulli(srcDest, srcDest, (int16_t)rhs.value);
    } else {
        // Wouldn't be any of our strength-reduced shortcuts anyway.
        MOZ_ASSERT(srcDest != ScratchRegister);
        ma_li(ScratchRegister, rhs);
        as_mullw(srcDest, srcDest, ScratchRegister);
    }
    // Clear the upper 32 bits. We wouldn't use this for an intermediate
    // multiply anyway. Do not sign extend.
    as_rldicl(srcDest, srcDest, 0, 32); // "clrldi"
}

void
MacroAssembler::mul32(Register rhs, Register srcDest)
{
    as_mullw(srcDest, srcDest, rhs);
    // Clear the upper 32 bits. Do not sign extend.
    as_rldicl(srcDest, srcDest, 0, 32); // "clrldi"
}

void
MacroAssembler::mulFloat32(FloatRegister src, FloatRegister dest)
{
    as_fmuls(dest, dest, src);
}

void
MacroAssembler::mulDouble(FloatRegister src, FloatRegister dest)
{
    as_fmul(dest, dest, src);
}

void
MacroAssembler::mulDoublePtr(ImmPtr imm, Register temp, FloatRegister dest)
{
    movePtr(imm, SecondScratchReg);
    loadDouble(Address(SecondScratchReg, 0), ScratchDoubleReg);
    mulDouble(ScratchDoubleReg, dest);
}

void
MacroAssembler::remainder32(Register rhs, Register srcDest, bool isUnsigned)
{
    if (HasPPCISA3()) {
        if (isUnsigned) {
            as_moduw(srcDest, srcDest, rhs);
        } else {
            as_modsw(srcDest, srcDest, rhs);
        }
        return;
    }

    if (isUnsigned)
        as_divwu(ScratchRegister, srcDest, rhs);
    else
        as_divw(ScratchRegister, srcDest, rhs);
    // Recover remainder.
    as_mullw(SecondScratchReg, ScratchRegister, rhs);
    as_subf(srcDest, SecondScratchReg, srcDest); // T = B - A
}

void
MacroAssembler::divFloat32(FloatRegister src, FloatRegister dest)
{
    as_fdivs(dest, dest, src);
}

void
MacroAssembler::divDouble(FloatRegister src, FloatRegister dest)
{
    as_fdiv(dest, dest, src);
}

void
MacroAssembler::neg32(Register reg)
{
    as_neg(reg, reg);
}

void
MacroAssembler::negateDouble(FloatRegister reg)
{
    as_fneg(reg, reg);
}

void
MacroAssembler::negateFloat(FloatRegister reg)
{
    as_fneg(reg, reg);
}

void
MacroAssembler::abs32(Register src, Register dest)
{
    // PowerPC Compiler Writer's Guide page 50
    as_srawi(ScratchRegister, src, 31);
    as_xor(SecondScratchReg, ScratchRegister, src);
    as_subf(dest, ScratchRegister, SecondScratchReg);
}

void
MacroAssembler::absFloat32(FloatRegister src, FloatRegister dest)
{
    as_fabs(dest, src);
}

void
MacroAssembler::absDouble(FloatRegister src, FloatRegister dest)
{
    as_fabs(dest, src);
}

void
MacroAssembler::sqrtFloat32(FloatRegister src, FloatRegister dest)
{
    as_fsqrts(dest, src);
}

void
MacroAssembler::sqrtDouble(FloatRegister src, FloatRegister dest)
{
    as_fsqrt(dest, src);
}

void
MacroAssembler::minFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    minMaxDouble(srcDest, other, handleNaN, false);
}

void
MacroAssembler::minDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    minMaxDouble(srcDest, other, handleNaN, false);
}

void
MacroAssembler::maxFloat32(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    minMaxDouble(srcDest, other, handleNaN, true);
}

void
MacroAssembler::maxDouble(FloatRegister other, FloatRegister srcDest, bool handleNaN)
{
    minMaxDouble(srcDest, other, handleNaN, true);
}

// ===============================================================
// Shift functions

void
MacroAssembler::lshift32(Register src, Register dest)
{
    // slw will zero out any shift amount greater than 32, but JavaScript
    // expects this to act like a modulo, so ...
    MOZ_ASSERT(src != ScratchRegister);
    MOZ_ASSERT(dest != ScratchRegister);

    as_andi_rc(ScratchRegister, src, 31);
    as_slw(dest, dest, ScratchRegister);
}

void
MacroAssembler::lshift32(Imm32 imm, Register dest)
{
    // Mod the constant directly, et voila.
    x_slwi(dest, dest, imm.value % 32);
}

void
MacroAssembler::flexibleLshift32(Register src, Register dest)
{
    lshift32(src, dest);
}

void
MacroAssembler::rshift32(Register src, Register dest)
{
    // Same deal.
    MOZ_ASSERT(src != ScratchRegister);
    MOZ_ASSERT(dest != ScratchRegister);

    as_andi_rc(ScratchRegister, src, 31);
    as_srw(dest, dest, ScratchRegister);
}

void
MacroAssembler::rshift32(Imm32 imm, Register dest)
{
    // Same deal with a twist:
    // If imm.value is a multiple of 32, then n = 0, and we assert because
    // the underlying rlwinm has to encode 32 in a 5-bit field. So, if the
    // modulo is zero, do nothing or load zero instead (based on what srawi
    // does on 64-bit systems, minus the sign extension).
    if (!(imm.value % 32)) {
        if (imm.value == 0) {
            // No-op // as_rldicl(dest, dest, 0, 32); // "clrldi"
        } else {
            // Right-shifting a 32-bit quantity 32 bits is zero.
            xs_li(dest, 0);
        }
    } else {
        x_srwi(dest, dest, imm.value % 32);
    }
}

void
MacroAssembler::flexibleRshift32(Register src, Register dest)
{
    rshift32(src, dest);
}

void
MacroAssembler::rshift32Arithmetic(Register src, Register dest)
{
    // Same deal.
    MOZ_ASSERT(src != ScratchRegister);
    as_andi_rc(ScratchRegister, src, 31);
    as_sraw(dest, dest, ScratchRegister);
}

void
MacroAssembler::rshift32Arithmetic(Imm32 imm, Register dest)
{
    as_srawi(dest, dest, imm.value % 32);
}

void
MacroAssembler::flexibleRshift32Arithmetic(Register src, Register dest)
{
    rshift32Arithmetic(src, dest);
}

// ===============================================================
// Rotation functions (32-bit unless specified otherwise)
void
MacroAssembler::rotateLeft(Imm32 count, Register input, Register dest)
{
    if (count.value) {
        as_rlwinm(dest, input, count.value % 32, 0, 31); // "rotlwi"
    } else {
        if (input != dest)
            ma_move(dest, input);
    }
}
void
MacroAssembler::rotateLeft(Register count, Register input, Register dest)
{
    as_rlwnm(dest, input, count, 0, 31); // "rotlw"
}
void
MacroAssembler::rotateRight(Imm32 count, Register input, Register dest)
{
    if (count.value) {
        as_rlwinm(dest, input, 32 - (count.value % 32), 0, 31); // "rotrwi"
    } else {
        if (input != dest)
            ma_move(dest, input);
    }
}
void
MacroAssembler::rotateRight(Register count, Register input, Register dest)
{
    MOZ_ASSERT(input != ScratchRegister);
    MOZ_ASSERT(count != ScratchRegister);
    MOZ_ASSERT(input != SecondScratchReg);
    MOZ_ASSERT(count != SecondScratchReg);

    xs_li(ScratchRegister, 32);
    as_andi_rc(SecondScratchReg, count, 31);
    as_subf(ScratchRegister, SecondScratchReg, ScratchRegister); // T = B - A
    as_rlwnm(dest, input, ScratchRegister, 0, 31); // "rotrw"
}

// ===============================================================
// Bit counting functions

void
MacroAssembler::clz32(Register src, Register dest, bool knownNotZero)
{
    as_cntlzw(dest, src);
}

void
MacroAssembler::ctz32(Register src, Register dest, bool knownNotZero)
{
    as_cnttzw(dest, src);
}

void
MacroAssembler::popcnt32(Register input,  Register output, Register tmp)
{
    // Sing to the tune of Revolution No. 9:
    // POWER9
    // POWER9
    // POWER9
    // POWER9 etc.
    as_popcntw(output, input);
}

// ===============================================================
// Condition functions

void MacroAssembler::cmp8Set(Condition cond, Address lhs, Imm32 rhs,
                             Register dest) {
  MOZ_ASSERT(lhs.base != SecondScratchReg);

  // Automatically generates a 32-bit compare.
  switch (cond) {
    case Assembler::Equal:
    case Assembler::NotEqual:
    case Assembler::Above:
    case Assembler::AboveOrEqual:
    case Assembler::Below:
    case Assembler::BelowOrEqual:
      load8ZeroExtend(lhs, SecondScratchReg);
      ma_cmp_set(dest, SecondScratchReg, Imm32(uint8_t(rhs.value)), cond);
      break;

    case Assembler::GreaterThan:
    case Assembler::GreaterThanOrEqual:
    case Assembler::LessThan:
    case Assembler::LessThanOrEqual:
      load8SignExtend(lhs, SecondScratchReg);
      ma_cmp_set(dest, SecondScratchReg, Imm32(int8_t(rhs.value)), cond);
      break;

    default:
      MOZ_CRASH("unexpected condition");
  }
}

void MacroAssembler::cmp16Set(Condition cond, Address lhs, Imm32 rhs,
                              Register dest) {
  MOZ_ASSERT(lhs.base != SecondScratchReg);

  // Automatically generates a 32-bit compare.
  switch (cond) {
    case Assembler::Equal:
    case Assembler::NotEqual:
    case Assembler::Above:
    case Assembler::AboveOrEqual:
    case Assembler::Below:
    case Assembler::BelowOrEqual:
      load16ZeroExtend(lhs, SecondScratchReg);
      ma_cmp_set(dest, SecondScratchReg, Imm32(uint16_t(rhs.value)), cond);
      break;

    case Assembler::GreaterThan:
    case Assembler::GreaterThanOrEqual:
    case Assembler::LessThan:
    case Assembler::LessThanOrEqual:
      load16SignExtend(lhs, SecondScratchReg);
      ma_cmp_set(dest, SecondScratchReg, Imm32(int16_t(rhs.value)), cond);
      break;

    default:
      MOZ_CRASH("unexpected condition");
  }
}


// ===============================================================
// Branch functions

void MacroAssembler::branch8(Condition cond, const Address& lhs, Imm32 rhs,
                             Label* label) {
  MOZ_ASSERT(lhs.base != SecondScratchReg);

  switch (cond) {
    case Assembler::Equal:
    case Assembler::NotEqual: 
    case Assembler::Above:
    case Assembler::AboveOrEqual:
    case Assembler::Below:
    case Assembler::BelowOrEqual:
      load8ZeroExtend(lhs, SecondScratchReg);
      branch32(cond, SecondScratchReg, Imm32(uint8_t(rhs.value)), label);
      break;
  
    case Assembler::GreaterThan:
    case Assembler::GreaterThanOrEqual:
    case Assembler::LessThan:
    case Assembler::LessThanOrEqual:
      load8SignExtend(lhs, SecondScratchReg);
      branch32(cond, SecondScratchReg, Imm32(int8_t(rhs.value)), label);
      break;

    default:
      MOZ_CRASH("unexpected condition"); 
  }
} 

void MacroAssembler::branch16(Condition cond, const Address& lhs, Imm32 rhs,
                              Label* label) {
  MOZ_ASSERT(lhs.base != SecondScratchReg);

  switch (cond) {
    case Assembler::Equal:
    case Assembler::NotEqual: 
    case Assembler::Above:
    case Assembler::AboveOrEqual:
    case Assembler::Below:
    case Assembler::BelowOrEqual:
      load16ZeroExtend(lhs, SecondScratchReg);
      branch32(cond, SecondScratchReg, Imm32(uint16_t(rhs.value)), label);
      break;
  
    case Assembler::GreaterThan:
    case Assembler::GreaterThanOrEqual:
    case Assembler::LessThan:
    case Assembler::LessThanOrEqual:
      load16SignExtend(lhs, SecondScratchReg);
      branch32(cond, SecondScratchReg, Imm32(int16_t(rhs.value)), label);
      break;

    default:
      MOZ_CRASH("unexpected condition"); 
  }
} 

// XXX: need 32-bit hints here
template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Register rhs, L label)
{
    ma_bc(lhs, rhs, label, cond);
}

template <class L>
void
MacroAssembler::branch32(Condition cond, Register lhs, Imm32 imm, L label)
{
    ma_bc(lhs, imm, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Register rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_bc(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_bc(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_bc(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_bc(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, const BaseIndex& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    ma_bc(SecondScratchReg, rhs, label, cond);
}

void
MacroAssembler::branch32(Condition cond, wasm::SymbolicAddress addr, Imm32 imm, Label* label)
{
    load32(addr, SecondScratchReg);
    ma_bc(SecondScratchReg, imm, label, cond);
}

template <class L>
void
MacroAssembler::branchPtr(Condition cond, Register lhs, Register rhs, L label)
{
    ma_bc(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    // Need to hint the MacroAssembler that this needs a 64-bit compare.
    ma_bc64(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmPtr rhs, Label* label)
{
    ma_bc(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmGCPtr rhs, Label* label)
{
    ma_bc(lhs, rhs, label, cond);
}

void
MacroAssembler::branchPtr(Condition cond, Register lhs, ImmWord rhs, Label* label)
{
    ma_bc(lhs, rhs, label, cond);
}

template <class L>
void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, Register rhs, L label)
{
    MOZ_ASSERT(rhs != SecondScratchReg);
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmPtr rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmGCPtr rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const Address& lhs, ImmWord rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, Register rhs, Label* label)
{
    MOZ_ASSERT(rhs != SecondScratchReg);
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const AbsoluteAddress& lhs, ImmWord rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, wasm::SymbolicAddress lhs, Register rhs, Label* label)
{
    MOZ_ASSERT(rhs != SecondScratchReg);
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const BaseIndex& lhs, ImmWord rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchPtr(Condition cond, const BaseIndex& lhs, Register rhs, Label* label)
{
    MOZ_ASSERT(rhs != SecondScratchReg);
    loadPtr(lhs, SecondScratchReg);
    branchPtr(cond, SecondScratchReg, rhs, label);
}

void MacroAssembler::branchTestValue(Condition cond, const BaseIndex& lhs,
                                     const ValueOperand& rhs, Label* label) {
  MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
  branchPtr(cond, lhs, rhs.valueReg(), label);
}

void
MacroAssembler::branchFloat(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                            Label* label)
{
    ma_bc(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTruncateFloat32ToInt32(FloatRegister src, Register dest, Label* fail)
{
    MOZ_CRASH();
}

void
MacroAssembler::branchDouble(DoubleCondition cond, FloatRegister lhs, FloatRegister rhs,
                             Label* label)
{
    ma_bc(cond, lhs, rhs, label);
}

void
MacroAssembler::branchTruncateDoubleToInt32(FloatRegister src, Register dest, Label* fail)
{
    MOZ_CRASH();
}

void
MacroAssembler::branchMulPtr(Condition cond, Register src, Register dest, Label *overflow)
{
    as_mulldo_rc(dest, src, dest);
    ma_bc(cond, overflow);
}

// The src type will autodetermine 32-bit mode.
template <typename T>
void
MacroAssembler::branchAdd32(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_addTestOverflow(dest, dest, src, overflow);
        break;
      case CarryClear:
      case CarrySet:
        ma_addTestCarry(cond, dest, dest, src, overflow);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

template <typename T>
void
MacroAssembler::branchSub32(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_subTestOverflow(dest, dest, src, overflow);
        break;
      case NonZero:
      case Zero:
      case NotSigned:
      case Signed:
        ma_subu(dest, src);
        ma_bc(dest, dest, overflow, cond);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

template <typename T>
void
MacroAssembler::branchAddPtr(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_addTestOverflow(dest, dest, src, overflow, /* is32 */ false);
        break;
      case CarryClear:
      case CarrySet:
        ma_addTestCarry(cond, dest, dest, src, overflow, /* is32 */ false);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

template <typename T>
void
MacroAssembler::branchSubPtr(Condition cond, T src, Register dest, Label* overflow)
{
    switch (cond) {
      case Overflow:
        ma_subTestOverflow(dest, dest, src, overflow, /* is32 */ false);
        break;
      case NonZero:
      case Zero:
      case NotSigned:
      case Signed:
        ma_subu(dest, src);
        ma_bc(dest, dest, overflow, cond);
        break;
      default:
        MOZ_CRASH("NYI");
    }
}

template <>
inline void
MacroAssembler::branchMul32(Condition cond, Register src, Register dest, Label* label)
{
    as_mullwo_rc(dest, dest, src);
    ma_bc(cond, label);
}

template <typename T>
void
MacroAssembler::branchMul32(Condition cond, T src, Register dest, Label* label)
{
    MOZ_CRASH("NYI");
}

template <>
inline void
MacroAssembler::branchRshift32(Condition cond, Imm32 shift, Register srcDest,
        Label *label)
{
    as_rlwinm_rc(srcDest, srcDest, 32 - shift.value, shift.value, 31);
    ma_bc(cond, label);
}

template<typename T>
void
MacroAssembler::branchRshift32(Condition cond, T shift, Register srcDest,
        Label *label)
{
    MOZ_CRASH("No default implementation");
}

void
MacroAssembler::decBranchPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    subPtr(rhs, lhs);
    branchPtr(cond, lhs, Imm32(0), label);
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    if (cond == Signed || cond == NotSigned) {
        MOZ_ASSERT(lhs == rhs);
        // Sign extend first.
        as_extsw(lhs, lhs);
    }
    if (lhs == rhs) {
        ma_bc(lhs, rhs, label, cond);
    } else {
        // XXX: trim to 32 bits?
        as_and(ScratchRegister, lhs, rhs);
        ma_bc(ScratchRegister, ScratchRegister, label, cond);
    }
}

template <class L>
void
MacroAssembler::branchTest32(Condition cond, Register lhs, Imm32 rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero);
    // XXX: trim to 32 bits?
    ma_and(ScratchRegister, lhs, rhs);
    ma_bc(ScratchRegister, ScratchRegister, label, cond);
}

void
MacroAssembler::branchTest32(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    branchTest32(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchTest32(Condition cond, const AbsoluteAddress& lhs, Imm32 rhs, Label* label)
{
    load32(lhs, SecondScratchReg);
    branchTest32(cond, SecondScratchReg, rhs, label);
}

// XXX: Improve branchTest* to use |and.| and test results directly, as noted:
template <class L>
void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Register rhs, L label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    if (lhs == rhs) {
        ma_bc(lhs, rhs, label, cond);
    } else {
        as_and(ScratchRegister, lhs, rhs);
        ma_bc(ScratchRegister, ScratchRegister, label, cond);
    }
}

void
MacroAssembler::branchTestPtr(Condition cond, Register lhs, Imm32 rhs, Label* label)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == NotSigned);
    ma_and(ScratchRegister, lhs, rhs);
    ma_bc(ScratchRegister, ScratchRegister, label, cond);
}

void
MacroAssembler::branchTestPtr(Condition cond, const Address& lhs, Imm32 rhs, Label* label)
{
    loadPtr(lhs, SecondScratchReg);
    branchTestPtr(cond, SecondScratchReg, rhs, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_UNDEFINED), label, cond);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestUndefined(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestUndefined(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_INT32), label, cond);
}

void
MacroAssembler::branchTestInt32(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestInt32(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestInt32(cond, scratch2, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestDouble(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestDouble(cond, scratch2, label);
}

void
MacroAssembler::branchTestDoubleTruthy(bool b, FloatRegister value, Label* label)
{
    ma_lid(ScratchDoubleReg, 0.0);
    DoubleCondition cond = b ? DoubleNotEqual : DoubleEqualOrUnordered;
    ma_bc(cond, value, ScratchDoubleReg, label);
}

void
MacroAssembler::branchTestNumber(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    Condition actual = cond == Equal ? BelowOrEqual : Above;
    ma_bc(tag, ImmTag(JS::detail::ValueUpperInclNumberTag), label, actual);
}

void
MacroAssembler::branchTestBoolean(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_BOOLEAN), label, cond);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestBoolean(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestBoolean(cond, scratch2, label);
}

void
MacroAssembler::branchTestString(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_STRING), label, cond);
}

void
MacroAssembler::branchTestString(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestString(cond, scratch2, label);
}

void
MacroAssembler::branchTestString(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestString(cond, scratch2, label);
}

void
MacroAssembler::branchTestSymbol(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_SYMBOL), label, cond);
}

void
MacroAssembler::branchTestSymbol(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestSymbol(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_NULL), label, cond);
}

void
MacroAssembler::branchTestNull(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestNull(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestNull(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_OBJECT), label, cond);
}

void
MacroAssembler::branchTestObject(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestObject(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestObject(cond, scratch2, label);
}

void
MacroAssembler::branchTestGCThing(Condition cond, const Address& address, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    ma_bc(scratch2, ImmTag(JS::detail::ValueLowerInclGCThingTag), label,
         (cond == Equal) ? AboveOrEqual : Below);
}
void
MacroAssembler::branchTestGCThing(Condition cond, const BaseIndex& address, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    ma_bc(scratch2, ImmTag(JS::detail::ValueLowerInclGCThingTag), label,
         (cond == Equal) ? AboveOrEqual : Below);
}
void
MacroAssembler::branchTestGCThing(Condition cond, const ValueOperand& address, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    ma_bc(scratch2, ImmTag(JS::detail::ValueLowerInclGCThingTag), label,
         (cond == Equal) ? AboveOrEqual : Below);
}

void
MacroAssembler::branchTestPrimitive(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JS::detail::ValueUpperExclPrimitiveTag), label,
         (cond == Equal) ? Below : AboveOrEqual);
}

void
MacroAssembler::branchTestMagic(Condition cond, Register tag, Label* label)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_bc(tag, ImmTag(JSVAL_TAG_MAGIC), label, cond);
}

void
MacroAssembler::branchTestMagic(Condition cond, const Address& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestMagic(cond, scratch2, label);
}

void
MacroAssembler::branchTestMagic(Condition cond, const BaseIndex& address, Label* label)
{
    SecondScratchRegisterScope scratch2(*this);
    extractTag(address, scratch2);
    branchTestMagic(cond, scratch2, label);
}

void
MacroAssembler::branchToComputedAddress(const BaseIndex& addr)
{
    // TODO: We should just be able to call loadPtr here, but we get values
    // with the high word still set.
    //loadPtr(addr, ScratchRegister);

    int32_t shift = Imm32::ShiftOf(addr.scale).value;
    MOZ_ASSERT(addr.base != ScratchRegister);
    MOZ_ASSERT(addr.index != ScratchRegister);
    MOZ_ASSERT(addr.base != SecondScratchReg);
    MOZ_ASSERT(addr.index != SecondScratchReg);

    // Ass-U-Me that addr.index is always positive.
    if (shift) {
        ma_dsll(ScratchRegister, addr.index, Imm32(shift));
        as_rldicl(ScratchRegister, ScratchRegister, 0, 32); // "clrldi"
        as_add(SecondScratchReg, addr.base, ScratchRegister);
    } else {
        // XXX: merge with following ld into ldx
        as_add(SecondScratchReg, addr.base, addr.index);
    }
    as_ld(SecondScratchReg, SecondScratchReg, 0);
    xs_mtctr(SecondScratchReg);
    as_bctr(DontLinkB);
}

void
MacroAssembler::cmp32Move32(Condition cond, Register lhs, Register rhs, Register src,
                            Register dest)
{
    ma_cmp32(lhs, rhs, cond);
    // Ass-U-Me that ma_cmp32 selected the correct compare, and mask off any
    // synthetic bits. isel will assert on any conditions it can't encode.
    as_isel(dest, src, dest, (cond & 0xff));
}

void
MacroAssembler::cmp32MovePtr(Condition cond, Register lhs, Imm32 rhs, Register src,
                             Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp32(lhs, rhs, cond);
    // isel cannot test for the absence of a bit.
    if (cond == Equal) {
        as_isel(dest, src, dest, Equal);
    } else {
        // Flip the order.
        as_isel(dest, dest, src, Equal);
    }
}

void
MacroAssembler::cmp32Move32(Condition cond, Register lhs, const Address& rhs,
                            Register src,
                            Register dest)
{
    if (cond == Below) {
        ma_cmp32(lhs, rhs, cond);
        as_isel(dest, src, dest, LessThan);
        return;
    }
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp32(lhs, rhs, cond);
    if (cond == Equal) {
        as_isel(dest, src, dest, Equal);
    } else {
        as_isel(dest, dest, src, Equal);
    }
}

void MacroAssembler::cmpPtrMovePtr(Condition cond, Register lhs, Register rhs,
                                   Register src, Register dest) {
    if (cond == Below) {
        as_cmpld(lhs, rhs);
        as_isel(dest, src, dest, LessThan);
        return;
    }
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    as_cmpd(lhs, rhs);
    if (cond == Equal) {
        as_isel(dest, src, dest, Equal);
    } else {
        as_isel(dest, dest, src, Equal);
    }
}

void MacroAssembler::cmpPtrMovePtr(Condition cond, Register lhs,
                                   const Address& rhs, Register src,
                                   Register dest) {
    MOZ_ASSERT(lhs != ScratchRegister);
    MOZ_ASSERT(src != ScratchRegister);
    MOZ_ASSERT(lhs != SecondScratchReg); // may be used by ma_load
    MOZ_ASSERT(src != SecondScratchReg);
    MOZ_ASSERT(dest != ScratchRegister);
    MOZ_ASSERT(dest != SecondScratchReg);

    ma_load(ScratchRegister, rhs, SizeDouble);
    cmpPtrMovePtr(cond, lhs, ScratchRegister, src, dest);
}

void MacroAssembler::cmp32Load32(Condition cond, Register lhs,
                                 const Address& rhs, const Address& src,
                                 Register dest)
{
  MOZ_CRASH("No known use cases");
}

void
MacroAssembler::cmp32Load32(Condition cond, Register lhs, Register rhs,
                            const Address& src, Register dest)
{
  MOZ_CRASH("No known use cases");
}

void
MacroAssembler::cmp32LoadPtr(Condition cond, const Address& lhs, Imm32 rhs,
                             const Address& src, Register dest)
{
    Label skip;
    // XXX: turn into a load and isel
    branch32(Assembler::InvertCondition(cond), lhs, rhs, &skip);
    loadPtr(src, dest);
    bind(&skip);
}

// Constant-time conditional moves (basically isel all the things, because
// it is not subject to branch prediction).
void
MacroAssembler::test32LoadPtr(Condition cond, const Address& addr, Imm32 mask,
                              const Address& src, Register dest)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero);
    const uint32_t mm = mask.value;

    ma_load(ScratchRegister, addr, SizeWord, ZeroExtend);
    if (Imm16::IsInUnsignedRange(mm)) {
        as_andi_rc(ScratchRegister, ScratchRegister, mm); // -> CR0[EQ]
    } else {
        ma_li(SecondScratchReg, mm);
        as_and_rc(ScratchRegister, ScratchRegister, SecondScratchReg);
    }
    ma_load(SecondScratchReg, src, SizeDouble); // pointer-sized
    // If the condition is true, set dest to src. However, isel cannot
    // test for the absence of a bit, and it cannot test for multiple bits, so
    // footwork is required.
    if (cond == Zero) {
        MOZ_ASSERT(cond == (Equal | ConditionZero));
        as_isel(dest, SecondScratchReg, dest, Assembler::Equal);
    } else {
        // Flip the order.
        MOZ_ASSERT(cond == (NotEqual | ConditionZero));
        as_isel(dest, dest, SecondScratchReg, Assembler::Equal);
    }
}

void
MacroAssembler::test32MovePtr(Condition cond, const Address& addr, Imm32 mask,
                              Register src, Register dest)
{
    MOZ_ASSERT(cond == Zero || cond == NonZero);
    MOZ_ASSERT(src != ScratchRegister);
    MOZ_ASSERT(src != SecondScratchReg);
    const uint32_t mm = mask.value;

    ma_load(ScratchRegister, addr, SizeWord, ZeroExtend);
    if (Imm16::IsInUnsignedRange(mm)) {
        as_andi_rc(ScratchRegister, ScratchRegister, mm); // -> CR0[EQ]
    } else {
        ma_li(SecondScratchReg, mm);
        as_and_rc(ScratchRegister, ScratchRegister, SecondScratchReg);
    }
    if (cond == Zero) {
        MOZ_ASSERT(cond == (Equal | ConditionZero));
        as_isel(dest, src, dest, Assembler::Equal);
    } else {
        // Flip the order.
        MOZ_ASSERT(cond == (NotEqual | ConditionZero));
        as_isel(dest, dest, src, Assembler::Equal);
    }
}

void
MacroAssembler::spectreBoundsCheck32(Register index, Register length,
                                     Register maybeScratch, Label* failure)
{
    branch32(Assembler::BelowOrEqual, length, index, failure);
    if (JitOptions.spectreIndexMasking) {
        // The result of the compare is still in cr0, and the compare was
        // already done unsigned, so we just generate an iselgt. The second
        // register is unimportant, because we know this will always be true.
        as_isel(index, index, length, Assembler::GreaterThan);
    }
}

void
MacroAssembler::spectreBoundsCheck32(Register index, const Address& length,
                                     Register maybeScratch, Label* failure)
{
    branch32(Assembler::BelowOrEqual, length, index, failure);
    if (JitOptions.spectreIndexMasking) {
        // r12 will likely have |length| in it anyway from the above
        // operation, but it doesn't matter anyhow; see above.
        as_isel(index, index, SecondScratchReg, Assembler::GreaterThan);
    }
}

// Same as above
void
MacroAssembler::spectreBoundsCheckPtr(Register index, Register length,
                                      Register maybeScratch, Label* failure)
{
    branchPtr(Assembler::BelowOrEqual, length, index, failure);
    if (JitOptions.spectreIndexMasking) {
        as_isel(index, index, length, Assembler::GreaterThan);
    }
}

void
MacroAssembler::spectreBoundsCheckPtr(Register index, const Address& length,
                                      Register maybeScratch, Label* failure)
{
    branchPtr(Assembler::BelowOrEqual, length, index, failure);
    if (JitOptions.spectreIndexMasking) {
        as_isel(index, index, SecondScratchReg, Assembler::GreaterThan);
    }
}

void
MacroAssembler::spectreMovePtr(Condition cond, Register src, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);

    // isel cannot test for the non-existence of a bit.
    if (cond == Equal) {
        as_isel(dest, src, dest, Assembler::Equal);
    } else {
        // Flip the order.
        as_isel(dest, dest, src, Assembler::Equal);
    }
}

void
MacroAssembler::spectreZeroRegister(Condition cond, Register scratch, Register dest)
{
    // Zero the register if *true*. Hold my beer.
    MOZ_ASSERT(cond == Equal || cond == NotEqual);

    if (cond == NotEqual) {
        xs_li(ScratchRegister, 0);
        as_isel(dest, dest, ScratchRegister, Assembler::Equal);
    } else {
        as_isel0(dest, ScratchRegister, dest, Assembler::Equal); // mscdfr0
    }
}

void
MacroAssembler::fallibleUnboxPtr(const ValueOperand& src, Register dest,
                                      JSValueType type, Label* fail)
{
    MOZ_ASSERT(type == JSVAL_TYPE_OBJECT || type == JSVAL_TYPE_STRING ||
            type == JSVAL_TYPE_SYMBOL || type == JSVAL_TYPE_BIGINT);
    // dest := src XOR mask
    // scratch := dest >> JSVAL_TAG_SHIFT
    // fail if scratch != 0
    //
    // Note: src and dest can be the same register
    ScratchRegisterScope scratch(asMasm());
    mov(ImmWord(JSVAL_TYPE_TO_SHIFTED_TAG(type)), scratch);
    ma_xor(scratch, src.valueReg());
    ma_move(dest, scratch);
    x_srdi(scratch, scratch, JSVAL_TAG_SHIFT);
    ma_bc(scratch, Imm32(0), fail, Assembler::NotEqual);
}

void
MacroAssembler::fallibleUnboxPtr(const Address& src, Register dest,
                                      JSValueType type, Label* fail)
{
    loadValue(src, ValueOperand(dest));
    fallibleUnboxPtr(ValueOperand(dest), dest, type, fail);
}

void MacroAssembler::fallibleUnboxPtr(const BaseIndex& src, Register dest,
                                      JSValueType type, Label* fail)
{
    loadValue(src, ValueOperand(dest));
    fallibleUnboxPtr(ValueOperand(dest), dest, type, fail);
}

// ========================================================================
// Memory access primitives.

void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const Address& addr)
{
    ma_sd(src, addr);
}
void
MacroAssembler::storeUncanonicalizedDouble(FloatRegister src, const BaseIndex& addr)
{
    ma_sd(src, addr);
}

void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const Address& addr)
{
    ma_ss(src, addr);
}
void
MacroAssembler::storeUncanonicalizedFloat32(FloatRegister src, const BaseIndex& addr)
{
    ma_ss(src, addr);
}

void
MacroAssembler::memoryBarrier(MemoryBarrierBits barrier)
{
    xs_lwsync();
}

// ===============================================================
// Clamping functions.

void
MacroAssembler::clampIntToUint8(Register reg)
{
    // If reg is < 0, then we want to clamp to 0.
    // If reg is >= 255, then we want to clamp to 255.
    // Essentially, compute max(reg,0), then min(reg,255).
    // This is pretty much what isel was designed for.
    ma_li(ScratchRegister, (int64_t)0); // make gcc happy just make it happy
    ma_li(SecondScratchReg, 255);
    as_cmpd(reg, ScratchRegister); // emit to CR0
    as_cmpd(cr1, reg, SecondScratchReg); // emit to CR1
    // Naughtiness: since ScratchRegister is r0, the load is
    // zero anyway (this is a "mscdfr0" instruction). I just
    // wanted to point out to you how clever I am.
    as_isel0(reg, ScratchRegister, reg, (uint16_t)Assembler::LessThan); // CR0[LT]
    as_isel(reg, SecondScratchReg, reg, (uint16_t)Assembler::GreaterThan, cr1); // CR1[GT]
}

//}}} check_macroassembler_style
// ===============================================================

} // namespace jit
} // namespace js

#endif /* jit_ppc64le_MacroAssembler_ppc64le_inl_h */
