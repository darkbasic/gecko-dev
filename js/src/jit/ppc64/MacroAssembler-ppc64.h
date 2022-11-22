/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc64_shared_MacroAssembler_ppc64_shared_h
#define jit_ppc64_shared_MacroAssembler_ppc64_shared_h

#include "jit/AtomicOp.h"
#include "jit/JitFrames.h"
#include "jit/MoveResolver.h"
#include "jit/ppc64/Assembler-ppc64.h"
#include "vm/BytecodeUtil.h"


namespace js {
namespace jit {

enum LoadStoreSize
{
    SizeByte = 8,
    SizeHalfWord = 16,
    SizeWord = 32,
    SizeDouble = 64
};

enum LoadStoreExtension
{
    ZeroExtend = 0,
    SignExtend = 1
};

enum JumpKind
{
    LongJump = 0,
    ShortJump = 1
};

enum LiFlags
{
    Li64 = 0,
    Li48 = 1,
};

struct ImmShiftedTag : public ImmWord
{
    explicit ImmShiftedTag(JSValueShiftedTag shtag)
      : ImmWord((uintptr_t)shtag)
    { }

    explicit ImmShiftedTag(JSValueType type)
      : ImmWord(uintptr_t(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type))))
    { }
};

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32_t(mask))
    { }
};

static const int defaultShift = 3;
static_assert(1 << defaultShift == sizeof(JS::Value), "The defaultShift is wrong");

// See documentation for ScratchTagScope and ScratchTagScopeRelease in
// MacroAssembler-x64.h.
class ScratchTagScope : public SecondScratchRegisterScope
{
  public:
    ScratchTagScope(MacroAssembler& masm, const ValueOperand&)
      : SecondScratchRegisterScope(masm)
    {}
};

class ScratchTagScopeRelease
{
    ScratchTagScope* ts_;
  public:
    explicit ScratchTagScopeRelease(ScratchTagScope* ts) : ts_(ts) {
        ts_->release();
    }
    ~ScratchTagScopeRelease() {
        ts_->reacquire();
    }
};

static Register CallReg = r12; // XXX

class MacroAssemblerPPC64 : public Assembler
{
  protected:
    // Perform a downcast.
    MacroAssembler& asMasm();
    const MacroAssembler& asMasm() const;

/*
    Condition ma_cmp(Register rd, Register lhs, Register rhs, Condition c);
    Condition ma_cmp(Register rd, Register lhs, Imm32 imm, Condition c);
*/
    void compareFloatingPoint(FloatRegister lhs, FloatRegister rhs,
                              DoubleCondition c);

/* XXX: TO DO: ma_d* variants are probably superfluous */
  public:
    void ma_move(Register rd, Register rs);

    void ma_li(Register dest, ImmGCPtr ptr);
    void ma_li(Register dest, Imm32 imm);
    void ma_li(Register dest, Imm64 imm);
    void ma_liPatchable(Register dest, Imm32 imm);

    void ma_li(Register dest, CodeLabel* label);
    void ma_li(Register dest, ImmWord imm);
    void ma_li(Register dest, int64_t imm);
    void ma_liPatchable(Register dest, ImmPtr imm);
    void ma_liPatchable(Register dest, ImmWord imm);

    // Shift operations
    void ma_sll(Register rd, Register rt, Imm32 shift);
    void ma_srl(Register rd, Register rt, Imm32 shift);
    void ma_sra(Register rd, Register rt, Imm32 shift);

    void ma_sll(Register rd, Register rt, Register shift);

    void ma_dsll(Register rd, Register rt, Imm32 shift);
    void ma_dsrl(Register rd, Register rt, Imm32 shift);
    void ma_dsra(Register rd, Register rt, Imm32 shift);
    void ma_dror(Register rd, Register rt, Imm32 shift);
    void ma_drol(Register rd, Register rt, Imm32 shift);

    void ma_dsll(Register rd, Register rt, Register shift);
    void ma_dsrl(Register rd, Register rt, Register shift);
    void ma_dsra(Register rd, Register rt, Register shift);

    // Negate
    void ma_negu(Register rd, Register rs);
    void ma_dnegu(Register rd, Register rs);

    void ma_not(Register rd, Register rs);

    // Bit extract/insert
    void ma_ext(Register rt, Register rs, uint16_t pos, uint16_t size);
    void ma_ins(Register rt, Register rs, uint16_t pos, uint16_t size);

    // and
    void ma_and(Register rd, Register rs);
    void ma_and(Register rd, Imm32 imm);
    void ma_and(Register rd, Register rs, Imm32 imm);

    // or
    void ma_or(Register rd, Register rs);
    void ma_or(Register rd, Imm32 imm);
    void ma_or(Register rd, Register rs, Imm32 imm);

    // xor
    void ma_xor(Register rd, Register rs);
    void ma_xor(Register rd, Imm32 imm);
    void ma_xor(Register rd, Register rs, Imm32 imm);

    void ma_ctz(Register rd, Register rs);

    // load
    uint32_t ma_load(Register dest, const BaseIndex& src, LoadStoreSize size = SizeWord,
                 LoadStoreExtension extension = SignExtend);
    void ma_load_unaligned(const wasm::MemoryAccessDesc& access, Register dest, const BaseIndex& src, Register temp,
                           LoadStoreSize size, LoadStoreExtension extension);

    // store
    uint32_t ma_store(Register data, const BaseIndex& dest, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);
    uint32_t ma_store(Imm32 imm, const BaseIndex& dest, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);
    void ma_store_unaligned(Register data, const Address& dest,
                            LoadStoreSize size = SizeWord);
    void ma_store_unaligned(Register data, const BaseIndex& dest,
                            LoadStoreSize size = SizeWord);
    void ma_store_unaligned(const wasm::MemoryAccessDesc& access, Register data, const BaseIndex& dest, Register temp,
                            LoadStoreSize size, LoadStoreExtension extension);

    // arithmetic based ops
    // add
    void ma_add(Register rd, Register rs, Imm32 imm);
    void ma_add(Register rd, Register rs);
    void ma_add(Register rd, Imm32 imm);
    void ma_addTestCarry(Condition cond, Register rd, Register rs, Register rt, Label* overflow, bool is32 = true);
    void ma_addTestCarry(Condition cond, Register rd, Register rs, ImmWord imm, Label* overflow, bool is32 = false);
    void ma_addTestCarry(Condition cond, Register rd, Register rs, Imm32 imm, Label* overflow, bool is32 = true);

    // subtract
    void ma_subu(Register rd, Register rs, Imm32 imm);
    void ma_subu(Register rd, Register rs);
    void ma_subu(Register rd, Imm32 imm);
    void ma_subTestOverflow(Register rd, Register rs, Imm32 imm, Label* overflow, bool is32 = true);

    // multiplies.  For now, there are only few that we care about.
    void ma_mul(Register rd, Register rs, Imm32 imm);
    void ma_mul_branch_overflow(Register rd, Register rs, Register rt, Label* overflow);
    void ma_mul_branch_overflow(Register rd, Register rs, Imm32 imm, Label* overflow);

    /*
    // divisions
    void ma_div_branch_overflow(Register rd, Register rs, Register rt, Label* overflow);
    void ma_div_branch_overflow(Register rd, Register rs, Imm32 imm, Label* overflow);

    // fast mod, uses scratch registers, and thus needs to be in the assembler
    // implicitly assumes that we can overwrite dest at the beginning of the sequence
    void ma_mod_mask(Register src, Register dest, Register hold, Register remain,
                     int32_t shift, Label* negZero = nullptr);
    */

    // branches when done from within platform-specific code
    void ma_bc(Condition c, Label* l, JumpKind jumpKind = LongJump);
    template <typename T>
    void ma_bc(CRegisterID cr, T c, Label* l, JumpKind jumpKind = LongJump);

    void ma_bc(Register lhs, Register rhs, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Register lhs, Imm32 imm, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Register lhs, ImmPtr imm, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Register lhs, ImmGCPtr imm, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        MOZ_ASSERT(lhs != ScratchRegister);
        ma_li(ScratchRegister, imm);
        ma_bc(lhs, ScratchRegister, l, c, jumpKind);
    }
    // Explicit compare width
    void ma_bc32(Register lhs, Register rhs, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc64(Register lhs, Imm32 imm, Label* l, Condition c, JumpKind jumpKind = LongJump);

    void ma_b(Label* l, JumpKind jumpKind = LongJump);

    // fp instructions
    void ma_lis(FloatRegister dest, float value);

    void ma_sd(FloatRegister src, BaseIndex address);
    void ma_ss(FloatRegister src, BaseIndex address);

    void ma_ld(FloatRegister dest, const BaseIndex& src);
    void ma_ls(FloatRegister dest, const BaseIndex& src);

    // FP branches
    void ma_bc(DoubleCondition c, FloatRegister lhs, FloatRegister rhs, Label* label,
                 JumpKind jumpKind = LongJump);
    void ma_bc(DoubleCondition c, Label* label, JumpKind jumpKind = LongJump);
    //void ma_bc(FloatRegister lhs, FloatRegister rhs, Label* label,
    //             ConditionRegister cr,
    //             DoubleCondition c, JumpKind jumpKind = LongJump);

    void ma_call(ImmPtr dest);

    void ma_spectre_isel(Condition cond, Register lhs, Register rhs);

    void ma_jump(ImmPtr dest);

    void ma_cmp64(Register lhs, Imm64 rhs, Condition c);
    void ma_cmp32(Register lhs, Imm32 rhs, Condition c);
    void ma_cmp32(Register lhs, Register rhs, Condition c);
    void ma_cmp32(Register lhs, const Address& rhs, Condition c);
    void ma_cmp_set(Register dest, Address lhs, Register rhs, Condition c, bool useCmpw = false);
    void ma_cmp_set(Register dest, Address lhs, Imm32 rhs, Condition c, bool useCmpw = false);
    void ma_cmp_set(Register dest, Address lhs, Imm64 rhs, Condition c, bool useCmpw = true);
    void ma_cmp_set(Register dst, Register lhs, Register rhs, Condition c, bool useCmpw = false);
    void ma_cmp_set(Register dst, Register lhs, Imm16 imm, Condition c, bool useCmpw = false);
    void ma_cmp_set(Register dst, Register lhs, Imm32 imm, Condition c, bool useCmpw = true) {
        MOZ_ASSERT(useCmpw);
        ma_cmp32(lhs, imm, c);
        ma_cmp_set_coda(dst, c);
    }
    void ma_cmp_set(Register dst, Register lhs, Imm64 imm, Condition c, bool useCmpw = false) {
        MOZ_ASSERT(!useCmpw);
        ma_cmp64(lhs, imm, c);
        ma_cmp_set_coda(dst, c);
    }
    void ma_cmp_set_coda(Register rd, Condition c);
    void ma_cmp_set_double(Register dst, FloatRegister lhs, FloatRegister rhs, DoubleCondition c);

    // Evaluate srcDest = minmax<isMax>{Float32,Double}(srcDest, other).
    // Handle NaN specially if handleNaN is true.
    void minMaxDouble(FloatRegister srcDest, FloatRegister other, bool handleNaN, bool isMax);
    void minMaxFloat32(FloatRegister srcDest, FloatRegister other, bool handleNaN, bool isMax);

    void loadDouble(const Address& addr, FloatRegister dest);
    void loadDouble(const BaseIndex& src, FloatRegister dest);

    // Load a float value into a register, then expand it to a double.
    void loadFloatAsDouble(const Address& addr, FloatRegister dest);
    void loadFloatAsDouble(const BaseIndex& src, FloatRegister dest);

    void loadFloat32(const Address& addr, FloatRegister dest);
    void loadFloat32(const BaseIndex& src, FloatRegister dest);

   void outOfLineWasmTruncateToInt32Check(FloatRegister input, Register output, MIRType fromType,
                                           TruncFlags flags, Label* rejoin,
                                           wasm::BytecodeOffset trapOffset);
    void outOfLineWasmTruncateToInt64Check(FloatRegister input, Register64 output, MIRType fromType,
                                           TruncFlags flags, Label* rejoin,
                                           wasm::BytecodeOffset trapOffset);

  protected:
    void wasmLoadImpl(const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
                      Register ptrScratch, AnyRegister output, Register tmp);
    void wasmStoreImpl(const wasm::MemoryAccessDesc& access, AnyRegister value, Register memoryBase,
                       Register ptr, Register ptrScratch, Register tmp);

  public:

    // Negate

    void ma_dins(Register rt, Register rs, Imm32 pos, Imm32 size);
    void ma_dext(Register rt, Register rs, Imm32 pos, Imm32 size);

    void ma_dctz(Register rd, Register rs);

    // load
    uint32_t ma_load(Register dest, Address address, LoadStoreSize size = SizeWord,
                 LoadStoreExtension extension = SignExtend);

    // store
    uint32_t ma_store(Register data, Address address, LoadStoreSize size = SizeWord,
                  LoadStoreExtension extension = SignExtend);

    // arithmetic based ops
    // add
    void ma_addTestOverflow(Register rd, Register rs, Register rt, Label* overflow, bool is32 = true);
    void ma_addTestOverflow(Register rd, Register rs, ImmWord imm, Label* overflow, bool is32 = false);
    void ma_addTestOverflow(Register rd, Register rs, Imm32 imm, Label* overflow, bool is32 = true);

    // neg
    void ma_negTestOverflow(Register rd, Label* overflow);

    // subtract
    void ma_dsubu(Register rd, Register rs, Imm32 imm);
    void ma_dsubu(Register rd, Register rs);
    void ma_dsubu(Register rd, Imm32 imm);
    void ma_subTestOverflow(Register rd, Register rs, Register rt, Label* overflow, bool is32 = true);

    // multiplies.  For now, there are only few that we care about.
    void ma_dmult(Register rs, Imm32 imm);

    // stack
    void ma_pop(Register r);
    void ma_push(Register r);

    // branches when done from within PPC64-specific code
    void ma_bc(Register lhs, ImmWord imm, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Register lhs, Address addr, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Address addr, Imm32 imm, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Address addr, ImmGCPtr imm, Label* l, Condition c, JumpKind jumpKind = LongJump);
    void ma_bc(Address addr, Register rhs, Label* l, Condition c, JumpKind jumpKind = LongJump) {
        MOZ_ASSERT(rhs != ScratchRegister);
        ma_load(ScratchRegister, addr, SizeDouble);
        ma_bc(ScratchRegister, rhs, l, c, jumpKind);
    }

    void ma_bal(Label* l);

    // fp instructions
    void ma_lid(FloatRegister dest, double value);

    void ma_mv(FloatRegister src, ValueOperand dest);
    void ma_mv(ValueOperand src, FloatRegister dest);

    void ma_ls(FloatRegister ft, Address address);
    void ma_ld(FloatRegister ft, Address address);
    void ma_sd(FloatRegister ft, Address address);
    void ma_ss(FloatRegister ft, Address address);

    void ma_pop(FloatRegister f);
    void ma_push(FloatRegister f);

    void ma_cmp_set(Register dst, Register lhs, ImmWord imm, Condition c, bool useCmpw = false);
    void ma_cmp_set(Register dst, Register lhs, ImmPtr imm, Condition c, bool useCmpw = false);
};

class MacroAssembler;

class MacroAssemblerPPC64Compat : public MacroAssemblerPPC64
{
  public:
    using MacroAssemblerPPC64::call;

    MacroAssemblerPPC64Compat()
    { }

    void convertBoolToInt32(Register source, Register dest);
    void convertInt32ToDouble(Register src, FloatRegister dest);
    void convertInt32ToDouble(const Address& src, FloatRegister dest);
    void convertInt32ToDouble(const BaseIndex& src, FloatRegister dest);
    void convertUInt32ToDouble(Register src, FloatRegister dest);
    void convertUInt32ToFloat32(Register src, FloatRegister dest);
    void convertDoubleToFloat32(FloatRegister src, FloatRegister dest);
    void convertDoubleToInt32(FloatRegister src, Register dest, Label* fail,
                              bool negativeZeroCheck = true);
    void convertDoubleToPtr(FloatRegister src, Register dest, Label* fail,
                            bool negativeZeroCheck = true);
    void convertFloat32ToInt32(FloatRegister src, Register dest, Label* fail,
                               bool negativeZeroCheck = true);

    void convertFloat32ToDouble(FloatRegister src, FloatRegister dest);
    void convertInt32ToFloat32(Register src, FloatRegister dest);
    void convertInt32ToFloat32(const Address& src, FloatRegister dest);

    void movq(Register rs, Register rd);

    void computeScaledAddress(const BaseIndex& address, Register dest);

    inline void computeEffectiveAddress(const Address& address, Register dest) {
        ma_add(dest, address.base, Imm32(address.offset));
    }

    inline void computeEffectiveAddress(const BaseIndex &address, Register dest) {
        computeScaledAddress(address, dest);
        if (address.offset) {
            ma_add(dest, dest, Imm32(address.offset));
        }
    }


    void j(Condition cond, Label* dest) {
        ma_b(dest);
    }

    void mov(Register src, Register dest) {
        if (dest != src)
            as_or(dest, src, src);
    }
    void mov(ImmWord imm, Register dest) {
        ma_li(dest, imm);
    }
    void mov(ImmPtr imm, Register dest) {
        mov(ImmWord(uintptr_t(imm.value)), dest);
    }
    void mov(CodeLabel* label, Register dest) {
        ma_li(dest, label);
    }
    void mov(Register src, Address dest) {
        MOZ_CRASH("NYI-IC");
    }
    void mov(Address src, Register dest) {
        MOZ_CRASH("NYI-IC");
    }

#if(0)
  // load: offset to the load instruction obtained by movePatchablePtr().
  void writeDataRelocation(ImmGCPtr ptr, BufferOffset load) {
    // Raw GC pointer relocations and Value relocations both end up in
    // Assembler::TraceDataRelocations.
    if (ptr.value) {
      if (gc::IsInsideNursery(ptr.value)) {
        embedsNurseryPointers_ = true;
      }
      dataRelocations_.writeUnsigned(load.getOffset());
    }
  }
  void writeDataRelocation(const Value& val, BufferOffset load) {
    // Raw GC pointer relocations and Value relocations both end up in
    // Assembler::TraceDataRelocations.
    if (val.isGCThing()) {
      gc::Cell* cell = val.toGCThing();
      if (cell && gc::IsInsideNursery(cell)) {
        embedsNurseryPointers_ = true;
      }
      dataRelocations_.writeUnsigned(load.getOffset());
    }
  }
#else
    void writeDataRelocation(const Value& val) {
        if (val.isGCThing()) {
            gc::Cell* cell = val.toGCThing();
            if (cell && gc::IsInsideNursery(cell))
                embedsNurseryPointers_ = true;
            dataRelocations_.writeUnsigned(currentOffset());
        }
    }
#endif

    void hop_skip_nop_jump() {
        // Common stanza at the end of these CTR branches.
        as_nop();
        as_nop();
        as_nop(); // branch slot
        as_bctr();
    }

    void jump(Label* label) {
        ma_b(label);
    }
    void jump(ImmPtr ptr) {
        m_buffer.ensureSpace(7 * sizeof(uint32_t));
        BufferOffset bo = m_buffer.nextOffset();
        addPendingJump(bo, ptr, RelocationKind::HARDCODED);
        ma_jump(ptr);
    }
    void jump(Register reg) {
        // This could be to any code, so definitely use r12.
        if (reg != SecondScratchReg)
            as_or(SecondScratchReg, reg, reg); // make r12 == CTR
        xs_mtctr(SecondScratchReg); // new dispatch group
        hop_skip_nop_jump();
    }
    void jump(const Address& address) {
        loadPtr(address, SecondScratchReg);
        xs_mtctr(SecondScratchReg); // new dispatch group
        as_nop();
        hop_skip_nop_jump();
    }
    void jump(TrampolinePtr code) {
        jump(ImmPtr(code.value));
    }
    void branch(JitCode* c) {
        // This is to Ion code, but we still use r12 anyway.
        BufferOffset bo = m_buffer.nextOffset();
        addPendingJump(bo, ImmPtr(c->raw()), RelocationKind::JITCODE);
        ma_liPatchable(SecondScratchReg, ImmPtr(c->raw()));
        xs_mtctr(SecondScratchReg); // new dispatch group
        // Keep the branch out of the same dispatch group.
        as_nop();
        hop_skip_nop_jump();
    }
    void jump(JitCode* code) {
        branch(code);
    }
    void branch(const Register reg) {
        jump(reg);
    }

    void nop() {
        as_nop();
    }
    void ret() {
        // Pop LR and return.
        ma_pop(ScratchRegister);
        xs_mtlr(ScratchRegister);
        as_blr();
    }
    inline void retn(Imm32 n);
    void push(Imm32 imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
    }
    void push(ImmWord imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
    }
    void push(ImmGCPtr imm) {
        ma_li(ScratchRegister, imm);
        ma_push(ScratchRegister);
    }
    void push(const Address& address) {
        loadPtr(address, ScratchRegister);
        ma_push(ScratchRegister);
    }
    void push(Register reg) {
        ma_push(reg);
    }
    void push(FloatRegister reg) {
        ma_push(reg);
    }
    void pop(Register reg) {
        ma_pop(reg);
    }
    void pop(FloatRegister reg) {
        ma_pop(reg);
    }

    CodeOffset toggledJump(Label* label);
    CodeOffset toggledCall(JitCode* target, bool enabled);
    static size_t ToggledCallSize(uint8_t* code) {
        // A full branch stanza plus the oris/b gate.
        return 8 * sizeof(uint32_t);
    }

    CodeOffset pushWithPatch(ImmWord imm) {
        CodeOffset offset = movWithPatch(imm, ScratchRegister);
        ma_push(ScratchRegister);
        return offset;
    }

    CodeOffset movWithPatch(ImmWord imm, Register dest) {
        CodeOffset offset = CodeOffset(currentOffset());
        ma_liPatchable(dest, imm);
        return offset;
    }
    CodeOffset movWithPatch(ImmPtr imm, Register dest) {
        CodeOffset offset = CodeOffset(currentOffset());
        ma_liPatchable(dest, imm);
        return offset;
    }

    void writeCodePointer(CodeLabel* label) {
        label->patchAt()->bind(currentOffset());
        label->setLinkMode(CodeLabel::RawPointer);
        m_buffer.ensureSpace(sizeof(void*));
        writeInst(-1);
        writeInst(-1);
    }

    void splitTag(Register src, Register dest) {
        x_srdi(dest, src, JSVAL_TAG_SHIFT);
    }

    void splitTag(const ValueOperand& operand, Register dest) {
        splitTag(operand.valueReg(), dest);
    }

    void splitTagForTest(const ValueOperand& value, ScratchTagScope& tag) {
        splitTag(value, tag);
    }

    // unboxing code
    void unboxNonDouble(const ValueOperand& operand, Register dest, JSValueType type) {
        unboxNonDouble(operand.valueReg(), dest, type);
    }

    template <typename T>
    void unboxNonDouble(T src, Register dest, JSValueType type) {
        MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
        if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
            load32(src, dest);
            return;
        }
        loadPtr(src, dest);
        unboxNonDouble(dest, dest, type);
    }

    void unboxNonDouble(Register src, Register dest, JSValueType type) {
        MOZ_ASSERT(type != JSVAL_TYPE_DOUBLE);
        if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
            // This has the effect of clearing bits 32-63 and the tag
            // with it, plus extending the sign, all in one operation.
            as_srawi(dest, src, 0);
            return;
        }
        // Blank out the tag.
        as_rldicl(dest, src, 0, 64-JSVAL_TAG_SHIFT); // "clrldi"
    }

    template <typename T>
    void unboxObjectOrNull(const T& src, Register dest) {
        unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
        // Assuming this assert is true, we've already cleared that bit
        // by clearing the tag. We could also do what ARM does.
        static_assert(JS::detail::ValueObjectOrNullBit ==
                     (uint64_t(0x8) << JSVAL_TAG_SHIFT));
    }

    void unboxGCThingForGCBarrier(const Address& src, Register dest) {
        loadPtr(src, dest);
        as_rldicl(dest, dest, 0, 64-JSVAL_TAG_SHIFT); // "clrldi"
    }
    void unboxGCThingForGCBarrier(const ValueOperand& src, Register dest) {
        as_rldicl(dest, src.valueReg(), 0, 64-JSVAL_TAG_SHIFT); // "clrldi"
    }

    void unboxInt32(const ValueOperand& operand, Register dest);
    void unboxInt32(Register src, Register dest);
    void unboxInt32(const Address& src, Register dest);
    void unboxInt32(const BaseIndex& src, Register dest);
    void unboxBoolean(const ValueOperand& operand, Register dest);
    void unboxBoolean(Register src, Register dest);
    void unboxBoolean(const Address& src, Register dest);
    void unboxBoolean(const BaseIndex& src, Register dest);
    void unboxDouble(const ValueOperand& operand, FloatRegister dest);
    void unboxDouble(Register src, Register dest);
    void unboxDouble(const Address& src, FloatRegister dest);
    void unboxDouble(const BaseIndex& src, FloatRegister dest);
    void unboxString(const ValueOperand& operand, Register dest);
    void unboxString(Register src, Register dest);
    void unboxString(const Address& src, Register dest);
    void unboxSymbol(const ValueOperand& src, Register dest);
    void unboxSymbol(Register src, Register dest);
    void unboxSymbol(const Address& src, Register dest);
    void unboxBigInt(const ValueOperand& operand, Register dest);
    void unboxBigInt(Register src, Register dest);
    void unboxBigInt(const Address& src, Register dest);
    void unboxObject(const ValueOperand& src, Register dest);
    void unboxObject(Register src, Register dest);
    void unboxObject(const Address& src, Register dest);
    void unboxObject(const BaseIndex& src, Register dest) { unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT); }
    void unboxValue(const ValueOperand& src, AnyRegister dest, JSValueType type);
    void unboxPrivate(const ValueOperand& src, Register dest);

    void notBoolean(const ValueOperand& val) {
        as_xori(val.valueReg(), val.valueReg(), 1);
    }

    // boxing code
    void boxDouble(FloatRegister src, const ValueOperand& dest, FloatRegister);
    void boxNonDouble(JSValueType type, Register src, const ValueOperand& dest);

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address& address, Register scratch);
    [[nodiscard]] Register extractObject(const ValueOperand& value, Register scratch) {
        unboxObject(value, scratch);
        return scratch;
    }
    [[nodiscard]] Register extractString(const ValueOperand& value, Register scratch) {
        unboxString(value, scratch);
        return scratch;
    }
    [[nodiscard]] Register extractSymbol(const ValueOperand& value, Register scratch) {
        unboxSymbol(value, scratch);
        return scratch;
    }
    [[nodiscard]] Register extractInt32(const ValueOperand& value, Register scratch) {
        unboxInt32(value, scratch);
        return scratch;
    }
    [[nodiscard]] Register extractBoolean(const ValueOperand& value, Register scratch) {
        unboxBoolean(value, scratch);
        return scratch;
    }
    Register extractTag(const Address& address, Register scratch);
    Register extractTag(const BaseIndex& address, Register scratch);
    Register extractTag(const ValueOperand& value, Register scratch) {
        MOZ_ASSERT(scratch != ScratchRegister);
        splitTag(value, scratch);
        return scratch;
    }

    void boolValueToDouble(const ValueOperand& operand, FloatRegister dest);
    void int32ValueToDouble(const ValueOperand& operand, FloatRegister dest);
    void loadInt32OrDouble(const Address& src, FloatRegister dest);
    void loadInt32OrDouble(const BaseIndex& addr, FloatRegister dest);
    void loadConstantDouble(double dp, FloatRegister dest);

    void boolValueToFloat32(const ValueOperand& operand, FloatRegister dest);
    void int32ValueToFloat32(const ValueOperand& operand, FloatRegister dest);
    void loadConstantFloat32(float f, FloatRegister dest);

    void testNullSet(Condition cond, const ValueOperand& value, Register dest);
    void testObjectSet(Condition cond, const ValueOperand& value, Register dest);
    void testBigIntSet(Condition cond, const ValueOperand& value, Register dest);
    void testNumberSet(Condition cond, const ValueOperand& value, Register dest);
    void testStringSet(Condition cond, const ValueOperand& value, Register dest);
    void testSymbolSet(Condition cond, const ValueOperand& value, Register dest);
    void testBooleanSet(Condition cond, const ValueOperand& value, Register dest);
    void testUndefinedSet(Condition cond, const ValueOperand& value, Register dest);

    // higher level tag testing code
    Address ToPayload(Address value) {
        return value;
    }

    template <typename T>
    void loadUnboxedValue(const T& address, MIRType type, AnyRegister dest) {
        if (dest.isFloat())
            loadInt32OrDouble(address, dest.fpu());
        else
            unboxNonDouble(address, dest.gpr(), ValueTypeFromMIRType(type));
    }

    void storeUnboxedPayload(ValueOperand value, BaseIndex address, size_t nbytes, JSValueType type) {
        switch (nbytes) {
          case 8:
            if (type == JSVAL_TYPE_OBJECT)
                unboxObjectOrNull(value, SecondScratchReg);
            else
                unboxNonDouble(value, SecondScratchReg, type);
            computeEffectiveAddress(address, ScratchRegister);
            as_std(SecondScratchReg, ScratchRegister, 0);
            return;
          case 4:
            store32(value.valueReg(), address);
            return;
          case 1:
            store8(value.valueReg(), address);
            return;
          default: MOZ_CRASH("Bad payload width");
        }
    }

    void storeUnboxedPayload(ValueOperand value, Address address, size_t nbytes, JSValueType type) {
        switch (nbytes) {
          case 8:
            if (type == JSVAL_TYPE_OBJECT)
                unboxObjectOrNull(value, SecondScratchReg);
            else
                unboxNonDouble(value, SecondScratchReg, type);
            storePtr(SecondScratchReg, address);
            return;
          case 4:
            store32(value.valueReg(), address);
            return;
          case 1:
            store8(value.valueReg(), address);
            return;
          default: MOZ_CRASH("Bad payload width");
        }
    }

    void boxValue(JSValueType type, Register src, Register dest);
    void storeValue(ValueOperand val, Operand dst);
    void storeValue(ValueOperand val, const BaseIndex& dest);
    void storeValue(JSValueType type, Register reg, BaseIndex dest);
    void storeValue(ValueOperand val, const Address& dest);
    void storeValue(JSValueType type, Register reg, Address dest);
    void storeValue(const Value& val, Address dest);
    void storeValue(const Value& val, BaseIndex dest);
    void storeValue(const Address& src, const Address& dest, Register temp) {
        loadPtr(src, temp);
        storePtr(temp, dest);
    }
    void storePrivateValue(Register src, const Address& dest) {
        storePtr(src, dest);
    }
    void storePrivateValue(ImmGCPtr imm, const Address& dest) {
        storePtr(imm, dest);
    }

    void loadValue(Address src, ValueOperand val);
    void loadValue(Operand dest, ValueOperand val) {
        loadValue(dest.toAddress(), val);
    }
    void loadValue(const BaseIndex& addr, ValueOperand val);

    void loadUnalignedValue(const Address& src, ValueOperand dest) {
        loadValue(src, dest);
    }

    void tagValue(JSValueType type, Register payload, ValueOperand dest);

    void pushValue(ValueOperand val);
    void popValue(ValueOperand val);
    void pushValue(const Value& val) {
        if (val.isGCThing()) {
            writeDataRelocation(val);
            movWithPatch(ImmWord(val.asRawBits()), ScratchRegister);
            push(ScratchRegister);
        } else {
            push(ImmWord(val.asRawBits()));
        }
    }
    void pushValue(JSValueType type, Register reg) {
        // Use SecondScratchReg as the temp since boxValue uses ScratchRegister
        // for the tag.
        boxValue(type, reg, SecondScratchReg);
        push(SecondScratchReg);
    }
    void pushValue(const Address& addr);

    void handleFailureWithHandlerTail(Label* profilerExitTail);

    /////////////////////////////////////////////////////////////////
    // Common interface.
    /////////////////////////////////////////////////////////////////
  public:
    // The following functions are exposed for use in platform-shared code.

    inline void incrementInt32Value(const Address& addr);

    void move32(Imm32 imm, Register dest);
    void move32(Register src, Register dest);

    void movePtr(Register src, Register dest);
    void movePtr(ImmWord imm, Register dest);
    void movePtr(ImmPtr imm, Register dest);
    void movePtr(wasm::SymbolicAddress imm, Register dest);
    void movePtr(ImmGCPtr imm, Register dest);

    void load8SignExtend(const Address& address, Register dest);
    void load8SignExtend(const BaseIndex& src, Register dest);

    void load8ZeroExtend(const Address& address, Register dest);
    void load8ZeroExtend(const BaseIndex& src, Register dest);

    void load16SignExtend(const Address& address, Register dest);
    void load16SignExtend(const BaseIndex& src, Register dest);

    template <typename S>
    void load16UnalignedSignExtend(const S& src, Register dest) {
        ma_load(dest, src, SizeHalfWord, SignExtend);
    }

    void load16ZeroExtend(const Address& address, Register dest);
    void load16ZeroExtend(const BaseIndex& src, Register dest);

    template <typename S>
    void load16UnalignedZeroExtend(const S& src, Register dest) {
        ma_load(dest, src, SizeHalfWord, ZeroExtend);
    }

    void load32(const Address& address, Register dest);
    void load32(const BaseIndex& address, Register dest);
    void load32ZeroExtend(const Address& address, Register dest);
    void load32ZeroExtend(const BaseIndex& address, Register dest);
    void load32(AbsoluteAddress address, Register dest);
    void load32(wasm::SymbolicAddress address, Register dest);

    void load64(const Address& address, Register64 dest) {
        loadPtr(address, dest.reg);
    }

    template <typename S>
    void load32Unaligned(const S& src, Register dest) {
        ma_load(dest, src, SizeWord, SignExtend);
    }

    void load64(const BaseIndex& address, Register64 dest) {
        loadPtr(address, dest.reg);
    }

    template <typename S>
    void load64Unaligned(const S& src, Register64 dest) {
        ma_load(dest.reg, src, SizeDouble, ZeroExtend);
    }

    void loadPtr(Register src, Register dest);
    void loadPtr(const Address& address, Register dest);
    void loadPtr(const BaseIndex& src, Register dest);
    void loadPtr(AbsoluteAddress address, Register dest);
    void loadPtr(wasm::SymbolicAddress address, Register dest);

    void loadPrivate(const Address& address, Register dest);

    void loadInt32x1(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x1(const BaseIndex& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x2(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x2(const BaseIndex& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x3(const Address& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x3(const BaseIndex& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadInt32x4(const Address& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeInt32x1(FloatRegister src, const Address& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x1(FloatRegister src, const BaseIndex& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x2(FloatRegister src, const Address& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x2(FloatRegister src, const BaseIndex& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x3(FloatRegister src, const Address& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x3(FloatRegister src, const BaseIndex& dest) { MOZ_CRASH("NYI"); }
    void storeInt32x4(FloatRegister src, const Address& dest) { MOZ_CRASH("NYI"); }
    void loadAlignedSimd128Int(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeAlignedSimd128Int(FloatRegister src, Address addr) { MOZ_CRASH("NYI"); }
    void loadUnalignedSimd128Int(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadUnalignedSimd128Int(const BaseIndex& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeUnalignedSimd128Int(FloatRegister src, Address addr) { MOZ_CRASH("NYI"); }
    void storeUnalignedSimd128Int(FloatRegister src, BaseIndex addr) { MOZ_CRASH("NYI"); }

    void loadFloat32x3(const Address& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadFloat32x3(const BaseIndex& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadFloat32x4(const Address& src, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeFloat32x4(FloatRegister src, const Address& addr) { MOZ_CRASH("NYI"); }

    void loadAlignedSimd128Float(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeAlignedSimd128Float(FloatRegister src, Address addr) { MOZ_CRASH("NYI"); }
    void loadUnalignedSimd128Float(const Address& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void loadUnalignedSimd128Float(const BaseIndex& addr, FloatRegister dest) { MOZ_CRASH("NYI"); }
    void storeUnalignedSimd128Float(FloatRegister src, Address addr) { MOZ_CRASH("NYI"); }
    void storeUnalignedSimd128Float(FloatRegister src, BaseIndex addr) { MOZ_CRASH("NYI"); }

    void loadUnalignedDouble(const wasm::MemoryAccessDesc& access, const BaseIndex& src,
                             Register temp, FloatRegister dest);
    void loadUnalignedFloat32(const wasm::MemoryAccessDesc& access, const BaseIndex& src,
                              Register temp, FloatRegister dest);

    void store8(Register src, const Address& address);
    void store8(Imm32 imm, const Address& address);
    void store8(Register src, const BaseIndex& address);
    void store8(Imm32 imm, const BaseIndex& address);

    void store16(Register src, const Address& address);
    void store16(Imm32 imm, const Address& address);
    void store16(Register src, const BaseIndex& address);
    void store16(Imm32 imm, const BaseIndex& address);

    template <typename T>
    void store16Unaligned(Register src, const T& dest) {
        //ma_store_unaligned(src, dest, SizeHalfWord);
        store16(src, dest);
    }

    void store32(Register src, AbsoluteAddress address);
    void store32(Register src, const Address& address);
    void store32(Register src, const BaseIndex& address);
    void store32(Imm32 src, const Address& address);
    void store32(Imm32 src, const BaseIndex& address);

    // NOTE: This will use second scratch on PPC64. Only ARM needs the
    // implementation without second scratch.
    void store32_NoSecondScratch(Imm32 src, const Address& address) {
        store32(src, address);
    }

    template <typename T>
    void store32Unaligned(Register src, const T& dest) {
        //ma_store_unaligned(src, dest, SizeWord);
        store32(src, dest);
    }

    void store64(Imm64 imm, Address address) {
        storePtr(ImmWord(imm.value), address);
    }

    void store64(Register64 src, Address address) {
        storePtr(src.reg, address);
    }

    void store64(Register64 src, const BaseIndex& address) {
        storePtr(src.reg, address);
    }

    template <typename T>
    void store64Unaligned(Register64 src, const T& dest) {
        //ma_store_unaligned(src.reg, dest, SizeDouble);
        store64(src, dest);
    }

    template <typename T> void storePtr(ImmWord imm, T address);
    template <typename T> void storePtr(ImmPtr imm, T address);
    template <typename T> void storePtr(ImmGCPtr imm, T address);
    void storePtr(Register src, const Address& address);
    void storePtr(Register src, const BaseIndex& address);
    void storePtr(Register src, AbsoluteAddress dest);

    void storeUnalignedFloat32(const wasm::MemoryAccessDesc& access, FloatRegister src,
                               Register temp, const BaseIndex& dest);
    void storeUnalignedDouble(const wasm::MemoryAccessDesc& access, FloatRegister src,
                              Register temp, const BaseIndex& dest);

    void moveDouble(FloatRegister src, FloatRegister dest) {
        if (src != dest) as_fmr(dest, src);
    }

    void zeroDouble(FloatRegister reg) {
        if (HasPPCISA3()) {
            // This should also work on POWER8.
            // It's my favourite mnemonic in the whole instruction set.
            // Just say it six times fast. It's positively Asgardian.
            as_xxlxor(reg, reg, reg);
        } else {
            xs_li(ScratchRegister, 0);
            moveToDouble(ScratchRegister, reg);
        }
    }

    void moveFromDouble(FloatRegister src, Register dest) {
        if (HasPPCISA3()) {
            // This should also work on POWER8.
            as_mfvsrd(dest, src);
        } else {
            // Sigh.
            as_stfdu(src, StackPointer, -8);
            as_ld(dest, StackPointer, 0);
            as_addi(StackPointer, StackPointer, 8);
        }
    }

    void moveToDouble(Register src, FloatRegister dest) {
        if (HasPPCISA3()) {
            // This should also work on POWER8.
            as_mtvsrd(dest, src);
        } else {
            // Sigh.
            as_stdu(src, StackPointer, -8);
            as_lfd(dest, StackPointer, 0);
            as_addi(StackPointer, StackPointer, 8);
        }
    }

    void moveFromFloat32(FloatRegister src, Register dest) {
        if (HasPPCISA3()) {
            MOZ_ASSERT(src != ScratchDoubleReg);
            // Enforce rounding mode 0b00 (round-to-nearest tie-to-even).
            as_mtfsfi(7, 0);
            // Downconvert prior to processing and splat it into 32-bit singles.
            as_xscvdpspn(ScratchDoubleReg, src); // preserve sNaN bit, all equal
            as_mfvsrd(dest, ScratchDoubleReg);
            // Take off the top word, leaving the float.
            as_rldicl(dest, dest, 0, 32); // "clrldi"
        } else {
            // Sigh.
            as_stfsu(src, StackPointer, -4);
            as_lwz(dest, StackPointer, 0);
            as_addi(StackPointer, StackPointer, 4);
        }
    }

    void moveToFloat32(Register src, FloatRegister dest) {
        if (HasPPCISA3()) {
            // Splat the 32-bit word as singles throughout the VSR, the
            // upconvert to double.
            as_mtvsrws(dest, src); // loads into both words of DW 0
            as_xscvspdpn(dest, dest); // preserve sNaN bit, both doubles equal
// XXX: what if we put an frsp here to get rid of it in the 32->64 upconvert?
        } else {
            // Sigh.
            as_stwu(src, StackPointer, -4);
            as_lfs(dest, StackPointer, 0);
            as_addi(StackPointer, StackPointer, 4);
        }
    }
    void convertUInt64ToDouble(Register src, FloatRegister dest);

    void breakpoint();

    void checkStackAlignment();

    static void calculateAlignedStackPointer(void** stackPointer);

    // If source is a double, load it into dest. If source is int32,
    // convert it to double. Else, branch to failure.
    void ensureDouble(const ValueOperand& source, FloatRegister dest, Label* failure);

    void cmpPtrSet(Assembler::Condition cond, Address lhs, ImmPtr rhs, Register dest);
    void cmpPtrSet(Assembler::Condition cond, Register lhs, Address rhs, Register dest);

    void cmp32Set(Assembler::Condition cond, Register lhs, Address rhs, Register dest);

  protected:
    bool buildOOLFakeExitFrame(void* fakeReturnAddr);

    void wasmLoadI64Impl(const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
                         Register ptrScratch, Register64 output, Register tmp);
    void wasmStoreI64Impl(const wasm::MemoryAccessDesc& access, Register64 value, Register memoryBase,
                          Register ptr, Register ptrScratch, Register tmp);

  public:
    CodeOffset labelForPatch() {
        return CodeOffset(nextOffset().getOffset());
    }

    void lea(Operand addr, Register dest) {
        ma_add(dest, addr.baseReg(), Imm32(addr.disp()));
    }

    void abiret() {
        as_blr();
    }

    void moveFloat32(FloatRegister src, FloatRegister dest) {
        as_fmr(dest, src);
    }

    void loadWasmGlobalPtr(uint32_t globalDataOffset, Register dest) {
        loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, globalArea) + globalDataOffset), dest);
    }
    void loadWasmPinnedRegsFromTls() {
        loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, memoryBase)), HeapReg);
    }

    // Instrumentation for entering and leaving the profiler.
    void profilerEnterFrame(Register framePtr, Register scratch);
    void profilerExitFrame();
};

typedef MacroAssemblerPPC64Compat MacroAssemblerSpecific;

} // namespace jit
} // namespace js

#endif /* jit_ppc64_MacroAssembler_ppc64_h */
