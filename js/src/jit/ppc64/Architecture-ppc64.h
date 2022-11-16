/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc_Architecture_ppc_h
#define jit_ppc_Architecture_ppc_h

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/shared/Architecture-shared.h"

#include "js/Utility.h"

namespace js {
namespace jit {

// Not used on PPC.
static const uint32_t ShadowStackSpace = 0;
// The return address is in LR, not in memory/stack.
static const uint32_t SizeOfReturnAddressAfterCall = 0u;

// Size of each bailout table entry.
// For PowerPC this is a single bl.
static const uint32_t BAILOUT_TABLE_ENTRY_SIZE = sizeof(void *);

// Range of an immediate jump (26 bit jumps). Take a fudge out in case.
static constexpr uint32_t JumpImmediateRange = (32 * 1024 * 1024) - 32;

// GPRs.
class Registers
{
  public:
    enum RegisterID {
        r0 = 0,
        tempRegister = r0,
        r1,
        sp = r1,
        stackPointerRegister = r1,
        r2,
        r3,
        r4,
        r5,
        r6,
        r7,
        r8,
        r9,
        r10,
        r11,
        r12,
        addressTempRegister = r12,
        r13,
        r14,
        r15,
        r16,
        r17,
        r18,
        r19,
        r20,
        r21,
        r22,
        r23,
        r24,
        r25,
        r26,
        r27,
        r28,
        r29,
        r30,
        r31,
        invalid_reg
    };

    typedef uint8_t Code;
    typedef uint32_t Encoding;
    typedef uint32_t SetType;
    
    // Content spilled during bailouts.
    union RegisterContent {
    	uintptr_t r;
    };

    static const char *GetName(Code code) {
        static const char *Names[] = {
             "r0",  "sp",  "toc", "r3",  "r4",  "r5",  "r6",  "r7",
             "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
             "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
             "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"};
        return Names[code];
    }
    static const char *GetName(uint32_t i) {
        MOZ_ASSERT(i < Total);
        return GetName(Code(i));
    }

    static Code FromName(const char *name);

    static const Encoding StackPointer = sp;
    static const Encoding Invalid = invalid_reg;

    // XXX: Currently Safepoints restricts us to a uint32_t-sized non-FPR
    // mask, so we can't work SPRs into this yet.
    static const uint32_t Total = 32;
    static const uint32_t Allocatable = 24;

    static const SetType AllMask = 0xffffffff;
    static const SetType ArgRegMask =
        (1 << Registers::r3) |
        (1 << Registers::r4) |
        (1 << Registers::r5) |
        (1 << Registers::r6) |
        (1 << Registers::r7) |
        (1 << Registers::r8) |
        (1 << Registers::r9) |
        (1 << Registers::r10);

    // We use this constant to save registers when entering functions.
    // Don't bother saving r0, r1, r11 or r12.
    static const SetType VolatileMask = ArgRegMask;

    static const SetType NonVolatileMask = (
    	(1 << Registers::r2)  |
        (1 << Registers::r13) |
        (1 << Registers::r14) |
        (1 << Registers::r15) |
        (1 << Registers::r16) |
        (1 << Registers::r17) |
        (1 << Registers::r18) |
        (1 << Registers::r19) |
        (1 << Registers::r20) |
        (1 << Registers::r21) |
        (1 << Registers::r22) |
        (1 << Registers::r23) |
        (1 << Registers::r24) |
        (1 << Registers::r25) |
        (1 << Registers::r26) |
        (1 << Registers::r27) |
        (1 << Registers::r28) |
        (1 << Registers::r29) |
        (1 << Registers::r30) |
        (1 << Registers::r31)
    // Watch out for sign extension if this is ever 64-bit!
        ) & AllMask;

    // Also uses r11.
    static const SetType WrapperMask = VolatileMask;

    static const SetType NonAllocatableMask =
        // Used by assembler.
        (1 << Registers::r0)  |
        (1 << Registers::sp)  |
        (1 << Registers::r2)  |
        // Temp registers.
        (1 << Registers::r11) |
        (1 << Registers::r12) |
        // r13 is the pointer for TLS in ELF v2.
        (1 << Registers::r13) |
        // Non-volatile work registers.
        (1 << Registers::r16);
        // r17 is the InterpreterPCReg and must be allocatable.
        // r18 is the WasmTlsReg and must be allocatable.
        // Despite its use as a rectifier, r19 must be allocatable (see
        // ICCallScriptedCompiler::generateStubCode).

    // Registers that can be allocated without being saved, generally.
    static const SetType TempMask = VolatileMask & ~NonAllocatableMask;

    // Registers returned from a JS -> JS call.
    static const SetType JSCallMask =
        (1 << Registers::r5);

    // Registers returned from a JS -> C call.
    static const SetType CallMask =
        (1 << Registers::r3);
 
    static const SetType AllocatableMask = (
    	// Be explicit
        (1 << Registers::r3)  |
        (1 << Registers::r4)  |
        (1 << Registers::r5)  |
        (1 << Registers::r6)  |
        (1 << Registers::r7)  |
        (1 << Registers::r8)  |
        (1 << Registers::r9)  |
        (1 << Registers::r10) |
        (1 << Registers::r14) |
        (1 << Registers::r15) |
        (1 << Registers::r17) |
        (1 << Registers::r18) |
        (1 << Registers::r19) |
        (1 << Registers::r20) |
        (1 << Registers::r21) |
        (1 << Registers::r22) |
        (1 << Registers::r23) |
        (1 << Registers::r24) |
        //(1 << Registers::r25) |
        (1 << Registers::r26) |
        (1 << Registers::r27) |
        (1 << Registers::r28) |
        (1 << Registers::r29) |
        (1 << Registers::r30) |
        (1 << Registers::r31)
        // Watch out for sign extension!
        ) & AllMask;

    static uint32_t SetSize(SetType x) {
        // XXX: see above
        static_assert(sizeof(SetType) == 4, "SetType must be 32 bits");
        return mozilla::CountPopulation32(x);
    }
    static uint32_t FirstBit(SetType x) {
        return mozilla::CountTrailingZeroes32(x);
    }
    static uint32_t LastBit(SetType x) {
        // XXX: see above (31 would be 63 if we used uint64_t)
        return 31 - mozilla::CountLeadingZeroes32(x);
    }
};

// Smallest integer type that can hold a register bitmask. (GPRs.)
// Safepoints asserts this is 32-bit or smaller.
typedef uint32_t PackedRegisterMask;

// FPRs.
// PowerPC FPRs can be both double and single precision, like MIPS. We tell
// Ion there are 64 FPRs, but each is an aliased pair.
class FloatRegisters
{
  public:
    enum FPRegisterID {
        f0 = 0,
        f1,
        f2,
        f3,
        f4,
        f5,
        f6,
        f7,
        f8,
        f9,
        f10,
        f11,
        f12,
        f13,
        f14,
        f15,
        f16,
        f17,
        f18,
        f19,
        f20,
        f21,
        f22,
        f23,
        f24,
        f25,
        f26,
        f27,
        f28,
        f29,
        f30,
        f31,
        invalid_freg
    };
    static_assert(f31 == 31);
  // Eight bits: (invalid << 7) | (kind << 5) | encoding
  typedef uint8_t Code;
  typedef FPRegisterID Encoding;
  typedef uint64_t SetType;

    // Make clear that the base type is Double; Single is a veneer upon it. XXX: VSX
  enum Kind : uint8_t {
    Double,
    Single,
    NumTypes
  };

  // Content spilled during bailouts.
  union RegisterContent {
    float s;
    double d;
  };
    static const Code Invalid = 0x80;

   static const char *GetName(Encoding code) {
        static const char * const Names[] = { "f0", "f1", "f2", "f3",  "f4", "f5",  "f6", "f7",
                                              "f8", "f9",  "f10", "f11", "f12", "f13",
                                              "f14", "f15", "f16", "f17", "f18", "f19",
                                              "f20", "f21", "f22", "f23", "f24", "f25",
                                              "f26", "f27", "f28", "f29", "f30", "f31"};
        MOZ_ASSERT(code < TotalPhys);
        return Names[code];
    }
    static const char *GetName(Code i) {
        MOZ_ASSERT(i < Total);
        MOZ_ASSERT(i != Invalid);
        return GetName(Encoding(i % TotalPhys));
    }

    static Code FromName(const char *name);


  static const uint32_t TotalPhys = 32;
  static const uint32_t NumScalarTypes = 2;
  static const uint32_t Total = TotalPhys * NumScalarTypes;
  static const uint32_t TotalWithSimd = TotalPhys * NumTypes;
  static const uint32_t Allocatable = 31;  // Without f0, the scratch register.

  static_assert(sizeof(SetType) * 8 >= Total,
                "SetType should be large enough to enumerate all registers.");

  static const SetType SpreadSingle = SetType(1)
                                      << (uint32_t(Single) * TotalPhys);
  static const SetType SpreadDouble = SetType(1)
                                      << (uint32_t(Double) * TotalPhys);
  static const SetType Spread = SpreadSingle | SpreadDouble;

  static const SetType AllPhysMask = (SetType(1) << TotalPhys) - 1;
  static const SetType AllMask = AllPhysMask * Spread;
  static const SetType AllDoubleMask = AllPhysMask * SpreadDouble;
  static const SetType AllSingleMask = AllPhysMask * SpreadSingle;
  static const SetType NoneMask = SetType(0);

  // f0 is the ScratchFloatReg.
  static const SetType VolatileMask =
      SetType((1 << FloatRegisters::f1)  | (1 << FloatRegisters::f2)  |
              (1 << FloatRegisters::f3)  | (1 << FloatRegisters::f4)  |
              (1 << FloatRegisters::f5)  | (1 << FloatRegisters::f6)  |
              (1 << FloatRegisters::f7)  | (1 << FloatRegisters::f8)  |
              (1 << FloatRegisters::f9)  | (1 << FloatRegisters::f10) |
              (1 << FloatRegisters::f11) | (1 << FloatRegisters::f12) |
              (1 << FloatRegisters::f13)) *
      Spread;

  static const SetType NonVolatileMask = AllMask & ~VolatileMask;

  static const SetType WrapperMask = VolatileMask;

  // d31 is the ScratchFloatReg.
  static const SetType NonAllocatableMask =
      (SetType(1) << FloatRegisters::f0) * Spread;

  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;

  static constexpr Encoding encoding(Code c) {
    // assert() not available in constexpr function.
    // assert(c < TotalWithSimd);
    return Encoding(c & 31);
  }

  static constexpr Kind kind(Code c) {
    // assert() not available in constexpr function.
    // assert(c < TotalWithSimd && ((c >> 5) & 3) < NumTypes);
    return Kind((c >> 5) & 3);
  }

  static constexpr Code fromParts(uint32_t encoding, uint32_t kind,
                                  uint32_t invalid) {
    return Code((invalid << 7) | (kind << 5) | encoding);
  }
};

template <typename T>
class TypedRegisterSet;

// Shamelessly rip off aargh, um, aarch64
struct FloatRegister {
  typedef FloatRegisters Codes;
  typedef Codes::Code Code;
  typedef Codes::Encoding Encoding;
  typedef Codes::SetType SetType;
  typedef Codes::Kind Kind;

  static uint32_t SetSize(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType must be 64 bits");
    x |= x >> FloatRegisters::TotalPhys;
    x &= FloatRegisters::AllPhysMask;
    return mozilla::CountPopulation32(x);
  }

  static uint32_t FirstBit(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType");
    return mozilla::CountTrailingZeroes64(x);
  }
  static uint32_t LastBit(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType");
    return 63 - mozilla::CountLeadingZeroes64(x);
  }

  static constexpr size_t SizeOfSimd128 = 16;

 private:
  // These fields only hold valid values: an invalid register is always
  // represented as a valid encoding and kind with the invalid_ bit set.
  uint8_t encoding_;  // 32 encodings
  uint8_t kind_;      // Double, Single, Simd128
  bool invalid_;

 public:
  constexpr FloatRegister(Encoding encoding, Kind kind = FloatRegisters::Double)
      : encoding_(encoding), kind_(kind), invalid_(false) {
    // assert(uint32_t(encoding) < Codes::TotalPhys);
  }
  constexpr FloatRegister(uint32_t r, Kind kind = FloatRegisters::Double)
      : encoding_(Codes::Encoding(r)), kind_(kind), invalid_(false) {}
  constexpr FloatRegister()
      : encoding_(0), kind_(FloatRegisters::Double), invalid_(true) {}

  static FloatRegister FromCode(uint32_t i) {
    MOZ_ASSERT(i < Codes::TotalWithSimd);
    return FloatRegister(FloatRegisters::encoding(i), FloatRegisters::kind(i));
  }

  bool isSingle() const {
    MOZ_ASSERT(!invalid_);
    return kind_ == FloatRegisters::Single;
  }
  bool isDouble() const {
    MOZ_ASSERT(!invalid_);
    return kind_ == FloatRegisters::Double;
  }
  bool isSimd128() const {
    MOZ_ASSERT(!invalid_);
    return false;
  }
  bool isInvalid() const { return invalid_; }

  FloatRegister asSingle() const {
    MOZ_ASSERT(!invalid_);
    return FloatRegister(Encoding(encoding_), FloatRegisters::Single);
  }
  FloatRegister asDouble() const {
    MOZ_ASSERT(!invalid_);
    return FloatRegister(Encoding(encoding_), FloatRegisters::Double);
  }
  FloatRegister asSimd128() const {
    MOZ_CRASH("No SIMD support");
  }

  constexpr uint32_t size() const {
    MOZ_ASSERT(!invalid_);
    if (kind_ == FloatRegisters::Double) {
      return sizeof(double);
    }
    if (kind_ == FloatRegisters::Single) {
      return sizeof(float); // not used in stack calculations, apparently
    }
    MOZ_CRASH("No SIMD support");
  }

  constexpr Code code() const {
    // assert(!invalid_);
    return Codes::fromParts(encoding_, kind_, invalid_);
  }

  constexpr Encoding encoding() const {
    MOZ_ASSERT(!invalid_);
    return Encoding(encoding_);
  }

  const char* name() const { return FloatRegisters::GetName(code()); }
  bool volatile_() const {
    MOZ_ASSERT(!invalid_);
    return !!((SetType(1) << code()) & FloatRegisters::VolatileMask);
  }
  constexpr bool operator!=(FloatRegister other) const {
    return code() != other.code();
  }
  constexpr bool operator==(FloatRegister other) const {
    return code() == other.code();
  }

  bool aliases(FloatRegister other) const {
    return other.encoding_ == encoding_;
  }
  // All spades are groovy. -- Firesign Theatre
  bool equiv(FloatRegister other) const {
    MOZ_ASSERT(!invalid_);
    return kind_ == other.kind_;
  }

  // numAliased is used only by Ion's register allocator, ergo we ignore SIMD
  // registers here as Ion will not be exposed to SIMD on this platform.
  uint32_t numAliased() const { return Codes::NumScalarTypes; }
  uint32_t numAlignedAliased() { return numAliased(); }

  FloatRegister aliased(uint32_t aliasIdx) {
    MOZ_ASSERT(!invalid_);
    MOZ_ASSERT(aliasIdx < numAliased());
    return FloatRegister(Encoding(encoding_),
                         Kind((aliasIdx + kind_) % numAliased()));
  }
  FloatRegister alignedAliased(uint32_t aliasIdx) {
    MOZ_ASSERT(aliasIdx < numAliased());
    return aliased(aliasIdx);
  }
  SetType alignedOrDominatedAliasedSet() const {
    return Codes::Spread << encoding_;
  }

  static constexpr RegTypeName DefaultType = RegTypeName::Float64;

  template <RegTypeName Name = DefaultType>
  static SetType LiveAsIndexableSet(SetType s) {
    return SetType(0);
  }

  template <RegTypeName Name = DefaultType>
  static SetType AllocatableAsIndexableSet(SetType s) {
    static_assert(Name != RegTypeName::Any, "Allocatable set are not iterable");
    return LiveAsIndexableSet<Name>(s);
  }

  static TypedRegisterSet<FloatRegister> ReduceSetForPush(
      const TypedRegisterSet<FloatRegister>& s);
  static uint32_t GetPushSizeInBytes(const TypedRegisterSet<FloatRegister>& s);
  uint32_t getRegisterDumpOffsetInBytes();
};

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Float32>(SetType set) {
  return set & FloatRegisters::AllSingleMask;
}

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Float64>(SetType set) {
  return set & FloatRegisters::AllDoubleMask;
}

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Any>(SetType set) {
  return set;
}

inline bool hasUnaliasedDouble() { return false; }
inline bool hasMultiAlias() { return false; }

// XXX: This needs to be rolled into somewhere else

// SPRs (PPC backend specific).
// These have no peer in lesser chips. That is because PPC has no peer in
// lesser chips. These don't count against the register cap because the
// allocator is unaware of them. In fact, we don't treat these as regular
// registers at all (hey, they're Special Purpose anyway).
#if(0)
class SPRs // Class definition not yet supported.
{
  public:
#endif
    enum SPRegisterID {
      xer = 1,
      lr_spr = 8, /* I'm in the REAL witness protection program. */
      ctr = 9,
      vrsave = 256, /* for future SIMD JS */
      invalid_spreg
    };
#if(0)
    static const char *getSPRName(SPRegisterID code) {
#define XXX "INVALID"
        static const char *N_vrsave = "vrsave";
        static const char *N_bogus = XXX;
        static const char *Names[] = {
            XXX, "xer", XXX, XXX, XXX, XXX, XXX, XXX,
            "lr","ctr"
        };
#undef XXX
        return 
               (code == vrsave) ? N_vrsave :
               (code >  ctr)    ? N_bogus :
               Names[code];
    }
};
#endif

// CRs (PPC backend specific).
// We have eight condition registers, each for how unconditionally wonderful
// PowerPC is, and sometimes for storing condition results.
// Assume CR0 as default.
#if(0)
class CRs // Class definition not yet supported.
{
  public:
#endif
    enum CRegisterID {
      cr0 = 0,
      cr1,
/*
Suppress CR2-CR4 because these are non-volatile.
      cr2,
      cr3,
      cr4,
*/
      cr5 = 5,
      cr6,
      cr7,
      invalid_creg
    };
#if(0)
    static const char *getCRName(CRegisterID code) {
        static const char *Names[] = {
            "cr0",  "cr1",  "cr2",  "cr3",  "cr4",  "cr5",  "cr6",  "cr7"
        };
        return Names[code];
    }
}
#endif

inline uint32_t GetPPC64Flags() { return 0; /* XXX? */ }

// Statically true if --mcpu=power9.
#if defined(__POWER9_VECTOR__) && defined(__LITTLE_ENDIAN__)
inline bool HasPPCISA3() { return true; }
#else
bool HasPPCISA3();
#endif

} // namespace jit
} // namespace js

#endif /* jit_ppc_Architecture_ppc_h */
