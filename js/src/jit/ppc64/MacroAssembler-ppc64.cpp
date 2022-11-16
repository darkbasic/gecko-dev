/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ppc64/MacroAssembler-ppc64.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include <cmath>

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#include "jit/MacroAssembler.h"
#include "jit/MoveEmitter.h"
#include "jit/SharedICRegisters.h"

#include "vm/JitActivation.h"
#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace jit;

using mozilla::Abs;
using mozilla::CheckedInt;

#if DEBUG
#define spew(...) JitSpew(JitSpew_Codegen, __VA_ARGS__)
#else
#define spew(...)
#endif

#if DEBUG

/* Useful class to print visual guard blocks. */
class MASMAutoDeBlock
{
    private:
        const char *blockname;

    public:
        MASMAutoDeBlock(const char *name, int line) {
            blockname = name;
            JitSpew(JitSpew_Codegen, "[[ CGPPC line %d: %s", line, blockname);
        }

        ~MASMAutoDeBlock() {
            JitSpew(JitSpew_Codegen, "   CGPPC: %s ]]", blockname);
        }
};
#undef ADBlock
#define ADBlock()  MASMAutoDeBlock _adbx(__PRETTY_FUNCTION__, __LINE__)
#else

/* Useful macro to completely elide visual guard blocks. */
#define ADBlock()  ;

#endif


static_assert(sizeof(intptr_t) == 8, "Not 64-bit clean.");

void
MacroAssemblerPPC64Compat::convertBoolToInt32(Register src, Register dest)
{
    ADBlock();
    // Note that C++ bool is only 1 byte, so zero extend it to clear the
    // higher-order bits.
    ma_and(dest, src, Imm32(0xff));
}

void
MacroAssemblerPPC64Compat::convertInt32ToDouble(Register src, FloatRegister dest)
{
    ADBlock();

    moveToDouble(src, dest);
    as_fcfid(dest, dest); // easy!
}

void
MacroAssemblerPPC64Compat::convertUInt64ToDouble(Register src, FloatRegister dest)
{
    // Approximately the same as above, but using fcfidu.
    ADBlock();

    moveToDouble(src, dest);
    as_fcfidu(dest, dest);
}

void
MacroAssemblerPPC64Compat::convertInt32ToDouble(const Address& src, FloatRegister dest)
{
    ADBlock();
    MOZ_ASSERT(src.base != SecondScratchReg);

    load32(src, SecondScratchReg);
    convertInt32ToDouble(SecondScratchReg, dest);
}

void
MacroAssemblerPPC64Compat::convertInt32ToDouble(const BaseIndex& src, FloatRegister dest)
{
    ADBlock();
    MOZ_ASSERT(src.base != SecondScratchReg);

    computeScaledAddress(src, SecondScratchReg);
    load32(Address(SecondScratchReg, src.offset), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::convertUInt32ToDouble(Register src, FloatRegister dest)
{
    ADBlock();
    // Clear any tag and sign.
    as_rldicl(ScratchRegister, src, 0, 32); // "clrldi"
    asMasm().convertUInt64ToDouble(Register64(ScratchRegister), dest, InvalidReg);
}


void
MacroAssemblerPPC64Compat::convertUInt32ToFloat32(Register src, FloatRegister dest)
{
    ADBlock();
    as_rldicl(ScratchRegister, src, 0, 32); // "clrldi"
    asMasm().convertUInt64ToFloat32(Register64(ScratchRegister), dest, InvalidReg);
}

void
MacroAssemblerPPC64Compat::convertDoubleToFloat32(FloatRegister src, FloatRegister dest)
{
    ADBlock();
    as_frsp(dest, src);
}

// Checks whether a double is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void
MacroAssemblerPPC64Compat::convertDoubleToInt32(FloatRegister src, Register dest,
                                                 Label* fail, bool negativeZeroCheck)
{
    ADBlock();
    MOZ_ASSERT(src != ScratchDoubleReg);

    // Throw an failure if the integer conversion is invalid (VXCVI) or
    // inexact (FI).
    as_mtfsb0(23); // whack VXCVI; FI is not sticky
    as_fctiwz(ScratchDoubleReg, src);
    as_mcrfs(cr0, 3); // VXVC - FR - FI - FPRF[C]
    as_mcrfs(cr1, 5); // resv'd - VXSOFT - VXSQRT - VXCVI
    moveFromDouble(ScratchDoubleReg, dest);
    as_cror(0, 2, 7); // CR0[LT] = FI->CR0[EQ] | VXCVI->CR1[SO]
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    ma_bc(Assembler::LessThan, fail);

    if (negativeZeroCheck) {
        // If we need to check negative 0, then grab the original FPR
        // and look at the sign bit.
        // The MIPS version happily clobbers dest from the beginning, so
        // no worries doing this check here to save some work.
        Label done;
        MOZ_ASSERT(dest != ScratchRegister);

        // Don't bother if the result was not zero.
        as_cmpldi(dest, 0);
        ma_bc(Assembler::NotEqual, &done, ShortJump);

        // Damn, the result was zero. Treat as a 64-bit int and check sign.
        moveFromDouble(src, ScratchRegister);
        as_cmpdi(ScratchRegister, 0);
        ma_bc(Assembler::LessThan, fail);

        bind(&done);
    }
}

// Checks whether a float32 is representable as a 32-bit integer.
void
MacroAssemblerPPC64Compat::convertFloat32ToInt32(FloatRegister src, Register dest,
                                                  Label* fail, bool negativeZeroCheck)
{
    // Since 32-bit and 64-bit FPRs are the same registers, use the same
    // routine above.
    ADBlock();
    convertDoubleToInt32(src, dest, fail, negativeZeroCheck);
}

// Same, but 64-bit. XXX: consolidate with 32-bit version, minimal difference
void
MacroAssemblerPPC64Compat::convertDoubleToPtr(FloatRegister src, Register dest,
                                              Label* fail, bool negativeZeroCheck)
{
    ADBlock();
    MOZ_ASSERT(src != ScratchDoubleReg);

    // Throw an failure if the integer conversion is invalid (VXCVI) or
    // inexact (FI).
    as_mtfsb0(23); // whack VXCVI; FI is not sticky
    as_fctidz(ScratchDoubleReg, src);
    as_mcrfs(cr0, 3); // VXVC - FR - FI - FPRF[C]
    as_mcrfs(cr1, 5); // resv'd - VXSOFT - VXSQRT - VXCVI
    // Don't clear the upper word or sign extend as we do for 32-bit.
    moveFromDouble(ScratchDoubleReg, dest);
    as_cror(0, 2, 7); // CR0[LT] = FI->CR0[EQ] | VXCVI->CR1[SO]
    ma_bc(Assembler::LessThan, fail);

    if (negativeZeroCheck) {
        // If we need to check negative 0, then grab the original FPR
        // and look at the sign bit.
        // The MIPS version happily clobbers dest from the beginning, so
        // no worries doing this check here to save some work.
        Label done;
        MOZ_ASSERT(dest != ScratchRegister);

        // Don't bother if the result was not zero.
        as_cmpldi(dest, 0);
        ma_bc(Assembler::NotEqual, &done, ShortJump);

        // Damn, the result was zero. Treat as a 64-bit int and check sign.
        moveFromDouble(src, ScratchRegister);
        as_cmpdi(ScratchRegister, 0);
        ma_bc(Assembler::LessThan, fail);

        bind(&done);
    }
}

void
MacroAssemblerPPC64Compat::convertFloat32ToDouble(FloatRegister src, FloatRegister dest)
{
    ADBlock();
    // Although the ISA allows us to treat single and double FPRs mostly
    // interchangeably, the Wasm spec requires that NaNs be canonicalized,
    // essentially making them qNaNs. frsp does this in the other direction
    // but VSX conversion does no such cleanup in this direction. The most
    // straightforward workaround is to just frsp again: the value is already
    // supposed to be 32-bit, and the next 64-bit operation will upconvert.
    // Unfortunately in opt builds we don't know if we're in Wasm code or not,
    // so we have to do this everywhere (grumpf); it's not necessary in
    // non-Wasm code (i.e., can't check IsCompilingWasm()). This may also not
    // be needed on non-VSX CPUs, which is some consolation for their heavier
    // float-GPR conversions.
    as_frsp(dest, src); // even if dest == src
}

void
MacroAssemblerPPC64Compat::convertInt32ToFloat32(Register src, FloatRegister dest)
{
    ADBlock();
    moveToDouble(src, dest);
    // Enforce rounding mode 0b00 (round-to-nearest ties-to-even).
// XXX: make this into a round followed by fcfids
    as_mtfsfi(7, 0);
    as_fcfids(dest, dest);
}

void
MacroAssemblerPPC64Compat::convertInt32ToFloat32(const Address& src, FloatRegister dest)
{
    ADBlock();
MOZ_CRASH("NYI convertInt32ToFloat32");
xs_trap();
    ma_li(ScratchRegister, ImmWord(src.offset));
    as_lfiwax(dest, src.base, ScratchRegister);
    as_fcfid(dest, dest);
}

void
MacroAssemblerPPC64Compat::movq(Register rs, Register rd)
{
    ma_move(rd, rs);
}

void
MacroAssemblerPPC64::ma_li(Register dest, CodeLabel* label)
{
    BufferOffset bo = m_buffer.nextOffset();
    ma_liPatchable(dest, ImmWord(/* placeholder */ 0));
    label->patchAt()->bind(bo.getOffset());
    label->setLinkMode(CodeLabel::MoveImmediate);
}

// Generate an optimized sequence to load a 64-bit immediate.
void
MacroAssemblerPPC64::ma_li(Register dest, int64_t value)
{
#if(1)
  // Shamelessly cribbed from MIPS64 because I like its style.

  // Handle low-short only values first.
  if (-1 == (value >> 15) || 0 == (value >> 15)) {
    // Sign extension OK!
    xs_li(dest, value);
    return;
  }
  if (0 == (value >> 16)) {
    // Sign extension NOT OK!
    xs_li(dest, 0);
    as_ori(dest, dest, value);
    return;
  }

  if (-1 == (value >> 31) || 0 == (value >> 31)) {
    // Sign extension OK!
    xs_lis(dest, uint16_t(value >> 16));
  } else if (0 == (value >> 32)) {
    // Sign extension NOT OK!
    xs_lis(dest, uint16_t(value >> 16));
    as_rldicl(dest, dest, 0, 32); // "clrldi" ~== "dinsu" for mips $zero
  } else if (-1 == (value >> 47) || 0 == (value >> 47)) {
    // Sign extension OK!
    xs_lis(dest, uint16_t(value >> 32));
    if (uint16_t(value >> 16)) {
      as_ori(dest, dest, uint16_t(value >> 16));
    }
    as_rldicr(dest, dest, 16, 47); // "sld" == "dsll"
  } else if (0 == (value >> 48)) {
    // Sign extension NOT OK!
    xs_lis(dest, uint16_t(value >> 32));
    as_rldicl(dest, dest, 0, 32); // "clrldi" ~== "dinsu" for mips $zero
    if (uint16_t(value >> 16)) {
      as_ori(dest, dest, uint16_t(value >> 16));
    }
    as_rldicr(dest, dest, 16, 47); // "sld" == "dsll"
  } else {
    // Sign extension ... IRRELEVANT!
    xs_lis(dest, uint16_t(value >> 48));
    if (uint16_t(value >> 32)) {
      as_ori(dest, dest, uint16_t(value >> 32));
    }
    as_rldicr(dest, dest, 32, 31);
    if (uint16_t(value >> 16)) {
      as_oris(dest, dest, uint16_t(value >> 16));
    }
  }

  // Lowest short handled separately.
  if (uint16_t(value)) {
    as_ori(dest, dest, uint16_t(value));
  }
#else
    uint64_t bits = (uint64_t)value;
    bool loweronly = true;

    // Handle trivial 16-bit quantities.
    if (value > -32769 && value < 32768) {
        // fits in 16 low bits
        xs_li(dest, value); // mscdfr0 asserts
        return;
    }
    if ((bits & 0xffffffff8000ffff) == 0 || // sign extension!
            (bits & 0xffffffff8000ffff) == 0xffffffff80000000) {
        // fits in 16 high bits
        xs_lis(dest, value >> 16); // mscdfr0 asserts
        return;
    }

    // Emit optimized sequence based on occupied bits.
    if (bits & 0xffff000000000000) {
        // Need to set upper word and shift.
        xs_lis(dest, bits >> 48);
        if (bits & 0x0000ffff00000000) {
            as_ori(dest, dest, (bits >> 32) & 0xffff);
        }
        as_rldicr(dest, dest, 32, 31);
        loweronly = false;
    } else if (bits & 0x0000ffff00000000) {
        if (bits & 0x0000800000000000) { // sign extension!
            xs_li(dest, 0);
            as_ori(dest, dest, (bits >> 32) & 0xffff);
        } else {
            xs_li(dest, (bits >> 32) & 0xffff);
        }
        as_rldicr(dest, dest, 32, 31);
        loweronly = false;
    } else if ((bits & 0x80000000) || (bits & 0x00008000)) {
        // No upper bits were set, so we can't use addi(s) for the lower word
        // or it will improperly sign-extend.
        xs_li(dest, 0);
        loweronly = false;
    }

    // Now the lower word. Don't clobber the upper word!
    bits &= 0x00000000ffffffff;
    if (bits & 0xffff0000) {
        if (loweronly) {
            xs_lis(dest, bits >> 16);
        } else {
            as_oris(dest, dest, bits >> 16);
        }
        if (bits & 0x0000ffff) {
            as_ori(dest, dest, bits & 0xffff);
        }
    } else if (bits & 0x0000ffff) {
        if (loweronly) {
            xs_li(dest, bits & 0xffff);
        } else {
            as_ori(dest, dest, bits & 0xffff);
        }
    }
#endif
}
void
MacroAssemblerPPC64::ma_li(Register dest, ImmWord imm)
{
    ADBlock();
    ma_li(dest, (int64_t)imm.value);
}

void
MacroAssemblerPPC64::ma_li(Register dest, Imm64 imm)
{
    ADBlock();
    ma_li(dest, (int64_t)imm.value);
}

// This generates immediate loads as well, but always in the
// long form so that they can be patched.
void
MacroAssemblerPPC64::ma_liPatchable(Register dest, ImmPtr imm)
{
    ma_liPatchable(dest, ImmWord(uintptr_t(imm.value)));
}

void
MacroAssemblerPPC64::ma_liPatchable(Register dest, ImmWord imm)
{
    // 64-bit load.
    m_buffer.ensureSpace(5 * sizeof(uint32_t));
    xs_lis(dest, Imm16::Upper(Imm32(imm.value >> 32)).encode());
    as_ori(dest, dest, Imm16::Lower(Imm32(imm.value >> 32)).encode());
    as_rldicr(dest, dest, 32, 31);
    as_oris(dest, dest, Imm16::Upper(Imm32(imm.value)).encode());
    as_ori(dest, dest, Imm16::Lower(Imm32(imm.value)).encode());
}

void
MacroAssemblerPPC64::ma_dnegu(Register rd, Register rs)
{
    as_neg(rd, rs);
}

// Shifts
void
MacroAssemblerPPC64::ma_dsll(Register rd, Register rt, Imm32 shift)
{
    MOZ_ASSERT(shift.value < 64);
    as_rldicr(rd, rt, shift.value, 63-(shift.value)); // "sldi"
}

void
MacroAssemblerPPC64::ma_dsrl(Register rd, Register rt, Imm32 shift)
{
    MOZ_ASSERT(shift.value < 64);
    as_rldicl(rd, rt, 64-(shift.value), shift.value); // "srdi"
}

void
MacroAssemblerPPC64::ma_dsll(Register rd, Register rt, Register shift)
{
    as_sld(rd, rt, shift);
}

void
MacroAssemblerPPC64::ma_dsrl(Register rd, Register rt, Register shift)
{
    as_srd(rd, rt, shift);
}

void
MacroAssemblerPPC64::ma_dins(Register rt, Register rs, Imm32 pos, Imm32 size)
{
    as_rldimi(rt, rs, 64-(pos.value + size.value), pos.value); // "insrdi"
}

void
MacroAssemblerPPC64::ma_dext(Register rt, Register rs, Imm32 pos, Imm32 size)
{
    // MIPS dext is right-justified, so use rldicl to simulate.
MOZ_CRASH("ma_dext whyyyy");
xs_trap(); // not sure if trap
    as_rldicl(rt, rs, (pos.value + size.value), 64 - (size.value)); // "extrdi"
}

void
MacroAssemblerPPC64::ma_dctz(Register rd, Register rs)
{
    as_cnttzd(rd, rs);
}

// Arithmetic-based ops.

// Add.
void
MacroAssemblerPPC64::ma_add(Register rd, Register rs, Imm32 imm)
{
    MOZ_ASSERT(rs != ScratchRegister);
    if (Imm16::IsInSignedRange(imm.value)) {
        as_addi(rd, rs, imm.value);
    } else {
        ma_li(ScratchRegister, imm);
        as_add(rd, rs, ScratchRegister);
    }
}

void
MacroAssemblerPPC64::ma_add(Register rd, Register rs)
{
    as_add(rd, rd, rs);
}

void
MacroAssemblerPPC64::ma_add(Register rd, Imm32 imm)
{
    ma_add(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_addTestOverflow(Register rd, Register rs, Register rt, Label* overflow, bool is32)
{
    // MIPS clobbers rd, so we can too.
    ADBlock();
    MOZ_ASSERT(rs != ScratchRegister);
    MOZ_ASSERT(rt != ScratchRegister);
    // This is testing a 32-bit overflow, so we need to whack and test
    // XER[OV32].
    xs_li(ScratchRegister, 0);
    xs_mtxer(ScratchRegister);
    as_addo(rd, rs, rt);
    if (is32) {
        // Handled in assembler.
        ma_bc(Assembler::Overflow, overflow);
    } else {
        // Do it manually. (XXX: Hoist into assembler too?)
        as_mcrxrx(cr0);
        ma_bc(Assembler::LessThan, overflow); // OV bit
    }
}

void
MacroAssemblerPPC64::ma_addTestOverflow(Register rd, Register rs, Imm32 imm, Label* overflow, bool is32)
{
    // There is no addio, daddy-o, so use the regular overflow, yo.
    ADBlock();
    MOZ_ASSERT(rs != SecondScratchReg);

    ma_li(SecondScratchReg, imm);
    ma_addTestOverflow(rd, rs, SecondScratchReg, overflow, is32);
}

void
MacroAssemblerPPC64::ma_addTestOverflow(Register rd, Register rs, ImmWord imm, Label* overflow, bool is32)
{
    ADBlock();
    MOZ_ASSERT(rs != SecondScratchReg);
    // This can only come in from a 64-bit routine, so assert that.
    MOZ_ASSERT(!is32);

    ma_li(SecondScratchReg, imm);
    ma_addTestOverflow(rd, rs, SecondScratchReg, overflow, is32);
}

void
MacroAssemblerPPC64::ma_negTestOverflow(Register rd, Label* overflow)
{
    ADBlock();
    MOZ_ASSERT(rd != ScratchRegister);

    xs_li(ScratchRegister, 0);
    xs_mtxer(ScratchRegister);
    as_nego(rd, rd);
    ma_bc(Assembler::Overflow, overflow);
}

void
MacroAssemblerPPC64::ma_not(Register rd, Register ra)
{
    ADBlock();
    as_nor(rd, ra, ra); // OPPCC p.540
}

// Subtract.
// ma_* subtraction functions invert operand order for as_subf.
void
MacroAssemblerPPC64::ma_dsubu(Register rd, Register rs, Imm32 imm)
{
    MOZ_ASSERT(rs != ScratchRegister);
    if (Imm16::IsInSignedRange(-imm.value)) {
        as_addi(rd, rs, -imm.value);
    } else {
        ma_li(ScratchRegister, imm);
        as_subf(rd, ScratchRegister, rs); // T = B - A
    }
}

void
MacroAssemblerPPC64::ma_dsubu(Register rd, Register rs)
{
    as_subf(rd, rs, rd); // T = B - A
}

void
MacroAssemblerPPC64::ma_dsubu(Register rd, Imm32 imm)
{
    ma_dsubu(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_subTestOverflow(Register rd, Register rs, Register rt, Label* overflow, bool is32)
{
    // MIPS clobbers rd, so we can too.
    ADBlock();
    MOZ_ASSERT(rs != ScratchRegister);
    MOZ_ASSERT(rt != ScratchRegister);
    xs_li(ScratchRegister, 0);
    xs_mtxer(ScratchRegister);
    as_subfo(rd, rt, rs); // T = B - A
    if (is32) {
        // This is a 32-bit operation, so we need to whack and test XER[OV32].
        // Handled in assembler.
        ma_bc(Assembler::Overflow, overflow);
    } else {
        // Do it manually. (XXX: Hoist into assembler too?)
        as_mcrxrx(cr0);
        ma_bc(Assembler::LessThan, overflow); // OV bit
    }
}

void
MacroAssemblerPPC64::ma_subTestOverflow(Register rd, Register rs, Imm32 imm, Label* overflow, bool is32)
{
    ADBlock();
    if (imm.value != INT32_MIN) {
        asMasm().ma_addTestOverflow(rd, rs, Imm32(-imm.value), overflow, is32);
    } else {
        ma_li(ScratchRegister, Imm32(imm.value));
        asMasm().ma_subTestOverflow(rd, rs, ScratchRegister, overflow, is32);
    }
}

// Memory.

uint32_t
MacroAssemblerPPC64::ma_load(Register dest, Address address,
                              LoadStoreSize size, LoadStoreExtension extension)
{
    // ADBlock(); // spammy
    int16_t encodedOffset;
    uint32_t loadInst;
    Register base;
    MOZ_ASSERT(extension == ZeroExtend || extension == SignExtend);
    MOZ_ASSERT(address.base != ScratchRegister);

    // XXX: Consider spinning this off into a separate function since the
    // logic gets repeated.
    // XXX: Consider, for lwa/ld, adding any unaligned constant offset to r12
    // so we can use lwa/ld and decomplexify this code and wasmLoad/StoreImpl
    // by not having to return aberrant values to mark a split access.
    // (See also ma_store below.)
    if (!Imm16::IsInSignedRange(address.offset)) {
        MOZ_ASSERT(address.base != SecondScratchReg);
        ma_li(SecondScratchReg, Imm32(address.offset));
        as_add(SecondScratchReg, address.base, SecondScratchReg);
        base = SecondScratchReg;
        encodedOffset = 0;
    } else {
        encodedOffset = Imm16(address.offset).encode();
        base = address.base;
    }

    // It is entirely possible for the encodedOffset to trigger an
    // unaligned load (for example, this regularly occurs with the Baseline
    // Interpreter). We should see if there is potential for optimization,
    // but the operation is deliberate, and we must not assert.
    switch (size) {
      case SizeByte:
        as_lbz(dest, base, encodedOffset);
        loadInst = asMasm().size() - 4;
        if (SignExtend == extension)
            as_extsb(dest, dest);
        break;
      case SizeHalfWord:
        if (SignExtend == extension)
            as_lha(dest, base, encodedOffset);
        else
            as_lhz(dest, base, encodedOffset);
        loadInst = asMasm().size() - 4;
        break;
      case SizeWord:
        if (ZeroExtend == extension) {
            as_lwz(dest, base, encodedOffset);
            loadInst = asMasm().size() - 4;
        } else {
            // lwa only valid if the offset is word-aligned.
            if (encodedOffset & 0x03) {
                as_lwz(dest, base, encodedOffset);
                loadInst = asMasm().size() - 4;
                as_extsw(dest, dest);
            } else {
                as_lwa(dest, base, encodedOffset);
                loadInst = asMasm().size() - 4;
            }
        }
        break;
      case SizeDouble:
        // ld only valid if the offset is word-aligned.
        if (encodedOffset & 0x03) {
            // Load as two word halves. ENDIAN!
            Register t = (dest == ScratchRegister && base == SecondScratchReg) ? ThirdScratchReg : (dest == ScratchRegister) ? SecondScratchReg : ScratchRegister;
            MOZ_ASSERT(base != t);

            // There are two loads here, so mark the highest address.
            as_lwz(t, base, encodedOffset+4); // hi
            loadInst = asMasm().size() - 4;
            // Load dest last, in case dest == base (not rare).
            as_lwz(dest, base, encodedOffset); // lo
            as_rldicr(t, t, 32, 31); // shift
            as_or(dest, dest, t); // merge
            loadInst |= 1; // advise two marks required
        } else {
            as_ld(dest, base, encodedOffset);
            loadInst = asMasm().size() - 4;
        }
        break;
      default:
        MOZ_CRASH("Invalid argument for ma_load");
    }

    return loadInst;
}

// XXX: LoadStoreExtension not used
uint32_t
MacroAssemblerPPC64::ma_store(Register data, Address address, LoadStoreSize size,
                               LoadStoreExtension extension)
{
    //ADBlock(); // spammy
    int16_t encodedOffset;
    uint32_t loadInst = 0;
    Register base;
    MOZ_ASSERT(address.base != ScratchRegister);

    // XXX: as above
    // Use worst-case here in case we have to break a double store apart.
    if (!Imm16::IsInSignedRange(address.offset + 4)) {
        MOZ_ASSERT(address.base != SecondScratchReg);
        ma_li(SecondScratchReg, Imm32(address.offset));
        as_add(SecondScratchReg, address.base, SecondScratchReg);
        base = SecondScratchReg;
        encodedOffset = 0;
    } else {
        encodedOffset = Imm16(address.offset).encode();
        base = address.base;
    }

    switch (size) {
      case SizeByte:
        as_stb(data, base, encodedOffset);
        loadInst = asMasm().size() - 4;
        break;
      case SizeHalfWord:
        as_sth(data, base, encodedOffset);
        loadInst = asMasm().size() - 4;
        break;
      case SizeWord:
        as_stw(data, base, encodedOffset);
        loadInst = asMasm().size() - 4;
        break;
      case SizeDouble:
        // std only valid if the offset is word-aligned.
        if (encodedOffset & 0x03) {
            // Store as two word halves. ENDIAN!
            Register t = (data == ScratchRegister && base == SecondScratchReg) ? ThirdScratchReg : (data == ScratchRegister) ? SecondScratchReg : ScratchRegister;
            MOZ_ASSERT(base != t);
            MOZ_ASSERT(data != t);

            as_stw(data, base, encodedOffset); // lo
            loadInst = asMasm().size() - 4;
            as_rldicl(t, data, 32, 32); // "srdi"
            as_stw(t, base, encodedOffset+4); // hi
            loadInst |= 1; // advise two marks required
        } else {
            as_std(data, base, encodedOffset);
            loadInst = asMasm().size() - 4;
        }
        break;
      default:
        MOZ_CRASH("Invalid argument for ma_store");
    }

    return loadInst;
}

void
MacroAssemblerPPC64Compat::computeScaledAddress(const BaseIndex& address, Register dest)
{
    // TODO: The non-shift case can be essentially an ldx or stdx (or whatever
    // size), so we should figure out a way to signal the caller that this
    // optimization is possible.
    int32_t shift = Imm32::ShiftOf(address.scale).value;
    if (shift) {
        MOZ_ASSERT(address.base != ScratchRegister);
        ma_dsll(ScratchRegister, address.index, Imm32(shift)); // 64-bit needed
        as_add(dest, address.base, ScratchRegister);
    } else {
        as_add(dest, address.base, address.index);
    }
}

void
MacroAssemblerPPC64::ma_pop(Register r)
{
    ADBlock();
    MOZ_ASSERT(sizeof(uintptr_t) == 8);
    as_ld(r, StackPointer, 0);
    as_addi(StackPointer, StackPointer, sizeof(uintptr_t));
}

void
MacroAssemblerPPC64::ma_push(Register r)
{
    ADBlock();
    MOZ_ASSERT(sizeof(uintptr_t) == 8);
    as_stdu(r, StackPointer, (int32_t)-sizeof(intptr_t));
}

// Branches when done from within PPC-specific code.
void
MacroAssemblerPPC64::ma_bc(Condition c, Label* l, JumpKind jumpKind)
{
    // Shorthand for a branch based on CR0.
    ma_bc(cr0, c, l, jumpKind);
}

void
MacroAssemblerPPC64::ma_bc(DoubleCondition c, Label *l, JumpKind jumpKind)
{
    ma_bc(cr0, c, l, jumpKind);
}

void
MacroAssemblerPPC64::ma_bc(DoubleCondition c, FloatRegister lhs,
                           FloatRegister rhs, Label *label, JumpKind jumpKind)
{
    ADBlock();

    compareFloatingPoint(lhs, rhs, c);
    ma_bc(c, label, jumpKind);
}

template <typename T>
void
MacroAssemblerPPC64::ma_bc(CRegisterID cr, T c, Label* label, JumpKind jumpKind)
{
    ADBlock();
    // Branch on the condition bit in the specified condition register.
    // XXX: Likely bits NYI.
    spew("bc .Llabel %p @ %08x", label, currentOffset());
    if (label->bound()) {
        int64_t offset = label->offset() - m_buffer.nextOffset().getOffset();
        spew("# target offset: %08x (diff: %ld)\n", label->offset(), offset);

        if (BOffImm16::IsInSignedRange(offset))
            jumpKind = ShortJump;

        if (jumpKind == ShortJump) {
            MOZ_ASSERT(BOffImm16::IsInSignedRange(offset));
            as_bc(BOffImm16(offset).encode(), c, cr, NotLikelyB, DontLinkB);
            return;
        }

        // Generate a long branch stanza, but invert the sense so that we
        // can run a short branch, assuming the "real" branch is not taken.
        // However, overflow doesn't do reversed sense, so we do "footwork."
        m_buffer.ensureSpace(12 * sizeof(uint32_t)); // Worst case + CR ops
        if (c & ConditionOnlyXER) {
            // This is why we don't need to account for the mcrxrx in
            // Assembler-ppc64, because we generate this:
            //
            // bc cond .+8 (DO NOT ADJUST)
            // b .+32
            // long jump
            as_bc(2 * sizeof(uint32_t), c, cr, NotLikelyB, DontLinkB);
            as_b(8 * sizeof(uint32_t));
        } else {
            as_bc(8 * sizeof(uint32_t), InvertCondition(c), cr, NotLikelyB, DontLinkB);
        }
        addLongJump(nextOffset(), BufferOffset(label->offset()));
        ma_liPatchable(SecondScratchReg, ImmWord(LabelBase::INVALID_OFFSET)); // 5
        xs_mtctr(SecondScratchReg); // 6
        as_bctr(); // 7
        return;
    }

    // Generate open jump and link it to a label.
    // Second word holds a pointer to the next branch in label's chain.
    uint32_t nextInChain = label->used() ? label->offset() : LabelBase::INVALID_OFFSET;

    if (jumpKind == ShortJump) {
        // Store the condition with a dummy branch, plus the next in chain.
        // Unfortunately there is no way to make this take up less than two
        // instructions, so we end up burning a nop at link time. Make the
        // whole branch continuous in the buffer.
        m_buffer.ensureSpace(4 * sizeof(uint32_t));

        // Use a dummy short jump. This includes all the branch encoding, so
        // we just have to change the offset at link time.
        BufferOffset bo = as_bc(4, c, cr, NotLikelyB, DontLinkB);
        spew(".long %08x ; next in chain", nextInChain);
        writeInst(nextInChain);
        if (!oom())
            label->use(bo.getOffset());
        return;
    }

    // As above with a reverse-sense long stanza.
    BufferOffset bo;
    m_buffer.ensureSpace(12 * sizeof(uint32_t)); // Worst case, with CR ops
    if (c & ConditionOnlyXER) {
        // bc cond .+8 (DO NOT ADJUST: see above)
        // b .+32
        // long jump
        as_bc(2 * sizeof(uint32_t), c, cr, NotLikelyB, DontLinkB);
        as_b(8 * sizeof(uint32_t));
        bo = xs_trap_tagged(BTag); // don't try to flip sense when optimizing
    } else {
        as_bc(8 * sizeof(uint32_t), InvertCondition(c), cr, NotLikelyB, DontLinkB);
        bo = xs_trap_tagged(BCTag);
    }
    spew(".long %08x ; next in chain", nextInChain);
    // The tagged trap must be the offset, not the leading bc. See Assembler::bind and
    // Assembler::retarget for why.
    writeInst(nextInChain);
    if (!oom())
        label->use(bo.getOffset());
    // Leave space for potential long jump.
    as_nop(); // rldicr
    as_nop(); // oris
    as_nop(); // ori
    as_nop(); // mtctr
    as_nop(); // bctr
}

void
MacroAssemblerPPC64::ma_bc(Register lhs, ImmWord imm, Label* label, Condition c, JumpKind jumpKind)
{
    if (imm.value <= INT32_MAX) {
        ma_bc64(lhs, Imm32(uint32_t(imm.value)), label, c, jumpKind);
    } else {
        MOZ_ASSERT(lhs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_bc(lhs, ScratchRegister, label, c, jumpKind);
    }
}

void
MacroAssemblerPPC64::ma_bc(Register lhs, Address addr, Label* label, Condition c, JumpKind jumpKind)
{
    MOZ_ASSERT(lhs != ScratchRegister);
    ma_load(ScratchRegister, addr, SizeDouble);
    ma_bc(lhs, ScratchRegister, label, c, jumpKind);
}

void
MacroAssemblerPPC64::ma_bc(Address addr, Imm32 imm, Label* label, Condition c, JumpKind jumpKind)
{
    ma_load(SecondScratchReg, addr, SizeDouble);
    ma_bc(SecondScratchReg, imm, label, c, jumpKind);
}

void
MacroAssemblerPPC64::ma_bc(Address addr, ImmGCPtr imm, Label* label, Condition c, JumpKind jumpKind)
{
    ma_load(SecondScratchReg, addr, SizeDouble);
    ma_bc(SecondScratchReg, imm, label, c, jumpKind);
}

void
MacroAssemblerPPC64::ma_bal(Label* label) // The whole world has gone MIPS, I tell ya.
{
    ADBlock();

    // Branch to a subroutine.
    spew("bl .Llabel %p", label);
    if (label->bound()) {
        // An entire 7-instruction stanza must be generated so that no matter how this
        // is patched, the return address is the same (i.e., the instruction after the
        // stanza). If this is a short branch, then it's 6 nops with the bl at the end.
        BufferOffset b(label->offset());
        m_buffer.ensureSpace(7 * sizeof(uint32_t));
        BufferOffset dest = nextOffset();
        int64_t offset = label->offset() - (dest.getOffset() + 6*sizeof(uint32_t));
        if (JOffImm26::IsInRange(offset)) {
            JOffImm26 j(offset);

            as_nop();
            as_nop();
            as_nop();
            as_nop(); // Yawn.
            as_nop();
            as_nop(); // Sigh.
            as_b(j, RelativeBranch, LinkB);
            return;
        }

        // Although this is to Ion code, use r12 to keep calls "as expected."
        addLongJump(dest, b);
        ma_liPatchable(SecondScratchReg, ImmWord(LabelBase::INVALID_OFFSET));
        xs_mtctr(SecondScratchReg);
        as_bctr(LinkB); // bctrl
        return;
    }

    // Second word holds a pointer to the next branch in label's chain.
    uint32_t nextInChain = label->used() ? label->offset() : LabelBase::INVALID_OFFSET;
    // Keep the whole branch stanza continuous in the buffer.
    m_buffer.ensureSpace(7 * sizeof(uint32_t));
    // Insert a tagged trap so the patcher knows what this is supposed to be.
    BufferOffset bo = xs_trap_tagged(CallTag);
    writeInst(nextInChain);
    if (!oom())
        label->use(bo.getOffset());
    // Leave space for long jump.
    as_nop(); // rldicr
    as_nop(); // oris
    as_nop(); // ori
    as_nop(); // mtctr
    as_nop(); // bctrl
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Register rs, ImmWord imm, Condition c, bool useCmpw)
{
    ADBlock();
    if (imm.value <= INT16_MAX) { // unsigned
        ma_cmp_set(rd, rs, Imm16(imm.value), c, useCmpw);
    } else if (imm.value <= INT32_MAX) { // unsigned
        ma_cmp_set(rd, rs, Imm32(imm.value), c, useCmpw);
    } else {
        MOZ_ASSERT(rd != ScratchRegister);
        MOZ_ASSERT(rs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_cmp_set(rd, rs, ScratchRegister, c, useCmpw);
    }
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Register rs, ImmPtr imm, Condition c, bool useCmpw)
{
    ma_cmp_set(rd, rs, ImmWord(uintptr_t(imm.value)), c, useCmpw);
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Address addr, Register rs, Condition c, bool useCmpw)
{
    ADBlock();
    MOZ_ASSERT(rd != ScratchRegister);
    MOZ_ASSERT(rs != ScratchRegister);

    asMasm().loadPtr(addr, ScratchRegister);
    ma_cmp_set(rd, ScratchRegister, rs, c, useCmpw);
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Address addr, Imm32 imm, Condition c, bool useCmpw)
{
    ADBlock();
    MOZ_ASSERT(rd != ScratchRegister);
    MOZ_ASSERT(rd != SecondScratchReg);

    asMasm().loadPtr(addr, ScratchRegister);
    ma_li(SecondScratchReg, imm);
    ma_cmp_set(rd, ScratchRegister, SecondScratchReg, c, useCmpw);
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Address addr, Imm64 imm, Condition c, bool useCmpw)
{
    ADBlock();
    MOZ_ASSERT(rd != ScratchRegister);
    MOZ_ASSERT(rd != SecondScratchReg);
    MOZ_ASSERT(!useCmpw);

    asMasm().loadPtr(addr, ScratchRegister);
    ma_li(SecondScratchReg, imm);
    ma_cmp_set(rd, ScratchRegister, SecondScratchReg, c, useCmpw);
}

// fp instructions
void
MacroAssemblerPPC64::ma_lid(FloatRegister dest, double value)
{
    ADBlock();
    ImmWord imm(mozilla::BitwiseCast<uint64_t>(value));

    ma_li(ScratchRegister, imm);
    asMasm().moveToDouble(ScratchRegister, dest);
}

void
MacroAssemblerPPC64::ma_ls(FloatRegister ft, Address address)
{
    if (Imm16::IsInSignedRange(address.offset)) {
        as_lfs(ft, address.base, address.offset);
    } else {
        MOZ_ASSERT(address.base != ScratchRegister);
        ma_li(ScratchRegister, Imm32(address.offset));
        as_lfsx(ft, address.base, ScratchRegister);
    }
}

void
MacroAssemblerPPC64::ma_ld(FloatRegister ft, Address address)
{
    if (Imm16::IsInSignedRange(address.offset)) {
        as_lfd(ft, address.base, address.offset);
    } else {
        MOZ_ASSERT(address.base != ScratchRegister);
        ma_li(ScratchRegister, Imm32(address.offset));
        as_lfdx(ft, address.base, ScratchRegister);
    }
}

void
MacroAssemblerPPC64::ma_sd(FloatRegister ft, Address address)
{
    if (Imm16::IsInSignedRange(address.offset)) {
        as_stfd(ft, address.base, address.offset);
    } else {
        MOZ_ASSERT(address.base != ScratchRegister);
        ma_li(ScratchRegister, Imm32(address.offset));
        as_stfdx(ft, address.base, ScratchRegister);
    }
}

void
MacroAssemblerPPC64::ma_ss(FloatRegister ft, Address address)
{
    if (Imm16::IsInSignedRange(address.offset)) {
        as_stfs(ft, address.base, address.offset);
    } else {
        MOZ_ASSERT(address.base != ScratchRegister);
        ma_li(ScratchRegister, Imm32(address.offset));
        as_stfsx(ft, address.base, ScratchRegister);
    }
}

// Keep pushes and pops of floats and doubles aligned to float.
void
MacroAssemblerPPC64::ma_pop(FloatRegister f)
{
    if (f.isSingle()) {
        as_lfs(f, StackPointer, 0);
    } else {
        as_lfd(f, StackPointer, 0);
    }
    as_addi(StackPointer, StackPointer, sizeof(double));
}

void
MacroAssemblerPPC64::ma_push(FloatRegister f)
{
    if (f.isSingle())
        as_stfsu(f, StackPointer, (int32_t)-sizeof(double));
    else
        as_stfdu(f, StackPointer, (int32_t)-sizeof(double));
}

bool
MacroAssemblerPPC64Compat::buildOOLFakeExitFrame(void* fakeReturnAddr)
{
    uint32_t descriptor = MakeFrameDescriptor(asMasm().framePushed(), FrameType::IonJS,
                                              ExitFrameLayout::Size());

    asMasm().Push(Imm32(descriptor)); // descriptor_
    asMasm().Push(ImmPtr(fakeReturnAddr));

    return true;
}

void
MacroAssemblerPPC64Compat::move32(Imm32 imm, Register dest)
{
    ADBlock();
    //uint64_t bits = (uint64_t)((int64_t)imm.value & 0x00000000ffffffff);
    //ma_li(dest, bits);
// XXX: why not just cast to int64_t now that we're fixed?
    ma_li(dest, imm);
}

void
MacroAssemblerPPC64Compat::move32(Register src, Register dest)
{
    ADBlock();
    ma_move(dest, src);
    // Ass-U-Me we explicitly want the upper 32-bits cleared.
    //as_rldicl(dest, src, 0, 32); // "clrldi"
}

void
MacroAssemblerPPC64Compat::movePtr(Register src, Register dest)
{
    ma_move(dest, src);
}
void
MacroAssemblerPPC64Compat::movePtr(ImmWord imm, Register dest)
{
    ma_li(dest, imm);
}

void
MacroAssemblerPPC64Compat::movePtr(ImmGCPtr imm, Register dest)
{
    ma_li(dest, imm); // tags the pointer for us
}

void
MacroAssemblerPPC64Compat::movePtr(ImmPtr imm, Register dest)
{
    movePtr(ImmWord(uintptr_t(imm.value)), dest);
}
void
MacroAssemblerPPC64Compat::movePtr(wasm::SymbolicAddress imm, Register dest)
{
    append(wasm::SymbolicAccess(CodeOffset(nextOffset().getOffset()), imm));
    ma_liPatchable(dest, ImmWord(-1));
}

CodeOffset MacroAssembler::moveNearAddressWithPatch(Register dest)
{
    return movWithPatch(ImmPtr(nullptr), dest);
}

void
MacroAssembler::patchNearAddressMove(CodeLocationLabel loc,
                                     CodeLocationLabel target)
{
    PatchDataWithValueCheck(loc, ImmPtr(target.raw()), ImmPtr(nullptr));
}

void
MacroAssemblerPPC64Compat::load8ZeroExtend(const Address& address, Register dest)
{
    ma_load(dest, address, SizeByte, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load8ZeroExtend(const BaseIndex& src, Register dest)
{
    ma_load(dest, src, SizeByte, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load8SignExtend(const Address& address, Register dest)
{
    ma_load(dest, address, SizeByte, SignExtend);
}

void
MacroAssemblerPPC64Compat::load8SignExtend(const BaseIndex& src, Register dest)
{
    ma_load(dest, src, SizeByte, SignExtend);
}

void
MacroAssemblerPPC64Compat::load16ZeroExtend(const Address& address, Register dest)
{
    ma_load(dest, address, SizeHalfWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load16ZeroExtend(const BaseIndex& src, Register dest)
{
    ma_load(dest, src, SizeHalfWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load16SignExtend(const Address& address, Register dest)
{
    ma_load(dest, address, SizeHalfWord, SignExtend);
}

void
MacroAssemblerPPC64Compat::load16SignExtend(const BaseIndex& src, Register dest)
{
    ma_load(dest, src, SizeHalfWord, SignExtend);
}

void
MacroAssemblerPPC64Compat::load32(const Address& address, Register dest)
{
    // This must sign-extend for arithmetic to function correctly.
    ma_load(dest, address, SizeWord);
}

void
MacroAssemblerPPC64Compat::load32(const BaseIndex& address, Register dest)
{
    // This must sign-extend for arithmetic to function correctly.
    ma_load(dest, address, SizeWord);
}

// Zero-extend versions, mostly for wasm.
void
MacroAssemblerPPC64Compat::load32ZeroExtend(const Address& address, Register dest)
{
    // This must sign-extend for arithmetic to function correctly.
    ma_load(dest, address, SizeWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load32ZeroExtend(const BaseIndex& address, Register dest)
{
    // This must sign-extend for arithmetic to function correctly.
    ma_load(dest, address, SizeWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::load32(AbsoluteAddress address, Register dest)
{
    movePtr(ImmPtr(address.addr), SecondScratchReg);
    load32(Address(SecondScratchReg, 0), dest);
}

void
MacroAssemblerPPC64Compat::load32(wasm::SymbolicAddress address, Register dest)
{
    movePtr(address, SecondScratchReg);
    load32(Address(SecondScratchReg, 0), dest);
}

void
MacroAssemblerPPC64Compat::loadPtr(Register src, Register dest)
{
    if (src != dest)
        xs_mr(dest, src);
}

void
MacroAssemblerPPC64Compat::loadPtr(const Address& address, Register dest)
{
    ma_load(dest, address, SizeDouble);
}

void
MacroAssemblerPPC64Compat::loadPtr(const BaseIndex& src, Register dest)
{
    ma_load(dest, src, SizeDouble);
}

void
MacroAssemblerPPC64Compat::loadPtr(AbsoluteAddress address, Register dest)
{
    movePtr(ImmPtr(address.addr), SecondScratchReg);
    loadPtr(Address(SecondScratchReg, 0), dest);
}

void
MacroAssemblerPPC64Compat::loadPtr(wasm::SymbolicAddress address, Register dest)
{
    movePtr(address, SecondScratchReg);
    loadPtr(Address(SecondScratchReg, 0), dest);
}

void
MacroAssemblerPPC64Compat::loadPrivate(const Address& address, Register dest)
{
    loadPtr(address, dest);
}

void
MacroAssemblerPPC64Compat::loadUnalignedDouble(const wasm::MemoryAccessDesc& access,
                                                const BaseIndex& src, Register temp, FloatRegister dest)
{
    loadDouble(src, dest);
    asMasm().append(access, asMasm().size() - 4); // lfd terminates
}

void
MacroAssemblerPPC64Compat::loadUnalignedFloat32(const wasm::MemoryAccessDesc& access,
                                                 const BaseIndex& src, Register temp, FloatRegister dest)
{
    loadFloat32(src, dest);
    asMasm().append(access, asMasm().size() - 4); // lfs terminates
}

void
MacroAssemblerPPC64Compat::store8(Imm32 imm, const Address& address)
{
    MOZ_ASSERT(address.base != ScratchRegister);
    ma_li(ScratchRegister, imm);
    ma_store(ScratchRegister, address, SizeByte);
}

void
MacroAssemblerPPC64Compat::store8(Register src, const Address& address)
{
    ma_store(src, address, SizeByte);
}

void
MacroAssemblerPPC64Compat::store8(Imm32 imm, const BaseIndex& dest)
{
    ma_store(imm, dest, SizeByte);
}

void
MacroAssemblerPPC64Compat::store8(Register src, const BaseIndex& dest)
{
    ma_store(src, dest, SizeByte);
}

void
MacroAssemblerPPC64Compat::store16(Imm32 imm, const Address& address)
{
    MOZ_ASSERT(address.base != ScratchRegister);
    ma_li(ScratchRegister, imm);
    ma_store(ScratchRegister, address, SizeHalfWord);
}

void
MacroAssemblerPPC64Compat::store16(Register src, const Address& address)
{
    ma_store(src, address, SizeHalfWord);
}

void
MacroAssemblerPPC64Compat::store16(Imm32 imm, const BaseIndex& dest)
{
    ma_store(imm, dest, SizeHalfWord);
}

void
MacroAssemblerPPC64Compat::store16(Register src, const BaseIndex& address)
{
    ma_store(src, address, SizeHalfWord);
}

void
MacroAssemblerPPC64Compat::store32(Register src, AbsoluteAddress address)
{
    MOZ_ASSERT(src != SecondScratchReg);
    movePtr(ImmPtr(address.addr), SecondScratchReg);
    store32(src, Address(SecondScratchReg, 0));
}

void
MacroAssemblerPPC64Compat::store32(Register src, const Address& address)
{
    ma_store(src, address, SizeWord);
}

void
MacroAssemblerPPC64Compat::store32(Imm32 src, const Address& address)
{
    MOZ_ASSERT(address.base != ScratchRegister);
    move32(src, ScratchRegister);
    ma_store(ScratchRegister, address, SizeWord);
}

void
MacroAssemblerPPC64Compat::store32(Imm32 imm, const BaseIndex& dest)
{
    ma_store(imm, dest, SizeWord);
}

void
MacroAssemblerPPC64Compat::store32(Register src, const BaseIndex& dest)
{
    ma_store(src, dest, SizeWord);
}

template <typename T>
void
MacroAssemblerPPC64Compat::storePtr(ImmWord imm, T address)
{
    ma_li(ScratchRegister, imm);
    ma_store(ScratchRegister, address, SizeDouble);
}

template void MacroAssemblerPPC64Compat::storePtr<Address>(ImmWord imm, Address address);
template void MacroAssemblerPPC64Compat::storePtr<BaseIndex>(ImmWord imm, BaseIndex address);

template <typename T>
void
MacroAssemblerPPC64Compat::storePtr(ImmPtr imm, T address)
{
    storePtr(ImmWord(uintptr_t(imm.value)), address);
}

template void MacroAssemblerPPC64Compat::storePtr<Address>(ImmPtr imm, Address address);
template void MacroAssemblerPPC64Compat::storePtr<BaseIndex>(ImmPtr imm, BaseIndex address);

template <typename T>
void
MacroAssemblerPPC64Compat::storePtr(ImmGCPtr imm, T address)
{
    movePtr(imm, ScratchRegister);
    storePtr(ScratchRegister, address);
}

template void MacroAssemblerPPC64Compat::storePtr<Address>(ImmGCPtr imm, Address address);
template void MacroAssemblerPPC64Compat::storePtr<BaseIndex>(ImmGCPtr imm, BaseIndex address);

void
MacroAssemblerPPC64Compat::storePtr(Register src, const Address& address)
{
    ma_store(src, address, SizeDouble);
}

void
MacroAssemblerPPC64Compat::storePtr(Register src, const BaseIndex& address)
{
    ma_store(src, address, SizeDouble);
}

void
MacroAssemblerPPC64Compat::storePtr(Register src, AbsoluteAddress dest)
{
    MOZ_ASSERT(src != SecondScratchReg);
    movePtr(ImmPtr(dest.addr), SecondScratchReg);
    storePtr(src, Address(SecondScratchReg, 0));
}

void
MacroAssemblerPPC64Compat::storeUnalignedFloat32(const wasm::MemoryAccessDesc& access,
                                                  FloatRegister src, Register temp, const BaseIndex& dest)
{
    computeScaledAddress(dest, SecondScratchReg);

    as_stfs(src, SecondScratchReg, 0);
    append(access, asMasm().size() - 4);
}

void
MacroAssemblerPPC64Compat::storeUnalignedDouble(const wasm::MemoryAccessDesc& access,
                                                 FloatRegister src, Register temp, const BaseIndex& dest)
{
    computeScaledAddress(dest, SecondScratchReg);

    as_stfd(src, SecondScratchReg, 0);
    append(access, asMasm().size() - 4);
}

void
MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output)
{
    ADBlock();
    MOZ_ASSERT(input != ScratchDoubleReg);

    // Default rounding mode 0b00 (round nearest)
    as_fctid(ScratchDoubleReg, input);
    moveFromDouble(ScratchDoubleReg, output);
    clampIntToUint8(output);
}

void
MacroAssemblerPPC64Compat::testNullSet(Condition cond, const ValueOperand& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_NULL), cond);
}

void
MacroAssemblerPPC64Compat::testObjectSet(Condition cond, const ValueOperand& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_OBJECT), cond);
}

void
MacroAssemblerPPC64Compat::testUndefinedSet(Condition cond, const ValueOperand& value, Register dest)
{
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    splitTag(value, SecondScratchReg);
    ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_UNDEFINED), cond);
}

void
MacroAssemblerPPC64Compat::unboxInt32(const ValueOperand& operand, Register dest)
{
    unboxInt32(operand.valueReg(), dest);
}

void
MacroAssemblerPPC64Compat::unboxInt32(Register src, Register dest)
{
    // Ensure there is no tag. This erases bits 32-63 and sign extends
    // in one single operation.
    as_srawi(dest, src, 0);
}

void
MacroAssemblerPPC64Compat::unboxInt32(const Address& src, Register dest)
{
    load32(Address(src.base, src.offset), dest);
}

void
MacroAssemblerPPC64Compat::unboxInt32(const BaseIndex& src, Register dest)
{
    computeScaledAddress(src, SecondScratchReg);
    load32(Address(SecondScratchReg, src.offset), dest);
}

void
MacroAssemblerPPC64Compat::unboxBoolean(const ValueOperand& operand, Register dest)
{
    unboxBoolean(operand.valueReg(), dest);
}

void
MacroAssemblerPPC64Compat::unboxBoolean(Register src, Register dest)
{
    // Clear upper 32 bits. srawi would also work.
    as_rldicl(dest, src, 0, 32); // "clrldi"
}

void
MacroAssemblerPPC64Compat::unboxBoolean(const Address& src, Register dest)
{
    ma_load(dest, Address(src.base, src.offset), SizeWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::unboxBoolean(const BaseIndex& src, Register dest)
{
    computeScaledAddress(src, SecondScratchReg);
    ma_load(dest, Address(SecondScratchReg, src.offset), SizeWord, ZeroExtend);
}

void
MacroAssemblerPPC64Compat::unboxDouble(const ValueOperand& operand, FloatRegister dest)
{
    moveToDouble(operand.valueReg(), dest);
}

void
MacroAssemblerPPC64Compat::unboxDouble(const Address& src, FloatRegister dest)
{
    ma_ld(dest, src);
}

void
MacroAssemblerPPC64Compat::unboxDouble(const BaseIndex& src, FloatRegister dest)
{
    ADBlock();
    MOZ_ASSERT(src.base != SecondScratchReg);

    computeScaledAddress(src, SecondScratchReg);
    ma_ld(dest, Address(SecondScratchReg, src.offset));
}

void
MacroAssemblerPPC64Compat::unboxString(const ValueOperand& operand, Register dest)
{
    unboxNonDouble(operand, dest, JSVAL_TYPE_STRING);
}

void
MacroAssemblerPPC64Compat::unboxString(Register src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_STRING);
}

void
MacroAssemblerPPC64Compat::unboxString(const Address& src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_STRING);
}

void
MacroAssemblerPPC64Compat::unboxSymbol(const ValueOperand& operand, Register dest)
{
    unboxNonDouble(operand, dest, JSVAL_TYPE_SYMBOL);
}

void
MacroAssemblerPPC64Compat::unboxSymbol(Register src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_SYMBOL);
}

void
MacroAssemblerPPC64Compat::unboxSymbol(const Address& src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_SYMBOL);
}

void
MacroAssemblerPPC64Compat::unboxObject(const ValueOperand& src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void
MacroAssemblerPPC64Compat::unboxObject(Register src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void
MacroAssemblerPPC64Compat::unboxObject(const Address& src, Register dest)
{
    unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void
MacroAssemblerPPC64Compat::unboxValue(const ValueOperand& src, AnyRegister dest, JSValueType type)
{
    if (dest.isFloat()) {
        Label notInt32, end;
        asMasm().branchTestInt32(Assembler::NotEqual, src, &notInt32);
        convertInt32ToDouble(src.valueReg(), dest.fpu());
        ma_b(&end, ShortJump);
        bind(&notInt32);
        unboxDouble(src, dest.fpu());
        bind(&end);
    } else {
        unboxNonDouble(src, dest.gpr(), type);
    }
}

void
MacroAssemblerPPC64Compat::unboxPrivate(const ValueOperand& src, Register dest)
{
    ma_dsll(dest, src.valueReg(), Imm32(1));
}

void
MacroAssemblerPPC64Compat::boxDouble(FloatRegister src, const ValueOperand& dest, FloatRegister)
{
    moveFromDouble(src, dest.valueReg());
}

void MacroAssemblerPPC64Compat::unboxBigInt(const ValueOperand& operand,
                                            Register dest) {
  unboxNonDouble(operand, dest, JSVAL_TYPE_BIGINT);
}

void MacroAssemblerPPC64Compat::unboxBigInt(Register src, Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_BIGINT);
}

void MacroAssemblerPPC64Compat::unboxBigInt(const Address& src,
                                            Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_BIGINT);
}

void
MacroAssemblerPPC64Compat::boxNonDouble(JSValueType type, Register src,
                                         const ValueOperand& dest)
{
    boxValue(type, src, dest.valueReg());
}

void
MacroAssemblerPPC64Compat::boolValueToDouble(const ValueOperand& operand, FloatRegister dest)
{
    ADBlock();
    convertBoolToInt32(operand.valueReg(), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::int32ValueToDouble(const ValueOperand& operand,
                                               FloatRegister dest)
{
    ADBlock();
    // Use the lower bits of the operand's value register.
    MOZ_ASSERT(operand.valueReg() != ScratchRegister);
    as_srawi(ScratchRegister, operand.valueReg(), 0);
    convertInt32ToDouble(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::boolValueToFloat32(const ValueOperand& operand,
                                               FloatRegister dest)
{
    ADBlock();
    convertBoolToInt32(operand.valueReg(), ScratchRegister);
    convertInt32ToFloat32(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::int32ValueToFloat32(const ValueOperand& operand,
                                                FloatRegister dest)
{
    ADBlock();
    // Use the lower bits of the operand's value register.
    MOZ_ASSERT(operand.valueReg() != ScratchRegister);
    as_srawi(ScratchRegister, operand.valueReg(), 0);
    convertInt32ToFloat32(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::loadConstantFloat32(float f, FloatRegister dest)
{
    if (f == 0.0 && !signbit(f)) zeroDouble(dest);
    else ma_lis(dest, f);
}

void
MacroAssemblerPPC64Compat::loadInt32OrDouble(const Address& src, FloatRegister dest)
{
    ADBlock();
    Label notInt32, end;

MOZ_CRASH("NYI loadInt32OrDouble");
xs_trap();
    // If it's an int, convert it to double.
    loadPtr(Address(src.base, src.offset), ScratchRegister);
    ma_dsrl(SecondScratchReg, ScratchRegister, Imm32(JSVAL_TAG_SHIFT));
    asMasm().branchTestInt32(Assembler::NotEqual, SecondScratchReg, &notInt32);
    loadPtr(Address(src.base, src.offset), SecondScratchReg);
    convertInt32ToDouble(SecondScratchReg, dest);
    ma_b(&end, ShortJump);

    // Not an int, just load as double.
    bind(&notInt32);
    ma_ld(dest, src);
    bind(&end);
}

void
MacroAssemblerPPC64Compat::loadInt32OrDouble(const BaseIndex& addr, FloatRegister dest)
{
    ADBlock();
    Label notInt32, end;

MOZ_CRASH("NYI loadInt32OrDouble BI");
xs_trap();
    // If it's an int, convert it to double.
    computeScaledAddress(addr, SecondScratchReg);
    // Since we only have one scratch, we need to stomp over it with the tag.
    loadPtr(Address(SecondScratchReg, 0), ScratchRegister);
    ma_dsrl(SecondScratchReg, ScratchRegister, Imm32(JSVAL_TAG_SHIFT));
    asMasm().branchTestInt32(Assembler::NotEqual, SecondScratchReg, &notInt32);

    computeScaledAddress(addr, SecondScratchReg);
    loadPtr(Address(SecondScratchReg, 0), SecondScratchReg);
    convertInt32ToDouble(SecondScratchReg, dest);
    ma_b(&end, ShortJump);

    // Not an int, just load as double.
    bind(&notInt32);
    // First, recompute the offset that had been stored in the scratch register
    // since the scratch register was overwritten loading in the type.
    computeScaledAddress(addr, SecondScratchReg);
    loadDouble(Address(SecondScratchReg, 0), dest);
    bind(&end);
}

void
MacroAssemblerPPC64Compat::loadConstantDouble(double dp, FloatRegister dest)
{
    if (dp == 0.0 && !signbit(dp)) zeroDouble(dest);
    else ma_lid(dest, dp);
}

Register
MacroAssemblerPPC64Compat::extractObject(const Address& address, Register scratch)
{
    loadPtr(Address(address.base, address.offset), scratch);
    as_rldicl(scratch, scratch, 0, 64-JSVAL_TAG_SHIFT); // clrldi the tag
    return scratch;
}

Register
MacroAssemblerPPC64Compat::extractTag(const Address& address, Register scratch)
{
    loadPtr(Address(address.base, address.offset), scratch);
    as_rldicl(scratch, scratch, 64-JSVAL_TAG_SHIFT, JSVAL_TAG_SHIFT); // "srdi"
    return scratch;
}

Register
MacroAssemblerPPC64Compat::extractTag(const BaseIndex& address, Register scratch)
{
    computeScaledAddress(address, scratch);
    return extractTag(Address(scratch, address.offset), scratch);
}

/////////////////////////////////////////////////////////////////
// X86/X64-common/ARM/MIPS interface.
/////////////////////////////////////////////////////////////////
void
MacroAssemblerPPC64Compat::storeValue(ValueOperand val, Operand dst)
{
    storeValue(val, Address(Register::FromCode(dst.base()), dst.disp()));
}

void
MacroAssemblerPPC64Compat::storeValue(ValueOperand val, const BaseIndex& dest)
{
    ADBlock();
    computeScaledAddress(dest, SecondScratchReg);

    int32_t offset = dest.offset;
    if (!Imm16::IsInSignedRange(offset)) {
        ma_li(ScratchRegister, Imm32(offset));
        as_add(SecondScratchReg, ScratchRegister, SecondScratchReg);
        offset = 0;
    }

    storeValue(val, Address(SecondScratchReg, offset));
}

void
MacroAssemblerPPC64Compat::storeValue(JSValueType type, Register reg, BaseIndex dest)
{
    ADBlock();
    computeScaledAddress(dest, SecondScratchReg);

    int32_t offset = dest.offset;
    if (!Imm16::IsInSignedRange(offset)) {
        ma_li(ScratchRegister, Imm32(offset));
        as_add(SecondScratchReg, ScratchRegister, SecondScratchReg);
        offset = 0;
    }

    storeValue(type, reg, Address(SecondScratchReg, offset));
}

void
MacroAssemblerPPC64Compat::storeValue(ValueOperand val, const Address& dest)
{
    storePtr(val.valueReg(), dest);
}

static_assert(JSVAL_TAG_SHIFT == 47); // a lot would break if this changed ...
void
MacroAssemblerPPC64Compat::storeValue(JSValueType type, Register reg, Address dest)
{
    ADBlock();

    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
        // This isn't elegant, but saves us some scratch register contention.
        MOZ_ASSERT(reg != ScratchRegister);
        MOZ_ASSERT(dest.base != ScratchRegister);
        MOZ_ASSERT(Imm16::IsInSignedRange(dest.offset + 4));

        store32(reg, dest);
        uint32_t utag = (uint32_t)JSVAL_TYPE_TO_TAG(type) << (JSVAL_TAG_SHIFT - 32);
        // Hardcode this as ma_li doesn't generate good code here. It will
        // never fit into a 16-bit immediate.
        xs_lis(ScratchRegister, (utag >> 16));
        as_ori(ScratchRegister, ScratchRegister, utag & 0x0000ffff);
        // ENDIAN!!!
        store32(ScratchRegister, Address(dest.base, dest.offset + 4));
    } else {
        MOZ_ASSERT(reg != SecondScratchReg);
        if (dest.base == SecondScratchReg) {
            MOZ_ASSERT(reg != ScratchRegister);

            // The destination register for boxValue can't be r0, so we need
            // the third scratch register.
            boxValue(type, reg, ThirdScratchReg);
            storePtr(ThirdScratchReg, dest);
        } else {
            boxValue(type, reg, SecondScratchReg);
            storePtr(SecondScratchReg, dest);
        }
    }
}

void
MacroAssemblerPPC64Compat::storeValue(const Value& val, Address dest)
{
    ADBlock();
    if (val.isGCThing()) {
        writeDataRelocation(val);
        movWithPatch(ImmWord(val.asRawBits()), ScratchRegister);
    } else {
        ma_li(ScratchRegister, ImmWord(val.asRawBits()));
    }
    storePtr(ScratchRegister, dest);
}

void
MacroAssemblerPPC64Compat::storeValue(const Value& val, BaseIndex dest)
{
    ADBlock();
    computeScaledAddress(dest, SecondScratchReg);

    int32_t offset = dest.offset;
    if (!Imm16::IsInSignedRange(offset)) {
        ma_li(ScratchRegister, Imm32(offset));
        as_add(SecondScratchReg, ScratchRegister, SecondScratchReg);
        offset = 0;
    }
    storeValue(val, Address(SecondScratchReg, offset));
}

void
MacroAssemblerPPC64Compat::loadValue(const BaseIndex& addr, ValueOperand val)
{
    computeScaledAddress(addr, SecondScratchReg);
    loadValue(Address(SecondScratchReg, addr.offset), val);
}

void
MacroAssemblerPPC64Compat::loadValue(Address src, ValueOperand val)
{
    loadPtr(src, val.valueReg());
}

void
MacroAssemblerPPC64Compat::boxValue(JSValueType type, Register src, Register dest) {
    MOZ_ASSERT(dest != ScratchRegister);
    if (dest != src)
      ma_move(dest, src);
    ma_li(ScratchRegister, ImmTag(JSVAL_TYPE_TO_TAG(type)));
    // It is possible we sign extended too far, and this matters to code
    // working directly with values, so clear the bits for int32 and bool.
    if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
        as_rldicl(dest, dest, 0, 32); // "clrldi"
    }
    // Shift the tag left and mask in the value, which is pretty much
    // what rldimi/rlwimi were created for.
    as_rldimi(dest, ScratchRegister, JSVAL_TAG_SHIFT, 0);
}

void
MacroAssemblerPPC64Compat::tagValue(JSValueType type, Register payload, ValueOperand dest)
{
    ADBlock();
    boxValue(type, payload, dest.valueReg());
}

void
MacroAssemblerPPC64Compat::pushValue(ValueOperand val)
{
    as_stdu(val.valueReg(), StackPointer, -8);
}

void
MacroAssemblerPPC64Compat::pushValue(const Address& addr)
{
    // Load value before allocating stack: addr.base may be SP.
    loadPtr(Address(addr.base, addr.offset), ScratchRegister);
    ma_dsubu(StackPointer, StackPointer, Imm32(sizeof(Value)));
    storePtr(ScratchRegister, Address(StackPointer, 0));
}

void
MacroAssemblerPPC64Compat::popValue(ValueOperand val)
{
    as_ld(val.valueReg(), StackPointer, 0);
    as_addi(StackPointer, StackPointer, sizeof(Value));
}

void
MacroAssemblerPPC64Compat::breakpoint()
{
    xs_trap();
}

void
MacroAssemblerPPC64Compat::ensureDouble(const ValueOperand& source, FloatRegister dest,
                                         Label* failure)
{
    Label isDouble, done;
    {
        ScratchTagScope tag(asMasm(), source);
        splitTagForTest(source, tag);
        asMasm().branchTestDouble(Assembler::Equal, tag, &isDouble);
        asMasm().branchTestInt32(Assembler::NotEqual, tag, failure);
    }

    unboxInt32(source, ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
    jump(&done);

    bind(&isDouble);
    unboxDouble(source, dest);

    bind(&done);
}

void
MacroAssemblerPPC64Compat::checkStackAlignment()
{
#ifdef DEBUG
    Label aligned;
    as_andi_rc(ScratchRegister, sp, StackAlignment - 1);
    ma_bc(ScratchRegister, ScratchRegister, &aligned, Zero, ShortJump);
    xs_trap(); /* untagged so we know it's a bug */
    bind(&aligned);
#endif
}

void
MacroAssemblerPPC64Compat::handleFailureWithHandlerTail(Label* profilerExitTail)
{
    // Reserve space for exception information.
    int size = (sizeof(ResumeFromException) + ABIStackAlignment) & ~(ABIStackAlignment - 1);
    asMasm().subPtr(Imm32(size), StackPointer);
    ma_move(r3, StackPointer); // Use r3 since it is a first function argument

    using Fn = void (*)(ResumeFromException * rfe);
    // Call the handler.
    asMasm().setupUnalignedABICall(r4);
    asMasm().passABIArg(r3);
    asMasm().callWithABI<Fn, HandleException>(MoveOp::GENERAL,
            CheckUnsafeCallWithABI::DontCheckHasExitFrame);

    Label entryFrame;
    Label catch_;
    Label finally;
    Label return_;
    Label bailout;
    Label wasm;

    // Already clobbered r3, so use it...
    load32(Address(StackPointer, offsetof(ResumeFromException, kind)), r3);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_ENTRY_FRAME),
                      &entryFrame);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_CATCH), &catch_);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_FINALLY), &finally);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_FORCED_RETURN),
                      &return_);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_BAILOUT), &bailout);
    asMasm().branch32(Assembler::Equal, r3, Imm32(ResumeFromException::RESUME_WASM), &wasm);

    xs_trap(); // Invalid kind.

    // No exception handler. Load the error value, load the new stack pointer
    // and return from the entry frame.
    bind(&entryFrame);
    asMasm().moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)), StackPointer);

    // We're going to be returning by the ion calling convention
    ma_pop(ScratchRegister);
    xs_mtlr(ScratchRegister);
    as_blr();

    // If we found a catch handler, this must be a baseline frame. Restore
    // state and jump to the catch block.
    bind(&catch_);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, target)), r3);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, framePointer)), BaselineFrameReg);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)), StackPointer);
    jump(r3);

    // If we found a finally block, this must be a baseline frame. Push
    // two values expected by JSOP_RETSUB: BooleanValue(true) and the
    // exception.
    bind(&finally);
    ValueOperand exception = ValueOperand(r4);
    loadValue(Address(sp, offsetof(ResumeFromException, exception)), exception);

    loadPtr(Address(sp, offsetof(ResumeFromException, target)), r3);
    loadPtr(Address(sp, offsetof(ResumeFromException, framePointer)), BaselineFrameReg);
    loadPtr(Address(sp, offsetof(ResumeFromException, stackPointer)), sp);

    pushValue(BooleanValue(true));
    pushValue(exception);
    jump(r3);

    // Only used in debug mode. Return BaselineFrame->returnValue() to the
    // caller.
    bind(&return_);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, framePointer)), BaselineFrameReg);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)), StackPointer);
    loadValue(Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfReturnValue()),
              JSReturnOperand);
    ma_move(StackPointer, BaselineFrameReg);
    pop(BaselineFrameReg);

    // If profiling is enabled, then update the lastProfilingFrame to refer to caller
    // frame before returning.
    {
        Label skipProfilingInstrumentation;
        // Test if profiler enabled.
        AbsoluteAddress addressOfEnabled(GetJitContext()->runtime->geckoProfiler().addressOfEnabled());
        asMasm().branch32(Assembler::Equal, addressOfEnabled, Imm32(0),
                          &skipProfilingInstrumentation);
        jump(profilerExitTail);
        bind(&skipProfilingInstrumentation);
    }

    ret();

    // If we are bailing out to baseline to handle an exception, jump to
    // the bailout tail stub.
    bind(&bailout);
    loadPtr(Address(sp, offsetof(ResumeFromException, bailoutInfo)), r5);
    ma_li(ReturnReg, Imm32(1));
    loadPtr(Address(sp, offsetof(ResumeFromException, target)), r4);
    jump(r4);

    // If we are throwing and the innermost frame was a wasm frame, reset SP and
    // FP; SP is pointing to the unwound return address to the wasm entry, so
    // we can just ret().
    bind(&wasm);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, framePointer)), FramePointer);
    loadPtr(Address(StackPointer, offsetof(ResumeFromException, stackPointer)), StackPointer);
    ret();
}

// Toggled jumps and calls consist of oris r0,r0,0 followed by a full 7-instruction stanza.
// This distinguishes it from other kinds of nops and is unusual enough to be noticed.
// The leading oris 0,0,0 gets patched to a b .+32 when disabled.
CodeOffset
MacroAssemblerPPC64Compat::toggledJump(Label* label)
{
    ADBlock();
    CodeOffset ret(nextOffset().getOffset());
    as_oris(r0, r0, 0);
    ma_b(label);
    // The b() may emit a varying number of instructions, so add enough nops to pad.
    while((nextOffset().getOffset() - ret.offset()) < ToggledCallSize(nullptr)) {
        as_nop();
    }
    return ret;
}

CodeOffset
MacroAssemblerPPC64Compat::toggledCall(JitCode* target, bool enabled)
{
    ADBlock();
    BufferOffset bo = nextOffset();
    CodeOffset offset(bo.getOffset());
    if (enabled) {
        as_oris(r0, r0, 0);
    } else {
        as_b(32, RelativeBranch, DontLinkB);
    }
    addPendingJump(nextOffset(), ImmPtr(target->raw()), RelocationKind::JITCODE);
    ma_liPatchable(SecondScratchReg, ImmPtr(target->raw()));
    xs_mtctr(SecondScratchReg);
    as_bctr(LinkB);
    MOZ_ASSERT_IF(!oom(), nextOffset().getOffset() - offset.offset() == ToggledCallSize(nullptr));
    return offset;
}

void
MacroAssemblerPPC64Compat::profilerEnterFrame(Register framePtr, Register scratch)
{
    asMasm().loadJSContext(scratch);
    loadPtr(Address(scratch, offsetof(JSContext, profilingActivation_)), scratch);
    storePtr(framePtr, Address(scratch, JitActivation::offsetOfLastProfilingFrame()));
    storePtr(ImmPtr(nullptr), Address(scratch, JitActivation::offsetOfLastProfilingCallSite()));
}

void
MacroAssemblerPPC64Compat::profilerExitFrame()
{
    jump(GetJitContext()->runtime->jitRuntime()->getProfilerExitFrameTail());
}

void
MacroAssembler::subFromStackPtr(Imm32 imm32)
{
    if (imm32.value)
        asMasm().subPtr(imm32, StackPointer);
}

//{{{ check_macroassembler_style
// ===============================================================
// Stack manipulation functions.

// XXX: Check usage of this routine in Ion and see what assumes LR is a GPR. If so, then
// maybe we need to find a way to abstract away SPRs vs GPRs after all.
void
MacroAssembler::PushRegsInMask(LiveRegisterSet set)
{
    int32_t diff = PushRegsInMaskSizeInBytes(set);
    const int32_t reserved = diff;

    reserveStack(reserved);
    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ++iter) {
        diff -= sizeof(intptr_t);
        storePtr(*iter, Address(StackPointer, diff));
    }
    for (FloatRegisterBackwardIterator iter(set.fpus().reduceSetForPush()); iter.more(); ++iter) {
        diff -= sizeof(double);
        storeDouble(*iter, Address(StackPointer, diff));
    }
    MOZ_ASSERT(diff == 0);
}
size_t
MacroAssembler::PushRegsInMaskSizeInBytes(LiveRegisterSet set)
{
    return (set.gprs().size() * sizeof(intptr_t) + set.fpus().getPushSizeInBytes());
}

void
MacroAssembler::PopRegsInMaskIgnore(LiveRegisterSet set, LiveRegisterSet ignore)
{
    int32_t diff = set.gprs().size() * sizeof(intptr_t) +
        set.fpus().getPushSizeInBytes();
    const int32_t reserved = diff;

    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ++iter) {
        diff -= sizeof(intptr_t);
        if (!ignore.has(*iter))
          loadPtr(Address(StackPointer, diff), *iter);
    }
    for (FloatRegisterBackwardIterator iter(set.fpus().reduceSetForPush()); iter.more(); ++iter) {
        diff -= sizeof(double);
        if (!ignore.has(*iter))
          loadDouble(Address(StackPointer, diff), *iter);
    }
    MOZ_ASSERT(diff == 0);
    freeStack(reserved);
}

void
MacroAssembler::storeRegsInMask(LiveRegisterSet set, Address dest, Register)
{
    FloatRegisterSet fpuSet(set.fpus().reduceSetForPush());
    unsigned numFpu = fpuSet.size();
    int32_t diffF = fpuSet.getPushSizeInBytes();
    int32_t diffG = set.gprs().size() * sizeof(intptr_t);

    MOZ_ASSERT(dest.offset >= diffG + diffF);

    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ++iter) {
        diffG -= sizeof(intptr_t);
        dest.offset -= sizeof(intptr_t);
        storePtr(*iter, dest);
    }
    MOZ_ASSERT(diffG == 0);

    for (FloatRegisterBackwardIterator iter(fpuSet); iter.more(); ++iter) {
        FloatRegister reg = *iter;
        diffF -= reg.size();
        numFpu -= 1;
        dest.offset -= reg.size();
        if (reg.isDouble())
            storeDouble(reg, dest);
        else if (reg.isSingle())
            storeFloat32(reg, dest);
        else
            MOZ_CRASH("Unknown register type.");
    }
    MOZ_ASSERT(numFpu == 0);
    diffF -= diffF % sizeof(uintptr_t);
    MOZ_ASSERT(diffF == 0);
}
// ===============================================================
// ABI function calls.

void
MacroAssembler::setupUnalignedABICall(Register scratch)
{
    ADBlock();
    MOZ_ASSERT(!IsCompilingWasm(), "wasm should only use aligned ABI calls"); // XXX?? arm doesn't do this
    MOZ_ASSERT(scratch != ScratchRegister);
    MOZ_ASSERT(scratch != SecondScratchReg);

    setupNativeABICall();
    dynamicAlignment_ = true;

    // Even though this is ostensibly an ABI-compliant call, save both LR
    // and SP; Baseline ICs assume that LR isn't modified.
    xs_mflr(SecondScratchReg); // ma_and may clobber r0.
    ma_move(scratch, StackPointer);
    asMasm().subPtr(Imm32(sizeof(uintptr_t)*2), StackPointer);
    ma_and(StackPointer, StackPointer, Imm32(~(ABIStackAlignment - 1)));
    as_std(SecondScratchReg, StackPointer, sizeof(uintptr_t));
    storePtr(scratch, Address(StackPointer, 0));
}

void
MacroAssembler::callWithABIPre(uint32_t* stackAdjust, bool callFromWasm)
{
    ADBlock();
    MOZ_ASSERT(inCall_);
    uint32_t stackForCall = abiArgs_.stackBytesConsumedSoFar();

    if (dynamicAlignment_) {
        stackForCall += ComputeByteAlignment(stackForCall, ABIStackAlignment);
    } else {
        uint32_t alignmentAtPrologue = callFromWasm ? sizeof(wasm::Frame) : 0;
        stackForCall += ComputeByteAlignment(stackForCall + framePushed() + alignmentAtPrologue,
                                             ABIStackAlignment);
    }

    // The callee save area must minimally include room for the SP back chain
    // pointer plus CR and LR. For 16-byte alignment we'll just ask for 32
    // bytes. This guarantees nothing we're trying to keep on the stack will
    // get overwritten.
    stackForCall += 32;

    *stackAdjust = stackForCall;
    reserveStack(stackForCall);

    // Position all arguments.
    {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver_);
        emitter.finish();
    }

    assertStackAlignment(ABIStackAlignment);
}

void
MacroAssembler::callWithABIPost(uint32_t stackAdjust, MoveOp::Type result, bool callFromWasm)
{
    ADBlock();

    if (dynamicAlignment_) {
        // Restore LR and SP (as stored in setupUnalignedABICall).
        as_ld(ScratchRegister, StackPointer, stackAdjust+sizeof(uintptr_t));
        xs_mtlr(ScratchRegister);
        loadPtr(Address(StackPointer, stackAdjust), StackPointer);
        // Use adjustFrame instead of freeStack because we already restored SP.
        adjustFrame(-stackAdjust);
    } else {
        // LR isn't stored in this instance.
        freeStack(stackAdjust);
    }

#ifdef DEBUG
    MOZ_ASSERT(inCall_);
    inCall_ = false;
#endif
}

void
MacroAssembler::callWithABINoProfiler(Register fun, MoveOp::Type result)
{
    ADBlock();

    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    call(fun);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssembler::callWithABINoProfiler(const Address& fun, MoveOp::Type result)
{
    uint32_t stackAdjust;
    ADBlock();
    MOZ_ASSERT(fun.base != SecondScratchReg);

    // This requires a bit of fancy dancing: the address base could be one
    // of the argregs and the MoveEmitter might clobber it positioning the
    // arguments. To avoid this problem we'll load CTR early, a great
    // example of turning necessity into virtue since it's faster too.
    MOZ_ASSERT(Imm16::IsInSignedRange(fun.offset));
    loadPtr(Address(fun.base, fun.offset), SecondScratchReg); // must use r12
    xs_mtctr(SecondScratchReg);

    // It's now safe to call the MoveEmitter.
    callWithABIPre(&stackAdjust);
    as_bctr(LinkB);
    callWithABIPost(stackAdjust, result);
}

// ===============================================================
// Move

void
MacroAssembler::moveValue(const TypedOrValueRegister& src, const ValueOperand& dest)
{
    if (src.hasValue()) {
        moveValue(src.valueReg(), dest);
        return;
    }

    MIRType type = src.type();
    AnyRegister reg = src.typedReg();

    if (!IsFloatingPointType(type)) {
        boxNonDouble(ValueTypeFromMIRType(type), reg.gpr(), dest);
        return;
    }

    FloatRegister scratch = ScratchDoubleReg;
    FloatRegister freg = reg.fpu();
    if (type == MIRType::Float32) {
        convertFloat32ToDouble(freg, scratch);
        freg = scratch;
    }
    boxDouble(freg, dest, scratch);
}

void
MacroAssembler::moveValue(const ValueOperand& src, const ValueOperand& dest)
{
    if (src == dest)
        return;
    movePtr(src.valueReg(), dest.valueReg());
}

void
MacroAssembler::moveValue(const Value& src, const ValueOperand& dest)
{
    if(!src.isGCThing()) {
        ma_li(dest.valueReg(), ImmWord(src.asRawBits()));
        return;
    }

    writeDataRelocation(src);
    movWithPatch(ImmWord(src.asRawBits()), dest.valueReg());
}

// ===============================================================
// Branch functions

// assumed by unboxGCThingForGCBarrier
static_assert(JS::detail::ValueGCThingPayloadMask == 0x0000'7FFF'FFFF'FFFF);

void
MacroAssembler::branchValueIsNurseryCell(Condition cond, const Address& address, Register temp,
                                         Label* label)
{
    ADBlock();
    MOZ_ASSERT(temp != InvalidReg);
    loadValue(address, ValueOperand(temp));
    branchValueIsNurseryCell(cond, ValueOperand(temp), InvalidReg, label);
}
void
MacroAssembler::branchValueIsNurseryCell(Condition cond, ValueOperand value, Register temp,
                                         Label* label)
{
    ADBlock();
    MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    Label done;
    branchTestGCThing(Assembler::NotEqual, value,
                      cond == Assembler::Equal ? &done : label);

    if (temp != InvalidReg) {
        unboxGCThingForGCBarrier(value, temp);
        orPtr(Imm32(gc::ChunkMask), temp);
        loadPtr(Address(temp, gc::ChunkStoreBufferOffsetFromLastByte), temp);
        branchPtr(InvertCondition(cond), temp, ImmWord(0), label);
    } else {
        // Honey, Ion stole the temp register again. Get out the baseball
        // bat, would you?
        //
        // Both constants are too large to be immediates.
        unboxGCThingForGCBarrier(value, ScratchRegister);
        ma_li(SecondScratchReg, gc::ChunkMask);
        as_or(SecondScratchReg, ScratchRegister, SecondScratchReg);
        ma_li(ScratchRegister, gc::ChunkStoreBufferOffsetFromLastByte);
        as_add(SecondScratchReg, SecondScratchReg, ScratchRegister);
        as_ld(ScratchRegister, SecondScratchReg, 0);
        as_cmpdi(ScratchRegister, 0);
        ma_bc(InvertCondition(cond), label);
    }
    bind(&done);
}

void
MacroAssembler::branchTestValue(Condition cond, const ValueOperand& lhs,
                                const Value& rhs, Label* label)
{
    ADBlock();
    MOZ_ASSERT(cond == Equal || cond == NotEqual);
    ScratchRegisterScope scratch(*this);
    MOZ_ASSERT(lhs.valueReg() != scratch);
    moveValue(rhs, ValueOperand(scratch));
    ma_bc(lhs.valueReg(), scratch, label, cond);
}

// ========================================================================
// Memory access primitives.
template <typename T>
void
MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value, MIRType valueType,
                                  const T& dest, MIRType slotType)
{
    if (valueType == MIRType::Double) {
        storeDouble(value.reg().typedReg().fpu(), dest);
        return;
    }

    // For known integers and booleans, we can just store the unboxed value if
    // the slot has the same type.
    if ((valueType == MIRType::Int32 || valueType == MIRType::Boolean) && slotType == valueType) {
        if (value.constant()) {
            Value val = value.value();
            if (valueType == MIRType::Int32)
                store32(Imm32(val.toInt32()), dest);
            else
                store32(Imm32(val.toBoolean() ? 1 : 0), dest);
        } else {
            store32(value.reg().typedReg().gpr(), dest);
        }
        return;
    }

    if (value.constant())
        storeValue(value.value(), dest);
    else
        storeValue(ValueTypeFromMIRType(valueType), value.reg().typedReg().gpr(), dest);
}

template void
MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value, MIRType valueType,
                                  const Address& dest, MIRType slotType);
template void
MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value, MIRType valueType,
                                  const BaseIndex& dest, MIRType slotType);
template void
MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value, MIRType valueType,
                                  const BaseObjectElementIndex& dest, MIRType slotType);

void
MacroAssembler::PushBoxed(FloatRegister reg)
{
    MOZ_ASSERT(reg.isDouble());
    subFromStackPtr(Imm32(sizeof(double)));
    boxDouble(reg, Address(getStackPointer(), 0));
    adjustFrame(sizeof(double));
}

void
MacroAssemblerPPC64::ma_spectre_isel(Condition cond, Register lhs, Register rhs) {
    if (JitOptions.spectreIndexMasking) {
        if (cond == NotEqual) {
            as_isel(lhs, lhs, rhs, Assembler::Equal);
        } else if (cond == Equal) {
            as_isel(lhs, rhs, lhs, Assembler::Equal);
        } else if (cond == LessThan || cond == Below) {
            as_isel(lhs, rhs, lhs, Assembler::LessThan);
        } else if (cond == GreaterThanOrEqual || cond == AboveOrEqual) {
            as_isel(lhs, lhs, rhs, Assembler::LessThan);
        } else if (cond == GreaterThan || cond == Above) {
            as_isel(lhs, rhs, lhs, Assembler::GreaterThan);
        } else if (cond == LessThanOrEqual || cond == BelowOrEqual) {
            as_isel(lhs, lhs, rhs, Assembler::GreaterThan);
        } else {
            MOZ_CRASH("unhandled condition");
        }
    }
}

void
MacroAssembler::wasmBoundsCheck32(Condition cond, Register index,
                                  Register boundsCheckLimit, Label* label)
{
    ADBlock();
    // We can safely clean the upper word out because this must be 32-bit.
    as_rldicl(index, index, 0, 32); // "clrldi"
    ma_bc32(index, boundsCheckLimit, label, cond);
    ma_spectre_isel(cond, index, boundsCheckLimit);
}

void
MacroAssembler::wasmBoundsCheck32(Condition cond, Register index,
                                  Address boundsCheckLimit, Label* label)
{
    ADBlock();
    SecondScratchRegisterScope scratch2(*this);
    // We can safely clean the upper word out because this must be 32-bit.
    as_rldicl(index, index, 0, 32); // "clrldi"
    load32ZeroExtend(boundsCheckLimit, SecondScratchReg);
    ma_bc32(index, SecondScratchReg, label, cond);
    ma_spectre_isel(cond, index, SecondScratchReg);
}

void MacroAssembler::wasmBoundsCheck64(Condition cond, Register64 index,
                                       Register64 boundsCheckLimit,
                                       Label* label) {
    ADBlock();
    branchPtr(cond, index.reg, boundsCheckLimit.reg, label);
    ma_spectre_isel(cond, index.reg, boundsCheckLimit.reg);
}

void MacroAssembler::wasmBoundsCheck64(Condition cond, Register64 index,
                                       Address boundsCheckLimit, Label* label) {
    ADBlock();
    MOZ_ASSERT(index.reg != SecondScratchReg);

    Register64 limit(SecondScratchReg);
    loadPtr(boundsCheckLimit, SecondScratchReg);
    wasmBoundsCheck64(cond, index, limit, label);
}

void MacroAssembler::widenInt32(Register r) {
    ADBlock();

    move32To64SignExtend(r, Register64(r));
}

void
MacroAssembler::wasmTruncateDoubleToUInt32(FloatRegister input, Register output, bool isSaturating,
                                           Label* oolEntry)
{
    ADBlock();

    // We only care if the conversion is invalid, not if it's inexact.
    // Negative zero is treated like positive zero.
    // Whack VXCVI.
    as_mtfsb0(23);
    as_fctiwuz(ScratchDoubleReg, input);

    if (isSaturating) {
        // There is no need to call the out-of-line routine: fctiwuz saturates
        // NaN to 0 and all other values to their extents, so we're done. We
        // ignore any bits that are set in the FPSCR.
    } else {
        // VXCVI is a failure (over/underflow, NaN, etc.)
        as_mcrfs(cr0, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR0[...SO]
        // OutOfLineTruncateCheckF32/F64ToU32 -> outOfLineWasmTruncateToInt32Check
        ma_bc(Assembler::SOBit, oolEntry);
    }

    moveFromDouble(ScratchDoubleReg, ScratchRegister);
    // Don't sign extend.
    as_rldicl(output, ScratchRegister, 0, 32); // "clrldi"
}

void
MacroAssembler::wasmTruncateFloat32ToUInt32(FloatRegister input, Register output, bool isSaturating,
                                            Label* oolEntry)
{
    wasmTruncateDoubleToUInt32(input, output, isSaturating, oolEntry);
}

void
MacroAssembler::wasmLoadI64(const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
                            Register ptrScratch, Register64 output)
{
    wasmLoadI64Impl(access, memoryBase, ptr, ptrScratch, output, InvalidReg);
}

void
MacroAssembler::wasmUnalignedLoadI64(const wasm::MemoryAccessDesc& access, Register memoryBase,
                                     Register ptr, Register ptrScratch, Register64 output,
                                     Register tmp)
{
    wasmLoadI64Impl(access, memoryBase, ptr, ptrScratch, output, tmp);
}

void
MacroAssembler::wasmStoreI64(const wasm::MemoryAccessDesc& access, Register64 value,
                             Register memoryBase, Register ptr, Register ptrScratch)
{
    wasmStoreI64Impl(access, value, memoryBase, ptr, ptrScratch, InvalidReg);
}

void
MacroAssembler::wasmUnalignedStoreI64(const wasm::MemoryAccessDesc& access, Register64 value,
                                      Register memoryBase, Register ptr, Register ptrScratch,
                                      Register tmp)
{
    wasmStoreI64Impl(access, value, memoryBase, ptr, ptrScratch, tmp);
}

void
MacroAssembler::wasmTruncateDoubleToInt64(FloatRegister input, Register64 output_,
                                          bool isSaturating, Label* oolEntry,
                                          Label* oolRejoin, FloatRegister tempDouble)
{
    ADBlock();
    MOZ_ASSERT(tempDouble.isInvalid());
    Register output = output_.reg;

    // We only care if the conversion is invalid, not if it's inexact.
    // Negative zero is treated like positive zero.
    // Whack VXCVI.
    as_mtfsb0(23);
    as_fctidz(ScratchDoubleReg, input);
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr0, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR0[...SO]
    // OutOfLineTruncateCheckF32OrF64ToI64 -> outOfLineWasmTruncateToInt64Check
    ma_bc(Assembler::SOBit, oolEntry);

    moveFromDouble(ScratchDoubleReg, output);
    bind(oolRejoin);
}

void
MacroAssembler::wasmTruncateDoubleToUInt64(FloatRegister input, Register64 output_,
                                           bool isSaturating, Label* oolEntry,
                                           Label* oolRejoin, FloatRegister tempDouble)
{
    ADBlock();
    MOZ_ASSERT(tempDouble.isInvalid());
    Register output = output_.reg;

    // We only care if the conversion is invalid, not if it's inexact.
    // Negative zero is treated like positive zero.
    // Whack VXCVI.
    as_mtfsb0(23);
    as_fctiduz(ScratchDoubleReg, input);
    if (isSaturating) {
        // There is no need to call the out-of-line routine: fctiduz saturates
        // NaN to 0 and all other values to their extents, so we're done. We
        // ignore any bits that are set in the FPSCR.
    } else {
        // VXCVI is a failure (over/underflow, NaN, etc.)
        as_mcrfs(cr0, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR0[...SO]
        // OutOfLineTruncateCheckF32OrF64ToI64 -> outOfLineWasmTruncateToInt64Check
        ma_bc(Assembler::SOBit, oolEntry);
    }

    moveFromDouble(ScratchDoubleReg, output);
    bind(oolRejoin);
}

void
MacroAssembler::wasmTruncateFloat32ToInt64(FloatRegister input, Register64 output,
                                           bool isSaturating, Label* oolEntry,
                                           Label* oolRejoin, FloatRegister tempFloat)
{
    wasmTruncateDoubleToInt64(input, output, isSaturating, oolEntry,
            oolRejoin, tempFloat);
}

void
MacroAssembler::wasmTruncateFloat32ToUInt64(FloatRegister input, Register64 output,
                                            bool isSaturating, Label* oolEntry,
                                            Label* oolRejoin, FloatRegister tempFloat)
{
    wasmTruncateDoubleToUInt64(input, output, isSaturating, oolEntry,
            oolRejoin, tempFloat);
}

void
MacroAssemblerPPC64Compat::wasmLoadI64Impl(const wasm::MemoryAccessDesc& access,
                                            Register memoryBase, Register ptr, Register ptrScratch,
                                            Register64 output, Register tmp)
{
    uint32_t offset = access.offset();
    MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
    MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

    // Maybe add the offset.
    if (offset) {
        asMasm().addPtr(Imm32(offset), ptrScratch);
        ptr = ptrScratch;
    }

    unsigned byteSize = access.byteSize();
    bool isSigned;

    switch (access.type()) {
      case Scalar::Int8:   isSigned = true; break;
      case Scalar::Uint8:  isSigned = false; break;
      case Scalar::Int16:  isSigned = true; break;
      case Scalar::Uint16: isSigned = false; break;
      case Scalar::Int32:  isSigned = true; break;
      case Scalar::Uint32: isSigned = false; break;
      case Scalar::Int64:  isSigned = true; break;
      default: MOZ_CRASH("unexpected array type");
    }

    BaseIndex address(memoryBase, ptr, TimesOne);
/*
    if (IsUnaligned(access)) {
        asMasm().ma_load(output.reg, address, static_cast<LoadStoreSize>(8 * byteSize), isSigned ? SignExtend : ZeroExtend);
        return;
    }
*/

    // threadsafe
    asMasm().memoryBarrierBefore(access.sync());
    uint32_t loadSize = asMasm().ma_load(output.reg, address, static_cast<LoadStoreSize>(8 * byteSize),
                     isSigned ? SignExtend : ZeroExtend);
    if (loadSize & 0x01) {
        // Split load emitted.
        asMasm().append(access, loadSize - 1);
        // The second load immediately follows the first load.
        asMasm().append(access, loadSize + 3);
    } else {
        asMasm().append(access, loadSize);
    }
    asMasm().memoryBarrierAfter(access.sync());
}

void
MacroAssemblerPPC64Compat::wasmStoreI64Impl(const wasm::MemoryAccessDesc& access, Register64 value,
                                             Register memoryBase, Register ptr, Register ptrScratch,
                                             Register tmp)
{
    uint32_t offset = access.offset();
    MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
    MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

    // Maybe add the offset.
    if (offset) {
        asMasm().addPtr(Imm32(offset), ptrScratch);
        ptr = ptrScratch;
    }

    unsigned byteSize = access.byteSize();
    bool isSigned;
    switch (access.type()) {
      case Scalar::Int8:   isSigned = true; break;
      case Scalar::Uint8:  isSigned = false; break;
      case Scalar::Int16:  isSigned = true; break;
      case Scalar::Uint16: isSigned = false; break;
      case Scalar::Int32:  isSigned = true; break;
      case Scalar::Uint32: isSigned = false; break;
      case Scalar::Int64:  isSigned = true; break;
      default: MOZ_CRASH("unexpected array type");
    }

    BaseIndex address(memoryBase, ptr, TimesOne);
/*
    if (IsUnaligned(access)) {
        asMasm().ma_store(value.reg, address, static_cast<LoadStoreSize>(8 * byteSize), isSigned ? SignExtend : ZeroExtend);
        return;
    }
*/

    // threadsafe
    asMasm().memoryBarrierBefore(access.sync());
    uint32_t loadSize = asMasm().ma_store(value.reg, address, static_cast<LoadStoreSize>(8 * byteSize),
                      isSigned ? SignExtend : ZeroExtend);
    if (loadSize & 0x01) {
        // Split store emitted.
        asMasm().append(access, loadSize - 1);
        // The second store is always the last instruction.
        asMasm().append(access, asMasm().size() - 4);
    } else {
        asMasm().append(access, loadSize);
    }
    asMasm().memoryBarrierAfter(access.sync());
}

template <typename T>
static void
CompareExchange64(MacroAssembler& masm, const wasm::MemoryAccessDesc* access, const Synchronization& sync, const T& mem,
                  Register64 expect, Register64 replace, Register64 output)
{
    // use Acquired Lock semantics, ISA v3.0B reference p915
    masm.computeEffectiveAddress(mem, SecondScratchReg);

    Label tryAgain;
    Label exit;

    masm.memoryBarrierBefore(sync);
    masm.bind(&tryAgain);

    if (access) masm.append(*access, masm.size());
    // 'r0' for 'ra' indicates hard 0, not GPR r0
    masm.as_ldarx(output.reg, r0, SecondScratchReg);
    masm.ma_bc(output.reg, expect.reg, &exit, Assembler::NotEqual, ShortJump);
    masm.movePtr(replace.reg, ScratchRegister);
    if (access) masm.append(*access, masm.size());
    masm.as_stdcx(ScratchRegister, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &tryAgain, ShortJump);

    masm.memoryBarrierAfter(sync);
    //masm.as_isync();
    masm.bind(&exit);
}

void
MacroAssembler::compareExchange64(const Synchronization& sync, const Address& mem,
                                  Register64 expect, Register64 replace, Register64 output)
{
    CompareExchange64(*this, nullptr, sync, mem, expect, replace, output);
}

void MacroAssembler::compareExchange64(const Synchronization& sync,
                                       const BaseIndex& mem, Register64 expect,
                                       Register64 replace, Register64 output) {
  CompareExchange64(*this, nullptr, sync, mem, expect, replace, output);
}

template <typename T>
static void
AtomicExchange64(MacroAssembler& masm, const wasm::MemoryAccessDesc* access, const Synchronization& sync, const T& mem,
                 Register64 src, Register64 output)
{
    masm.computeEffectiveAddress(mem, SecondScratchReg);

    Label tryAgain;

    masm.memoryBarrierBefore(sync);

    masm.bind(&tryAgain);

    if (access) masm.append(*access, masm.size());
    // 'r0' for 'ra' indicates hard 0, not GPR r0
    masm.as_ldarx(output.reg, r0, SecondScratchReg);
    if (access) masm.append(*access, masm.size());
    masm.as_stdcx(src.reg, r0, SecondScratchReg);
    masm.ma_bc(cr0, Assembler::NotEqual, &tryAgain, ShortJump);

    //masm.as_isync();
    masm.memoryBarrierAfter(sync);
}

void
MacroAssembler::atomicExchange64(const Synchronization& sync, const Address& mem, Register64 src,
                                 Register64 output)
{
    AtomicExchange64(*this, nullptr, sync, mem, src, output);
}

void
MacroAssembler::atomicExchange64(const Synchronization& sync, const BaseIndex& mem, Register64 src,
                                 Register64 output)
{
    AtomicExchange64(*this, nullptr, sync, mem, src, output);
}

template<typename T>
static void
AtomicFetchOp64(MacroAssembler& masm, const wasm::MemoryAccessDesc* access, const Synchronization& sync, AtomicOp op, Register64 value,
                const T& mem, Register64 temp, Register64 output)
{
    masm.computeEffectiveAddress(mem, SecondScratchReg);

    Label tryAgain;

    masm.memoryBarrierBefore(sync);

    masm.bind(&tryAgain);

    if (access) masm.append(*access, masm.size());
    // 'r0' for 'ra' indicates hard 0, not GPR r0
    masm.as_ldarx(output.reg, r0, SecondScratchReg);

    switch(op) {
      case AtomicFetchAddOp:
        masm.as_add(temp.reg, output.reg, value.reg);
        break;
      case AtomicFetchSubOp:
        masm.as_subf(temp.reg, value.reg, output.reg);
        break;
      case AtomicFetchAndOp:
        masm.as_and(temp.reg, output.reg, value.reg);
        break;
      case AtomicFetchOrOp:
        masm.as_or(temp.reg, output.reg, value.reg);
        break;
      case AtomicFetchXorOp:
        masm.as_xor(temp.reg, output.reg, value.reg);
        break;
      default:
        MOZ_CRASH();
    }

    if (access) masm.append(*access, masm.size());
    masm.as_stdcx(temp.reg, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &tryAgain, ShortJump);

    //masm.as_isync();
    masm.memoryBarrierAfter(sync);
}

void
MacroAssembler::atomicFetchOp64(const Synchronization& sync, AtomicOp op, Register64 value,
                                const Address& mem, Register64 temp, Register64 output)
{
    AtomicFetchOp64(*this, nullptr, sync, op, value, mem, temp, output);
}

void MacroAssembler::atomicFetchOp64(const Synchronization& sync, AtomicOp op,
                                     Register64 value, const BaseIndex& mem,
                                     Register64 temp, Register64 output) {
  AtomicFetchOp64(*this, nullptr, sync, op, value, mem, temp, output);
}

void MacroAssembler::atomicEffectOp64(const Synchronization& sync, AtomicOp op,
                                      Register64 value, const Address& mem,
                                      Register64 temp) {
  AtomicFetchOp64(*this, nullptr, sync, op, value, mem, temp, temp);
}

void MacroAssembler::atomicEffectOp64(const Synchronization& sync, AtomicOp op,
                                      Register64 value, const BaseIndex& mem,
                                      Register64 temp) {
  AtomicFetchOp64(*this, nullptr, sync, op, value, mem, temp, temp);
}

void
MacroAssembler::wasmCompareExchange64(const wasm::MemoryAccessDesc& access,
                                      const BaseIndex& mem,
                                      Register64 expect,
                                      Register64 replace,
                                      Register64 output) {
    CompareExchange64(*this, &access, access.sync(), mem, expect, replace, output);
}

void
MacroAssembler::wasmAtomicExchange64(const wasm::MemoryAccessDesc& access,
                                     const BaseIndex& mem,
                                     Register64 value, Register64 output)
{
    AtomicExchange64(*this, &access, access.sync(), mem, value, output);
}

void
MacroAssembler::wasmAtomicFetchOp64(const wasm::MemoryAccessDesc& access,
                                    AtomicOp op, Register64 value,
                                    const BaseIndex& mem, Register64 temp,
                                    Register64 output)
{
    AtomicFetchOp64(*this, &access, access.sync(), op, value, mem, temp, output);
}


// ========================================================================
// Convert floating point.

void
MacroAssembler::convertInt64ToDouble(Register64 src, FloatRegister dest)
{
    ADBlock();

    moveToDouble(src.reg, dest);
    as_fcfid(dest, dest);
}

void
MacroAssembler::convertIntPtrToDouble(Register src, FloatRegister dest)
{
    ADBlock();

    moveToDouble(src, dest);
    as_fcfid(dest, dest);
}

void
MacroAssembler::convertInt64ToFloat32(Register64 src, FloatRegister dest)
{
    ADBlock();

    moveToDouble(src.reg, dest);
    // Enforce rounding mode 0b00 (round-to-nearest ties-to-even).
    as_mtfsfi(7, 0);
    as_fcfids(dest, dest);
}

bool
MacroAssembler::convertUInt64ToDoubleNeedsTemp()
{
    // We're not like those other inferior pansy architectures.
    return false;
}

void
MacroAssembler::convertUInt64ToDouble(Register64 src, FloatRegister dest, Register temp)
{
    ADBlock();
    MOZ_ASSERT(temp == Register::Invalid());

    MacroAssemblerSpecific::convertUInt64ToDouble(src.reg, dest);
}

void
MacroAssembler::convertUInt64ToFloat32(Register64 src, FloatRegister dest, Register temp)
{
    ADBlock();
    MOZ_ASSERT(temp == Register::Invalid());

    moveToDouble(src.reg, dest);
    // Enforce rounding mode 0b00 (round-to-nearest ties-to-even).
    as_mtfsfi(7, 0);
    as_fcfidus(dest, dest);
}

void
MacroAssembler::copySignDouble(FloatRegister lhs, FloatRegister rhs, FloatRegister dest)
{
    // From inspection, 'rhs' is the sign and 'lhs' is the value.  Opposite of
    // what the instruction takes.
    as_fcpsgn(dest, rhs, lhs);
}

void
MacroAssembler::truncFloat32ToInt32(FloatRegister src, Register dest, Label* fail)
{
    return truncDoubleToInt32(src, dest, fail);
}

void
MacroAssembler::truncDoubleToInt32(FloatRegister src, Register dest, Label* fail)
{
    ADBlock();
    MOZ_ASSERT(dest != ScratchRegister);
    MOZ_ASSERT(src != ScratchDoubleReg);

    // We only care if the conversion is invalid, not if it's inexact.
    // However, JavaScript defines Math.trunc(-0) == -0, so we need a check.
    // Whack VXCVI.
    as_mtfsb0(23);
    as_fctiwz(ScratchDoubleReg, src);
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr1, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR1[...SO]
    moveFromDouble(ScratchDoubleReg, dest);
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    as_cmpdi(cr7, dest, 0);  // check for zero
    moveFromDouble(src, ScratchRegister);
    as_cmpdi(ScratchRegister, 0); // check sign bit of original float
    as_crand(0, 0, 30); // CR0[LT] &= CR7[EQ] (only bail if - and 0)
    as_cror(0, 0, 7); // Bond, James Bond: CR0[LT] |= CR1[SO]
    ma_bc(Assembler::LessThan, fail);
}

void
MacroAssembler::nearbyIntDouble(RoundingMode mode, FloatRegister src,
                                FloatRegister dest)
{
    ADBlock();

    switch (mode) {
        case RoundingMode::Up:
            as_frip(dest, src);
            break;
        case RoundingMode::Down:
            as_frim(dest, src);
            break;
        case RoundingMode::NearestTiesToEven:
            // This is actually IEEE nearest ties-to-even. |frin| does not
            // do this behaviour as specified by JavaScript, so we don't
            // support this, and it should never be called ... right?
            MOZ_CRASH("on a scale of one to even I just can't");
            break;
        case RoundingMode::TowardsZero:
            as_friz(dest, src);
            break;
        default:
            MOZ_CRASH("unsupported mode");
            break;
    }
}

void
MacroAssembler::nearbyIntFloat32(RoundingMode mode, FloatRegister src,
                                 FloatRegister dest)
{
    return nearbyIntDouble(mode, src, dest);
}

void
MacroAssembler::ceilFloat32ToInt32(FloatRegister src, Register dest,
                                   Label* fail)
{
    return ceilDoubleToInt32(src, dest, fail);
}

void
MacroAssembler::ceilDoubleToInt32(FloatRegister src, Register dest, Label* fail)
{
    ADBlock();
    MOZ_ASSERT(dest != ScratchRegister);
    MOZ_ASSERT(src != ScratchDoubleReg);

    // We only care if the conversion is invalid, not if it's inexact.
    // However, JavaScript defines Math.ceil(-0) == -0, so we need a check.
    // Whack VXCVI.
    as_mtfsb0(23);
    // "Pre-round" to +inf. Any NaN will get passed to fctiw.
    // (For pre-v2.02, set rounding to 0b10.)
    as_frip(ScratchDoubleReg, src);
    as_fctiw(ScratchDoubleReg, ScratchDoubleReg);
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr1, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR1[...SO]
    moveFromDouble(ScratchDoubleReg, dest);
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    as_cmpdi(cr7, dest, 0);  // check for zero
    moveFromDouble(src, ScratchRegister);
    as_cmpdi(ScratchRegister, 0); // check sign bit of original float
    as_crand(0, 0, 30); // CR0[LT] &= CR7[EQ] (only bail if - and 0)
    as_cror(0, 0, 7); // Licenced to kill: CR0[LT] |= CR1[SO]
    ma_bc(Assembler::LessThan, fail);
}

void
MacroAssembler::floorFloat32ToInt32(FloatRegister src, Register dest,
                                   Label* fail)
{
    return floorDoubleToInt32(src, dest, fail);
}

void
MacroAssembler::floorDoubleToInt32(FloatRegister src, Register dest, Label* fail)
{
    ADBlock();
    MOZ_ASSERT(dest != ScratchRegister);
    MOZ_ASSERT(src != ScratchDoubleReg);

    // We only care if the conversion is invalid, not if it's inexact.
    // However, we have to check -0 here too for the same stupid reason.
    // Whack VXCVI.
    as_mtfsb0(23);
    // "Pre-round" to -inf. Any NaN will get passed to fctiw.
    // (For pre-v2.02, set rounding to 0b11.)
    as_frim(ScratchDoubleReg, src);
    as_fctiw(ScratchDoubleReg, ScratchDoubleReg);
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr1, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR1[...SO]
    moveFromDouble(ScratchDoubleReg, dest);
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    as_cmpdi(cr7, dest, 0);  // check for zero
    moveFromDouble(src, ScratchRegister);
    as_cmpdi(ScratchRegister, 0); // check sign bit of original float
    as_crand(0, 0, 30); // CR0[LT] &= CR7[EQ] (only bail if - and 0)
    as_cror(0, 0, 7); // Nobody does it better: CR0[LT] |= CR1[SO]
    ma_bc(Assembler::LessThan, fail);
}

void
MacroAssembler::roundFloat32ToInt32(FloatRegister src, Register dest,
                                    FloatRegister temp, Label* fail)
{
    return floorDoubleToInt32(src, dest, fail);
}

void
MacroAssembler::roundDoubleToInt32(FloatRegister src, Register dest,
                                   FloatRegister temp, Label* fail)
{
    ADBlock();
    MOZ_ASSERT(dest != ScratchRegister);
    MOZ_ASSERT(src != ScratchDoubleReg);

    // We only care if the conversion is invalid, not if it's inexact.
    // And, you know, negative zero. BECAUSE THAT HAPPENS SOOO MUCH.
    // Whack VXCVI.
    as_mtfsb0(23);
    // The default b00 rounding mode is implemented as IEEE round-to-nearest
    // and ties-to-even. This means round(0.5) == 0. However, JavaScript
    // expects round(0.5) == 1. |frin| is not an exact duplicate for this
    // behaviour either. The best option is to fudge it by adding 0.5 and
    // round towards -Inf, which works in both the positive and negative cases.
    xs_lis(ScratchRegister, 0x3f00); // 0x3f000000 = 0.5
    moveToFloat32(ScratchRegister, ScratchDoubleReg);
    as_fadd(ScratchDoubleReg, ScratchDoubleReg, src); // preserves NaN
    as_mtfsfi(7, 3); // RN to -Inf
    as_fctiw(ScratchDoubleReg, ScratchDoubleReg); // truncate according to RN
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr1, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR1[...SO]
    as_mtfsfi(7, 0); // RN to default
    moveFromDouble(ScratchDoubleReg, dest);
    as_srawi(dest, dest, 0); // clear upper word and sign extend
    as_cmpdi(cr7, dest, 0);  // check for zero
    moveFromDouble(src, ScratchRegister);
    as_cmpdi(ScratchRegister, 0); // check sign bit of original float
    as_crand(0, 0, 30); // CR0[LT] &= CR7[EQ] (only bail if - and 0)
    as_cror(0, 0, 7); // Makes me feel sad for the rest: CR0[LT] |= CR1[SO]
    ma_bc(Assembler::LessThan, fail);
}

void
MacroAssembler::flexibleRemainder32(Register rhs, Register srcDest,
                                    bool isUnsigned, const LiveRegisterSet&)
{
    ADBlock();
    remainder32(rhs, srcDest, isUnsigned);
}

void
MacroAssembler::flexibleQuotient32(Register rhs, Register srcDest,
                                   bool isUnsigned,
                                   const LiveRegisterSet&)
{
    ADBlock();
    quotient32(rhs, srcDest, isUnsigned);
}

void
MacroAssembler::flexibleDivMod32(Register rhs, Register srcDest,
                                 Register remOutput, bool isUnsigned,
                                 const LiveRegisterSet&)
{
    ADBlock();

    if (HasPPCISA3()) {
        if (isUnsigned) {
            as_moduw(remOutput, srcDest, rhs);
            as_divwu(srcDest, srcDest, rhs);
        } else {
            as_modsw(remOutput, srcDest, rhs);
            as_divw(srcDest, srcDest, rhs);
        }
        return;
    }

    Register scratch = ScratchRegister;
    if (isUnsigned) {
        as_divwu(scratch, srcDest, rhs);
    } else {
        as_divw(scratch, srcDest, rhs);
    }
    // Compute remainder
    as_mullw(remOutput, srcDest, rhs);
    as_subf(remOutput, scratch, srcDest);
    xs_mr(srcDest, scratch);
}

//}}} check_macroassembler_style
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EndianUtils.h"

#include "jit/MacroAssembler.h"

using namespace js;
using namespace jit;

void
MacroAssemblerPPC64::ma_move(Register rd, Register rs)
{
    if (rd != rs)
      as_or(rd, rs, rs);
}

void
MacroAssemblerPPC64::ma_li(Register dest, ImmGCPtr ptr)
{
    writeDataRelocation(ptr);
    asMasm().ma_liPatchable(dest, ImmPtr(ptr.value));
}

void
MacroAssemblerPPC64::ma_li(Register dest, Imm32 imm)
{
  ADBlock();
  // signed
// XXX: why??? why not just call ma_li(Register dest, int64_t value)?
  if (Imm16::IsInSignedRange(imm.value)) {
    xs_li(dest, imm.value);
  } else if (Imm16::IsInUnsignedRange(imm.value)) {
    xs_li(dest, 0);
    as_ori(dest, dest, Imm16::Lower(imm).encode());
  } else if (Imm16::Lower(imm).encode() == 0) {
    xs_lis(dest, Imm16::Upper(imm).encode());
  } else {
    xs_lis(dest, Imm16::Upper(imm).encode());
    as_ori(dest, dest, Imm16::Lower(imm).encode());
  }
}

// This method generates lis and ori instruction pair that can be modified by
// UpdateLisOriValue, either during compilation (eg. Assembler::bind), or
// during execution (eg. jit::PatchJump).
void
MacroAssemblerPPC64::ma_liPatchable(Register dest, Imm32 imm)
{
    m_buffer.ensureSpace(2 * sizeof(uint32_t));
    xs_lis(dest, Imm16::Upper(imm).encode());
    as_ori(dest, dest, Imm16::Lower(imm).encode());
}

// Shifts

// Bit extract/insert
void
MacroAssemblerPPC64::ma_ext(Register rt, Register rs, uint16_t pos, uint16_t size) {
    MOZ_ASSERT(pos < 32);
    MOZ_ASSERT(pos + size < 33);

    as_rlwinm(rt, rs, 0, pos, size);
}

void
MacroAssemblerPPC64::ma_ins(Register rt, Register rs, uint16_t pos, uint16_t size) {
    MOZ_ASSERT(pos < 32);
    MOZ_ASSERT(pos + size <= 32);
    MOZ_ASSERT(size != 0);

    as_rlwimi(rt, rs, 0, pos, size);
}


// And.
void
MacroAssemblerPPC64::ma_and(Register rd, Register rs)
{
    as_and(rd, rd, rs);
}

void
MacroAssemblerPPC64::ma_and(Register rd, Imm32 imm)
{
    ma_and(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_and(Register rd, Register rs, Imm32 imm)
{
    if (Imm16::IsInUnsignedRange(imm.value)) {
        as_andi_rc(rd, rs, imm.value);
    } else {
        MOZ_ASSERT(rs != ScratchRegister);

        ma_li(ScratchRegister, imm);
        as_and(rd, rs, ScratchRegister);
    }
}

// Or.
void
MacroAssemblerPPC64::ma_or(Register rd, Register rs)
{
    as_or(rd, rd, rs);
}

void
MacroAssemblerPPC64::ma_or(Register rd, Imm32 imm)
{
    ma_or(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_or(Register rd, Register rs, Imm32 imm)
{
    if (Imm16::IsInUnsignedRange(imm.value)) {
        as_ori(rd, rs, imm.value);
    } else {
        MOZ_ASSERT(rs != ScratchRegister);

        ma_li(ScratchRegister, imm);
        as_or(rd, rs, ScratchRegister);
    }
}

// xor
void
MacroAssemblerPPC64::ma_xor(Register rd, Register rs)
{
    as_xor(rd, rd, rs);
}

void
MacroAssemblerPPC64::ma_xor(Register rd, Imm32 imm)
{
    ma_xor(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_xor(Register rd, Register rs, Imm32 imm)
{
    if (Imm16::IsInUnsignedRange(imm.value)) {
        as_xori(rd, rs, imm.value);
    } else {
        MOZ_ASSERT(rs != ScratchRegister);

        ma_li(ScratchRegister, imm);
        as_xor(rd, rs, ScratchRegister);
    }
}



// Arithmetic-based ops.

// Add.

void
MacroAssemblerPPC64::ma_addTestCarry(Condition cond, Register rd, Register rs, Register rt,
                                          Label* overflow, bool is32)
{
    ADBlock();
    MOZ_ASSERT(cond == Assembler::CarrySet || cond == Assembler::CarryClear);
    MOZ_ASSERT_IF(rd == rs, rt != rd);
    as_addc(rd, rs, rt);
    as_mcrxrx(cr0);
    if (is32) {
        // CA32 (Power ISA 3.0B spec page 120)
        ma_bc(cond == Assembler::CarrySet ? Assembler::SOBit : Assembler::NSOBit, overflow);
    } else {
        // regular CA
        ma_bc(cond == Assembler::CarrySet ? Assembler::Equal : Assembler::NotEqual, overflow);
    }
}

// XXX: add short option
void
MacroAssemblerPPC64::ma_addTestCarry(Condition cond, Register rd, Register rs, Imm32 imm,
                                          Label* overflow, bool is32)
{
    ADBlock();
    MOZ_ASSERT(cond == Assembler::CarrySet || cond == Assembler::CarryClear);
    if (!Imm16::IsInSignedRange(imm.value)) {
        MOZ_ASSERT(rs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_addTestCarry(cond, rd, rs, ScratchRegister, overflow, is32);
        return;
    }
    as_addic(rd, rs, imm.value);
    as_mcrxrx(cr0);
    if (is32) {
        // CA32
        ma_bc(cond == Assembler::CarrySet ? Assembler::SOBit : Assembler::NSOBit, overflow);
    } else {
        // regular CA
        ma_bc(cond == Assembler::CarrySet ? Assembler::Equal : Assembler::NotEqual, overflow);
    }
}

// XXX: add short option
void
MacroAssemblerPPC64::ma_addTestCarry(Condition cond, Register rd, Register rs, ImmWord imm,
                                          Label* overflow, bool is32)
{
    ADBlock();
    MOZ_ASSERT(cond == Assembler::CarrySet || cond == Assembler::CarryClear);
    // This can only enter through a 64-bit path.
    MOZ_ASSERT(!is32);
;
    if (!Imm16::IsInSignedRange(imm.value)) {
        MOZ_ASSERT(rs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_addTestCarry(cond, rd, rs, ScratchRegister, overflow, is32);
        return;
    }
    as_addic(rd, rs, imm.value);
    as_mcrxrx(cr0);
    ma_bc(cond == Assembler::CarrySet ? Assembler::Equal : Assembler::NotEqual, overflow);
}

// Subtract.
void
MacroAssemblerPPC64::ma_subu(Register rd, Register rs, Imm32 imm)
{
    if (Imm16::IsInSignedRange(-imm.value)) {
        as_addi(rd, rs, -imm.value);
    } else {
        ma_li(ScratchRegister, imm);
        as_subf(rd, ScratchRegister, rs);
    }
}

void
MacroAssemblerPPC64::ma_subu(Register rd, Imm32 imm)
{
    ma_subu(rd, rd, imm);
}

void
MacroAssemblerPPC64::ma_subu(Register rd, Register rs)
{
    as_subf(rd, rs, rd);
}

void
MacroAssemblerPPC64::ma_mul(Register rd, Register rs, Imm32 imm)
{
    ADBlock();
    MOZ_ASSERT(rs != ScratchRegister);
    ma_li(ScratchRegister, imm);
    as_mulld(rd, rs, ScratchRegister);
}

void
MacroAssemblerPPC64::ma_mul_branch_overflow(Register rd, Register rs, Register rt, Label* overflow)
{
    ADBlock();
    MOZ_ASSERT(rs != ScratchRegister);
    MOZ_ASSERT(rt != ScratchRegister);

    // This is a 32-bit operation, so we need to whack and test XER[OV32].
    xs_li(ScratchRegister, 0);
    xs_mtxer(ScratchRegister);
    as_mullwo(rd, rs, rt);
    ma_bc(Assembler::Overflow, overflow);
}

void
MacroAssemblerPPC64::ma_mul_branch_overflow(Register rd, Register rs, Imm32 imm, Label* overflow)
{
    ADBlock();
    ma_li(SecondScratchReg, imm);
    ma_mul_branch_overflow(rd, rs, SecondScratchReg, overflow);
}

// Memory.

uint32_t
MacroAssemblerPPC64::ma_load(Register dest, const BaseIndex& src,
                                  LoadStoreSize size, LoadStoreExtension extension)
{
    // TODO: See note in ::computeScaledAddress. Can we turn this into
    // a smaller instruction sequence?
    asMasm().computeScaledAddress(src, SecondScratchReg);

    // If src.offset is out of 16-bit signed range, we will hit an assert
    // doing the next ma_load() because the second scratch register is needed
    // again. In that case, hoist the add up since we can freely clobber it.
    if (!Imm16::IsInSignedRange(src.offset)) {
        ma_add(SecondScratchReg, SecondScratchReg, Imm32(src.offset));
        return ma_load(dest, Address(SecondScratchReg, 0), size, extension);
    } else {
        return asMasm().ma_load(dest, Address(SecondScratchReg, src.offset), size, extension);
    }
}

// XXX: remove
void
MacroAssemblerPPC64::ma_load_unaligned(const wasm::MemoryAccessDesc& access, Register dest, const BaseIndex& src, Register temp,
                                            LoadStoreSize size, LoadStoreExtension extension)
{
    MOZ_ASSERT(MOZ_LITTLE_ENDIAN(), "Wasm-only; wasm is disabled on big-endian.");
    MOZ_CRASH("only needed for 64-bit loads");
#if 0
    int16_t lowOffset, hiOffset;
    Register base;

    asMasm().computeScaledAddress(src, SecondScratchReg);

    if (Imm16::IsInSignedRange(src.offset) && Imm16::IsInSignedRange(src.offset + size / 8 - 1)) {
        base = SecondScratchReg;
        lowOffset = Imm16(src.offset).encode();
        hiOffset = Imm16(src.offset + size / 8 - 1).encode();
    } else {
        ma_li(ScratchRegister, Imm32(src.offset));
        asMasm().addPtr(SecondScratchReg, ScratchRegister);
        base = ScratchRegister;
        lowOffset = Imm16(0).encode();
        hiOffset = Imm16(size / 8 - 1).encode();
    }

    BufferOffset load;
    switch (size) {
      case SizeHalfWord:
        if (extension != ZeroExtend)
            load = as_lbu(temp, base, hiOffset);
        else
            load = as_lb(temp, base, hiOffset);
        as_lbu(dest, base, lowOffset);
        ma_ins(dest, temp, 8, 24);
        break;
      case SizeWord:
        load = as_lwl(dest, base, hiOffset);
        as_lwr(dest, base, lowOffset);
#ifdef JS_CODEGEN_PPC64
        if (extension != ZeroExtend)
            as_dext(dest, dest, 0, 32);
#endif
        break;
#ifdef JS_CODEGEN_PPC64
      case SizeDouble:
        load = as_ldl(dest, base, hiOffset);
        as_ldr(dest, base, lowOffset);
        break;
#endif
      default:
        MOZ_CRASH("Invalid argument for ma_load");
    }

    append(access, load.getOffset());
#endif
}

uint32_t
MacroAssemblerPPC64::ma_store(Register data, const BaseIndex& dest,
                                   LoadStoreSize size, LoadStoreExtension extension)
{
    // TODO: See note in ::computeScaledAddress. Can we turn this into
    // a smaller instruction sequence?
    MOZ_ASSERT(data != SecondScratchReg);
    asMasm().computeScaledAddress(dest, SecondScratchReg);

    // If dest.offset is out of 16-bit signed range, we will hit an assert
    // doing the next ma_store() because the second scratch register is needed
    // again. In that case, hoist the add up since we can freely clobber it.
    if (!Imm16::IsInSignedRange(dest.offset)) {
        ma_add(SecondScratchReg, SecondScratchReg, Imm32(dest.offset));
        return ma_store(data, Address(SecondScratchReg, 0), size, extension);
    } else {
        return asMasm().ma_store(data, Address(SecondScratchReg, dest.offset), size, extension);
    }
}

uint32_t
MacroAssemblerPPC64::ma_store(Imm32 imm, const BaseIndex& dest,
                                   LoadStoreSize size, LoadStoreExtension extension)
{
    // Make sure that SecondScratchReg contains absolute address so that
    // offset is 0.
    asMasm().computeEffectiveAddress(dest, SecondScratchReg);

    // Scrach register is free now, use it for loading imm value
    ma_li(ScratchRegister, imm);

    // with offset=0 ScratchRegister will not be used in ma_store()
    // so we can use it as a parameter here
    return asMasm().ma_store(ScratchRegister, Address(SecondScratchReg, 0), size, extension);
}

// XXX: remove
void
MacroAssemblerPPC64::ma_store_unaligned(Register src, const BaseIndex& dest,
                                        LoadStoreSize size)
{
    MOZ_CRASH("NYI");
}

// XXX: remove
void
MacroAssemblerPPC64::ma_store_unaligned(const wasm::MemoryAccessDesc& access, Register data,
                                             const BaseIndex& dest, Register temp,
                                             LoadStoreSize size, LoadStoreExtension extension)
{
    MOZ_ASSERT(MOZ_LITTLE_ENDIAN(), "Wasm-only; wasm is disabled on big-endian.");
    MOZ_CRASH("only needed for 64-bit loads");
#if 0
    int16_t lowOffset, hiOffset;
    Register base;

    asMasm().computeScaledAddress(dest, SecondScratchReg);

    if (Imm16::IsInSignedRange(dest.offset) && Imm16::IsInSignedRange(dest.offset + size / 8 - 1)) {
        base = SecondScratchReg;
        lowOffset = Imm16(dest.offset).encode();
        hiOffset = Imm16(dest.offset + size / 8 - 1).encode();
    } else {
        ma_li(ScratchRegister, Imm32(dest.offset));
        asMasm().addPtr(SecondScratchReg, ScratchRegister);
        base = ScratchRegister;
        lowOffset = Imm16(0).encode();
        hiOffset = Imm16(size / 8 - 1).encode();
    }

    BufferOffset store;
    switch (size) {
      case SizeHalfWord:
        ma_ext(temp, data, 8, 8);
        store = as_sb(temp, base, hiOffset);
        as_sb(data, base, lowOffset);
        break;
      case SizeWord:
        store = as_swl(data, base, hiOffset);
        as_swr(data, base, lowOffset);
        break;
#ifdef JS_CODEGEN_PPC64
      case SizeDouble:
        store = as_sdl(data, base, hiOffset);
        as_sdr(data, base, lowOffset);
        break;
#endif
      default:
        MOZ_CRASH("Invalid argument for ma_store");
    }
    append(access, store.getOffset());
#endif
}

// Branches when done from within ppc64-specific code.
void
MacroAssemblerPPC64::ma_bc(Register lhs, Register rhs, Label* label, Condition c, JumpKind jumpKind)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    if (c == Always) {
        ma_b(label, jumpKind);
    } else if (c & ConditionZero) {
        MOZ_ASSERT(lhs == rhs);
        as_cmpdi(lhs, 0);
        ma_bc(c, label, jumpKind);
    } else if (c & ConditionUnsigned) {
        as_cmpld(lhs, rhs);
        ma_bc(c, label, jumpKind);
    } else {
        MOZ_ASSERT(c < 0x100); // paranoia
        as_cmpd(lhs, rhs);
        ma_bc(c, label, jumpKind);
    }
}

// For an explicit 32-bit compare (mostly wasm).
void
MacroAssemblerPPC64::ma_bc32(Register lhs, Register rhs, Label* label, Condition c, JumpKind jumpKind)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    if (c == Always) {
        ma_b(label, jumpKind);
    } else if (c & ConditionZero) {
        MOZ_ASSERT(lhs == rhs);
        as_cmpwi(lhs, 0);
        ma_bc(c, label, jumpKind);
    } else if (c & ConditionUnsigned) {
        as_cmplw(lhs, rhs);
        ma_bc(c, label, jumpKind);
    } else {
        MOZ_ASSERT(c < 0x100); // paranoia
        as_cmpw(lhs, rhs);
        ma_bc(c, label, jumpKind);
    }
}

// For an explicit 64-bit compare.
void
MacroAssemblerPPC64::ma_bc64(Register lhs, Imm32 imm, Label* label, Condition c, JumpKind jumpKind)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    if (c == Always) {
        ma_b(label, jumpKind);
        return;
    }
    if (c & ConditionZero) {
        MOZ_ASSERT(imm.value == 0);
        as_cmpdi(lhs, 0);
        ma_bc(c, label, jumpKind);
        return;
    }
    if (c & ConditionUnsigned) {
        if (Imm16::IsInUnsignedRange(imm.value)) {
            as_cmpldi(lhs, imm.value);
        } else {
            MOZ_ASSERT(lhs != ScratchRegister);
            ma_li(ScratchRegister, imm);
            as_cmpld(lhs, ScratchRegister);
        }
    } else {
        MOZ_ASSERT(c < 0x100); // just in case
        if (Imm16::IsInSignedRange(imm.value)) {
            as_cmpdi(lhs, imm.value);
        } else {
            MOZ_ASSERT(lhs != ScratchRegister);
            ma_li(ScratchRegister, imm);
            as_cmpd(lhs, ScratchRegister);
        }
    }
    ma_bc(c, label, jumpKind);
}

// For everyone else, there's MasterCard.
void
MacroAssemblerPPC64::ma_bc(Register lhs, Imm32 imm, Label* label, Condition c, JumpKind jumpKind)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    if (c == Always) {
        ma_b(label, jumpKind);
        return;
    }
    if (c & ConditionZero) {
        MOZ_ASSERT(imm.value == 0);
        as_cmpdi(lhs, 0);
        ma_bc(c, label, jumpKind);
        return;
    }
    if (c & ConditionUnsigned) {
        if (Imm16::IsInUnsignedRange(imm.value)) {
            as_cmplwi(lhs, imm.value);
        } else {
            MOZ_ASSERT(lhs != ScratchRegister);
            ma_li(ScratchRegister, imm);
            as_cmplw(lhs, ScratchRegister);
        }
    } else {
        MOZ_ASSERT(c < 0x100); // just in case
        if (Imm16::IsInSignedRange(imm.value)) {
            as_cmpwi(lhs, imm.value);
        } else {
            MOZ_ASSERT(lhs != ScratchRegister);
            ma_li(ScratchRegister, imm);
            as_cmpw(lhs, ScratchRegister);
        }
    }
    ma_bc(c, label, jumpKind);
}

void
MacroAssemblerPPC64::ma_bc(Register lhs, ImmPtr imm, Label* l, Condition c, JumpKind jumpKind)
{
    asMasm().ma_bc(lhs, ImmWord(uintptr_t(imm.value)), l, c, jumpKind);
}

void
MacroAssemblerPPC64::ma_b(Label* label, JumpKind jumpKind)
{
    ADBlock();
    if (!label->bound()) {
        BufferOffset bo;

        // Emit an unbound branch to be bound later by |Assembler::bind|.
        // This and ma_bc() are largely the same in this respect.
        spew(".Llabel %p", label);
        uint32_t nextInChain = label->used() ? label->offset() : LabelBase::INVALID_OFFSET;
        if (jumpKind == ShortJump) {
            // We know this branch must be short. Unfortunately, because we
            // have to also store the next-in-chain, we can't make this less
            // than two instructions.
            m_buffer.ensureSpace(2 * sizeof(uint32_t));
            bo = as_b(4, RelativeBranch, DontLinkB);
            spew(".long %08x ; next in chain", nextInChain);
            writeInst(nextInChain);
            if (!oom())
                label->use(bo.getOffset());
        } else {
            m_buffer.ensureSpace(7 * sizeof(uint32_t));
            bo = xs_trap_tagged(BTag);
            spew(".long %08x ; next in chain", nextInChain);
            writeInst(nextInChain);
            if (!oom())
                label->use(bo.getOffset());
            // Leave space for potential long jump.
            as_nop(); // rldicr
            as_nop(); // oris
            as_nop(); // ori
            as_nop(); // mtctr
            as_nop(); // bctr
        }
        return;
    }

    // Label is bound, emit final code.
    int64_t offset = label->offset() - currentOffset();
    if (jumpKind == ShortJump || JOffImm26::IsInRange(offset)) {
        spew("# static short jump %08x to label %p @ %08x (offset %ld)",
            currentOffset(), label, label->offset(), offset);
        MOZ_ASSERT(JOffImm26::IsInRange(offset));
        as_b(offset);
    } else {
        // Use r12 "as expected" even though this is probably not to ABI-compliant code.
        m_buffer.ensureSpace(7 * sizeof(uint32_t));
        addLongJump(nextOffset(), BufferOffset(label->offset()));
        ma_liPatchable(SecondScratchReg, ImmWord(LabelBase::INVALID_OFFSET));
        xs_mtctr(SecondScratchReg);
        as_bctr();
    }
}

void
MacroAssemblerPPC64::ma_cmp32(Register lhs, Register rhs, Condition c)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    MOZ_ASSERT(!(c & ConditionZero));

    if (c & ConditionUnsigned) {
        as_cmplw(lhs, rhs);
    } else {
        as_cmpw(lhs, rhs);
    }
}

void
MacroAssemblerPPC64::ma_cmp32(Register lhs, Imm32 rhs, Condition c)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    MOZ_ASSERT_IF((c & ConditionZero), (rhs.value == 0));

    if (c & ConditionZero) {
        as_cmpwi(lhs, 0);
    } else {
        if (c & ConditionUnsigned) {
            if (Imm16::IsInUnsignedRange(rhs.value)) {
                as_cmplwi(lhs, rhs.value);
            } else {
                MOZ_ASSERT(lhs != ScratchRegister);
                ma_li(ScratchRegister, rhs);
                as_cmplw(lhs, ScratchRegister);
            }
        } else {
            if (Imm16::IsInSignedRange(rhs.value)) {
                as_cmpwi(lhs, rhs.value);
            } else {
                MOZ_ASSERT(lhs != ScratchRegister);
                ma_li(ScratchRegister, rhs);
                as_cmpw(lhs, ScratchRegister);
            }
        }
    }
}

void
MacroAssemblerPPC64::ma_cmp64(Register lhs, Imm64 rhs, Condition c)
{
    ADBlock();
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    MOZ_ASSERT_IF((c & ConditionZero), (rhs.value == 0));

    if (c & ConditionZero) {
        as_cmpdi(lhs, 0);
    } else {
        if (c & ConditionUnsigned) {
            if (Imm16::IsInUnsignedRange(rhs.value)) {
                as_cmpldi(lhs, rhs.value);
            } else {
                MOZ_ASSERT(lhs != ScratchRegister);
                ma_li(ScratchRegister, rhs);
                as_cmpld(lhs, ScratchRegister);
            }
        } else {
            if (Imm16::IsInSignedRange(rhs.value)) {
                as_cmpdi(lhs, rhs.value);
            } else {
                MOZ_ASSERT(lhs != ScratchRegister);
                ma_li(ScratchRegister, rhs);
                as_cmpd(lhs, ScratchRegister);
            }
        }
    }
}

void
MacroAssemblerPPC64::ma_cmp32(Register lhs, const Address& rhs, Condition c)
{
    MOZ_ASSERT(lhs != ScratchRegister);
    ma_load(ScratchRegister, rhs, SizeWord);
    ma_cmp32(lhs, ScratchRegister, c);
}

void
MacroAssemblerPPC64::compareFloatingPoint(FloatRegister lhs, FloatRegister rhs,
                                          DoubleCondition c)
{
    if ((c & DoubleConditionUnordered) || (c == DoubleUnordered)) {
        as_fcmpu(lhs, rhs);
    } else {
        as_fcmpo(lhs, rhs);
    }
}

void
MacroAssemblerPPC64::ma_cmp_set_double(Register dest, FloatRegister lhs, FloatRegister rhs,
                                            DoubleCondition c)
{
    Label skip;
    compareFloatingPoint(lhs, rhs, c);

    ma_li(dest, 1L);

// XXX: use CR
    ma_bc(c, &skip);
    ma_li(dest, 0L);
    bind(&skip);
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Register rs, Imm16 imm, Condition c, bool useCmpw)
{
    ADBlock();
// XXX: remove duplicate code, just call ma_cmp32 or ma_cmp64 as appropriate

    // Handle any synthetic codes.
    MOZ_ASSERT_IF((c & ConditionZero), (imm.encode() == 0));
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    if (c & ConditionUnsigned) {
        MOZ_ASSERT(Imm16::IsInUnsignedRange(imm.encode())); // paranoia
        if (useCmpw) {
            as_cmplwi(rs, imm.encode());
        } else {
            as_cmpldi(rs, imm.encode());
        }
    } else {
        // Just because it's an Imm16 doesn't mean it always fits.
        if (!Imm16::IsInSignedRange(imm.decodeSigned())) {
            MOZ_ASSERT(rs != ScratchRegister);
            ma_li(ScratchRegister, imm.decodeSigned());
            if (useCmpw) {
                as_cmpw(rs, ScratchRegister);
            } else {
                as_cmpd(rs, ScratchRegister);
            }
        } else {
            if (useCmpw) {
                as_cmpwi(rs, imm.decodeSigned());
            } else {
                as_cmpdi(rs, imm.decodeSigned());
            }
        }
    }
    // Common routine to extract or flip the appropriate CR bit.
    ma_cmp_set_coda(rd, c);
}

void
MacroAssemblerPPC64::ma_cmp_set(Register rd, Register rs, Register rt, Condition c, bool useCmpw)
{
    ADBlock();

    // Handle any synthetic codes.
    MOZ_ASSERT(!(c & ConditionOnlyXER));
    MOZ_ASSERT(!(c & ConditionZero));
    if (c & ConditionUnsigned) {
        // Some compares should only pay attention to the lower word.
        if (useCmpw) {
            as_cmplw(rs, rt);
        } else {
            as_cmpld(rs, rt);
        }
    } else {
        if (useCmpw) {
            as_cmpw(rs, rt);
        } else {
            as_cmpd(rs, rt);
        }
    }
    ma_cmp_set_coda(rd, c);
}

static_assert(((Assembler::LessThanOrEqual & Assembler::BranchOptionMask) == Assembler::BranchOnClear),
        "Assembler conditions don't match CR bits");
void
MacroAssemblerPPC64::ma_cmp_set_coda(Register rd, Condition c) {
    MOZ_ASSERT(!(c & ConditionOnlyXER));

    // Extract the underlying CR field bit.
    as_mfcr(rd);
    switch(c & 0xff) {
        case Equal:
        case NotEqual:
            as_rlwinm(rd, rd, 3, 31, 31); // PowerPC CWG page 38
            break;
        case GreaterThan:
        case LessThanOrEqual:
            as_rlwinm(rd, rd, 2, 31, 31);
            break;
        case LessThan:
        case GreaterThanOrEqual:
            as_rlwinm(rd, rd, 1, 31, 31); // p40
            break;
        default:
            MOZ_CRASH("Unhandled condition");
            break;
    }

    // Negate the boolean if necessary.
    if ((c & BranchOptionMask) == BranchOnClear) {
        as_xori(rd, rd, 1);
    }
}

// fp instructions
void
MacroAssemblerPPC64::ma_lis(FloatRegister dest, float value)
{
    Imm32 imm(mozilla::BitwiseCast<uint32_t>(value));

    ma_li(ScratchRegister, imm);
    asMasm().moveToFloat32(ScratchRegister, dest);
}

void
MacroAssemblerPPC64::ma_sd(FloatRegister ft, BaseIndex address)
{
/*
    if (Imm16::IsInSignedRange(address.offset) && address.scale == TimesOne) {
        as_stfd(ft, address.base, address.offset);
        return;
    }
*/

    asMasm().computeScaledAddress(address, SecondScratchReg);
    asMasm().ma_sd(ft, Address(SecondScratchReg, address.offset));
}

void
MacroAssemblerPPC64::ma_ss(FloatRegister ft, BaseIndex address)
{
/*
    if (Imm16::IsInSignedRange(address.offset) && address.scale == TimesOne) {
        as_stfs(ft, address.base, address.offset);
        return;
    }
*/

    asMasm().computeScaledAddress(address, SecondScratchReg);
    asMasm().ma_ss(ft, Address(SecondScratchReg, address.offset));
}

void
MacroAssemblerPPC64::ma_ld(FloatRegister ft, const BaseIndex& src)
{
    asMasm().computeScaledAddress(src, SecondScratchReg);
    asMasm().ma_ld(ft, Address(SecondScratchReg, src.offset));
}

void
MacroAssemblerPPC64::ma_ls(FloatRegister ft, const BaseIndex& src)
{
    asMasm().computeScaledAddress(src, SecondScratchReg);
    asMasm().ma_ls(ft, Address(SecondScratchReg, src.offset));
}

void
MacroAssemblerPPC64::minMaxDouble(FloatRegister srcDest, FloatRegister second,
                                       bool handleNaN, bool isMax)
{
    ADBlock();
    Label zero, done, nan;

    if (handleNaN) {
        // First or second is NaN, result is NaN.
        compareFloatingPoint(srcDest, second, Assembler::DoubleUnordered);
        ma_bc(Assembler::DoubleUnordered, &nan, ShortJump);
    }
    // Check for zero and equal.
    asMasm().loadConstantDouble(0.0, ScratchDoubleReg);
    as_fcmpo(srcDest, ScratchDoubleReg);
    as_fcmpo(cr1, second, ScratchDoubleReg);
    // Hoist the sub here because we've already done the compare and the
    // crand will serialize.
    as_fsub(ScratchDoubleReg, srcDest, second);
    as_crand(2, 6, 2); // CR0[EQ] &= CR1[EQ]
    // We can use ::Equal, because the unordered check is done, and save
    // emitting a crandc we don't actually need.
    ma_bc(Assembler::Equal, &zero, ShortJump);

    // Neither can be zero. Use fsel.
    if (isMax) {
        as_fsel(srcDest, ScratchDoubleReg, srcDest, second);
    } else {
        as_fsel(srcDest, ScratchDoubleReg, second, srcDest);
    }
    ma_b(&done, ShortJump);

    if (handleNaN) {
        bind(&nan);
        asMasm().loadConstantDouble(JS::GenericNaN(), srcDest);
        ma_b(&done, ShortJump);
    }

    // Make sure we handle -0 and 0 right. Dump A into a GPR and check
    // the sign bit.
    bind(&zero);
    asMasm().moveFromDouble(srcDest, ScratchRegister);
    as_cmpdi(ScratchRegister, 0);
    if (isMax) {
        // If a == -0, then b is either -0 or 0. In this case, return b.
        ma_bc(Assembler::GreaterThanOrEqual, &done, ShortJump); // a == 0
    } else {
        // If a == -0, then b is either -0 or 0. In this case, return a.
        ma_bc(Assembler::LessThan, &done, ShortJump); // a == -0
    }
    as_fmr(srcDest, second);
    bind(&done);
}

void
MacroAssemblerPPC64::loadDouble(const Address& address, FloatRegister dest)
{
    ma_ld(dest, address);
}

void
MacroAssemblerPPC64::loadDouble(const BaseIndex& src, FloatRegister dest)
{
    ma_ld(dest, src);
}

void
MacroAssemblerPPC64::loadFloatAsDouble(const Address& address, FloatRegister dest)
{
    ma_ls(dest, address);
}

void
MacroAssemblerPPC64::loadFloatAsDouble(const BaseIndex& src, FloatRegister dest)
{
    ma_ls(dest, src);
}

void
MacroAssemblerPPC64::loadFloat32(const Address& address, FloatRegister dest)
{
    ma_ls(dest, address);
}

void
MacroAssemblerPPC64::loadFloat32(const BaseIndex& src, FloatRegister dest)
{
    ma_ls(dest, src);
}

void
MacroAssemblerPPC64::ma_call(ImmPtr dest)
{
    asMasm().ma_liPatchable(CallReg, dest);
    xs_mtctr(CallReg);
    as_bctr(LinkB);
}

void
MacroAssemblerPPC64::ma_jump(ImmPtr dest)
{
    asMasm().ma_liPatchable(SecondScratchReg, dest);
    xs_mtctr(SecondScratchReg);
    as_bctr();
}

MacroAssembler&
MacroAssemblerPPC64::asMasm()
{
    return *static_cast<MacroAssembler*>(this);
}

const MacroAssembler&
MacroAssemblerPPC64::asMasm() const
{
    return *static_cast<const MacroAssembler*>(this);
}

//{{{ check_macroassembler_style
// ===============================================================
// MacroAssembler high-level usage.

void
MacroAssembler::flush()
{
}

// ===============================================================
// Stack manipulation functions.

void
MacroAssembler::Push(Register reg)
{
    ma_push(reg);
    adjustFrame(int32_t(sizeof(intptr_t)));
}

void
MacroAssembler::Push(const Imm32 imm)
{
    ma_li(ScratchRegister, imm);
    ma_push(ScratchRegister);
    adjustFrame(int32_t(sizeof(intptr_t)));
}

void
MacroAssembler::Push(const ImmWord imm)
{
    ma_li(ScratchRegister, imm);
    ma_push(ScratchRegister);
    adjustFrame(int32_t(sizeof(intptr_t)));
}

void
MacroAssembler::Push(const ImmPtr imm)
{
    Push(ImmWord(uintptr_t(imm.value)));
}

void
MacroAssembler::Push(const ImmGCPtr ptr)
{
    ma_li(ScratchRegister, ptr);
    ma_push(ScratchRegister);
    adjustFrame(int32_t(sizeof(intptr_t)));
}

void
MacroAssembler::Push(FloatRegister f)
{
    ma_push(f);
    adjustFrame(sizeof(double)); // Keep stack aligned to double, even if it's a float.
}

void
MacroAssembler::Pop(Register reg)
{
    ma_pop(reg);
    adjustFrame(-int32_t(sizeof(intptr_t)));
}

void
MacroAssembler::Pop(FloatRegister f)
{
    ma_pop(f);
    adjustFrame(-sizeof(double));
}

void
MacroAssembler::Pop(const ValueOperand& val)
{
    popValue(val);
    adjustFrame(-int32_t(sizeof(Value)));
}

void
MacroAssembler::PopStackPtr()
{
    loadPtr(Address(StackPointer, 0), StackPointer);
    adjustFrame(-int32_t(sizeof(intptr_t)));
}


// ===============================================================
// Simple call functions.

CodeOffset
MacroAssembler::call(Register reg)
{
    xs_mtctr(reg);
    as_bctr(LinkB);
    return CodeOffset(currentOffset());
}

CodeOffset
MacroAssembler::call(Label* label)
{
    ma_bal(label);
    return CodeOffset(currentOffset());
}

CodeOffset
MacroAssembler::callWithPatch()
{
    ADBlock();

    // Patched with |patchCall|. Assuming we set JumpImmediateRange correctly
    // in Architecture-ppc64.h, we will never get a long branch given to this
    // routine, so a naked bl is sufficient.
    m_buffer.ensureSpace(sizeof(uint32_t));
    as_b(0, RelativeBranch, LinkB); // infinite loop if unpatched
    return CodeOffset(currentOffset());
}

void
MacroAssembler::patchCall(uint32_t callerOffset, uint32_t calleeOffset)
{
    // Account for the bl (the code offset points to the return address).
    int32_t offset = calleeOffset - callerOffset + 4;

    // Get a handle to the instruction.
    Instruction* inst = editSrc(BufferOffset(callerOffset - 4));
    MOZ_ASSERT(inst->extractOpcode() == PPC_b);
    MOZ_ASSERT(inst[0].encode() & LinkB); // only patch calls!
    MOZ_ASSERT(JOffImm26::IsInRange(offset)); // damn well better be

    inst->setData(PPC_b | JOffImm26(offset).encode() | LinkB);
    FlushICache(inst, sizeof(uint32_t));
}

CodeOffset
MacroAssembler::farJumpWithPatch()
{
    ADBlock();
    CodeOffset farJump;

    // Patched with |farJumpWithPatch|. It is guaranteed to be a full stanza.
    if (HasPPCISA3()) {
        // We can use addpcis to access the PC directly. Use r12 "as expected"
        // even though this is not necessarily to ABI-compliant code.
        m_buffer.ensureSpace(10 * sizeof(uint32_t));
        as_addpcis(SecondScratchReg, 0); // r12 = address of jump

        farJump.bind(currentOffset());
        // NIP is here (one word after the non-POWER9 version).
        ma_liPatchable(ScratchRegister, ImmWord(LabelBase::INVALID_OFFSET));
        as_add(SecondScratchReg, SecondScratchReg, ScratchRegister);
        xs_mtctr(SecondScratchReg);
        as_bctr();
    } else {
        // We need to do some footwork to get LR since this is PC-relative.
        m_buffer.ensureSpace(13 * sizeof(uint32_t));

        // Because this whacks LR, we have to save it first. Unfortunately
        // we'll need a third scratch register here.
        xs_mflr(ThirdScratchReg); // 4 // 48
        // Use the special bcl-Always (20,31) form to avoid tainting the BHRB.
        xs_bcl_always(4); // 8 // 44
        xs_mflr(SecondScratchReg); // 12; LR points here // 40

        farJump.bind(currentOffset());
        ma_liPatchable(ScratchRegister, ImmWord(LabelBase::INVALID_OFFSET)); // 32
        as_add(SecondScratchReg, SecondScratchReg, ScratchRegister); // 36 // 16
        xs_mtctr(SecondScratchReg); // 40 // 12
        xs_mtlr(ThirdScratchReg); // 44; restore LR // 8
        as_bctr(); // 48 // 4
    }

    return farJump;
}

void
MacroAssembler::patchFarJump(CodeOffset farJump, uint32_t targetOffset)
{
    int32_t offset;

    if (HasPPCISA3()) {
        // NIP is pointing at the stanza.
        offset = targetOffset - farJump.offset();
    } else {
        // The offset is the patchable stanza, but LR is pointing at the mflr
        // right before it.
        offset = sizeof(uint32_t) + targetOffset - farJump.offset();
    }

    // Get a handle to the branch stanza.
    Instruction* inst = editSrc(BufferOffset(farJump.offset()));
    MOZ_ASSERT(inst->extractOpcode() == PPC_addis);

    Assembler::WriteLoad64Instructions(inst, ScratchRegister, (uint64_t)offset);
    FlushICache(inst, sizeof(uint32_t) * 5);
}

CodeOffset
MacroAssembler::call(wasm::SymbolicAddress target)
{
    movePtr(target, CallReg);
    return call(CallReg);
}

void
MacroAssembler::call(const Address& addr)
{
    loadPtr(addr, CallReg);
    call(CallReg);
}

void
MacroAssembler::call(ImmWord target)
{
    call(ImmPtr((void*)target.value));
}

void
MacroAssembler::call(ImmPtr target)
{
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, target, RelocationKind::HARDCODED);
    ma_call(target);
}

void
MacroAssembler::call(JitCode* c)
{
    BufferOffset bo = m_buffer.nextOffset();
    addPendingJump(bo, ImmPtr(c->raw()), RelocationKind::JITCODE);
    ma_liPatchable(SecondScratchReg, ImmPtr(c->raw()));
    callJitNoProfiler(SecondScratchReg);
}

CodeOffset
MacroAssembler::nopPatchableToCall()
{
    ADBlock();
    as_nop();   // lis
    as_nop();   // ori
    as_nop();   // rldicr
    as_nop();   // oris
    as_nop();   // ori
    as_nop();   // mtctr
    as_nop();   // bctrl
    // Even though this isn't a call (yet), treat it like one, and have
    // the returned offset match the future return address.
    return CodeOffset(currentOffset());
}

void
MacroAssembler::patchNopToCall(uint8_t* call, uint8_t* target)
{
    // This will always be a full call stanza.
    Instruction* inst = (Instruction*) call;
    inst -= 7; // rewind to the first nop
#if DEBUG
    if (inst[0].encode() == PPC_nop) {
        MOZ_ASSERT(inst[1].encode() == PPC_nop);
        MOZ_ASSERT(inst[2].encode() == PPC_nop);
        MOZ_ASSERT(inst[3].encode() == PPC_nop);
        MOZ_ASSERT(inst[4].encode() == PPC_nop);
        MOZ_ASSERT(inst[5].encode() == PPC_nop);
        MOZ_ASSERT(inst[6].encode() == PPC_nop);
    } else {
        // It is permitted to patch a pre-existing call.
        MOZ_ASSERT(inst->extractOpcode() == PPC_addis); // lis
        MOZ_ASSERT(inst[6].encode() == (PPC_bctr | LinkB)); // bctrl
    }
#endif

    Assembler::WriteLoad64Instructions(inst, SecondScratchReg, (uint64_t) target);
    inst[5].makeOp_mtctr(SecondScratchReg);
    inst[6].makeOp_bctr(LinkB);

    FlushICache(inst, sizeof(uint32_t) * 7);
}

void
MacroAssembler::patchCallToNop(uint8_t* call)
{
    // everything be nops now yo
    Instruction* inst = (Instruction*) call;
    inst -= 7; // rewind to the stanza entry
    MOZ_ASSERT(inst->extractOpcode() == PPC_addis); // lis
    MOZ_ASSERT(inst[6].encode() == (PPC_bctr | LinkB)); // bctrl

    inst[0].makeOp_nop();
    inst[1].makeOp_nop();
    inst[2].makeOp_nop();
    inst[3].makeOp_nop();
    inst[4].makeOp_nop();
    inst[5].makeOp_nop();
    inst[6].makeOp_nop();

    FlushICache(inst, sizeof(uint32_t) * 7);
}

void
MacroAssembler::pushReturnAddress()
{
    ADBlock();
    xs_mflr(ScratchRegister);
    as_stdu(ScratchRegister, StackPointer, -8);
}

void
MacroAssembler::popReturnAddress()
{
    ADBlock();
    as_ld(ScratchRegister, StackPointer, 0);
    xs_mtlr(ScratchRegister);
    as_addi(StackPointer, StackPointer, 8);
}

// ===============================================================
// Jit Frames.

uint32_t
MacroAssembler::pushFakeReturnAddress(Register scratch)
{
    ADBlock();
    CodeLabel cl;

    ma_li(scratch, &cl);
    Push(scratch);
    bind(&cl);
    uint32_t retAddr = currentOffset();

    addCodeLabel(cl);
    return retAddr;
}

void
MacroAssembler::loadStoreBuffer(Register ptr, Register buffer)
{
    if (ptr != buffer)
        movePtr(ptr, buffer);
    orPtr(Imm32(gc::ChunkMask), buffer);
    loadPtr(Address(buffer, gc::ChunkStoreBufferOffsetFromLastByte), buffer);
}

void
MacroAssembler::branchPtrInNurseryChunk(Condition cond, Register ptr, Register temp,
                                        Label* label)
{
    ADBlock();
    MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    MOZ_ASSERT(ptr != temp);
    MOZ_ASSERT(ptr != SecondScratchReg);

    if (temp != InvalidReg) {
        movePtr(ptr, temp);
        orPtr(Imm32(gc::ChunkMask), temp);
        branchPtr(InvertCondition(cond),
                  Address(temp, gc::ChunkStoreBufferOffsetFromLastByte),
                  ImmWord(0), label);
    } else {
        // Why, those cheapskates. We have to provide our own temp too?
        // Did the bean counters cut our temp register budget this year?
        // (Ion hits this.)
        MOZ_ASSERT(ptr != ScratchRegister);

        // Both offsets are too big to be immediate displacements.
        ma_li(ScratchRegister, gc::ChunkMask);
        as_or(SecondScratchReg, ptr, ScratchRegister);
        ma_li(ScratchRegister, gc::ChunkStoreBufferOffsetFromLastByte);
        as_add(SecondScratchReg, SecondScratchReg, ScratchRegister);
        as_ld(ScratchRegister, SecondScratchReg, 0);
        as_cmpdi(ScratchRegister, 0);
        ma_bc(InvertCondition(cond), label);
    }
}

void
MacroAssembler::comment(const char* msg)
{
    Assembler::comment(msg);
}

// ===============================================================
// WebAssembly

CodeOffset
MacroAssembler::wasmTrapInstruction()
{
    CodeOffset offset(currentOffset());
    // Only Wasm uses this, and it uses it to force a signal that the Wasm
    // handler will then intercept. We don't want to use a trap instruction
    // because hooking SIGTRAP will interfere with debugging. So we use a
    // stop instruction: it was illegal prior to ISA 3.0, and is privileged
    // on 3.0+, so it will cause a SIGILL no matter what CPU it's run on.
    // Helpfully, SIGILL is exactly what the Wasm signal handler is watching
    // for.
    as_stop();
    return offset;
}

void
MacroAssembler::wasmTruncateDoubleToInt32(FloatRegister input, Register output, bool isSaturating,
                                          Label* oolEntry)
{
    ADBlock();

    // We only care if the conversion is invalid, not if it's inexact.
    // Negative zero is treated like positive zero.
    // Whack VXCVI.
    as_mtfsb0(23);
    as_fctiwz(ScratchDoubleReg, input);
    // VXCVI is a failure (over/underflow, NaN, etc.)
    as_mcrfs(cr0, 5); // reserved - VXSOFT - VXSQRT - VXCVI -> CR0[...SO]
    ma_bc(Assembler::SOBit, oolEntry);

    // OutOfLineTruncateCheckF32/F64ToI32 -> outOfLineWasmTruncateToInt32Check
    moveFromDouble(ScratchDoubleReg, output);
    // Clear and sign extend.
    as_srawi(output, output, 0);
}


void
MacroAssembler::wasmTruncateFloat32ToInt32(FloatRegister input, Register output, bool isSaturating,
                                           Label* oolEntry)
{
    wasmTruncateDoubleToInt32(input, output, isSaturating, oolEntry);
}

void
MacroAssembler::oolWasmTruncateCheckF32ToI32(FloatRegister input, Register output,
                                             TruncFlags flags, wasm::BytecodeOffset off,
                                             Label* rejoin)
{
    outOfLineWasmTruncateToInt32Check(input, output, MIRType::Float32, flags, rejoin, off);
}

void
MacroAssembler::oolWasmTruncateCheckF64ToI32(FloatRegister input, Register output,
                                             TruncFlags flags, wasm::BytecodeOffset off,
                                             Label* rejoin)
{
    outOfLineWasmTruncateToInt32Check(input, output, MIRType::Double, flags, rejoin, off);
}

void
MacroAssembler::oolWasmTruncateCheckF32ToI64(FloatRegister input, Register64 output,
                                             TruncFlags flags, wasm::BytecodeOffset off,
                                             Label* rejoin)
{
    outOfLineWasmTruncateToInt64Check(input, output, MIRType::Float32, flags, rejoin, off);
}

void
MacroAssembler::oolWasmTruncateCheckF64ToI64(FloatRegister input, Register64 output,
                                             TruncFlags flags, wasm::BytecodeOffset off,
                                             Label* rejoin)
{
    outOfLineWasmTruncateToInt64Check(input, output, MIRType::Double, flags, rejoin, off);
}

void
MacroAssemblerPPC64::outOfLineWasmTruncateToInt32Check(FloatRegister input, Register output,
                                                            MIRType fromType, TruncFlags flags,
                                                            Label* rejoin,
                                                            wasm::BytecodeOffset trapOffset)
{
    ADBlock();
    // We must be using a signed truncation or a truncation that is not
    // saturating, and the FPSCR VXCVI bit got set indicating an inexact
    // conversion or a NaN. Unsigned saturating truncates already "do the
    // right thing" with their conversion, so they never end up here.

    Label inputIsNaN;
    bool isSaturating = flags & TRUNC_SATURATING;

    // Test for NaN.
    compareFloatingPoint(input, input, Assembler::DoubleUnordered);
    ma_bc(Assembler::DoubleUnordered, &inputIsNaN, ShortJump);

    if (isSaturating) {
        // fctiwz and fctiwuz both saturate to their respective extents, so
        // we can rejoin; the output value is correct. Do the work we would
        // have done inline if there had been no exception. The scratch
        // register still contains the result.
        asMasm().moveFromDouble(ScratchDoubleReg, ScratchRegister);
        // Don't sign extend.
        as_rldicl(output, ScratchRegister, 0, 32); // "clrldi"
        // Rejoin. This follows the truncation stanza.
        ma_b(rejoin);
    } else {
        // We must have overflowed.
        asMasm().wasmTrap(wasm::Trap::IntegerOverflow, trapOffset);
    }

    asMasm().bind(&inputIsNaN);
    if (isSaturating) {
        // A saturated NaN is zero. (fctiwuz does this for us.)
        xs_li(output, 0);
        ma_b(rejoin);
    } else {
        asMasm().wasmTrap(wasm::Trap::InvalidConversionToInteger, trapOffset);
    }
}

void
MacroAssemblerPPC64::outOfLineWasmTruncateToInt64Check(FloatRegister input, Register64 output,
                                                            MIRType fromType, TruncFlags flags,
                                                            Label* rejoin,
                                                            wasm::BytecodeOffset trapOffset)
{
    ADBlock();
    // We must be using a signed truncation or a truncation that is not
    // saturating, and the FPSCR VXCVI bit got set indicating an inexact
    // conversion or a NaN. Unsigned saturating truncates already "do the
    // right thing" with their conversion, so they never end up here.

    Label inputIsNaN;
    bool isSaturating = flags & TRUNC_SATURATING;

    // Test for NaN.
    compareFloatingPoint(input, input, Assembler::DoubleUnordered);
    ma_bc(Assembler::DoubleUnordered, &inputIsNaN, ShortJump);

    if (isSaturating) {
        // fctidz and fctiduz both saturate to their respective extents, so
        // we can rejoin; the output value is correct. Do the work we would
        // have done inline if there had been no exception. The scratch
        // register still contains the result.
        asMasm().moveFromDouble(ScratchDoubleReg, output.reg);
        ma_b(rejoin);
    } else {
        // We must have overflowed.
        asMasm().wasmTrap(wasm::Trap::IntegerOverflow, trapOffset);
    }

    asMasm().bind(&inputIsNaN);
    if (isSaturating) {
        // A saturated NaN is zero. (fctiduz does this for us.)
        xs_li(output.reg, 0);
        ma_b(rejoin);
    } else {
        asMasm().wasmTrap(wasm::Trap::InvalidConversionToInteger, trapOffset);
    }
}

void
MacroAssembler::wasmLoad(const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
                         Register ptrScratch, AnyRegister output)
{
    ADBlock();
    wasmLoadImpl(access, memoryBase, ptr, ptrScratch, output, InvalidReg);
}

void
MacroAssembler::wasmUnalignedLoad(const wasm::MemoryAccessDesc& access, Register memoryBase,
                                  Register ptr, Register ptrScratch, Register output, Register tmp)
{
    ADBlock();
    wasmLoadImpl(access, memoryBase, ptr, ptrScratch, AnyRegister(output), tmp);
}

void
MacroAssembler::wasmUnalignedLoadFP(const wasm::MemoryAccessDesc& access, Register memoryBase,
                                    Register ptr, Register ptrScratch, FloatRegister output,
                                    Register tmp1)
{
    ADBlock();
    wasmLoadImpl(access, memoryBase, ptr, ptrScratch, AnyRegister(output), tmp1);
}

void
MacroAssembler::wasmStore(const wasm::MemoryAccessDesc& access, AnyRegister value,
                          Register memoryBase, Register ptr, Register ptrScratch)
{
    ADBlock();
    wasmStoreImpl(access, value, memoryBase, ptr, ptrScratch, InvalidReg);
}

void
MacroAssembler::wasmUnalignedStore(const wasm::MemoryAccessDesc& access, Register value,
                                   Register memoryBase, Register ptr, Register ptrScratch,
                                   Register tmp)
{
    ADBlock();
    wasmStoreImpl(access, AnyRegister(value), memoryBase, ptr, ptrScratch, tmp);
}

void
MacroAssembler::wasmUnalignedStoreFP(const wasm::MemoryAccessDesc& access, FloatRegister floatValue,
                                     Register memoryBase, Register ptr, Register ptrScratch,
                                     Register tmp)
{
    ADBlock();
    wasmStoreImpl(access, AnyRegister(floatValue), memoryBase, ptr, ptrScratch, tmp);
}

void
MacroAssemblerPPC64::wasmLoadImpl(const wasm::MemoryAccessDesc& access, Register memoryBase,
                                       Register ptr, Register ptrScratch, AnyRegister output,
                                       Register tmp)
{
    ADBlock();
    uint32_t offset = access.offset();
    uint32_t loadInst = 0;
    MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
    MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

    // Maybe add the offset.
    if (offset) {
        asMasm().addPtr(Imm32(offset), ptrScratch);
        ptr = ptrScratch;
    }

    unsigned byteSize = access.byteSize();
    bool isSigned;
    bool isFloat = false;

    switch (access.type()) {
      case Scalar::Int8:    isSigned = true;  break;
      case Scalar::Uint8:   isSigned = false; break;
      case Scalar::Int16:   isSigned = true;  break;
      case Scalar::Uint16:  isSigned = false; break;
      case Scalar::Int32:   isSigned = true;  break;
      case Scalar::Uint32:  isSigned = false; break;
      case Scalar::Float64: isFloat  = true;  break;
      case Scalar::Float32: isFloat  = true;  break;
      default: MOZ_CRASH("unexpected array type");
    }

    BaseIndex address(memoryBase, ptr, TimesOne);
/*
    if (IsUnaligned(access)) {
        if (isFloat) {
            if (byteSize == 4)
                asMasm().loadUnalignedFloat32(access, address, tmp, output.fpu());
            else
                asMasm().loadUnalignedDouble(access, address, tmp, output.fpu());
        } else {
            asMasm().ma_load(output.gpr(), address, static_cast<LoadStoreSize>(8 * byteSize), isSigned ? SignExtend : ZeroExtend);
        }
        return;
    }
*/

    asMasm().memoryBarrierBefore(access.sync());
    if (isFloat) {
        // The load will always be at the end, so we tag that access.
        if (byteSize == 4)
            asMasm().ma_ls(output.fpu(), address);
        else
           asMasm().ma_ld(output.fpu(), address);
        asMasm().append(access, asMasm().size() - 4);
    } else {
        // This function doesn't handle 64-bit ints, so we should never
        // end up in a situation where we have to break a load apart.
        MOZ_ASSERT(byteSize < 8);
        loadInst = asMasm().ma_load(output.gpr(), address, static_cast<LoadStoreSize>(8 * byteSize),
                         isSigned ? SignExtend : ZeroExtend);
        asMasm().append(access, loadInst);
    }
    asMasm().memoryBarrierAfter(access.sync());
}

void
MacroAssemblerPPC64::wasmStoreImpl(const wasm::MemoryAccessDesc& access, AnyRegister value,
                                        Register memoryBase, Register ptr, Register ptrScratch,
                                        Register tmp)
{
    ADBlock();
    uint32_t offset = access.offset();
    MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
    MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

    // Maybe add the offset.
    if (offset) {
        asMasm().addPtr(Imm32(offset), ptrScratch);
        ptr = ptrScratch;
    }

    unsigned byteSize = access.byteSize();
    bool isSigned;
    bool isFloat = false;

    switch (access.type()) {
      case Scalar::Int8:    isSigned = true;  break;
      case Scalar::Uint8:   isSigned = false; break;
      case Scalar::Int16:   isSigned = true;  break;
      case Scalar::Uint16:  isSigned = false; break;
      case Scalar::Int32:   isSigned = true;  break;
      case Scalar::Uint32:  isSigned = false; break;
      case Scalar::Int64:   isSigned = true;  break;
      case Scalar::Float64: isFloat  = true;  break;
      case Scalar::Float32: isFloat  = true;  break;
      default: MOZ_CRASH("unexpected array type");
    }

    BaseIndex address(memoryBase, ptr, TimesOne);
/*
    if (IsUnaligned(access)) {
        if (isFloat) {
            if (byteSize == 4)
                asMasm().storeUnalignedFloat32(access, value.fpu(), tmp, address);
            else
                asMasm().storeUnalignedDouble(access, value.fpu(), tmp, address);
        } else {
            asMasm().ma_store(value.gpr(), address,
                          static_cast<LoadStoreSize>(8 * byteSize),
                          isSigned ? SignExtend : ZeroExtend);
        }
        return;
    }
*/

    asMasm().memoryBarrierBefore(access.sync());
    if (isFloat) {
        // The store instruction will always be last.
        if (byteSize == 4)
            asMasm().ma_ss(value.fpu(), address);
        else
            asMasm().ma_sd(value.fpu(), address);
        asMasm().append(access, asMasm().size() - 4);
    } else {
        uint32_t loadSize = asMasm().ma_store(value.gpr(), address,
                      static_cast<LoadStoreSize>(8 * byteSize),
                      isSigned ? SignExtend : ZeroExtend);
        if (loadSize & 0x01) {
            // Split store emitted.
            asMasm().append(access, loadSize - 1);
            // The second store is always the last instruction.
            asMasm().append(access, asMasm().size() - 4);
        } else {
            asMasm().append(access, loadSize);
        }
    }
    asMasm().memoryBarrierAfter(access.sync());
}

void
MacroAssembler::enterFakeExitFrameForWasm(Register cxreg, Register scratch, ExitFrameType type)
{
    enterFakeExitFrame(cxreg, scratch, type);
}

// ========================================================================
// Primitive atomic operations.

template<typename T>
static void
CompareExchange(MacroAssembler& masm, const wasm::MemoryAccessDesc* access, 
Scalar::Type type, const Synchronization& sync, const T& mem,
                Register oldval, Register newval, Register valueTemp, Register offsetTemp,
                Register maskTemp, Register output)
{
    bool signExtend = Scalar::isSignedIntType(type);
    unsigned nbytes = Scalar::byteSize(type);

     switch (nbytes) {
        case 1:
        case 2:
            break;
        case 4:
            MOZ_ASSERT(valueTemp == InvalidReg);
            MOZ_ASSERT(offsetTemp == InvalidReg);
            MOZ_ASSERT(maskTemp == InvalidReg);
            break;
        default:
            MOZ_CRASH();
    }

    Label again, end;
    masm.computeEffectiveAddress(mem, SecondScratchReg);

    if (nbytes == 4) {
        masm.memoryBarrierBefore(sync);
        masm.bind(&again);

        if (access) masm.append(*access, masm.size());
        masm.as_lwarx(output, r0, SecondScratchReg);
        masm.ma_bc(output, oldval, &end, Assembler::NotEqual, ShortJump);
        if (access) masm.append(*access, masm.size());
        masm.as_stwcx(newval, r0, SecondScratchReg);
        masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

        masm.memoryBarrierAfter(sync);
        masm.bind(&end);

        return;
    }

    masm.as_andi_rc(offsetTemp, SecondScratchReg, 3);
    masm.subPtr(offsetTemp, SecondScratchReg);
#if !MOZ_LITTLE_ENDIAN()
    masm.as_xori(offsetTemp, offsetTemp, 3);
#endif
    masm.x_slwi(offsetTemp, offsetTemp, 3);
    masm.ma_li(maskTemp, Imm32(UINT32_MAX >> ((4 - nbytes) * 8)));
    masm.as_slw(maskTemp, maskTemp, offsetTemp);
    masm.as_nor(maskTemp, maskTemp, maskTemp);

    masm.memoryBarrierBefore(sync);

    masm.bind(&again);

    if (access) masm.append(*access, masm.size());
    masm.as_lwarx(ScratchRegister, r0, SecondScratchReg);

    masm.as_srw(output, ScratchRegister, offsetTemp);

    switch (nbytes) {
        case 1:
            masm.as_andi_rc(valueTemp, oldval, 0xff);
            masm.as_andi_rc(output, output, 0xff);
            if (signExtend) {
                masm.as_extsb(valueTemp, oldval);
                masm.as_extsb(output, output);
            }
            break;
        case 2:
            masm.as_andi_rc(valueTemp, oldval, 0xffff);
            masm.as_andi_rc(output, output, 0xffff);
            if (signExtend) {
                masm.as_extsh(valueTemp, oldval);
                masm.as_extsh(output, output);
            }
            break;
    }

    masm.ma_bc(output, valueTemp, &end, Assembler::NotEqual, ShortJump);

    masm.as_slw(valueTemp, newval, offsetTemp);
    masm.as_and(ScratchRegister, ScratchRegister, maskTemp);
    masm.as_or(ScratchRegister, ScratchRegister, valueTemp);

    if (access) masm.append(*access, masm.size());
    masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

    masm.memoryBarrierAfter(sync);
        masm.bind(&end);
}


void
MacroAssembler::compareExchange(Scalar::Type type, const Synchronization& sync, const Address& mem,
                                Register oldval, Register newval, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register output)
{
    CompareExchange(*this, nullptr, type, sync, mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                    output);
}

void
MacroAssembler::compareExchange(Scalar::Type type, const Synchronization& sync, const BaseIndex& mem,
                                Register oldval, Register newval, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register output)
{
    CompareExchange(*this, nullptr, type, sync, mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                    output);
}
void
MacroAssembler::wasmCompareExchange(const wasm::MemoryAccessDesc& access, const Address& mem,
                                Register oldval, Register newval, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register output)
{
    CompareExchange(*this, &access, access.type(), access.sync(), mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                    output);
}

void
MacroAssembler::wasmCompareExchange(const wasm::MemoryAccessDesc& access, const BaseIndex& mem,
                                Register oldval, Register newval, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register output)
{
    CompareExchange(*this, &access, access.type(), access.sync(), mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                    output);
}


template<typename T>
static void
AtomicExchange(MacroAssembler& masm, const wasm::MemoryAccessDesc* access,
Scalar::Type type, const Synchronization& sync, const T& mem,
               Register value, Register valueTemp, Register offsetTemp, Register maskTemp,
               Register output)
{
    bool signExtend = Scalar::isSignedIntType(type);
    unsigned nbytes = Scalar::byteSize(type);

     switch (nbytes) {
        case 1:
        case 2:
            break;
        case 4:
            MOZ_ASSERT(valueTemp == InvalidReg);
            MOZ_ASSERT(offsetTemp == InvalidReg);
            MOZ_ASSERT(maskTemp == InvalidReg);
            break;
        default:
            MOZ_CRASH();
    }

    Label again;

    masm.computeEffectiveAddress(mem, SecondScratchReg);

    if (nbytes == 4) {

        masm.memoryBarrierBefore(sync);
        masm.bind(&again);

        if (access) masm.append(*access, masm.size());
        masm.as_lwarx(output, r0, SecondScratchReg);
        if (access) masm.append(*access, masm.size());
        masm.as_stwcx(value, r0, SecondScratchReg);
        masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

        masm.memoryBarrierAfter(sync);

        return;
    }

    masm.as_andi_rc(offsetTemp, SecondScratchReg, 3);
    masm.subPtr(offsetTemp, SecondScratchReg);
#if !MOZ_LITTLE_ENDIAN()
    masm.as_xori(offsetTemp, offsetTemp, 3);
#endif
    masm.x_sldi(offsetTemp, offsetTemp, 3);
    masm.ma_li(maskTemp, Imm32(UINT32_MAX >> ((4 - nbytes) * 8)));
    masm.as_sld(maskTemp, maskTemp, offsetTemp);
    masm.as_nor(maskTemp, maskTemp, maskTemp);
    switch (nbytes) {
        case 1:
            masm.as_andi_rc(valueTemp, value, 0xff);
            break;
        case 2:
            masm.as_andi_rc(valueTemp, value, 0xffff);
            break;
    }
    masm.as_sld(valueTemp, valueTemp, offsetTemp);
    masm.memoryBarrierBefore(sync);
    masm.bind(&again);

    if (access) masm.append(*access, masm.size());
    masm.as_lwarx(output, r0, SecondScratchReg);
    masm.as_and(ScratchRegister, output, maskTemp);
    masm.as_or(ScratchRegister, ScratchRegister, valueTemp);

    if (access) masm.append(*access, masm.size());
    masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

    masm.as_srd(output, output, offsetTemp);

    switch (nbytes) {
        case 1:
            masm.as_andi_rc(output, output, 0xff);
            if (signExtend) {
                masm.as_extsb(output, output);
            }
            break;
        case 2:
            masm.as_andi_rc(output, output, 0xffff);
            if (signExtend) {
                masm.as_extsh(output, output);
            }
            break;
    }

    masm.memoryBarrierAfter(sync);
}


void
MacroAssembler::atomicExchange(Scalar::Type type, const Synchronization& sync, const Address& mem,
                               Register value, Register valueTemp, Register offsetTemp,
                               Register maskTemp, Register output)
{
    AtomicExchange(*this, nullptr, type, sync, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::atomicExchange(Scalar::Type type, const Synchronization& sync, const BaseIndex& mem,
                               Register value, Register valueTemp, Register offsetTemp,
                               Register maskTemp, Register output)
{
    AtomicExchange(*this, nullptr, type, sync, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::wasmAtomicExchange(const wasm::MemoryAccessDesc& access, const Address& mem,
                               Register value, Register valueTemp, Register offsetTemp,
                               Register maskTemp, Register output)
{
    AtomicExchange(*this, &access, access.type(), access.sync(), mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::wasmAtomicExchange(const wasm::MemoryAccessDesc& access, const BaseIndex& mem,
                               Register value, Register valueTemp, Register offsetTemp,
                               Register maskTemp, Register output)
{
    AtomicExchange(*this, &access, access.type(), access.sync(), mem, value, valueTemp, offsetTemp, maskTemp, output);
}


template<typename T>
static void
AtomicFetchOp(MacroAssembler& masm, const wasm::MemoryAccessDesc* access,
Scalar::Type type, const Synchronization& sync,
              AtomicOp op, const T& mem, Register value, Register valueTemp,
              Register offsetTemp, Register maskTemp, Register output)
{
    bool signExtend = Scalar::isSignedIntType(type);
    unsigned nbytes = Scalar::byteSize(type);

     switch (nbytes) {
        case 1:
        case 2:
            break;
        case 4:
            MOZ_ASSERT(valueTemp == InvalidReg);
            MOZ_ASSERT(offsetTemp == InvalidReg);
            MOZ_ASSERT(maskTemp == InvalidReg);
            break;
        default:
            MOZ_CRASH();
    }

    Label again;

    masm.computeEffectiveAddress(mem, SecondScratchReg);

    if (nbytes == 4) {

        masm.memoryBarrierBefore(sync);
        masm.bind(&again);

        if (access) masm.append(*access, masm.size());
        masm.as_lwarx(output, r0, SecondScratchReg);

        switch (op) {
        case AtomicFetchAddOp:
            masm.as_add(ScratchRegister, output, value);
            break;
        case AtomicFetchSubOp:
            masm.as_subf(ScratchRegister, value, output);
            break;
        case AtomicFetchAndOp:
            masm.as_and(ScratchRegister, output, value);
            break;
        case AtomicFetchOrOp:
            masm.as_or(ScratchRegister, output, value);
            break;
        case AtomicFetchXorOp:
            masm.as_xor(ScratchRegister, output, value);
            break;
        default:
            MOZ_CRASH();
        }

        if (access) masm.append(*access, masm.size());
        masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
        masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

        masm.memoryBarrierAfter(sync);

        return;
    }


    masm.as_andi_rc(offsetTemp, SecondScratchReg, 3);
    masm.subPtr(offsetTemp, SecondScratchReg);
#if !MOZ_LITTLE_ENDIAN()
    masm.as_xori(offsetTemp, offsetTemp, 3);
#endif
    masm.x_sldi(offsetTemp, offsetTemp, 3);
    masm.ma_li(maskTemp, Imm32(UINT32_MAX >> ((4 - nbytes) * 8)));
    masm.as_sld(maskTemp, maskTemp, offsetTemp);
    masm.as_nor(maskTemp, maskTemp, maskTemp);

    masm.memoryBarrierBefore(sync);

    masm.bind(&again);

    if (access) masm.append(*access, masm.size());
    masm.as_lwarx(ScratchRegister, r0, SecondScratchReg);
    masm.as_srd(output, ScratchRegister, offsetTemp);

    switch (op) {
        case AtomicFetchAddOp:
            masm.as_add(valueTemp, output, value);
            break;
        case AtomicFetchSubOp:
            masm.as_subf(valueTemp, value, output);
            break;
        case AtomicFetchAndOp:
            masm.as_and(valueTemp, output, value);
            break;
        case AtomicFetchOrOp:
            masm.as_or(valueTemp, output, value);
            break;
        case AtomicFetchXorOp:
            masm.as_xor(valueTemp, output, value);
            break;
        default:
            MOZ_CRASH();
    }

    switch (nbytes) {
        case 1:
            masm.as_andi_rc(valueTemp, valueTemp, 0xff);
            break;
        case 2:
            masm.as_andi_rc(valueTemp, valueTemp, 0xffff);
            break;
    }

    masm.as_sld(valueTemp, valueTemp, offsetTemp);

    masm.as_and(ScratchRegister, ScratchRegister, maskTemp);
    masm.as_or(ScratchRegister, ScratchRegister, valueTemp);

    if (access) masm.append(*access, masm.size());
    masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

    switch (nbytes) {
        case 1:
            masm.as_andi_rc(output, output, 0xff);
            if (signExtend) {
                masm.as_extsb(output, output);
            }
            break;
        case 2:
            masm.as_andi_rc(output, output, 0xffff);
            if (signExtend) {
                masm.as_extsh(output, output);
            }
            break;
        default:
            MOZ_CRASH();
    }

    masm.memoryBarrierAfter(sync);
}

void
MacroAssembler::atomicFetchOp(Scalar::Type type, const Synchronization& sync, AtomicOp op,
                              Register value, const Address& mem, Register valueTemp,
                              Register offsetTemp, Register maskTemp, Register output)
{
    AtomicFetchOp(*this, nullptr, type, sync, op, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::atomicFetchOp(Scalar::Type type, const Synchronization& sync, AtomicOp op,
                              Register value, const BaseIndex& mem, Register valueTemp,
                              Register offsetTemp, Register maskTemp, Register output)
{
    AtomicFetchOp(*this, nullptr, type, sync, op, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::wasmAtomicFetchOp(const wasm::MemoryAccessDesc& access, AtomicOp op,
                              Register value, const Address& mem, Register valueTemp,
                              Register offsetTemp, Register maskTemp, Register output)
{
    AtomicFetchOp(*this, &access, access.type(), access.sync(), op, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

void
MacroAssembler::wasmAtomicFetchOp(const wasm::MemoryAccessDesc& access, AtomicOp op,
                              Register value, const BaseIndex& mem, Register valueTemp,
                              Register offsetTemp, Register maskTemp, Register output)
{
    AtomicFetchOp(*this, &access, access.type(), access.sync(), op, mem, value, valueTemp, offsetTemp, maskTemp, output);
}

template<typename T>
static void
AtomicEffectOp(MacroAssembler& masm, const wasm::MemoryAccessDesc* access, Scalar::Type type, const Synchronization& sync, AtomicOp op,
        const T& mem, Register value, Register valueTemp, Register offsetTemp, Register maskTemp)
{
    unsigned nbytes = Scalar::byteSize(type);

     switch (nbytes) {
        case 1:
        case 2:
            break;
        case 4:
            MOZ_ASSERT(valueTemp == InvalidReg);
            MOZ_ASSERT(offsetTemp == InvalidReg);
            MOZ_ASSERT(maskTemp == InvalidReg);
            break;
        default:
            MOZ_CRASH();
    }

    Label again;

    masm.computeEffectiveAddress(mem, SecondScratchReg);

    if (nbytes == 4) {

        masm.memoryBarrierBefore(sync);
        masm.bind(&again);

        if (access) masm.append(*access, masm.size());
        masm.as_lwarx(ScratchRegister, r0, SecondScratchReg);

        switch (op) {
        case AtomicFetchAddOp:
            masm.as_add(ScratchRegister, ScratchRegister, value);
            break;
        case AtomicFetchSubOp:
            masm.as_subf(ScratchRegister, value, ScratchRegister);
            break;
        case AtomicFetchAndOp:
            masm.as_and(ScratchRegister, ScratchRegister, value);
            break;
        case AtomicFetchOrOp:
            masm.as_or(ScratchRegister, ScratchRegister, value);
            break;
        case AtomicFetchXorOp:
            masm.as_xor(ScratchRegister, ScratchRegister, value);
            break;
        default:
            MOZ_CRASH();
        }

        if (access) masm.append(*access, masm.size());
        masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
        masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

        masm.memoryBarrierAfter(sync);

        return;
    }

    masm.as_andi_rc(offsetTemp, SecondScratchReg, 3);
    masm.subPtr(offsetTemp, SecondScratchReg);
#if !MOZ_LITTLE_ENDIAN()
    masm.as_xori(offsetTemp, offsetTemp, 3);
#endif
    masm.x_sldi(offsetTemp, offsetTemp, 3);
    masm.ma_li(maskTemp, Imm32(UINT32_MAX >> ((4 - nbytes) * 8)));
    masm.as_sld(maskTemp, maskTemp, offsetTemp);
    masm.as_nor(maskTemp, maskTemp, maskTemp);

    masm.memoryBarrierBefore(sync);

    masm.bind(&again);

    if (access) masm.append(*access, masm.size());
    masm.as_lwarx(ScratchRegister, r0, SecondScratchReg);
    masm.as_srd(valueTemp, ScratchRegister, offsetTemp);

    switch (op) {
        case AtomicFetchAddOp:
            masm.as_add(valueTemp, valueTemp, value);
            break;
        case AtomicFetchSubOp:
            masm.as_subf(valueTemp, value, valueTemp);
            break;
        case AtomicFetchAndOp:
            masm.as_and(valueTemp, valueTemp, value);
            break;
        case AtomicFetchOrOp:
            masm.as_or(valueTemp, valueTemp, value);
            break;
        case AtomicFetchXorOp:
            masm.as_xor(valueTemp, valueTemp, value);
            break;
        default:
            MOZ_CRASH();
    }

    switch (nbytes) {
        case 1:
            masm.as_andi_rc(valueTemp, valueTemp, 0xff);
            break;
        case 2:
            masm.as_andi_rc(valueTemp, valueTemp, 0xffff);
            break;
        default:
            MOZ_CRASH();
    }

    masm.as_sld(valueTemp, valueTemp, offsetTemp);

    masm.as_and(ScratchRegister, ScratchRegister, maskTemp);
    masm.as_or(ScratchRegister, ScratchRegister, valueTemp);

    if (access) masm.append(*access, masm.size());
    masm.as_stwcx(ScratchRegister, r0, SecondScratchReg);
    masm.ma_bc(Assembler::NotEqual, &again, ShortJump);

    masm.memoryBarrierAfter(sync);
}


void
MacroAssembler::atomicEffectOpJS(Scalar::Type type, const Synchronization& sync, AtomicOp op,
                                 Register value, const Address& mem, Register valueTemp,
                                 Register offsetTemp, Register maskTemp)
{
    AtomicEffectOp(*this, nullptr, type, sync, op, mem, value, valueTemp, offsetTemp, maskTemp);
}

void
MacroAssembler::atomicEffectOpJS(Scalar::Type type, const Synchronization& sync, AtomicOp op,
                                 Register value, const BaseIndex& mem, Register valueTemp,
                                 Register offsetTemp, Register maskTemp)
{
    AtomicEffectOp(*this, nullptr, type, sync, op, mem, value, valueTemp, offsetTemp, maskTemp);
}

// ========================================================================
// JS atomic operations.

template<typename T>
static void
CompareExchangeJS(MacroAssembler& masm, Scalar::Type arrayType, const Synchronization& sync,
                  const T& mem, Register oldval, Register newval, Register valueTemp,
                  Register offsetTemp, Register maskTemp, Register temp, AnyRegister output)
{
    if (arrayType == Scalar::Uint32) {
        masm.compareExchange(arrayType, sync, mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                             temp);
        masm.convertUInt32ToDouble(temp, output.fpu());
    } else {
        masm.compareExchange(arrayType, sync, mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                             output.gpr());
    }
}

void
MacroAssembler::compareExchangeJS(Scalar::Type arrayType, const Synchronization& sync,
                                  const Address& mem, Register oldval, Register newval,
                                  Register valueTemp, Register offsetTemp, Register maskTemp,
                                  Register temp, AnyRegister output)
{
    CompareExchangeJS(*this, arrayType, sync, mem, oldval, newval, valueTemp, offsetTemp, maskTemp,
                      temp, output);
}

void
MacroAssembler::compareExchangeJS(Scalar::Type arrayType, const Synchronization& sync,
                                  const BaseIndex& mem, Register oldval, Register newval,
                                  Register valueTemp, Register offsetTemp, Register maskTemp,
                                  Register temp, AnyRegister output)
{
    CompareExchangeJS(*this, arrayType, sync, mem, oldval, newval,valueTemp, offsetTemp, maskTemp,
                      temp, output);
}

template<typename T>
static void
AtomicExchangeJS(MacroAssembler& masm, Scalar::Type arrayType, const Synchronization& sync,
                 const T& mem, Register value, Register valueTemp,
                 Register offsetTemp, Register maskTemp, Register temp, AnyRegister output)
{
    if (arrayType == Scalar::Uint32) {
        masm.atomicExchange(arrayType, sync, mem, value, valueTemp, offsetTemp, maskTemp, temp);
        masm.convertUInt32ToDouble(temp, output.fpu());
    } else {
        masm.atomicExchange(arrayType, sync, mem, value, valueTemp, offsetTemp, maskTemp,
                            output.gpr());
    }
}

void
MacroAssembler::atomicExchangeJS(Scalar::Type arrayType, const Synchronization& sync,
                                 const Address& mem, Register value, Register valueTemp,
                                 Register offsetTemp, Register maskTemp, Register temp,
                                 AnyRegister output)
{
    AtomicExchangeJS(*this, arrayType, sync, mem, value, valueTemp, offsetTemp, maskTemp, temp,
                     output);
}

void
MacroAssembler::atomicExchangeJS(Scalar::Type arrayType, const Synchronization& sync,
                                 const BaseIndex& mem, Register value, Register valueTemp,
                                 Register offsetTemp, Register maskTemp, Register temp,
                                 AnyRegister output)
{
    AtomicExchangeJS(*this, arrayType, sync, mem, value, valueTemp, offsetTemp, maskTemp, temp, output);
}

template<typename T>
static void
AtomicFetchOpJS(MacroAssembler& masm, Scalar::Type arrayType, const Synchronization& sync,
                AtomicOp op, Register value, const T& mem, Register valueTemp,
                Register offsetTemp, Register maskTemp, Register temp,
                AnyRegister output)
{
    if (arrayType == Scalar::Uint32) {
        masm.atomicFetchOp(arrayType, sync, op, value, mem, valueTemp, offsetTemp, maskTemp, temp);
        masm.convertUInt32ToDouble(temp, output.fpu());
    } else {
        masm.atomicFetchOp(arrayType, sync, op, value, mem, valueTemp, offsetTemp, maskTemp,
                           output.gpr());
    }
}

void
MacroAssembler::atomicFetchOpJS(Scalar::Type arrayType, const Synchronization& sync, AtomicOp op,
                                Register value, const Address& mem, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register temp,
                                AnyRegister output)
{
    AtomicFetchOpJS(*this, arrayType, sync, op, value, mem, valueTemp, offsetTemp, maskTemp, temp,
                    output);
}

void
MacroAssembler::atomicFetchOpJS(Scalar::Type arrayType, const Synchronization& sync, AtomicOp op,
                                Register value, const BaseIndex& mem, Register valueTemp,
                                Register offsetTemp, Register maskTemp, Register temp,
                                AnyRegister output)
{
    AtomicFetchOpJS(*this, arrayType, sync, op, value, mem, valueTemp, offsetTemp, maskTemp, temp,
                    output);
}

// ========================================================================
// Spectre Mitigations.

void
MacroAssembler::speculationBarrier()
{
    // eieio appears to be the fastest way of defeating Spectre, since its
    // memory ordering is sufficient to defeat gadgets and it's less heavy
    // than even so-called lwsync. Doo doo doo doo doo, a Spectre gadget ...
    // See a real world demonstration in Shen et al, Restricting Control
    // Flow During Speculative Execution with Venkman, pp6-7.
    as_eieio();
    // Go, Gadget, Go!
}
