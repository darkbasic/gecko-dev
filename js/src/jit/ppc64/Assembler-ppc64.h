/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc_Assembler_ppc_h
#define jit_ppc_Assembler_ppc_h

/* Mostly derived from TenFourFox IonPower (r.i.p.). */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/CompactBuffer.h"
#include "jit/JitCode.h"
#include "jit/JitSpewer.h"
#include "jit/ppc64/Architecture-ppc64.h"
#include "jit/shared/Assembler-shared.h"
#include "jit/shared/IonAssemblerBuffer.h"

#if DEBUG
#define ispew(x)    JitSpew(JitSpew_Codegen, "== " x " ==")
#else
#define ispew(x)    ;
#endif

namespace js {
namespace jit {

static constexpr Register r0{ Registers::r0 };
static constexpr Register r1{ Registers::r1 };
static constexpr Register sp{ Registers::r1 };
static constexpr Register r2{ Registers::r2 };
static constexpr Register r3{ Registers::r3 };
static constexpr Register r4{ Registers::r4 };
static constexpr Register r5{ Registers::r5 };
static constexpr Register r6{ Registers::r6 };
static constexpr Register r7{ Registers::r7 };
static constexpr Register r8{ Registers::r8 };
static constexpr Register r9{ Registers::r9 };
static constexpr Register r10{ Registers::r10 };
static constexpr Register r11{ Registers::r11 };
static constexpr Register r12{ Registers::r12 };
static constexpr Register r13{ Registers::r13 };
static constexpr Register r14{ Registers::r14 };
static constexpr Register r15{ Registers::r15 };
static constexpr Register r16{ Registers::r16 };
static constexpr Register r17{ Registers::r17 };
static constexpr Register r18{ Registers::r18 };
static constexpr Register r19{ Registers::r19 };
static constexpr Register r20{ Registers::r20 };
static constexpr Register r21{ Registers::r21 };
static constexpr Register r22{ Registers::r22 };
static constexpr Register r23{ Registers::r23 };
static constexpr Register r24{ Registers::r24 };
static constexpr Register r25{ Registers::r25 };
static constexpr Register r26{ Registers::r26 };
static constexpr Register r27{ Registers::r27 };
static constexpr Register r28{ Registers::r28 };
static constexpr Register r29{ Registers::r29 };
static constexpr Register r30{ Registers::r30 };
static constexpr Register r31{ Registers::r31 };

static constexpr FloatRegister f0{ FloatRegisters::f0, FloatRegisters::Double };
static constexpr FloatRegister f1{ FloatRegisters::f1, FloatRegisters::Double };
static constexpr FloatRegister f2{ FloatRegisters::f2, FloatRegisters::Double };
static constexpr FloatRegister f3{ FloatRegisters::f3, FloatRegisters::Double };
static constexpr FloatRegister f4{ FloatRegisters::f4, FloatRegisters::Double };
static constexpr FloatRegister f5{ FloatRegisters::f5, FloatRegisters::Double };
static constexpr FloatRegister f6{ FloatRegisters::f6, FloatRegisters::Double };
static constexpr FloatRegister f7{ FloatRegisters::f7, FloatRegisters::Double };
static constexpr FloatRegister f8{ FloatRegisters::f8, FloatRegisters::Double };
static constexpr FloatRegister f9{ FloatRegisters::f9, FloatRegisters::Double };
static constexpr FloatRegister f10{ FloatRegisters::f10, FloatRegisters::Double };
static constexpr FloatRegister f11{ FloatRegisters::f11, FloatRegisters::Double };
static constexpr FloatRegister f12{ FloatRegisters::f12, FloatRegisters::Double };
static constexpr FloatRegister f13{ FloatRegisters::f13, FloatRegisters::Double };
static constexpr FloatRegister f14{ FloatRegisters::f14, FloatRegisters::Double };
static constexpr FloatRegister f15{ FloatRegisters::f15, FloatRegisters::Double };
static constexpr FloatRegister f16{ FloatRegisters::f16, FloatRegisters::Double };
static constexpr FloatRegister f17{ FloatRegisters::f17, FloatRegisters::Double };
static constexpr FloatRegister f18{ FloatRegisters::f18, FloatRegisters::Double };
static constexpr FloatRegister f19{ FloatRegisters::f19, FloatRegisters::Double };
static constexpr FloatRegister f20{ FloatRegisters::f20, FloatRegisters::Double };
static constexpr FloatRegister f21{ FloatRegisters::f21, FloatRegisters::Double };
static constexpr FloatRegister f22{ FloatRegisters::f22, FloatRegisters::Double };
static constexpr FloatRegister f23{ FloatRegisters::f23, FloatRegisters::Double };
static constexpr FloatRegister f24{ FloatRegisters::f24, FloatRegisters::Double };
static constexpr FloatRegister f25{ FloatRegisters::f25, FloatRegisters::Double };
static constexpr FloatRegister f26{ FloatRegisters::f26, FloatRegisters::Double };
static constexpr FloatRegister f27{ FloatRegisters::f27, FloatRegisters::Double };
static constexpr FloatRegister f28{ FloatRegisters::f28, FloatRegisters::Double };
static constexpr FloatRegister f29{ FloatRegisters::f29, FloatRegisters::Double };
static constexpr FloatRegister f30{ FloatRegisters::f30, FloatRegisters::Double };
static constexpr FloatRegister f31{ FloatRegisters::f31, FloatRegisters::Double };
// The rest of the FPRs are the business of the allocator, not the assembler.
// SPRs and CRs are defined in their respective enums (see Architecture-ppc.h).

static constexpr Register OsrFrameReg = r6;
static constexpr Register ArgumentsRectifierReg = r19;
static constexpr Register CallTempReg0 = r8;
static constexpr Register CallTempReg1 = r9;
static constexpr Register CallTempReg2 = r10;
static constexpr Register CallTempReg3 = r7;
static constexpr Register CallTempReg4 = r5; // Bad things! Try not to use these!
static constexpr Register CallTempReg5 = r6;

static constexpr Register InterpreterPCReg = r17;

// irregexp
static constexpr Register IntArgReg0 = r3;
static constexpr Register IntArgReg1 = r4;
static constexpr Register IntArgReg2 = r5;
static constexpr Register IntArgReg3 = r6;
static constexpr Register IntArgReg4 = r7;
static constexpr Register IntArgReg5 = r8;
static constexpr Register IntArgReg6 = r9;
static constexpr Register IntArgReg7 = r10;

static constexpr Register GlobalReg = r23; // used by AsmJS. Allocatable, but non-volatile. Must not clash with wasm.
static constexpr Register HeapReg = r24; // Ditto.

// These are defined, but not actually used, at least by us (see GetTempRegForIntArg).
static constexpr Register CallTempNonArgRegs[] = { r10, r9, r8, r7 };
static const uint32_t NumCallTempNonArgRegs = mozilla::ArrayLength(CallTempNonArgRegs);

class ABIArgGenerator
{
	uint32_t stackOffset_;
	uint32_t usedGPRs_;
	uint32_t usedFPRs_;
    ABIArg current_;

  public:
    ABIArgGenerator();
    ABIArg next(MIRType argType);
    ABIArg &current() { return current_; }

    uint32_t stackBytesConsumedSoFar() const { return stackOffset_; }
    void increaseStackOffset(uint32_t bytes) { stackOffset_ += bytes; }
};

static constexpr Register ABINonArgReg0 = r19;
static constexpr Register ABINonArgReg1 = r20;
static constexpr Register ABINonArgReg2 = r21;
static constexpr Register ABINonArgReg3 = r22;
// These can be non-volatile; they are only used by Wasm, and only after
// all registers have been spilled. These must not be argregs as they may
// be used after all argregs are exhausted.
static constexpr Register ABINonArgReturnReg0 = r29;
static constexpr Register ABINonArgReturnReg1 = r30;
static constexpr Register ABINonArgReturnVolatileReg = r11;
static constexpr Register ABINonVolatileReg = r14;

static constexpr Register PreBarrierReg = r4;

static constexpr Register InvalidReg{ Registers::invalid_reg };
static constexpr FloatRegister InvalidFloatReg;

static constexpr Register StackPointer = sp;
static constexpr Register FramePointer = r31; // wasm

static constexpr Register ScratchRegister = r0;
static constexpr Register SecondScratchReg = r12;
static constexpr Register ThirdScratchReg = r11; // EMERGENCY! RESCUE r11!

// All return registers must be allocatable.
static constexpr Register JSReturnReg_Type = r6;
static constexpr Register JSReturnReg_Data = r5;
static constexpr Register JSReturnReg = r4;
static constexpr Register ReturnReg = r3;
static constexpr Register64 ReturnReg64{ReturnReg};
static constexpr FloatRegister ReturnFloat32Reg = {FloatRegisters::f1,
                                                   FloatRegisters::Single};
static constexpr FloatRegister ReturnDoubleReg = {FloatRegisters::f1,
                                                  FloatRegisters::Double};
static constexpr FloatRegister ABINonArgDoubleReg = {FloatRegisters::f14,
                                                     FloatRegisters::Double};
static constexpr ValueOperand JSReturnOperand = ValueOperand(JSReturnReg);

// Registers used in RegExpMatcher instruction (do not use JSReturnOperand).
static constexpr Register RegExpMatcherRegExpReg = CallTempReg0;
static constexpr Register RegExpMatcherStringReg = CallTempReg1;
static constexpr Register RegExpMatcherLastIndexReg = CallTempReg2;

// Registers used in RegExpTester instruction (do not use ReturnReg).
static constexpr Register RegExpTesterRegExpReg = CallTempReg0;
static constexpr Register RegExpTesterStringReg = CallTempReg1;
static constexpr Register RegExpTesterLastIndexReg = CallTempReg2;

// TLS pointer argument register for WebAssembly functions. This must not alias
// any other register used for passing function arguments or return values.
// Preserved by WebAssembly functions.
static constexpr Register WasmTlsReg = r18;

// Registers used for wasm table calls. These registers must be disjoint
// from the ABI argument registers, WasmTlsReg and each other.
static constexpr Register WasmTableCallScratchReg0 = ABINonArgReg0;
static constexpr Register WasmTableCallScratchReg1 = ABINonArgReg1;
static constexpr Register WasmTableCallSigReg = ABINonArgReg2;
static constexpr Register WasmTableCallIndexReg = ABINonArgReg3;

// Register used as a scratch along the return path in the fast js -> wasm stub
// code. This must not overlap ReturnReg, JSReturnOperand, or WasmTlsReg. It
// must be a volatile register.
static constexpr Register WasmJitEntryReturnScratch = r10;

static constexpr uint32_t WasmCheckedCallEntryOffset = 0u;
static constexpr uint32_t WasmCheckedTailEntryOffset = 32u; // damn mtspr

// Good grief. Must FPRs be vector registers on every architecture?
// I guess we could only support SIMD on processors with VSX.
static constexpr FloatRegister ReturnSimdReg = InvalidFloatReg;
static constexpr FloatRegister ReturnSimd128Reg = InvalidFloatReg;
static constexpr FloatRegister ReturnInt32x4Reg = InvalidFloatReg;
static constexpr FloatRegister ReturnFloat32x4Reg = InvalidFloatReg;
static constexpr FloatRegister ScratchSimdReg = InvalidFloatReg;
static constexpr FloatRegister ScratchSimd128Reg = InvalidFloatReg;

static constexpr FloatRegister ScratchFloat32Reg = {FloatRegisters::f0,
                                                    FloatRegisters::Single};
static constexpr FloatRegister ScratchDoubleReg = {FloatRegisters::f0,
                                                   FloatRegisters::Double};

struct ScratchFloat32Scope : public AutoFloatRegisterScope {
  explicit ScratchFloat32Scope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchFloat32Reg) {}
};
struct ScratchDoubleScope : public AutoFloatRegisterScope {
  explicit ScratchDoubleScope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchDoubleReg) {}
};

// A bias applied to the GlobalReg to allow the use of instructions with small
// negative immediate offsets which doubles the range of global data that can be
// accessed with a single instruction. (XXX)
static const int32_t AsmJSGlobalRegBias = 32768;

// Registers used in the GenerateFFIIonExit Enable Activation block. (Mirror MIPS.)
static constexpr Register AsmJSIonExitRegCallee = r7;
static constexpr Register AsmJSIonExitRegE0 = r3;
static constexpr Register AsmJSIonExitRegE1 = r4;
static constexpr Register AsmJSIonExitRegE2 = r5;
static constexpr Register AsmJSIonExitRegE3 = r6;

// Registers used in the GenerateFFIIonExit Disable Activation block.
static constexpr Register AsmJSIonExitRegReturnData = JSReturnReg_Data;
static constexpr Register AsmJSIonExitRegReturnType = JSReturnReg_Type;
static constexpr Register AsmJSIonExitRegD0 = r3;
static constexpr Register AsmJSIonExitRegD1 = r4;
static constexpr Register AsmJSIonExitRegD2 = r7;

static const uint32_t ABIStackAlignment = 16;
static const uint32_t CodeAlignment = 16;
// Ion code only. The 8-alignment is necessary because frames always have the return address
// at the top of the stack and are not padded (see the common frame layout in JitFrames.h).
static const uint32_t StackAlignment = 8;
static const uint32_t JitStackAlignment = 16;
static const uint32_t JitStackValueAlignment = 2;

// Helper classes for ScratchRegister usage. Asserts that only one piece
// of code thinks it has exclusive ownership of each scratch register.
struct ScratchRegisterScope : public AutoRegisterScope {
  explicit ScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, ScratchRegister) {}
};
struct SecondScratchRegisterScope : public AutoRegisterScope {
  explicit SecondScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, SecondScratchReg) {}
};

// Future.
static constexpr bool SupportsSimd = false;
static constexpr uint32_t SimdStackAlignment = 16;
static constexpr uint32_t SimdMemoryAlignment = 16;
static constexpr uint32_t AsmJSStackAlignment = 16;

static constexpr uint32_t WasmStackAlignment = SimdStackAlignment;

static const uint32_t WasmTrapInstructionLength = 4;

static const Scale ScalePointer = TimesEight;

enum PPCOpcodes {
    // Some we don't use yet (but we will).
    PPC_add     = 0x7C000214, // add
    PPC_addc    = 0x7C000014, // add carrying
    PPC_adde    = 0x7C000114, // add extended
    PPC_addi    = 0x38000000, // add immediate
    PPC_addic   = 0x30000000, // add immediate carrying
    PPC_addis   = 0x3C000000, // add immediate shifted
    PPC_addme   = 0x7C0001D4, // add -1 extended
    PPC_addo    = 0x7C000614, // add & OE=1 (can set OV)
    PPC_addpcis = 0x4C000004, // load next instruction address into register
    PPC_addze   = 0x7C000194, // add zero extended
    PPC_and     = 0x7C000038, // and
    PPC_andc    = 0x7C000078, // and with compliment
    PPC_andi    = 0x70000000, // and immediate
    PPC_andis   = 0x74000000, // and immediate shifted
    PPC_b       = 0x48000000, // branch
    PPC_bc      = 0x40000000, // branch conditional
    PPC_bctr    = 0x4E800420, // branch to CTR (+/- LR)
    PPC_bcctr   = 0x4C000420, // branch conditional to count register
    PPC_blr     = 0x4E800020, // branch to link register
    PPC_cmpd    = 0x7C200000, // compare
    PPC_cmpdi   = 0x2C200000, // compare immediate
    PPC_cmpld   = 0x7C200040, // compare logical
    PPC_cmpldi  = 0x28200000, // compare logical immediate
    PPC_cmpw    = 0x7C000000, // compare
    PPC_cmpwi   = 0x2C000000, // compare immediate
    PPC_cmplw   = 0x7C000040, // compare logical
    PPC_cmplwi  = 0x28000000, // compare logical immediate
    PPC_cntlzd  = 0x7C000074, // count leading zeroes
    PPC_cntlzw  = 0x7C000034, // count leading zeroes
    PPC_cnttzd  = 0x7C000474, // count leading zeroes
    PPC_cnttzw  = 0x7C000434, // count leading zeroes
    PPC_crand   = 0x4C000202, // condition register and
    PPC_crandc  = 0x4C000102, // condition register and-with-complement
    PPC_cror    = 0x4C000382, // condition register or
    PPC_crorc   = 0x4C000342, // condition register or-with-complement
    PPC_crxor   = 0x4C000182, // condition register xor
    PPC_divd    = 0x7C0003D2, // integer divide
    PPC_divdo   = 0x7C0007D2, // integer divide & OE=1 (can set OV)
    PPC_divdu   = 0x7C000392, // integer divide unsigned
    PPC_divduo  = 0x7C000792, // integer divide unsigned & OE=1 (can set OV)
    PPC_divw    = 0x7C0003D6, // integer divide
    PPC_divwo   = 0x7C0007D6, // integer divide & OE=1 (can set OV)
    PPC_divwu   = 0x7C000396, // integer divide unsigned
    PPC_divwuo  = 0x7C000796, // integer divide unsigned & OE=1 (can set OV)
    PPC_eieio   = 0x7C0006AC, // enforce in-order execution of I/O
    PPC_eqv     = 0x7C000238, // equivalence operator
    PPC_extsb   = 0x7C000774, // extend sign byte
    PPC_extsh   = 0x7C000734, // extend sign halfword
    PPC_extsw   = 0x7C0007B4, // extend sign word
    PPC_fabs    = 0xFC000210, // floating absolute value (double precision)
    PPC_fadd    = 0xFC00002A, // floating add (double precision)
    PPC_fadds   = 0xEC00002A, // floating add (single precision)
    PPC_fcpsgn  = 0xFC000010, // floating copy sign
    PPC_fcfid   = 0xFC00069C, // floating convert from integer doubleword
    PPC_fcfids  = 0xEC00069C, // floating convert from integer doubleword SP
    PPC_fcfidu  = 0xFC00079C, // floating convert from integer doubleword US
    PPC_fcfidus = 0xEC00079C, // floating convert from integer DW (SP+US)
    PPC_fcmpo   = 0xFC000040, // floating compare unordered
    PPC_fcmpu   = 0xFC000000, // floating compare unordered
    PPC_fctid   = 0xFC00065C, // floating convert to integer (to -Inf)
    PPC_fctidu  = 0xFC00075C, // floating convert to integer doubleword unsigned
    PPC_fctidz  = 0xFC00065E, // floating convert to integer DW signed (to zero)
    PPC_fctiduz = 0xFC00075E, // floating convert to integer USDW (to zero)
    PPC_fctiw   = 0xFC00001C, // floating convert to integer (to -Inf)
    PPC_fctiwu  = 0xFC00011C, // floating convert to integer (to -Inf)
    PPC_fctiwuz = 0xFC00011E, // floating convert to integer (to zero)
    PPC_fctiwz  = 0xFC00001E, // floating convert to integer (to zero)
    PPC_fdiv    = 0xFC000024, // floating divide (double precision)
    PPC_fdivs   = 0xEC000024, // floating divide (single precision)
    PPC_fmr     = 0xFC000090, // floating move register
    PPC_fmrgew  = 0xFC00078C, // floating merge even word
    PPC_fmul    = 0xFC000032, // floating multiply (double precision)
    PPC_fmuls   = 0xEC000032, // floating multiply (single precision)
    PPC_fneg    = 0xFC000050, // floating negate
    PPC_frim    = 0xFC0003d0, // floating round to integer minus
    PPC_frin    = 0xFC000310, // floating round to integer nearest
    PPC_frip    = 0xFC000390, // floating round to integer plus
    PPC_friz    = 0xFC000350, // floating round to integer toward zero
    PPC_frsp    = 0xFC000018, // convert to single precision
    PPC_fsel    = 0xFC00002E, // floating point select
    PPC_fsub    = 0xFC000028, // floating subtract (double precision)
    PPC_fsubs   = 0xEC000028, // floating subtract (single precision)
    PPC_fsqrt   = 0xFC00002C, // floating square root (double)
    PPC_fsqrts  = 0xEC00002C, // floating square root (double)
    PPC_frsqrte = 0xFC000034, // floating reciprocal square root estimate
    PPC_fnmsub  = 0xFC00003C, // floating fused negative multiply-subtract
    PPC_fmadd   = 0xFC00003A, // floating fused multiply-add
    PPC_isel    = 0x7C00001E, // integer select
    PPC_isync   = 0x4C00012C, // instruction synchronize
    PPC_lbz     = 0x88000000, // load byte and zero
    PPC_lbzx    = 0x7C0000AE, // load byte and zero indexed
    PPC_ld      = 0xE8000000, // load doubleword
    PPC_ldarx   = 0x7C0000A8, // load doubleword indexed
    PPC_ldx     = 0x7C00002A, // load doubleword indexed
    PPC_lfd     = 0xC8000000, // load floating point double
    PPC_lfdx    = 0x7C0004AE, // load floating-point double indexed
    PPC_lfiwax  = 0x7C0006AE, // load floating-point as integer word algebraic indexed
    PPC_lfiwzx  = 0x7C0006EE, // load floating-point as integer word algebraic indexed
    PPC_lfs     = 0xC0000000, // load single precision float
    PPC_lfsx    = 0x7C00042E, // load single precision float indexed
    PPC_lha     = 0xA8000000, // load halfword algebraic
    PPC_lhax    = 0x7C0002AE, // load halfword algebraic indexed
    PPC_lhz     = 0xA0000000, // load halfword and zero
    PPC_lhzx    = 0x7C00022E, // load halfword and zero indexed
    PPC_lhbrx   = 0x7C00062C, // load hw and zero indexed (byte swapped)
    PPC_lwa     = 0xE8000002, // load word algebraic
    PPC_lwarx   = 0x7c000028, // load word and reserve indexed
    PPC_lwz     = 0x80000000, // load word and zero
    PPC_lwzx    = 0x7C00002E, // load word and zero indexed
    PPC_lwbrx   = 0x7C00042C, // load word and zero indexed (byte swapped)
    PPC_mcrxrx  = 0x7C000480, // move XER[OV, OV32, CA, CA32] to CR[0-3]
    PPC_mcrf    = 0x4C000000, // move CR[0-3] to CR[0-3]
    PPC_mcrfs   = 0xFC000080, // move FPSCR fields to CR
    PPC_mfcr    = 0x7C000026, // move from condition register
    PPC_mfocrf  = 0x7C100120, // move from one condition register field
    PPC_mffs    = 0xFC00048E, // move from fpscr to fpr
    PPC_mfspr   = 0x7C0002A6, // move from spr (special purpose register)
    PPC_mfvsrd  = 0x7C000066, // move from VSR doubleword (used for FPR)
    PPC_modsd   = 0x7C000612, // integer remainder 64-bit (signed)
    PPC_modud   = 0x7c000212, // integer remainder 64-bit (unsigned)
    PPC_modsw   = 0x7C000616, // integer remainder (signed)
    PPC_moduw   = 0x7c000216, // integer remainder (unsigned)
    PPC_mtcrf   = 0x7C000120, // move to condition register field
    PPC_mtfsb0  = 0xFC00008C, // move zero bit into FPSCR
    PPC_mtfsb1  = 0xFC00004C, // move one bit into FPSCR
    PPC_mtfsfi  = 0xFC00010C, // move 4-bit immediate into FPSCR field
    PPC_mtvsrd  = 0x7C000166, // move to VSR doubleword (used for FPR)
    PPC_mtvsrws = 0x7C000326, // move to VSR word and splat (used for FPR)
    PPC_mtvsrwz = 0x7C0001E6, // move to VSR word and zero (used for FPR)
    PPC_mtspr   = 0x7C0003A6, // move to spr
    PPC_mulhd   = 0x7C000092, // multiply high signed doubleword
    PPC_mulhdu  = 0x7C000012, // multiply high signed doubleword
    PPC_mulhw   = 0x7C000096, // multiply high signed
    PPC_mulhwu  = 0x7C000016, // multiply high unsigned
    PPC_mulli   = 0x1C000000, // multiply low immediate
    PPC_mulld   = 0x7C0001D2, // multiply low doubleword
    PPC_mulldo  = 0x7C0005D2, // multiply low doubleword
    PPC_mullw   = 0x7C0001D6, // multiply low word
    PPC_mullwo  = 0x7C0005D6, // multiply low word with overflow
    PPC_nand    = 0x7C0003B8, // nand
    PPC_neg     = 0x7C0000D0, // negate
    PPC_nego    = 0x7C0004D0, // negate & OE=1 (can set OV)
    PPC_nor     = 0x7C0000F8, // nor
    PPC_or      = 0x7C000378, // or
    PPC_ori     = 0x60000000, // or immediate
    PPC_oris    = 0x64000000, // or immediate shifted
    PPC_popcntb = 0x7C0000F4, // population count doubleword
    PPC_popcntd = 0x7C0003F4, // population count doubleword
    PPC_popcntw = 0x7C0002F4, // population count doubleword
    PPC_rldcl   = 0x78000010, // rotate left doubleword then clear left
    PPC_rldicl  = 0x78000000, // rotate left doubleword immediate then clear left
    PPC_rldcr   = 0x78000012, // rotate left doubleword then clear right
    PPC_rldicr  = 0x78000004, // rotate left doubleword immediate then clear right
    PPC_rldimi  = 0x7800000C, // rotate left doubleword immediate then mask insert
    PPC_rlwimi  = 0x50000000, // rotate left word imm then mask insert
    PPC_rlwinm  = 0x54000000, // rotate left word imm then and with mask
    PPC_rlwnm   = 0x5C000000, // rotate left word then AND with mask
    PPC_sld     = 0x7C000036, // shift left doubleword
    PPC_slw     = 0x7C000030, // shift left word
    PPC_srad    = 0x7C000634, // shift right algebraic doubleword (sign ext)
    PPC_sradi   = 0x7C000674, // shift right algebraic doubleword immediate
    PPC_sraw    = 0x7C000630, // shift right algebraic word (sign ext)
    PPC_srawi   = 0x7C000670, // shift right algebraic word immediate
    PPC_srd     = 0x7C000436, // shift right doubleword (zero ext)
    PPC_srw     = 0x7C000430, // shift right word (zero ext)
    PPC_stb     = 0x98000000, // store byte
    PPC_stbx    = 0x7C0001AE, // store byte indexed
    PPC_std     = 0xF8000000, // store doubleword
    PPC_stdcx   = 0x7C0001AD, // store doubleword conditional indexed
    PPC_stdu    = 0xF8000001, // store doubleword with update
    PPC_stdux   = 0x7C00016A, // store doubleword with update indexed
    PPC_stdx    = 0x7C00012A, // store doubleword indexed
    PPC_stfd    = 0xD8000000, // store floating-point double
    PPC_stfdu   = 0xDC000000, // store floating-point double with update
    PPC_stfdx   = 0x7C0005AE, // store floating-point double indexed
    PPC_stfiwx  = 0x7C0007AE, // Store floating-point as integer word indexed
    PPC_stfs    = 0xD0000000, // store floating-point single
    PPC_stfsu   = 0xD4000000, // store floating-point single
    PPC_stfsx   = 0x7C00052E, // store floating-point single indexed
    PPC_sth     = 0xB0000000, // store halfword
    PPC_sthx    = 0x7C00032E, // store halfword indexed
    PPC_sthbrx  = 0x7C00072C, // store halfword indexed (byte swapped)
    PPC_stop    = 0x4C0002E4, // wasm-specific trap word (see note)
    PPC_stw     = 0x90000000, // store word
    PPC_stwu    = 0x94000000, // store word with update
    PPC_stwux   = 0x7C00016E, // store word with update indexed
    PPC_stwx    = 0x7C00012E, // store word indexed
    PPC_stwbrx  = 0x7C00052C, // store word indexed (byte swapped)
    PPC_stwcx   = 0x7C00012D, // store word indexed
    PPC_subf    = 0x7C000050, // subtract from
    PPC_subfc   = 0x7C000010, // subtract from with carry
    PPC_subfe   = 0x7C000110, // subtract from extended
    PPC_subfic  = 0x20000000, // subtract from immediate
    PPC_subfze  = 0x7C000190, // subtract from zero extended
    PPC_subfo   = 0x7C000450, // subtract from with overflow
    PPC_sync    = 0x7C0004AC, // sync
#if defined(__APPLE__) || defined(__linux__)
    PPC_trap    = 0x7FE00008, // trap word (extended from tw 31,r0,r0)
#elif defined(__FreeBSD__)
    PPC_trap    = 0x7C810808, // trap word (tweq r1, r1)
#else
#error Specify the trap word for your PPC operating system
#endif
    PPC_tw      = 0x7C000008, // trap word immediate
    PPC_twi     = 0x0C000000, // trap word immediate
    PPC_xor     = 0x7C000278, // xor
    PPC_xori    = 0x68000000, // xor immediate
    PPC_xoris   = 0x6C000000, // xor immediate shifted
    PPC_xscvdpsp= 0xF0000424, // VSX scalar convert double to single (for FPR)
    PPC_xscvdpspn=0xF000042C, // VSX scalar convert double to single sNaN-pres
    PPC_xscvspdp= 0xF0000524, // VSX scalar convert single to double (for FPR)
    PPC_xscvspdpn=0xF000052C, // VSX scalar convert single to double sNaN-pres
    PPC_xxbrd   = 0xF017076C, // VSX byte-reverse doubleword
    PPC_xxlxor  = 0xF00004D0, // VSX logical XOR (I love this mnemonic)

    // simplified mnemonics
    PPC_mr = PPC_or,
    PPC_not = PPC_nor,
    PPC_nop = PPC_ori,
    PPC_lwsync = PPC_sync | (1 << 21),
        
    PPC_MAJOR_OPCODE_MASK = 0xFC000000 // AND with this to get some idea of the opcode
};

class Instruction;
class InstImm;
class MacroAssemblerPPC;
class Operand;

// A BOffImm16 is a 16 bit (signed or unsigned) immediate that is used for branches.
class BOffImm16
{
    int32_t data;

  public:
    uint32_t encode() {
        MOZ_ASSERT(!isInvalid());
        return (uint32_t)data & 0xFFFC;
    }
    int32_t decode() {
        MOZ_ASSERT(!isInvalid());
        return data;
    }

    explicit BOffImm16(int offset)
    {
        MOZ_ASSERT((offset & 0x3) == 0);
        MOZ_ASSERT(IsInRange(offset) || IsInSignedRange(offset));
        data = offset;
    }
    static bool IsInRange(int offset) {
    	if (offset > 65535)
    		return false;
    	if (offset < 0)
    		return false;
    	return true;
    }
    static bool IsInSignedRange(int offset) {
        if (offset > 32767)
            return false;
        if (offset < -32767)
            return false;
        return true;
    }
    static const int32_t INVALID = 0x00020000;
    BOffImm16()
      : data(INVALID)
    { }

    bool isInvalid() {
        return data == INVALID;
    }
    Instruction *getDest(Instruction *src);

    BOffImm16(InstImm inst);
};

// A JOffImm26 is a 26 bit signed immediate that is used for unconditional jumps.
class JOffImm26
{
    int32_t data;

  public:
    uint32_t encode() {
        MOZ_ASSERT(!isInvalid());
        return (uint32_t)data & 0x03FFFFFC;
    }
    int32_t decode() {
        MOZ_ASSERT(!isInvalid());
        return data;
    }

    explicit JOffImm26(int offset)
    {
        MOZ_ASSERT((offset & 0x3) == 0);
        MOZ_ASSERT(IsInRange(offset));
        data = offset;
    }
    static bool IsInRange(int offset) {
        if (offset < -33554431)
            return false;
        if (offset > 33554431)
            return false;
        return true;
    }
    static const int32_t INVALID = 0x20000000;
    JOffImm26()
      : data(INVALID)
    { }

    bool isInvalid() {
        return data == INVALID;
    }
    Instruction *getDest(Instruction *src);

};

class Imm16
{
    int32_t value;

  public:
    Imm16();
    Imm16(uint32_t imm)
      : value(imm)
    {
    }
    uint32_t encode() {
        return (uint32_t)value & 0xffff;
    }
    int32_t decodeSigned() {
        return value;
    }
    uint32_t decodeUnsigned() {
        return value;
    }
    static bool IsInSignedRange(int32_t imm) {
        return imm >= INT16_MIN  && imm <= INT16_MAX;
    }
    static bool IsInUnsignedRange(uint32_t imm) {
        return imm <= UINT16_MAX ;
    }
    static Imm16 Lower (Imm32 imm) {
        return Imm16(imm.value & 0xffff);
    }
    static Imm16 Upper (Imm32 imm) {
        return Imm16((imm.value >> 16) & 0xffff);
    }
};

class Imm8
{
    uint8_t value;

  public:
    Imm8();
    Imm8(uint32_t imm) : value(imm) {}
    uint32_t encode(uint32_t shift) { return value << shift; }
    int32_t decodeSigned() { return value; }
    uint32_t decodeUnsigned() { return value; }
    static bool IsInSignedRange(int32_t imm) {
        return imm >= INT8_MIN && imm <= INT8_MAX;
    }
    static bool IsInUnsignedRange(uint32_t imm) { return imm <= UINT8_MAX; }
    static Imm8 Lower(Imm16 imm) { return Imm8(imm.decodeSigned() & 0xff); }
    static Imm8 Upper(Imm16 imm) {
        return Imm8((imm.decodeSigned() >> 8) & 0xff);
    }
};

class Operand
{
  public:
    enum Tag {
        REG,
        FREG,
        MEM
    };

  private:
    Tag tag : 3;
    uint32_t reg : 5; // XXX. This really should be Register::Code, but then float regs ...
    int32_t offset;

  public:
    Operand (Register reg_)
      : tag(REG), reg(reg_.code())
    { }

    Operand (FloatRegister freg)
      : tag(FREG), reg(freg.code())
    { }

    Operand (Register base, Imm32 off)
      : tag(MEM), reg(base.code()), offset(off.value)
    { }

    Operand (Register base, int32_t off)
      : tag(MEM), reg(base.code()), offset(off)
    { }

    Operand (const Address &addr)
      : tag(MEM), reg(addr.base.code()), offset(addr.offset)
    { }

    Tag getTag() const {
        return tag;
    }

    Register toReg() const {
        MOZ_ASSERT(tag == REG);
        return Register::FromCode((Register::Code)reg);
    }

    FloatRegister toFReg() const {
        MOZ_ASSERT(tag == FREG);
        return FloatRegister::FromCode((FloatRegister::Code)reg);
    }

    void toAddr(Register *r, Imm32 *dest) const {
        MOZ_ASSERT(tag == MEM);
        *r = Register::FromCode((Register::Code)reg);
        *dest = Imm32(offset);
    }
    Address toAddress() const {
        MOZ_ASSERT(tag == MEM);
        return Address(Register::FromCode((Register::Code)reg), offset);
    }
    int32_t disp() const {
        MOZ_ASSERT(tag == MEM);
        return offset;
    }

    int32_t base() const {
        MOZ_ASSERT(tag == MEM);
        return reg;
    }
    Register baseReg() const {
        MOZ_ASSERT(tag == MEM);
        return Register::FromCode((Register::Code)reg);
    }
};

inline Imm32
Imm64::firstHalf() const
{
    return hi(); // ENDIAN!
}

inline Imm32
Imm64::secondHalf() const
{
    return low(); // ENDIAN!
}

class Assembler;
typedef js::jit::AssemblerBuffer<1024, Instruction> PPCBuffer;

class PPCBufferWithExecutableCopy : public PPCBuffer
{
    static const int SliceSize = 1024;
  public:
    void executableCopy(uint8_t* buffer) {
        if (this->oom())
            return;

        for (Slice* cur = head; cur != nullptr; cur = cur->getNext()) {
            memcpy(buffer, &cur->instructions, cur->length());
            buffer += cur->length();
        }
    }

    bool appendRawCode(const uint8_t* code, size_t numBytes) {
        if (this->oom()) {
            return false;
        }
        while (numBytes > SliceSize) {
            this->putBytes(SliceSize, code);
            numBytes -= SliceSize;
            code += SliceSize;
        }
        this->putBytes(numBytes, code);
        return !this->oom();
    }
};

class Assembler : public AssemblerShared
{
  public:
    enum TrapTag { // FreeBSD and others may use r1 in their trap word, so don't allow bit 0 or > 15.
        BTag = 2,
        BCTag = 4,
        CallTag = 6,
        DebugTag0 = 10,
        DebugTag1 = 12,
        DebugTag2 = 14
    };

    enum BranchBits {
        BranchOnClear = 0x04,
        BranchOnSet = 0x0c,
        BranchOptionMask = 0x0f,
        BranchOptionInvert = 0x08 // XOR with this to invert the sense of a Condition
    };

    enum Condition {
        // Bit flag for unsigned comparisons (remember that you have to
        // choose the type of comparison at the compare step, not the
        // branch). We just mask this bit off, but the MacroAsssembler
        // may use it as a flag. This is a synthetic code.
        ConditionUnsigned   = 0x100,        // Computation only
        ConditionUnsignedHandled = 0x2ff,	// Mask off bit 8 but not 9 or 0-7

        // Bit flag for zero-relative Conditions. These are treated as
        // equivalent conditions relative to 0, but the MacroAssembler and
        // CodeGenerator may use this bit to reason about the intent of
        // generated instructions. This is a synthetic code.
        ConditionZero       = 0x400,        // Computation only

        // Bit flag for XER-only codes. We need to have XER in the CR using
        // mcrxrx or an equivalent first, but we don't need to check any CR
        // bits otherwise. This is a synthetic code. We use the 32-bit flags.
        ConditionOnlyXER    = 0x200,        // Computation only
        ConditionXERCA      = 0x23c,        // CA32 same as SO bit
        ConditionXERNCA     = 0x234,
        ConditionXEROV      = 0x21c,        // OV32 same as GT bit

    	// These are off pp370-1 in OPPCC. The top nybble is the offset
    	// to the CR field (the x in BIF*4+x), and the bottom is the BO.
    	// Synthetic condition flags sit in the MSB.
        Equal = 0x2c,
        NotEqual = 0x24,
        GreaterThan = 0x1c,
        GreaterThanOrEqual = 0x04, 
        LessThan = 0x0c,
        LessThanOrEqual = 0x14,

        Above = GreaterThan | ConditionUnsigned,
        AboveOrEqual = GreaterThanOrEqual | ConditionUnsigned,
        Below = LessThan | ConditionUnsigned,
        BelowOrEqual = LessThanOrEqual | ConditionUnsigned,

        Signed = LessThan | ConditionZero,
        // Don't mention negative zero to me. Don't wanna hear it. Nope.
        NotSigned = GreaterThanOrEqual | ConditionZero,
        Zero = Equal | ConditionZero,
        NonZero = NotEqual | ConditionZero,

        Overflow = ConditionXEROV,
        CarrySet = ConditionXERCA,
        CarryClear = ConditionXERNCA,

        Always = 0x1f,
        
        // This is specific to the SO bits in the CR, not the general overflow
        // condition in the way Ion conceives of it.
        SOBit = 0x3c,
        NSOBit = 0x34
    };

    enum DoubleCondition {
        DoubleConditionUnordered = 0x100,    // Computation only. This is also synthetic.
        
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleOrdered = 0x34,
        DoubleEqual = 0x2c,
        DoubleNotEqual = 0x24,
        DoubleGreaterThan = 0x1c,
        DoubleGreaterThanOrEqual = 0x04,
        DoubleLessThan = 0x0c,
        DoubleLessThanOrEqual = 0x14,
        // If either operand is NaN, these conditions always evaluate to true.
        // Except for DoubleUnordered, synthetic condition flags sit in the MSB
        // and are masked off by us but may be used by the MacroAssembler.
        DoubleUnordered = 0x3c,
        DoubleEqualOrUnordered = DoubleEqual | DoubleConditionUnordered,
        DoubleNotEqualOrUnordered = DoubleNotEqual | DoubleConditionUnordered,
        DoubleGreaterThanOrUnordered = DoubleGreaterThan | DoubleConditionUnordered,
        DoubleGreaterThanOrEqualOrUnordered = DoubleGreaterThanOrEqual | DoubleConditionUnordered,
        DoubleLessThanOrUnordered = DoubleLessThan | DoubleConditionUnordered,
        DoubleLessThanOrEqualOrUnordered = DoubleLessThanOrEqual | DoubleConditionUnordered,
    };
    
    enum JumpOrCall { BranchIsJump, BranchIsCall };

    enum LinkBit {
    	DontLinkB = 0,
    	LinkB = 1,
    };
    
    enum LikelyBit {
    	NotLikelyB = 0,
    	LikelyB = 1,
    };

	enum BranchAddressType {
		RelativeBranch = 0,
		AbsoluteBranch = 2,
	};
	
    BufferOffset nextOffset() {
        return m_buffer.nextOffset();
    }

    enum FloatFormat { SingleFloat, DoubleFloat };
    enum FloatTestKind { TestForTrue, TestForFalse };

  protected:
    Instruction * editSrc (BufferOffset bo) {
        return m_buffer.getInst(bo);
    }
  public:
    uint32_t actualOffset(uint32_t) const;
    uint32_t actualIndex(uint32_t) const;
    static uint8_t *PatchableJumpAddress(JitCode *code, uint32_t index);
    static uint64_t ExtractLoad64Value(Instruction *inst);
    static void     UpdateLoad64Value(Instruction *inst0, uint64_t value);
    static void     WriteLoad64Instructions(Instruction* inst0, Register reg,
                                            uint64_t value);
  protected:

    // structure for fixing up pc-relative loads/jumps when a the machine code
    // gets moved (executable copy, gc, etc.)
    struct RelativePatch
    {
        // the offset within the code buffer where the value is loaded that
        // we want to fix-up
        BufferOffset offset;
        void *target;
        RelocationKind kind;

        RelativePatch(BufferOffset offset, void *target, RelocationKind kind)
          : offset(offset),
            target(target),
            kind(kind)
        { }
    };

    //js::Vector<CodeLabel, 0, SystemAllocPolicy> codeLabels_;
    js::Vector<RelativePatch, 8, SystemAllocPolicy> jumps_;
    //js::Vector<uint32_t, 8, SystemAllocPolicy> longJumps_;

    CompactBufferWriter jumpRelocations_;
    CompactBufferWriter dataRelocations_;
    CompactBufferWriter relocations_;
    CompactBufferWriter preBarriers_;

    PPCBufferWithExecutableCopy m_buffer;

    private:
        char const * nGPR(Register rreg)
        {
        	uint32_t reg = rreg.code();
            MOZ_ASSERT(reg <= 31);
            //MOZ_ASSERT(reg >= 0);
            static char const *names[] = {
                "r0",  "sp",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
                "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
                "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
                "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
            };
            return names[reg];
        }

        char const * nFPR(FloatRegister freg)
        {
        	uint32_t reg = freg.code();
            MOZ_ASSERT(reg <= 31);
            //MOZ_ASSERT(reg >= 0);
            static char const *names[] = {
                "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
                "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
                "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
                "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
            };
            return names[reg];
        }
        
        char const * nCR(CRegisterID reg)
        {
            MOZ_ASSERT(reg <= 7);
            MOZ_ASSERT(reg >= 0);
            static char const *names[] = {
                "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7"
            };
            return names[reg];
        }

        char const * nSPR(SPRegisterID reg)
        {
            // XXX: we don't handle VRSAVE with this, but we don't use it yet.
            MOZ_ASSERT(reg >= 1);
            MOZ_ASSERT(reg <= 9);
            static char const *names[] = {
                "", "xer", "", "", "", "", "", "", "lr", "ctr"
            };
            return names[reg];
        }

	public: // used by MacroAssembler
        // Which absolute bit number does a condition register + Condition pair
        // refer to?
        static uint8_t crBit(CRegisterID cr, Condition cond)
        {
            return (cr << 2) + ((cond & 0xf0) >> 4);
        }
        
        static uint8_t crBit(CRegisterID cr, DoubleCondition cond)
        {
            return (cr << 2) + ((cond & 0xf0) >> 4);
        }

  public:
    Assembler()
      : m_buffer(),
        isFinished(false)
    { }

    void setUnlimitedBuffer() { m_buffer.setUnlimited(); }
    static Condition InvertCondition(Condition cond);
    static DoubleCondition InvertCondition(DoubleCondition cond);

    // MacroAssemblers hold onto gcthings, so they are traced by the GC.
    void trace(JSTracer *trc);
    void writeRelocation(BufferOffset src) {
        jumpRelocations_.writeUnsigned(src.getOffset());
    }

#if(1)
    // As opposed to the x86/x64 version, the data relocation must be executed
    // beforehand to recover the pointer, not after.
    void writeDataRelocation(ImmGCPtr ptr) {
        if (ptr.value) {
        	if (gc::IsInsideNursery(ptr.value))
        		embedsNurseryPointers_ = true;
            dataRelocations_.writeUnsigned(nextOffset().getOffset());
        }
    }
#endif

    void writePrebarrierOffset(CodeOffset label) {
        preBarriers_.writeUnsigned(label.offset());
    }

  public:
    static uintptr_t GetPointer(uint8_t *);

    bool oom() const;

    void setPrinter(Sprinter *sp) {
    }

    static const Register getStackPointer() {
        // This is stupid.
        return StackPointer;
    }
	
  private:
    bool isFinished;
  public:
#if defined(DEBUG)
	void spew_with_address(const char *fmt, uint32_t ins, ...);
#endif
    void finish();
    bool appendRawCode(const uint8_t* code, size_t numBytes);
    void executableCopy(void *buffer);
    void copyJumpRelocationTable(uint8_t *dest);
    void copyDataRelocationTable(uint8_t *dest);
    void copyPreBarrierTable(uint8_t *dest);

/*
    size_t numCodeLabels() const {
        return codeLabels_.length();
    }
    CodeLabel codeLabel(size_t i) {
        return codeLabels_[i];
    }
*/

    // Size of the instruction stream, in bytes.
    size_t size() const;
    // Size of the jump relocation table, in bytes.
    size_t jumpRelocationTableBytes() const;
    size_t dataRelocationTableBytes() const;
    size_t preBarrierTableBytes() const;

    // Size of the data table, in bytes.
    size_t bytesNeeded() const;

    // Write a blob of binary into the instruction stream *or*
    // into a destination address. If dest is nullptr (the default), then the
    // instruction gets written into the instruction stream. If dest is not null
    // it is interpreted as a pointer to the location that we want the
    // instruction to be written.
    BufferOffset writeInst(uint32_t x, uint32_t *dest = nullptr);
    // A static variant for the cases where we don't want to have an assembler
    // object at all. Normally, you would use the dummy (nullptr) object.
    static void WriteInstStatic(uint32_t x, uint32_t *dest);

  public:
    BufferOffset align(int alignment, bool useTrap = false);

    BufferOffset as_nop();
    BufferOffset as_eieio();
    BufferOffset as_isync();
    BufferOffset xs_lwsync();
    BufferOffset as_sync();

    // Branch and jump instructions.
    uint16_t computeConditionCode(Condition op, CRegisterID cr = cr0);
    uint16_t computeConditionCode(DoubleCondition cond, CRegisterID cr = cr0);
    BufferOffset as_b(JOffImm26 off, BranchAddressType bat = RelativeBranch, LinkBit lb = DontLinkB);
    BufferOffset as_b(int32_t off, BranchAddressType bat = RelativeBranch, LinkBit lb = DontLinkB); // stubs into the above
    BufferOffset as_blr(LinkBit lb = DontLinkB);
    BufferOffset as_bctr(LinkBit lb = DontLinkB);
    
    // Conditional branches.
    BufferOffset as_bc(BOffImm16 off, Condition cond, CRegisterID cr = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bc(int16_t off, Condition cond, CRegisterID cr = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bc(BOffImm16 off, DoubleCondition cond, CRegisterID = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bc(int16_t off, DoubleCondition cond, CRegisterID = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bcctr(Condition cond, CRegisterID cr = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bcctr(DoubleCondition cond, CRegisterID cr = cr0, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    
    BufferOffset as_bc(int16_t off, uint16_t op, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
    BufferOffset as_bcctr(uint16_t op, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);


	// SPR operations.
	BufferOffset as_mtspr(SPRegisterID spr, Register ra);
	BufferOffset as_mfspr(Register rd, SPRegisterID spr);
	
	// CR operations.
#define DEF_CRCR(op) BufferOffset as_##op(uint8_t t, uint8_t a, uint8_t b);

        DEF_CRCR(crand)
        DEF_CRCR(crandc)
        DEF_CRCR(cror)
        DEF_CRCR(crorc)
        DEF_CRCR(crxor)
#undef DEF_CRCR
	BufferOffset as_mtcrf(uint32_t mask, Register rs);
	BufferOffset as_mfcr(Register rd);
	BufferOffset as_mfocrf(Register rd, CRegisterID crfs);
	BufferOffset as_mcrxrx(CRegisterID crt);
	
	// GPR operations and load-stores.
	BufferOffset as_neg(Register rd, Register rs);
	BufferOffset as_nego(Register rd, Register rs);
	
	BufferOffset as_cmpd(CRegisterID cr, Register ra, Register rb);
	BufferOffset as_cmpdi(CRegisterID cr, Register ra, int16_t im);
	BufferOffset as_cmpld(CRegisterID cr, Register ra, Register rb);
	BufferOffset as_cmpldi(CRegisterID cr, Register ra, int16_t im);
	BufferOffset as_cmpw(CRegisterID cr, Register ra, Register rb);
	BufferOffset as_cmpwi(CRegisterID cr, Register ra, int16_t im);
	BufferOffset as_cmplw(CRegisterID cr, Register ra, Register rb);
	BufferOffset as_cmplwi(CRegisterID cr, Register ra, int16_t im);
	BufferOffset as_cmpd(Register ra, Register rb); // all implied cr0
	BufferOffset as_cmpdi(Register ra, int16_t im);
	BufferOffset as_cmpld(Register ra, Register rb);
	BufferOffset as_cmpldi(Register ra, int16_t im);
	BufferOffset as_cmpw(Register ra, Register rb);
	BufferOffset as_cmpwi(Register ra, int16_t im);
	BufferOffset as_cmplw(Register ra, Register rb);
	BufferOffset as_cmplwi(Register ra, int16_t im);
	
	BufferOffset as_srawi(Register id, Register rs, uint8_t n);
	
	BufferOffset as_rldcl(Register ra, Register rs, Register rb, uint8_t mb);
	BufferOffset as_rldcl_rc(Register ra, Register rs, Register rb, uint8_t mb);
	BufferOffset as_rldicl(Register ra, Register rs, uint8_t sh, uint8_t mb);
	BufferOffset as_rldicl_rc(Register ra, Register rs, uint8_t sh, uint8_t mb);
	BufferOffset as_rldicr(Register ra, Register rs, uint8_t sh, uint8_t mb);
	BufferOffset as_rldicr_rc(Register ra, Register rs, uint8_t sh, uint8_t mb);
	BufferOffset as_rlwinm(Register rd, Register rs, uint8_t sh, uint8_t mb, uint8_t me);
	BufferOffset as_rlwinm_rc(Register rd, Register rs, uint8_t sh, uint8_t mb, uint8_t me);
	BufferOffset as_rlwimi(Register rd, Register rs, uint8_t sh, uint8_t mb, uint8_t me); // cracked on G5
	BufferOffset as_rldimi(Register rd, Register rs, uint8_t sh, uint8_t mb);
	BufferOffset as_rlwnm(Register rd, Register rs, Register rb, uint8_t mb, uint8_t me);
	BufferOffset as_sradi(Register rd, Register rs, int n);
	
#define DEF_ALU2(op) BufferOffset as_##op(Register rd, Register ra, Register rb); \
                     BufferOffset as_##op##_rc(Register rd, Register ra, Register rb);         
        DEF_ALU2(add)
        DEF_ALU2(addc)
        DEF_ALU2(adde)
        DEF_ALU2(addo)
        DEF_ALU2(subf)
        DEF_ALU2(subfc)
        DEF_ALU2(subfe)
        DEF_ALU2(subfo)
        DEF_ALU2(divd)
        DEF_ALU2(divdo)
        DEF_ALU2(divdu)
        DEF_ALU2(divduo)
        DEF_ALU2(divw)
        DEF_ALU2(divwo)
        DEF_ALU2(divwu)
        DEF_ALU2(divwuo)
        DEF_ALU2(mulld)
        DEF_ALU2(mulhd)
        DEF_ALU2(mulhdu)
        DEF_ALU2(mulldo)
        DEF_ALU2(mullw)
        DEF_ALU2(mulhw)
        DEF_ALU2(mulhwu)
        DEF_ALU2(mullwo)
        DEF_ALU2(eqv) // NB: Implemented differently.
#undef DEF_ALU2

#define DEF_ALU2_NORC(op) BufferOffset as_##op(Register rt, Register ra, Register rb);
        DEF_ALU2_NORC(modsd)
        DEF_ALU2_NORC(modud)
        DEF_ALU2_NORC(modsw)
        DEF_ALU2_NORC(moduw)
#undef DEF_ALU2_NORC

// Special handling due to mscdfr0 (no _rc)
BufferOffset as_addi(Register rd, Register ra, int16_t im, bool actually_li = false);
BufferOffset as_addis(Register rd, Register ra, int16_t im, bool actually_lis = false);

#define DEF_ALUI(op) BufferOffset as_##op(Register rd, Register ra, int16_t im); \
                     BufferOffset as_##op##_rc(Register rd, Register ra, int16_t im);
        DEF_ALUI(addic)
        // NB: mulli is usually strength-reduced, since it can take up to five
        // cycles in the worst case. See xs_sr_mulli.
        DEF_ALUI(mulli)
        DEF_ALUI(subfic)
#undef DEF_ALUI

#define DEF_ALUE(op) BufferOffset as_##op(Register rd, Register ra); \
                     BufferOffset as_##op##_rc(Register rd, Register ra);
        DEF_ALUE(addme)
        DEF_ALUE(addze)
        DEF_ALUE(subfze)
        DEF_ALUE(cntlzw) // NB: In this case, rd = ra and ra = rs, but no biggie here.
        DEF_ALUE(cntlzd) // NB: In this case, rd = ra and ra = rs, but no biggie here.
        DEF_ALUE(cnttzd) // NB: In this case, rd = ra and ra = rs, but no biggie here.
        DEF_ALUE(cnttzw) // NB: In this case, rd = ra and ra = rs, but no biggie here.

        BufferOffset as_popcntd(Register ra, Register rs);
        BufferOffset as_popcntw(Register ra, Register rs);
#undef DEF_ALUE

#define DEF_BITALU2(op) BufferOffset as_##op(Register rd, Register rs, Register rb); \
                        BufferOffset as_##op##_rc(Register rd, Register rs, Register rb);
        DEF_BITALU2(andc)
        DEF_BITALU2(nand)
        DEF_BITALU2(nor)
        DEF_BITALU2(slw)
        DEF_BITALU2(srw)
        DEF_BITALU2(sraw)
        DEF_BITALU2(sld)
        DEF_BITALU2(srd)
        DEF_BITALU2(srad)
        DEF_BITALU2(and) // NB: See terminal _ constants above. This will have and_ and and__rc.
        DEF_BITALU2(or)
        DEF_BITALU2(xor)
#undef DEF_BITALU2

#define DEF_BITALUI(op) BufferOffset as_##op(Register rd, Register ra, uint16_t im);
        // There is no Rc form for these instructions.
        DEF_BITALUI(ori)
        DEF_BITALUI(oris)
        DEF_BITALUI(xori)
        DEF_BITALUI(xoris)
        // There is no Rc-less version of andi/andis.
        DEF_BITALUI(andi_rc)
        DEF_BITALUI(andis_rc)
#undef DEF_BITALUI
        
#define DEF_ALUEXT(op) BufferOffset as_##op(Register rd, Register rs); \
                       BufferOffset as_##op##_rc(Register rd, Register rs);
        DEF_ALUEXT(extsb)
        DEF_ALUEXT(extsh)
        DEF_ALUEXT(extsw)
#undef DEF_ALUEXT

#define DEF_MEMd(op) BufferOffset as_##op(Register rd, Register rb, int16_t off);
        DEF_MEMd(lbz)
        DEF_MEMd(lha)
        DEF_MEMd(lhz)
        DEF_MEMd(lwa)
        DEF_MEMd(lwz)
        DEF_MEMd(ld)

        DEF_MEMd(stb)
        DEF_MEMd(stw)
        DEF_MEMd(stwu)
        DEF_MEMd(sth)
        DEF_MEMd(std)
        DEF_MEMd(stdu)
#undef DEF_MEMd

#define DEF_MEMx(op) BufferOffset as_##op(Register rd, Register ra, Register rb);
        DEF_MEMx(lbzx)
        DEF_MEMx(lhax)
        DEF_MEMx(lhzx)
        DEF_MEMx(lhbrx)
        DEF_MEMx(lwzx)
        DEF_MEMx(lwbrx)
        DEF_MEMx(lwarx)
        DEF_MEMx(ldx)
        DEF_MEMx(ldarx)

        DEF_MEMx(stbx)
        DEF_MEMx(stwx)
        DEF_MEMx(stwux)
        DEF_MEMx(stwbrx)
        DEF_MEMx(sthx)
        DEF_MEMx(sthbrx)
        DEF_MEMx(stdx)
        DEF_MEMx(stdcx)
        DEF_MEMx(stdux)
        DEF_MEMx(stwcx)
#undef DEF_MEMx

    BufferOffset as_isel(Register rt, Register ra, Register rb, uint16_t rc, CRegisterID cr = cr0);
    BufferOffset as_isel0(Register rt, Register ra, Register rb, uint16_t rc, CRegisterID cr = cr0);

    // FPR operations and load-stores.
    BufferOffset as_fcmpo(CRegisterID cr, FloatRegister ra, FloatRegister rb);
    BufferOffset as_fcmpo(FloatRegister ra, FloatRegister rb); // implied cr0
    BufferOffset as_fcmpu(CRegisterID cr, FloatRegister ra, FloatRegister rb);
    BufferOffset as_fcmpu(FloatRegister ra, FloatRegister rb); // implied cr0
#define DEF_FPUAC(op) BufferOffset as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rc); \
                      BufferOffset as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rc);
        DEF_FPUAC(fmul)
        DEF_FPUAC(fmuls)
#undef DEF_FPUAC

#define DEF_FPUAB(op) BufferOffset as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rc); \
                      BufferOffset as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rc);
        DEF_FPUAB(fadd)
        DEF_FPUAB(fdiv)
        DEF_FPUAB(fsub)
        DEF_FPUAB(fadds)
        DEF_FPUAB(fdivs)
        DEF_FPUAB(fsubs)
        DEF_FPUAB(fcpsgn)
        DEF_FPUAB(fmrgew)
#undef DEF_FPUAB

#define DEF_FPUDS(op) BufferOffset as_##op(FloatRegister rd, FloatRegister rs); \
                      BufferOffset as_##op##_rc(FloatRegister rd, FloatRegister rs);
        DEF_FPUDS(fabs)
        DEF_FPUDS(fneg)
        DEF_FPUDS(fmr)
        DEF_FPUDS(fcfid)
        DEF_FPUDS(fcfids)
        DEF_FPUDS(fcfidu)
        DEF_FPUDS(fcfidus)
        DEF_FPUDS(fctid)
        DEF_FPUDS(fctidz)
        DEF_FPUDS(fctidu)
        DEF_FPUDS(fctiduz)
        DEF_FPUDS(fctiw)
        DEF_FPUDS(fctiwz)
        DEF_FPUDS(fctiwu)
        DEF_FPUDS(fctiwuz)
        DEF_FPUDS(frim)
        DEF_FPUDS(frin)
        DEF_FPUDS(frip)
        DEF_FPUDS(friz)
        DEF_FPUDS(frsp)
        DEF_FPUDS(frsqrte)
        DEF_FPUDS(fsqrt)
        DEF_FPUDS(fsqrts)
#undef DEF_FPUDS

// In Ion, the semantics for this macro are now corrected compared to JM/PPCBC. 
// (See OPPCC p.432, etc.)
#define DEF_FPUABC(op) BufferOffset as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rc, FloatRegister rb); \
                       BufferOffset as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rc, FloatRegister rb);
        DEF_FPUABC(fmadd)
        DEF_FPUABC(fnmsub)
        DEF_FPUABC(fsel)
#undef DEF_FPUABC

#define DEF_FMEMd(op) BufferOffset as_##op(FloatRegister rd, Register rb, int16_t off);
        DEF_FMEMd(lfd)
        DEF_FMEMd(lfs)
        DEF_FMEMd(stfd)
        DEF_FMEMd(stfs)
        DEF_FMEMd(stfdu)
        DEF_FMEMd(stfsu)
#undef DEF_FMEMd

#define DEF_FMEMx(op) BufferOffset as_##op(FloatRegister rd, Register ra, Register rb);
        DEF_FMEMx(lfdx)
        DEF_FMEMx(lfsx)
        DEF_FMEMx(lfiwax)
        DEF_FMEMx(stfiwx)
        DEF_FMEMx(stfdx)
        DEF_FMEMx(stfsx)
#undef DEF_FMEMx

// convert SPRid to 10-bit split encoding (OPPCC appendix A, p.514)
#define PPC_SPR(x) (((int)x>>5) | ((int)x & 31)<<5)

	BufferOffset as_mtfsb0(uint8_t bt);
	BufferOffset as_mtfsb1(uint8_t bt);
	BufferOffset as_mtfsfi(uint8_t fi, uint8_t imm);
	BufferOffset as_mcrf(CRegisterID bt, CRegisterID bs);
	BufferOffset as_mcrfs(CRegisterID bf, uint8_t bfa);

// VSX
// Currently supported only for FPRs.
    BufferOffset as_mfvsrd(Register ra, FloatRegister xs);
    BufferOffset as_mtvsrd(FloatRegister xs, Register ra);
    BufferOffset as_mtvsrwz(FloatRegister xs, Register ra);
    BufferOffset as_mtvsrws(FloatRegister xs, Register ra);
    BufferOffset as_xxbrd(FloatRegister xt, FloatRegister xb);
    BufferOffset as_xscvdpsp(FloatRegister xt, FloatRegister xb);
    BufferOffset as_xscvspdp(FloatRegister xt, FloatRegister xb);
    BufferOffset as_xscvdpspn(FloatRegister xt, FloatRegister xb);
    BufferOffset as_xscvspdpn(FloatRegister xt, FloatRegister xb);
    BufferOffset as_xxlxor(FloatRegister xt, FloatRegister xa, FloatRegister xb);

        BufferOffset as_addpcis(Register rt, uint16_t im = 0);

	// Conveniences and generally accepted alternate mnemonics.
// XXX: change these to xs_ and remove ones we don't actually use
	BufferOffset xs_trap();
	BufferOffset xs_trap_tagged(TrapTag tag); // Codegen for marking traps in output.
	BufferOffset xs_mr(Register rd, Register ra);
        BufferOffset xs_bcl_always(int16_t off, LikelyBit lkb = NotLikelyB);
	BufferOffset x_beq(CRegisterID cr, int16_t off, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
	BufferOffset x_bne(CRegisterID cr, int16_t off, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
	BufferOffset xs_bdnz(int16_t off, LikelyBit lkb = NotLikelyB, LinkBit lb = DontLinkB);
	BufferOffset xs_mtctr(Register ra);
	BufferOffset xs_mtlr(Register ra);
	BufferOffset xs_mflr(Register rd);
	BufferOffset xs_mtcr(Register rs);
	BufferOffset xs_mfxer(Register ra);
	BufferOffset xs_mtxer(Register ra);
	BufferOffset x_insertbits0_15(Register rd, Register rs);
	BufferOffset x_bit_value(Register rd, Register rs, unsigned bit);
	BufferOffset x_slwi(Register rd, Register rs, int n);
	BufferOffset x_sldi(Register rd, Register rs, int n);
	BufferOffset x_srwi(Register rd, Register rs, int n);
	BufferOffset x_srdi(Register rd, Register rs, int n);
	BufferOffset x_subi(Register rd, Register ra, int16_t im);
	BufferOffset xs_sr_mulli(Register rd, Register ra, int16_t im);

	BufferOffset x_not(Register rd, Register ra);

	// Large loads.	
	BufferOffset xs_li(Register rd, int16_t im);
	BufferOffset xs_lis(Register rd, int16_t im);

	// Traps
	BufferOffset as_tw(uint8_t to, Register ra, Register rb);
	BufferOffset as_twi(uint8_t to, Register ra, int16_t si);
        BufferOffset as_stop();

    // Label operations.
    void bind(InstImm* inst, uintptr_t branch, uintptr_t target, bool bound = false);
    void bind(Label* label) { bind(label, nextOffset()); }
    void bind(Label* label, BufferOffset boff);
    void bind(CodeLabel *label) { label->target()->bind(currentOffset()); }
    uint32_t currentOffset() {
        return nextOffset().getOffset();
    }
    void retarget(Label *label, Label *target);
    static void Bind(uint8_t *rawCode, const CodeLabel& label);

/* XXX: Remove bindS*, those are TenFourFox-specific and we aren't going to implement them */
    // Fast fixed branches.
#define SHORT_LABEL(w) BufferOffset w = nextOffset()
#define SHORT_LABEL_MASM(w) BufferOffset w = masm.nextOffset()
    // Binds instruction i to offset s as a fixed short branch.
    void bindS(BufferOffset s, BufferOffset i);
    void bindSS(BufferOffset i); // s is implied as the current offset.

    // See Bind.
    size_t labelOffsetToPatchOffset(size_t offset) {
        return actualOffset(offset);
    }

    void call(Label *label);
    void call(void *target);

    static void TraceJumpRelocations(JSTracer *trc, JitCode *code, CompactBufferReader &reader);
    static void TraceDataRelocations(JSTracer *trc, JitCode *code, CompactBufferReader &reader);
    /*
    static void FixupNurseryObjects(JSContext* cx, JitCode* code, CompactBufferReader& reader,
                                    const ObjectVector& nurseryObjects);
    */
    void assertNoGCThings() const {
#ifdef DEBUG
        MOZ_ASSERT(dataRelocations_.length() == 0);
/* XXX: This can only be uint32.
        for (auto& j : longJumps_) {
            MOZ_ASSERT(j.kind == RelocationKind::HARDCODED);
        }
*/
        for (auto& j : jumps_) {
            MOZ_ASSERT(j.kind == RelocationKind::HARDCODED);
        }
#endif
    }

    static bool SupportsFloatingPoint() {
        // BaselineInterpreter is not enabled unless this is true, and nothing
        // else is enabled if BaselineInterpreter isn't enabled, which yields
        // a convenient way to disable the JIT at runtime if needed. (We also
        // added code to disable irregexp JIT gated here as well.) Currently
        // the JIT is only supported on ISA 3.0 and up (POWER9 and up) in
        // little-endian mode. You can decide the manner in which you want the
        // JIT handled:
#if(0)
        // Manually: unless you turn the JIT off, it's on. If you're writing
        // support for a presently unsupported Power ISA CPU or big-endian
        // mode, then change if(0) to if(1) and grab onto your ankles.
        return true;
#elif defined(__POWER9_VECTOR__) && defined(__LITTLE_ENDIAN__)
        // Statically at compile time: if gcc is given --mcpu=power9, and it
        // was compiled on a little-endian host, then the resulting binary will
        // only work properly on ISA 3.0+ and thus the JIT must work too. All
        // ISA checks become static as well, so it's also slightly faster.
        return true;
#elif defined(__LITTLE_ENDIAN__)
        // Dynamically at runtime: ask the operating system. If AltiVec, VSX
        // and VSX3 are all supported, then this must be a supported CPU. When
        // additional CPU support is added, adjust the check here.
        return HasPPCISA3();
#else
        // Otherwise, statically disable on all unsupported or big-endian CPUs.
        return false;
#endif
    }
    static bool SupportsSimd() { return false; } // todo

    // Technically unaligned integer loads are supported in hardware, but
    // there is a non-zero penalty (though small on many implementations),
    // and certain unlikely edge cases can potentially fault to the operating
    // system.
    static bool SupportsUnalignedAccesses() { return true; }
    static bool SupportsFastUnalignedAccesses() { return false; }

    // However, 64-bit FP loads invariably fault to the operating system
    // when unaligned, so we really want to avoid that.
    static bool SupportsFastUnalignedFPAccesses() { return false; }

    static bool HasRoundInstruction(RoundingMode mode) {
      switch (mode) {
        case RoundingMode::Up:
        case RoundingMode::Down:
        case RoundingMode::TowardsZero:
            return true;

        // See note in MacroAssembler::nearbyIntDouble.
        case RoundingMode::NearestTiesToEven:
            return false;

        // fall through
        }
        MOZ_CRASH("unexpected mode");
    }

  protected:
    void bind(Instruction *inst, uint32_t branch, uint32_t target);
    void addPendingJump(BufferOffset src, ImmPtr target, RelocationKind kind) {
        enoughMemory_ &= jumps_.append(RelativePatch(src, target.value, kind));
        if (kind == RelocationKind::JITCODE)
            writeRelocation(src);
    }

    void addLongJump(BufferOffset src, BufferOffset dst) {
        //enoughMemory_ &= longJumps_.append(src.getOffset());
        CodeLabel cl;
        cl.patchAt()->bind(src.getOffset());
        cl.target()->bind(dst.getOffset());
        cl.setLinkMode(CodeLabel::JumpImmediate);
        addCodeLabel(std::move(cl));
    }

  public:
  /*
    size_t numLongJumps() const {
        return longJumps_.length();
    }
    uint32_t longJump(size_t i) {
        return longJumps_[i];
    }
  */

    void comment(const char *msg) {
    }
    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8_t *buffer);

    void flushBuffer() {
    }

    BufferOffset haltingAlign(int alignment);
    BufferOffset nopAlign(int alignment);

    static uint32_t PatchWrite_NearCallSize();
    static uint32_t NopSize() { return 4; }

    static uint32_t ExtractLisOriValue(Instruction *inst0, Instruction *inst1);
    static void UpdateLisOriValue(Instruction *inst0, Instruction *inst1, uint32_t value);
    static void WriteLisOriInstructions(Instruction *inst, Instruction *inst1,
                                        Register reg, uint32_t value);

    static void PatchWrite_NearCall(CodeLocationLabel start, CodeLocationLabel toCall);
    static void PatchDataWithValueCheck(CodeLocationLabel label, PatchedImmPtr newValue,
                                        PatchedImmPtr expectedValue);
    static void PatchDataWithValueCheck(CodeLocationLabel label, ImmPtr newValue,
                                        ImmPtr expectedValue);
    static void PatchWrite_Imm32(CodeLocationLabel label, Imm32 imm);

    static void PatchInstructionImmediate(uint8_t *code, PatchedImmPtr imm);

    static uint8_t *NextInstruction(uint8_t *instruction, uint32_t *count = nullptr);

    static void ToggleToJmp(CodeLocationLabel inst_);
    static void ToggleToCmp(CodeLocationLabel inst_);

    static void ToggleCall(CodeLocationLabel inst_, bool enabled);

    static void UpdateBoundsCheck(uint32_t logHeapSize, Instruction *inst);
    void processCodeLabels(uint8_t *rawCode);
    static int32_t ExtractCodeLabelOffset(uint8_t *code);

    void verifyHeapAccessDisassembly(uint32_t begin, uint32_t end,
                                     const Disassembler::HeapAccess& heapAccess)
    {
        // Implement this if we implement a disassembler.
    }

    bool swapBuffer(wasm::Bytes& bytes);
    bool reserve(size_t size) { return !oom(); }
}; // Assembler

static const uint32_t OpcodeShift = 26;
static const uint32_t OpcodeBits = 6;

class Instruction
{
  protected:
    uint32_t data;

  public:
    Instruction (uint32_t data_) : data(data_) { }
    Instruction (PPCOpcodes op_) : data((uint32_t)op_) { }

    uint32_t encode() const {
        return data;
    }

	// Quickies
    void makeOp_nop() {
        data = PPC_nop;
    }
    void makeOp_mtctr(Register r) {
    	data = PPC_mtspr | ((uint32_t)r.code())<<21 | PPC_SPR(ctr)<<11 ;
    }
    void makeOp_bctr(Assembler::LinkBit l = Assembler::DontLinkB) {
    	data = PPC_bctr | l;
    }

    void setData(uint32_t data) {
        this->data = data;
    }

    const Instruction & operator=(const Instruction &src) {
        data = src.data;
        return *this;
    }

    uint32_t extractOpcode() {
        return (data & PPC_MAJOR_OPCODE_MASK); // ">> 26"
    }
    bool isOpcode(uint32_t op) {
    	return (extractOpcode() == (op & PPC_MAJOR_OPCODE_MASK));
    }
  	void assertOpcode(uint32_t op) {
		MOZ_ASSERT(isOpcode(op));
	}

    // Get the next instruction in the instruction stream.
    // This will do neat things like ignore constant pools and their guards,
    // if we ever implement those again.
    Instruction *next() { return this + 1; };

    // Sometimes the backend wants a uint32_t (or a pointer to it) rather than
    // an instruction.  raw() just coerces this into a pointer to a uint32_t.
    const uint32_t *raw() const { return &data; }
    uint32_t size() const { return 4; }
}; // Instruction

class InstReg : public Instruction
{
  // Valid for reg/reg/reg instructions only.
  // XXX: Assert that at some point.
  public:
  	InstReg (PPCOpcodes op) : Instruction(op) { }
  	InstReg (PPCOpcodes op, Register rd, Register ra, Register rb)
  	    : Instruction(op | ((uint32_t)rd.code() << 21) |
  	            ((uint32_t)ra.code() << 16) | (uint32_t)rb.code() << 11) {}
  	     
  	
    void setBOffImm16(BOffImm16 off) {
    	data = (data & 0xFFFF0000) | off.encode();
    }
    void setImm16(Imm16 off) {
    	data = (data & 0xFFFF0000) | off.encode();
    }
    uint32_t extractImm16Value() {
    	return (data & 0x0000FFFF);
    }
    void setTargetReg(Register rt) {
    	data = (data & 0xFC1FFFFF) | ((uint32_t)rt.code() << 21);
    }
    void setUpperReg(Register ru) {
    	// Mask out upper register field and put in the new one.
    	// For addi/addis, this is the DESTINATION.
    	// For ori/oris, this is the SOURCE.
    	// For bc, this is BO.
    	data = (data & 0xFFE0FFFF) | ((uint32_t)ru.code() << 16);
    }
    void setLowerReg(Register rl) {
    	// Mask out lower register field and put in the new one.
    	// For addi/addis, this is the SOURCE. (For li/lis, this should be ZERO.)
    	// For ori/oris, this is the DESTINATION.
    	// For bc, this is BI.
    	data = (data & 0xFFFF07FF) | ((uint32_t)rl.code() << 11);
    }
}; // InstReg

class InstImm : public Instruction
{
  // Valid for reg/reg/imm instructions only and bc.
  // XXX: Assert that at some point.
  public:
  	InstImm (PPCOpcodes op) : Instruction(op) { }
  	InstImm (PPCOpcodes op, Register ra, Register rs, Imm16 im)
  	    : Instruction(op | ((uint32_t)ra.code() << 21) |
  	            ((uint32_t)rs.code() << 16) | im.encode()) {}

    void setBOffImm16(BOffImm16 off) {
    	data = (data & 0xFFFF0000) | off.encode();
    }
    void setImm16(Imm16 off) {
    	data = (data & 0xFFFF0000) | off.encode();
    }
    uint32_t extractImm16Value() {
    	return (data & 0x0000FFFF);
    }
    void setUpperReg(Register ru) {
    	// Mask out upper register field and put in the new one.
    	// For addi/addis, this is the DESTINATION.
    	// For ori/oris, this is the SOURCE.
    	// For bc, this is BO.
    	data = (data & 0xFC1FFFFF) | ((uint32_t)ru.code() << 21);
    }
    void setLowerReg(Register rl) {
    	// Mask out lower register field and put in the new one.
    	// For addi/addis, this is the SOURCE. (For li/lis, this should be ZERO.)
    	// For ori/oris, this is the DESTINATION.
    	// For bc, this is BI.
    	data = (data & 0xFFE0FFFF) | ((uint32_t)rl.code() << 16);
    }
    Assembler::TrapTag traptag();
}; // InstImm

// If this assert is not satisfied, we can't use Instruction to patch in-place.
static_assert(sizeof(Instruction) == 4, "sizeof(Instruction) must be 32-bit word.");

static const uint32_t NumIntArgRegs = 8;

static inline bool
GetIntArgReg(uint32_t usedArgSlots, Register *out)
{
    if (usedArgSlots < NumIntArgRegs) {
    	// Argregs start at r3!
        *out = Register::FromCode((Register::Code)(3 + usedArgSlots));
        return true;
    }
    return false;
}

// Get a register in which we plan to put a quantity that will be used as an
// integer argument. Because we have so many argument registers on PowerPC, this
// essentially stubs into GetIntArgReg because failure is incredibly improbable.
static inline bool
GetTempRegForIntArg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
    // NB: this implementation can't properly determine yet which regs are used if there are
    // float arguments.
    MOZ_ASSERT(usedFloatArgs == 0);

    if (GetIntArgReg(usedIntArgs, out))
        return true;
    return false;
}

static inline uint32_t
GetArgStackDisp(uint32_t usedArgSlots)
{
#if(0)
    // NYI. This situation should never occur.
    MOZ_ASSERT(usedArgSlots >= NumIntArgRegs);
    return usedArgSlots * sizeof(intptr_t);
#else
    MOZ_CRASH("unexpected spill to stack");
    return 0;
#endif
}

inline bool IsUnaligned(const wasm::MemoryAccessDesc& access) {
  if (!access.align()) {
    return false;
  }

  return access.align() < access.byteSize();
}

} // namespace jit
} // namespace js

// Convenience macros from JM/PPCBC.

// whether a (Trusted)Imm32 fits in an unsigned immediate value
#define PPC_IMM_OK_U(x) (MOZ_LIKELY(((x).m_value & 0xffff0000) == 0))

// whether a (Trusted)Imm32 fits in a signed immediate value
#define PPC_IMM_OK_S(x) (MOZ_LIKELY(((x).m_value & 0xffff8000) == 0 || \
    ((x).m_value & 0xffff8000) == 0xffff8000))

// whether the offset part of an Address fits in a (signed) immediate value
#define PPC_OFFS_OK(x) (MOZ_LIKELY(((x).offset & 0xffff8000) == 0 || \
    ((x).offset & 0xffff8000) == 0xffff8000))

// same test, but checking a bit ahead (for multiple loads)
#define PPC_OFFS_INCR_OK(x, incr) (MOZ_LIKELY((((x).offset + incr) & 0xffff8000) == 0 || \
    (((x).offset + incr) & 0xffff8000) == 0xffff8000))

#endif /* jit_ppc_Assembler_ppc_h */
