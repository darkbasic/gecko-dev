/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ppc64/Assembler-ppc64.h"
#include "mozilla/DebugOnly.h"
#include "jit/AutoWritableJitCode.h"
#include "jit/FlushICache.h"


#if DEBUG
#define spew(...) JitSpew(JitSpew_Codegen, __VA_ARGS__)
#else
#define spew(...)
#endif

using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

ABIArgGenerator::ABIArgGenerator()
  : stackOffset_(0),
    usedGPRs_(0),
    usedFPRs_(0),
    current_()
{}

// This is used for inter-Wasm procedure calls as well as regular ABI calls,
// so when we're compiling Wasm we can "expand" the ABI.
// However, we must not do anything that assumes the presence of an argument
// area, since our variant Frame doesn't have one.

ABIArg
ABIArgGenerator::next(MIRType type)
{
    switch (type) {
      case MIRType::Int32:
      case MIRType::Int64:
      case MIRType::Pointer:
      case MIRType::RefOrNull:
      case MIRType::StackResults: {
        if (usedGPRs_ > 7) {
            MOZ_ASSERT(IsCompilingWasm(), "no stack corruption from GPR overflow kthxbye");
            current_ = ABIArg(stackOffset_);
            stackOffset_ += sizeof(uintptr_t);
            break;
        }
        // Note: we could be passing a full 64-bit quantity as an argument to,
        // say, uint32_t. We have to compensate for that in other ways when
        // it makes a difference (see notes in wasm).
        current_ = ABIArg(Register::FromCode((Register::Code)(usedGPRs_ + 3)));
        usedGPRs_++;
        break;
      }
      case MIRType::Float32:
      case MIRType::Double: {
        if (usedFPRs_ == 12) {
            MOZ_ASSERT(IsCompilingWasm(), "no stack corruption from FPR overflow kthxbye");
            current_ = ABIArg(stackOffset_);
            stackOffset_ += sizeof(double); // keep stack aligned to double
            break;
        }
        current_ = ABIArg(FloatRegister(FloatRegisters::Encoding(usedFPRs_ + 1),
            type == MIRType::Double ? FloatRegisters::Double : FloatRegisters::Single));
        usedGPRs_++;
        usedFPRs_++;
        break;
      }
      default:
        MOZ_CRASH("Unexpected argument type");
    }
    return current_;
}

uintptr_t
Assembler::GetPointer(uint8_t* instPtr)
{
    Instruction* inst = (Instruction*)instPtr;
    return Assembler::ExtractLoad64Value(inst);
}

static JitCode *
CodeFromJump(Instruction* jump)
{
    uint8_t* target = (uint8_t*)Assembler::ExtractLoad64Value(jump);
    return JitCode::FromExecutable(target);
}

void
Assembler::TraceJumpRelocations(JSTracer* trc, JitCode* code, CompactBufferReader& reader)
{
    while (reader.more()) {
        JitCode* child = CodeFromJump((Instruction*)(code->raw() + reader.readUnsigned()));
        TraceManuallyBarrieredEdge(trc, &child, "rel32");
    }
}

static void
TraceOneDataRelocation(JSTracer* trc, Instruction* inst)
{
    void* ptr = (void*)Assembler::ExtractLoad64Value(inst);
    void* prior = ptr;

    // All pointers will have the top bits clear. If those bits
    // are not cleared, this must be a Value.
    uintptr_t word = reinterpret_cast<uintptr_t>(ptr);
    if (word >> JSVAL_TAG_SHIFT) {
        Value v = Value::fromRawBits(word);
        TraceManuallyBarrieredEdge(trc, &v, "ion-masm-value");
        ptr = (void*)v.bitsAsPunboxPointer();
    } else {
        // No barrier needed since these are constants.
        TraceManuallyBarrieredGenericPointerEdge(trc, reinterpret_cast<gc::Cell**>(&ptr),
                                                     "ion-masm-ptr");
    }

    if (ptr != prior) {
        Assembler::UpdateLoad64Value(inst, uint64_t(ptr));
        FlushICache(inst, 5 * sizeof(uint32_t));
    }
}

/* static */ void
Assembler::TraceDataRelocations(JSTracer* trc, JitCode* code, CompactBufferReader& reader)
{
    mozilla::Maybe<AutoWritableJitCode> awjc;

    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        Instruction* inst = (Instruction*)(code->raw() + offset);
        if (awjc.isNothing()) {
            awjc.emplace(code);
        }
        TraceOneDataRelocation(trc, inst);
    }
}

void
Assembler::Bind(uint8_t* rawCode, const CodeLabel& label)
{
    if (label.patchAt().bound()) {
        auto mode = label.linkMode();
        intptr_t offset = label.patchAt().offset();
        intptr_t target = label.target().offset();
        spew("# Bind mode=%d rawCode=%p offset=%lx target=%lx", (int)mode, rawCode, offset, target);

        if (mode == CodeLabel::RawPointer) {
            *reinterpret_cast<const void**>(rawCode + offset) = rawCode + target;
        } else {
            MOZ_ASSERT(mode == CodeLabel::MoveImmediate || mode == CodeLabel::JumpImmediate);
            Instruction* inst = (Instruction*) (rawCode + offset);
            Assembler::UpdateLoad64Value(inst, (uint64_t)(rawCode + target));
            FlushICache(inst, 5 * sizeof(uint32_t));
        }
    }
}

void
Assembler::bind(InstImm* inst, uintptr_t b, uintptr_t target, bool bound)
{
#if 0 // TODO: Assembler::bind()
    int64_t offset = target - branch;
    InstImm inst_bgezal = InstImm(op_regimm, r0, rt_bgezal, BOffImm16(0));
    InstImm inst_beq = InstImm(PPC_bc, r0, r0, BOffImm16(0));

    // If encoded offset is 4, then the jump must be short
    if (BOffImm16(inst[0]).decode() == 4) {
        MOZ_ASSERT(BOffImm16::IsInRange(offset));
        inst[0].setBOffImm16(BOffImm16(offset));
        inst[1].makeOp_nop();
        return;
    }

    // Generate the long jump for calls because return address has to be the
    // address after the reserved block.
    if (inst[0].encode() == inst_bgezal.encode()) {
        addLongJump(BufferOffset(branch));
        Assembler::WriteLoad64Instructions(inst, ScratchRegister, LabelBase::INVALID_OFFSET);
        inst[4] = InstReg(PPC_b | LinkB, ScratchRegister, r0).encode();
        // There is 1 nop after this.
        return;
    }

    if (BOffImm16::IsInRange(offset)) {
        // Don't skip trailing nops can improve performance
        // on Loongson3 platform.
        bool skipNops = (inst[0].encode() != inst_bgezal.encode() &&
                        inst[0].encode() != inst_beq.encode());

        inst[0].setBOffImm16(BOffImm16(offset));
        inst[1].makeOp_nop();

        if (skipNops) {
            inst[2] = InstImm(op_regimm, r0, rt_bgez, BOffImm16(5 * sizeof(uint32_t))).encode();
            // There are 4 nops after this
        }
        return;
    }

    if (inst[0].encode() == inst_beq.encode()) {
        // Handle long unconditional jump.
        addLongJump(BufferOffset(branch));
        Assembler::WriteLoad64Instructions(inst, ScratchRegister, LabelBase::INVALID_OFFSET);
        inst[4] = InstReg(op_special, ScratchRegister, r0, r0, ff_jr).encode();
        // There is 1 nop after this.
    } else {
        // Handle long conditional jump.
        inst[0] = invertBranch(inst[0], BOffImm16(7 * sizeof(uint32_t)));
        // No need for a "nop" here because we can clobber scratch.
        addLongJump(BufferOffset(branch + sizeof(uint32_t)));
        Assembler::WriteLoad64Instructions(&inst[1], ScratchRegister, LabelBase::INVALID_OFFSET);
        inst[5] = InstReg(op_special, ScratchRegister, r0, r0, ff_jr).encode();
        // There is 1 nop after this.
    }
#endif
    int64_t offset = target - b;
    spew("# bind %lx %lx (instruction: %08x)", b, target, inst[0].encode());
    MOZ_ASSERT(!(offset & 3));

        if (inst[0].isOpcode(PPC_addis)) { // pre-existing long stanza
            spew("# pending long jump");
            addLongJump(BufferOffset(b), BufferOffset(target));
        } else if (inst[0].isOpcode(PPC_tw)) { // tagged trap
            TrapTag t = inst[0].traptag();

            if (t == BCTag) {
                // Reverse-sense long stanza:
                // bc inverted, fail
                // tagged trap       << inst[0]    patch to lis
                // .long next_in_chain             patch to ori
                // nop                             patch to rldicr
                // nop                             patch to oris
                // nop                             patch to ori
                // nop                             patch to mtctr
                // nop                             patch to bctr

                // The negative index is required because of an invariant in ::retarget() that
                // always expects the next-in-chain word in slot 1 (any value there could be
                // potentially valid, including a trap word, so no sentinel value can
                // disambiguate).
                MOZ_ASSERT(inst[-1].isOpcode(PPC_bc));
                // I always like a clean stanza.
                MOZ_ASSERT(inst[2].encode() == PPC_nop);
                MOZ_ASSERT(inst[3].encode() == PPC_nop);
                MOZ_ASSERT(inst[4].encode() == PPC_nop);
                MOZ_ASSERT(inst[5].encode() == PPC_nop);
                MOZ_ASSERT(inst[6].encode() == PPC_nop);

                // If this was actually assigned, see if it's a short jump after all.
                if (bound && BOffImm16::IsInSignedRange(offset + sizeof(uint32_t))) { // see below
                    // It's a short jump after all.
                    // It's a short jump after all.
                    // It's a short jump after all.
                    // It's a short, short jump.
                    // Patch bc directly by inverting the sense, adding an instruction to
                    // offset the negative index.

                    spew("# writing in long jump as short bc");
                    // Make sure we're not going to patch in the wrong place.
                    MOZ_ASSERT(inst[7].encode() != PPC_bctr);
                    // Weirdo instructions like bdnz shouldn't come through here, just any
                    // bc with a BO of 0x04 or 0x0c (i.e., CR bit set or CR bit not set).
                    MOZ_ASSERT((inst[-1].encode() & 0x03e00000) == 0x00800000 ||
                               (inst[-1].encode() & 0x03e00000) == 0x01800000);
                    // XXX: should invert likely bits, too.
                    inst[-1].setData(((inst[-1].encode() ^ 0x01000000) & (0xffff0003)) | BOffImm16(offset+sizeof(uint32_t)).encode());
                    inst[0].setData(PPC_nop); // obliterate tagged trap
                    inst[1].setData(PPC_nop); // obliterate next in chain
                } else if (bound && JOffImm26::IsInRange(offset)) {
                    // It's a short(er) jump after all.
                    // It's a short(er) jump after all.
                    // It's ... why did you pick up that chainsaw?
                    // Why is it running?

                    spew("# writing in long jump as short bc/b");
                    // Make sure we're not going to patch in the wrong place.
                    MOZ_ASSERT(inst[7].encode() != PPC_bctr);
                    inst[0].setData(PPC_b | JOffImm26(offset).encode());
                    inst[1].setData(PPC_nop); // obliterate next in chain
                } else {
                    // Dang, no Disney songs for this.
                    // Although this should be to Ion code, use r12 to keep calls "as expected."

                    spew("# writing in and pending long bc");
                    addLongJump(BufferOffset(b), BufferOffset(target));
                    Assembler::WriteLoad64Instructions(inst, SecondScratchReg, LabelBase::INVALID_OFFSET);
                    inst[5].makeOp_mtctr(SecondScratchReg);
                    inst[6].makeOp_bctr(DontLinkB);
                }
            } else if (t == CallTag) {
                // I wanna taste pizzazz, all the taste clean Stanza has!
                // I wanna clean, I wanna ... Stan-za.
                MOZ_ASSERT(inst[2].encode() == PPC_nop);
                MOZ_ASSERT(inst[3].encode() == PPC_nop);
                MOZ_ASSERT(inst[4].encode() == PPC_nop);
                MOZ_ASSERT(inst[5].encode() == PPC_nop);
                MOZ_ASSERT(inst[6].encode() == PPC_nop);

                // And I get to sing Disney songs again!
                // See if it's a short jump after all!
                if (bound && JOffImm26::IsInRange(offset - 24)) {
                    // It's a short jump after all!
                    // It's a short #${{@~NO CARRIER
                    spew("# writing in short call");
                    // Make sure we're not going to patch in the wrong place.
                    MOZ_ASSERT(inst[7].encode() != PPC_bctr);
                    inst[0].setData(PPC_nop); // obliterate trap
                    inst[1].setData(PPC_nop); // obliterate next-in-chain
                    // So that the return is after the stanza, the link call
                    // must be the last instruction in the stanza, not the
                    // first. This means we also need to adjust the offset,
                    // which is where the 24 comes from (stanza length minus
                    // the bl instruction).
                    offset -= 24;
                    inst[6].setData(PPC_b | JOffImm26(offset).encode() | LinkB);
                } else {
                    // Why doesn't anyone like my singing?
                    spew("# writing in and pending long call");
                    addLongJump(BufferOffset(b), BufferOffset(target));
                    Assembler::WriteLoad64Instructions(inst, SecondScratchReg, LabelBase::INVALID_OFFSET);
                    inst[5].makeOp_mtctr(SecondScratchReg);
                    inst[6].makeOp_bctr(LinkB);
                }
            } else if (t == BTag) {
                // More or less a degenerate case of BCTag. But do they let me
                // sing Disney songs about that? Noooooooooo. They said it was
                // an HR violation and would summon lawyers from their undead
                // crypts to lay waste upon the earth. Wimps.
                MOZ_ASSERT(inst[2].encode() == PPC_nop);
                MOZ_ASSERT(inst[3].encode() == PPC_nop);
                MOZ_ASSERT(inst[4].encode() == PPC_nop);
                MOZ_ASSERT(inst[5].encode() == PPC_nop);
                MOZ_ASSERT(inst[6].encode() == PPC_nop);

                if (bound && JOffImm26::IsInRange(offset)) {
                    spew("# writing in short b");
                    // Make sure we're not going to patch in the wrong place.
                    MOZ_ASSERT(inst[7].encode() != PPC_bctr);
                    // The branch, in this case, really is in slot 0.
                    inst[0].setData(PPC_b | JOffImm26(offset).encode());
                    inst[1].setData(PPC_nop); // obliterate next in chain
                } else {
                    spew("# writing in and pending long b");
                    addLongJump(BufferOffset(b), BufferOffset(target));
                    Assembler::WriteLoad64Instructions(inst, SecondScratchReg, LabelBase::INVALID_OFFSET);
                    inst[5].makeOp_mtctr(SecondScratchReg);
                    inst[6].makeOp_bctr(DontLinkB);
                }
            } else {
                MOZ_CRASH("unhandled trap in slot 0");
            }
        } else if (inst[0].isOpcode(PPC_b)) {
            // Short jump emitted by ma_b. Set the b target and nop out the next-in-chain.
            spew("# setting short b");
            MOZ_ASSERT(JOffImm26::IsInRange(offset));
            inst[0].setData(PPC_b | JOffImm26(offset).encode());
            inst[1].setData(PPC_nop); // obliterate next-in-chain
        } else if (inst[0].isOpcode(PPC_bc)) {
            // Short jump emitted by ma_bc. Set the bc target and nop out the next-in-chain.
            spew("# setting short bc");
            MOZ_ASSERT(BOffImm16::IsInSignedRange(offset));
            inst[0].setData((inst[0].encode() & 0xffff0000) | BOffImm16(offset).encode());
            inst[1].setData(PPC_nop); // obliterate next-in-chain
        } else {
            MOZ_CRASH("Unhandled bind()");
        }

#if 0
        InstImm inst_beq = InstImm(op_beq, r0, r0, BOffImm16(0));
        uint64_t offset = dest.getOffset() - label->offset();

        // If first instruction is lui, then this is a long jump.
        // If second instruction is lui, then this is a loop backedge.
        if (inst[0].extractOpcode() == (uint32_t(op_lui) >> OpcodeShift)) {
            // For unconditional long branches generated by ma_liPatchable,
            // such as under:
            //     jumpWithpatch
            addLongJump(BufferOffset(label->offset()));
        } else if (inst[1].extractOpcode() == (uint32_t(op_lui) >> OpcodeShift) ||
                   BOffImm16::IsInRange(offset))
        {
            // Handle code produced by:
            //     backedgeJump
            MOZ_ASSERT(BOffImm16::IsInRange(offset));
            MOZ_ASSERT(inst[0].extractOpcode() == (uint32_t(op_beq) >> OpcodeShift) ||
                       inst[0].extractOpcode() == (uint32_t(op_bne) >> OpcodeShift) ||
                       inst[0].extractOpcode() == (uint32_t(op_blez) >> OpcodeShift) ||
                       inst[0].extractOpcode() == (uint32_t(op_bgtz) >> OpcodeShift) ||
                       (inst[0].extractOpcode() == (uint32_t(op_regimm) >> OpcodeShift) &&
                       inst[0].extractRT() == (uint32_t(rt_bltz) >> RTShift)));
            inst[0].setBOffImm16(BOffImm16(offset));
        } else if (inst[0].encode() == inst_beq.encode()) {
            // Handle open long unconditional jumps created by
            // MacroAssemblerMIPSShared::ma_bc(..., wasm::Trap, ...).
            // We need to add it to long jumps array here.
            MOZ_ASSERT(inst[1].encode() == NopInst);
            MOZ_ASSERT(inst[2].encode() == NopInst);
            MOZ_ASSERT(inst[3].encode() == NopInst);
            MOZ_ASSERT(inst[4].encode() == NopInst);
            MOZ_ASSERT(inst[5].encode() == NopInst);
            addLongJump(BufferOffset(label->offset()));
            Assembler::WriteLoad64Instructions(inst, ScratchRegister, LabelBase::INVALID_OFFSET);
            inst[4] = InstReg(op_special, ScratchRegister, r0, r0, ff_jr).encode();
        } else {
            // Handle open long conditional jumps created by
            // MacroAssemblerMIPSShared::ma_bc(..., wasm::Trap, ...).
            inst[0] = invertBranch(inst[0], BOffImm16(7 * sizeof(uint32_t)));
            // No need for a "nop" here because we can clobber scratch.
            // We need to add it to long jumps array here.
            // See MacroAssemblerMIPS64::branchWithCode().
            MOZ_ASSERT(inst[1].encode() == NopInst);
            MOZ_ASSERT(inst[2].encode() == NopInst);
            MOZ_ASSERT(inst[3].encode() == NopInst);
            MOZ_ASSERT(inst[4].encode() == NopInst);
            MOZ_ASSERT(inst[5].encode() == NopInst);
            MOZ_ASSERT(inst[6].encode() == NopInst);
            addLongJump(BufferOffset(label->offset() + sizeof(uint32_t)));
            Assembler::WriteLoad64Instructions(&inst[1], ScratchRegister, LabelBase::INVALID_OFFSET);
            inst[5] = InstReg(op_special, ScratchRegister, r0, r0, ff_jr).encode();
        }
#endif
}

void
Assembler::bind(Label* label, BufferOffset boff)
{
    spew(".set Llabel %p", label);
    // If our caller didn't give us an explicit target to bind to
    // then we want to bind to the location of the next instruction
    BufferOffset dest = boff.assigned() ? boff : nextOffset();
    if (label->used()) {
        int32_t next;

        // A used label holds a link to branch that uses it.
        BufferOffset b(label);
        do {
            // Even a 0 offset may be invalid if we're out of memory.
            if (oom()) {
                return;
            }

            Instruction* inst = editSrc(b);

            // Second word holds a pointer to the next branch in label's chain.
            next = inst[1].encode();
            bind(reinterpret_cast<InstImm*>(inst), b.getOffset(), dest.getOffset(), boff.assigned());

            b = BufferOffset(next);
        } while (next != LabelBase::INVALID_OFFSET);
    }
    label->bind(dest.getOffset());
}

void
Assembler::retarget(Label* label, Label* target)
{
    spew("retarget %p -> %p", label, target);
    if (label->used() && !oom()) {
        if (target->bound()) {
            bind(label, BufferOffset(target));
        } else if (target->used()) {
            // The target is not bound but used. Prepend label's branch list
            // onto target's.
            int32_t next;
            BufferOffset labelBranchOffset(label);

            // Find the head of the use chain for label.
            do {
                Instruction* inst = editSrc(labelBranchOffset);

                // Second word holds a pointer to the next branch in chain.
                next = inst[1].encode();
                labelBranchOffset = BufferOffset(next);
            } while (next != LabelBase::INVALID_OFFSET);

            // Then patch the head of label's use chain to the tail of
            // target's use chain, prepending the entire use chain of target.
            Instruction* inst = editSrc(labelBranchOffset);
            int32_t prev = target->offset();
            target->use(label->offset());
            inst[1].setData(prev);
        } else {
            // The target is unbound and unused.  We can just take the head of
            // the list hanging off of label, and dump that into target.
            target->use(label->offset());
        }
    }
    label->reset();
}

void
Assembler::processCodeLabels(uint8_t* rawCode)
{
    for (const CodeLabel& label : codeLabels_) {
        Bind(rawCode, label);
    }
}

uint32_t
Assembler::PatchWrite_NearCallSize()
{
    // Load an address needs 5 instructions, mtctr, bctrl
    return (5 + 2) * sizeof(uint32_t);
}

void
Assembler::PatchWrite_NearCall(CodeLocationLabel start, CodeLocationLabel toCall)
{
    // Overwrite whatever instruction used to be here with a call.
    MOZ_ASSERT(PatchWrite_NearCallSize() == 7 * sizeof(uint32_t));
    Instruction* inst = (Instruction*) start.raw();
    uint8_t* dest = toCall.raw();

    // We use a long stanza, but if we can, put nops/bl instead of a full
    // lis/ori/rldicr/lis/ori/mtctr/bctr because we can speculate better.
    // If we short it, the short branch is at the END of the stanza because
    // the return address must follow it.
    int64_t offset = ((uint64_t)dest - (uint64_t)inst) - 24;

    if (JOffImm26::IsInRange(offset)) {
        // "$5 says he shorts it."
        // Since this is a near call, we expect we won't need to repatch this.
        inst[0] = Instruction(PPC_nop);
        inst[1] = Instruction(PPC_nop);
        inst[2] = Instruction(PPC_nop);
        inst[3] = Instruction(PPC_nop);
        inst[4] = Instruction(PPC_nop);
        inst[5] = Instruction(PPC_nop);
        inst[6].setData(PPC_b | JOffImm26(offset).encode() | LinkB);
    } else {
        // Long jump required ...
        Assembler::WriteLoad64Instructions(inst, ScratchRegister, (uint64_t)dest);
        inst[5].makeOp_mtctr(ScratchRegister);
        inst[6].makeOp_bctr(LinkB);
    }

    // Ensure everyone sees the code that was just written into memory.
    FlushICache(inst, PatchWrite_NearCallSize());
}

void
Assembler::PatchWrite_Imm32(CodeLocationLabel label, Imm32 imm)
{
    uint32_t *l = (uint32_t *)label.raw();

    *(l - 1) = imm.value;
}

void
Assembler::UpdateLisOriValue(Instruction *inst0, Instruction *inst1,
                             uint32_t value)
{
    MOZ_ASSERT(inst0->extractOpcode() == (uint32_t)PPC_addis);
    MOZ_ASSERT(inst1->extractOpcode() == (uint32_t)PPC_ori);

    reinterpret_cast<InstImm*>(inst0)->setImm16(value >> 16);
    reinterpret_cast<InstImm*>(inst1)->setImm16(value & 0xffff);
}

uint64_t
Assembler::ExtractLoad64Value(Instruction* inst0)
{
    InstImm* i0 = (InstImm*) inst0; // lis
    InstImm* i1 = (InstImm*) i0->next(); // ori
    Instruction* i2 = (Instruction*) i1->next(); // rldicr
    InstImm* i3 = (InstImm*) i2->next(); // oris
    InstImm* i4 = (InstImm*) i3->next(); // ori

    MOZ_ASSERT(i0->extractOpcode() == (uint32_t)PPC_addis);
    MOZ_ASSERT(i1->extractOpcode() == (uint32_t)PPC_ori);
    MOZ_ASSERT(i2->extractOpcode() == (uint32_t)PPC_rldicl); // XXX: 0x78000000
    MOZ_ASSERT(i3->extractOpcode() == (uint32_t)PPC_oris);
    MOZ_ASSERT(i4->extractOpcode() == (uint32_t)PPC_ori);

    uint64_t value = (uint64_t(i0->extractImm16Value()) << 48) |
                     (uint64_t(i1->extractImm16Value()) << 32) |
                     (uint64_t(i3->extractImm16Value()) << 16) |
                     uint64_t(i4->extractImm16Value());
    return value;
}

void
Assembler::UpdateLoad64Value(Instruction* inst0, uint64_t value)
{
    InstImm* i0 = (InstImm*) inst0;
    InstImm* i1 = (InstImm*) i0->next();
    Instruction* i2 = (Instruction*) i1->next();
    InstImm* i3 = (InstImm*) i2->next();
    InstImm* i4 = (InstImm*) i3->next();

    MOZ_ASSERT(i0->extractOpcode() == (uint32_t)PPC_addis);
    MOZ_ASSERT(i1->extractOpcode() == (uint32_t)PPC_ori);
    MOZ_ASSERT(i2->extractOpcode() == (uint32_t)PPC_rldicl); // XXX
    MOZ_ASSERT(i3->extractOpcode() == (uint32_t)PPC_oris);
    MOZ_ASSERT(i4->extractOpcode() == (uint32_t)PPC_ori);

    i0->setImm16(Imm16::Upper(Imm32(value >> 32)));
    i1->setImm16(Imm16::Lower(Imm32(value >> 32)));
    i3->setImm16(Imm16::Upper(Imm32(value)));
    i4->setImm16(Imm16::Lower(Imm32(value)));
}

void
Assembler::WriteLoad64Instructions(Instruction* inst0, Register reg, uint64_t value)
{
    Instruction* inst1 = inst0->next();
    Instruction* inst2 = inst1->next();
    Instruction* inst3 = inst2->next();
    Instruction* inst4 = inst3->next();

    *inst0 = InstImm(PPC_addis, reg, r0, Imm16::Upper(Imm32(value >> 32)).encode()); // mscdfr0
    *inst1 = InstImm(PPC_ori, reg, reg, Imm16::Lower(Imm32(value >> 32)).encode());
    // rldicr reg, reg, 32, 31
    *inst2 = InstImm(PPC_rldicr, reg, reg, ((31 << 6) | (32 >> 4)));
    *inst3 = InstImm(PPC_oris, reg, reg, Imm16::Upper(Imm32(value)).encode());
    *inst4 = InstImm(PPC_ori, reg, reg, Imm16::Lower(Imm32(value)).encode());
}

void
Assembler::PatchDataWithValueCheck(CodeLocationLabel label, ImmPtr newValue,
                                   ImmPtr expectedValue)
{
    PatchDataWithValueCheck(label, PatchedImmPtr(newValue.value),
                            PatchedImmPtr(expectedValue.value));
}

void
Assembler::PatchDataWithValueCheck(CodeLocationLabel label, PatchedImmPtr newValue,
                                   PatchedImmPtr expectedValue)
{
    spew("# PatchDataWithValueCheck 0x%p", label.raw());
    Instruction* inst = (Instruction*) label.raw();

    // Extract old Value
    DebugOnly<uint64_t> value = Assembler::ExtractLoad64Value(inst);
    MOZ_ASSERT(value == uint64_t(expectedValue.value));

    // Replace with new value
    Assembler::UpdateLoad64Value(inst, uint64_t(newValue.value));
    FlushICache(inst, 5 * sizeof(uint32_t));
}

// See MacroAssemblerPPC64Compat::toggledJump and
// MacroAssemblerPPC64Compat::toggledCall.
// These patch our suspicious oris r0,r0,0 to either skip the next
// instruction or not.

void
Assembler::ToggleCall(CodeLocationLabel inst_, bool enabled)
{
    Instruction* inst = (Instruction*)inst_.raw();
    MOZ_ASSERT((inst->encode() == PPC_oris) || (inst->encode() == (PPC_b | 0x20)));
    if (enabled) {
        inst->setData(PPC_oris);     // oris 0,0,0
    } else {
        inst->setData(PPC_b | 0x20); // b .+32
    }

    FlushICache(inst, sizeof(uint32_t));
}

// JMP and CMP are from the icky x86 perspective. Since we have a jump
// stanza, making it a "JMP" means setting the gate instruction to oris
// so the stanza is run; making it a "CMP" means setting it to b+.32 so it
// isn't.

void
Assembler::ToggleToJmp(CodeLocationLabel inst_)
{
    Instruction* inst = (Instruction*)inst_.raw();
    MOZ_ASSERT(inst->encode() == (PPC_b | 0x20));
    inst->setData(PPC_oris);

    FlushICache(inst, sizeof(uint32_t));
}

void
Assembler::ToggleToCmp(CodeLocationLabel inst_)
{
    Instruction* inst = (Instruction*)inst_.raw();
    MOZ_ASSERT(inst->encode() == PPC_oris);
    inst->setData(PPC_b | 0x20);

    FlushICache(inst, sizeof(uint32_t));
}

Assembler::Condition
Assembler::InvertCondition( Condition cond)
{
    switch (cond) {
        case Equal:
            return NotEqual;
        case NotEqual:
            return Equal;
        case LessThan:
            return GreaterThanOrEqual;
        case LessThanOrEqual:
            return GreaterThan;
        case GreaterThan:
            return LessThanOrEqual;
        case GreaterThanOrEqual:
            return LessThan;
        case Above:
            return BelowOrEqual;
        case AboveOrEqual:
            return Below;
        case Below:
            return AboveOrEqual;
        case BelowOrEqual:
            return Above;
        case Zero:
            return NonZero;
        case NonZero:
            return Zero;
        case Signed:
            return NotSigned;
        case NotSigned:
            return Signed;
        case SOBit:
            return NSOBit;
        case NSOBit:
            return SOBit;
        default:
            MOZ_CRASH("unexpected condition");
    }
}

Assembler::DoubleCondition
Assembler::InvertCondition( DoubleCondition cond)
{
    switch (cond) {
        case DoubleOrdered:
            return DoubleUnordered;
        case DoubleEqual:
            return DoubleNotEqualOrUnordered;
        case DoubleNotEqual:
            return DoubleEqualOrUnordered;
        case DoubleGreaterThan:
            return DoubleLessThanOrEqualOrUnordered;
        case DoubleGreaterThanOrEqual:
            return DoubleLessThanOrUnordered;
        case DoubleLessThan:
            return DoubleGreaterThanOrEqualOrUnordered;
        case DoubleLessThanOrEqual:
            return DoubleGreaterThanOrUnordered;
        case DoubleUnordered:
            return DoubleOrdered;
        case DoubleEqualOrUnordered:
            return DoubleNotEqual;
        case DoubleNotEqualOrUnordered:
            return DoubleEqual;
        case DoubleGreaterThanOrUnordered:
            return DoubleLessThanOrEqual;
        case DoubleGreaterThanOrEqualOrUnordered:
            return DoubleLessThan;
        case DoubleLessThanOrUnordered:
            return DoubleGreaterThanOrEqual;
        case DoubleLessThanOrEqualOrUnordered:
            return DoubleGreaterThan;
        default:
            MOZ_CRASH("unexpected condition");
    }
}

bool
Assembler::swapBuffer(wasm::Bytes& bytes) {
    // For now, specialize to the one use case. As long as wasm::Bytes is a
    // Vector, not a linked-list of chunks, there's not much we can do other
    // than copy.
    MOZ_ASSERT(bytes.empty());
    if (!bytes.resize(bytesNeeded())) {
        return false;
    }
    m_buffer.executableCopy(bytes.begin());
    return true;
}

void
Assembler::copyJumpRelocationTable(uint8_t* dest)
{
    if (jumpRelocations_.length()) {
        memcpy(dest, jumpRelocations_.buffer(), jumpRelocations_.length());
    }
}

void
Assembler::copyDataRelocationTable(uint8_t* dest)
{
    if (dataRelocations_.length()) {
        memcpy(dest, dataRelocations_.buffer(), dataRelocations_.length());
    }
}

void
Assembler::finish()
{
    MOZ_ASSERT(!isFinished);
    isFinished = true;
}

void
Assembler::executableCopy(uint8_t* buffer)
{
    spew("# EXECUTABLE COPY TO %p\n", buffer);

    MOZ_ASSERT(isFinished);
    m_buffer.executableCopy(buffer);
}

bool
Assembler::appendRawCode(unsigned char const *bytes, unsigned long length)
{
    return m_buffer.appendRawCode(bytes, length);
}

bool Assembler::oom() const {
  return AssemblerShared::oom() || m_buffer.oom() || jumpRelocations_.oom() ||
         dataRelocations_.oom();
}

BufferOffset Assembler::haltingAlign(int align)
{
    BufferOffset ret;
    MOZ_ASSERT(m_buffer.isAligned(4));
    if (align == 8) {
        if (!m_buffer.isAligned(align)) {
            BufferOffset tmp = xs_trap();
            if (!ret.assigned()) {
                ret = tmp;
            }
        }
    } else {
        MOZ_ASSERT((align& (align- 1)) == 0);
        while (size() & (align- 1)) {
            BufferOffset tmp = xs_trap();
            if (!ret.assigned()) {
                ret = tmp;
            }
        }
    }
    return ret;
}

BufferOffset Assembler::nopAlign(int align)
{
    BufferOffset ret;
    MOZ_ASSERT(m_buffer.isAligned(4));
    if (align == 8) {
        if (!m_buffer.isAligned(align)) {
            BufferOffset tmp = as_nop();
            if (!ret.assigned()) {
                ret = tmp;
            }
        }
    } else {
        MOZ_ASSERT((align& (align- 1)) == 0);
        while (size() & (align- 1)) {
            BufferOffset tmp = as_nop();
            if (!ret.assigned()) {
                ret = tmp;
            }
        }
    }
    return ret;
}

size_t
Assembler::size() const
{
    return m_buffer.size();
}

size_t
Assembler::dataRelocationTableBytes() const
{
    return dataRelocations_.length();
}

size_t
Assembler::jumpRelocationTableBytes() const
{
    return jumpRelocations_.length();
}

size_t
Assembler::bytesNeeded() const
{
    return m_buffer.size() +
        jumpRelocations_.length() +
        dataRelocations_.length() +
        preBarriers_.length();
}

BufferOffset
Assembler::writeInst(uint32_t x, uint32_t *dest)
{
    if (dest == nullptr)
        return m_buffer.putInt(x);

    *dest = x;
    return BufferOffset();
}

BufferOffset Assembler::as_nop()
{
    spew("nop");
    return writeInst(PPC_nop);
}

BufferOffset Assembler::as_eieio()
{
    // Old McDonald had to order memory access ...
    spew("eieio");
    return writeInst(PPC_eieio);
}

BufferOffset Assembler::as_isync()
{
    spew("isync");
    return writeInst(PPC_isync);
}

BufferOffset Assembler::xs_lwsync()
{
    spew("lwsync");
    return writeInst(PPC_lwsync);
}

BufferOffset Assembler::as_sync()
{
    spew("sync");
    return writeInst(PPC_sync);
}

// Branch and jump instructions.
BufferOffset Assembler::as_b(JOffImm26 off, BranchAddressType bat, LinkBit lb)
{
    return as_b(off.encode(), bat, lb);
}

BufferOffset Assembler::as_b(int32_t off, BranchAddressType bat, LinkBit lb)
{
    spew("b%s%s\t%x", bat == AbsoluteBranch ? "a" : "", lb ? "l" : "", off);
    MOZ_ASSERT(!(off & 0x03));
    return writeInst(PPC_b | ((uint32_t)off & 0x3fffffc) | bat | lb);
}

BufferOffset Assembler::as_blr(LinkBit lb)
{
    spew("blr%s", lb ? "l" : "");
    return writeInst(PPC_blr | lb);
}

BufferOffset Assembler::as_bctr(LinkBit lb)
{
    spew("bctr%s", lb ? "l" : "");
    return writeInst(PPC_bctr | lb);
}

// Conditional branches.
//
// These utility functions turn a condition (possibly synthetic) and CR
// field number into BO _|_ BI. With DoubleConditions we may issue CR bit
// twiddles and change the op.
uint16_t Assembler::computeConditionCode(DoubleCondition op, CRegisterID cr)
{
	// Use condition register logic to combine the FU (FUUUU-! I mean, unordered)
	// bit with the actual condition bit.
	const uint8_t condBit = crBit(cr, op);
	const uint8_t fuBit = crBit(cr, DoubleUnordered);
	uint32_t newop = (uint32_t)op & 255;

	if (op & DoubleConditionUnordered) {
		// branch if condition true OR Unordered
		if ((op & BranchOptionMask) == BranchOnClear) {
			// invert the condBit, or it with fuBit, and branch on Set
			as_crorc(condBit, fuBit, condBit);
			newop |= BranchOnSet;
		} else {
			// or the condBit with fuBit, and then branch on Set
			if (condBit != fuBit)
				as_cror(condBit, fuBit, condBit);
		}
	} else {
		// branch if condition true AND ordered
		if ((op & BranchOptionMask) == BranchOnClear) {
			// or the condBit with fuBit, and branch on Clear
			if (condBit != fuBit)
				as_cror(condBit, fuBit, condBit);
		} else {
			// and the condBit with (!fuBit), and branch on Set, but
			// don't clear SO if this is actually DoubleUnordered
			// (fuBit == condBit), which is NOT a synthetic condition.
			if (condBit != fuBit)
				as_crandc(condBit, condBit, fuBit);
		}
	}

	// Set BIF to the proper CR. In cr0, the normal state, this just returns newop.
	return (newop + ((uint8_t)cr << 6));
}

// Do the same for GPR compares and XER-based conditions.
uint16_t Assembler::computeConditionCode(Condition op, CRegisterID cr)
{
	// Mask off the synthetic bits, if present. Hopefully we handled them already!
	uint32_t newop = (uint32_t)op & 255;

	// If this is an XER-mediated condition, then extract it.
	if (op & ConditionOnlyXER) {
		MOZ_ASSERT(op == Overflow);
		// Get XER into CR.
		as_mcrxrx(cr);
		// Convert op to GT (using the OV32 bit).
		newop = (uint32_t)GreaterThan;
	}

	// Set BIF to the proper CR. In cr0, the normal state, this just returns newop.
	return (newop + ((uint8_t)cr << 6));
}

// Given BO _|_ BI in a 16-bit quantity, emit the two halves suitable for bit masking.
static uint32_t makeOpMask(uint16_t op)
{
    MOZ_ASSERT(!(op & 0xfc00)); // must fit in 10 bits
    return ((op & 0x0f) << 21) | ((op & 0xfff0) << 12);
}

BufferOffset Assembler::as_bcctr(Condition cond, CRegisterID cr, LikelyBit lkb,
        LinkBit lb)
{
    return as_bcctr(computeConditionCode(cond, cr), lkb, lb);
}

BufferOffset Assembler::as_bcctr(DoubleCondition cond, CRegisterID cr,
        LikelyBit lkb, LinkBit lb)
{
    return as_bcctr(computeConditionCode(cond, cr), lkb, lb);
}

BufferOffset Assembler::as_bc(BOffImm16 off, Condition cond, CRegisterID cr,
        LikelyBit lkb, LinkBit lb)
{
    // fall through to the next one
    return as_bc(off.encode(), cond, cr, lkb, lb);
}

BufferOffset Assembler::as_bc(int16_t off, Condition cond, CRegisterID cr,
        LikelyBit lkb, LinkBit lb)
{
    // No current need to adjust for the |mcrxr|; see ma_bc() in the
    // MacroAssembler for why (no branch we generate that uses the result
    // needs to be retargeted).
    return as_bc(off, computeConditionCode(cond, cr), lkb, lb);
}

BufferOffset Assembler::as_bc(BOffImm16 off, DoubleCondition cond,
        CRegisterID cr, LikelyBit lkb, LinkBit lb)
{
    // fall through to the next one
    return as_bc(off.encode(), cond, cr, lkb, lb);
}

BufferOffset Assembler::as_bc(int16_t off, DoubleCondition cond, CRegisterID cr,
        LikelyBit lkb, LinkBit lb)
{
    // Adjust for issued CR twiddles, if any.
    uint32_t offs = currentOffset();
    uint16_t op = computeConditionCode(cond, cr);
    offs = currentOffset() - offs;
    MOZ_ASSERT(offs == 0 || offs == 4); // currently zero or one instruction
    return as_bc((off-offs), op, lkb, lb);
}

// These are the actual instruction emitters after conditions are converted
// and any offsets recalculated.
BufferOffset Assembler::as_bc(int16_t off, uint16_t op, LikelyBit lkb, LinkBit lb)
{
    spew("bc%s%s BO_BI=0x%04x,%d", (lb) ? "l" : "", (lkb) ? "+" : "", op, off);
    MOZ_ASSERT(!(off & 0x03));
    return writeInst(Instruction(PPC_bc | makeOpMask(op) | lkb << 21 | ((uint16_t)off & 0xfffc) | lb).encode());
}
BufferOffset Assembler::as_bcctr(uint16_t op, LikelyBit lkb, LinkBit lb)
{
    spew("bcctr%s%s", (lb) ? "l" : "", (lkb) ? "+" : "");
    return writeInst(PPC_bcctr | makeOpMask(op) | lkb << 21 | lb);
}

// SPR operations.
BufferOffset Assembler::as_mtspr(SPRegisterID spr, Register ra)
{
    spew("mtspr\t%d,%3s", spr, ra.name());
    return writeInst(PPC_mtspr | ra.code() << 21 | PPC_SPR(spr) << 11);
}
BufferOffset Assembler::as_mfspr(Register rd, SPRegisterID spr)
{
    spew("mfspr\t%3s,%d", rd.name(), spr);
    return writeInst(PPC_mfspr | rd.code() << 21 | PPC_SPR(spr) << 11);
}

// CR operations.
#define DEF_CRCR(op) \
        BufferOffset Assembler::as_##op(uint8_t t, uint8_t a, uint8_t b) { \
            spew(#op"\t%d,%d,%d", t, a, b); \
            return writeInst(PPC_##op | t << 21 | a << 16 | b << 11); }
DEF_CRCR(crand)
DEF_CRCR(crandc)
DEF_CRCR(cror)
DEF_CRCR(crorc)
DEF_CRCR(crxor)
#undef DEF_CRCR

BufferOffset Assembler::as_mtcrf(uint32_t mask, Register rs)
{
    spew("mtcrf %d,%3s", mask, rs.name());
    return writeInst(PPC_mtcrf | rs.code() << 21 | mask << 12);
}

BufferOffset Assembler::as_mfcr(Register rd)
{
    spew("mfcr %3s", rd.name());
    return writeInst(PPC_mfcr | rd.code() << 21);
}

BufferOffset Assembler::as_mfocrf(Register rd, CRegisterID crfs)
{
    spew("mfocrf %3s,cr%d", rd.name(), crfs);
    return writeInst(PPC_mfocrf | rd.code() << 21 | crfs << 12);
}

BufferOffset Assembler::as_mcrxrx(CRegisterID cr)
{
    spew("mcrxrx\tcr%d", cr);
    return writeInst(PPC_mcrxrx | cr << 23);
}

// GPR operations and load-stores.
BufferOffset Assembler::as_neg(Register rd, Register rs)
{
    spew("neg %3s,%3s", rd.name(), rs.name());
    return writeInst(InstReg(PPC_neg, rd, rs, r0).encode());
}

BufferOffset Assembler::as_nego(Register rd, Register rs)
{
    spew("nego %3s,%3s", rd.name(), rs.name());
    return writeInst(InstReg(PPC_nego, rd, rs, r0).encode());
}

BufferOffset Assembler::as_cmpd(CRegisterID cr, Register ra, Register rb)
{
    spew("cmpd\tcr%d,%3s,%3s", cr, ra.name(), rb.name());
    return writeInst(PPC_cmpd | cr << 23 | ra.code() << 16 | rb.code() << 11);
}

BufferOffset Assembler::as_cmpdi(CRegisterID cr, Register ra, int16_t im)
{
    spew("cmpdi\tcr%d,%3s,%d", cr, ra.name(), im);
    return writeInst(PPC_cmpdi | cr << 23 | ra.code() << 16 | ((uint16_t)im & 0xffff));
}

BufferOffset Assembler::as_cmpld(CRegisterID cr, Register ra, Register rb)
{
    spew("cmpld\tcr%d,%3s,%3s", cr, ra.name(), rb.name());
    return writeInst(PPC_cmpld | cr << 23 | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmpldi(CRegisterID cr, Register ra, int16_t im)
{
    spew("cmpldi\tcr%d,%3s,%d", cr, ra.name(), im);
    return writeInst(PPC_cmpldi | cr << 23 | ra.code() << 16 | ((uint16_t)im & 0xffff));
}
BufferOffset Assembler::as_cmpw(CRegisterID cr, Register ra, Register rb)
{
    spew("cmpw\t%3s,%3s", ra.name(), rb.name());
    return writeInst(PPC_cmpw | cr << 23 | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmpwi(CRegisterID cr, Register ra, int16_t im)
{
    spew("cmpwi\tcr%d,%3s,%d", cr, ra.name(), im);
    return writeInst(PPC_cmpwi | cr << 23 | ra.code() << 16 | ((uint16_t)im & 0xffff));
}
BufferOffset Assembler::as_cmplw(CRegisterID cr, Register ra, Register rb)
{
    spew("cmplw\tcr%d,%3s,%3s", cr, ra.name(), rb.name());
    return writeInst(PPC_cmplw | cr << 23 | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmplwi(CRegisterID cr, Register ra, int16_t im)
{
    spew("cmplwi\tcr%d,%3s,%d", cr, ra.name(), im);
    return writeInst(PPC_cmplwi | cr << 23 | ra.code() << 16 | ((uint16_t)im & 0xffff));
}
BufferOffset Assembler::as_cmpd(Register ra, Register rb)
{
    spew("cmpd\t%3s,%3s", ra.name(), rb.name());
    return writeInst(PPC_cmpd | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmpdi(Register ra, int16_t im)
{
    spew("cmpdi\t%3s,%d", ra.name(), im);
    return writeInst(PPC_cmpdi | ra.code() << 16 | ((uint16_t)im & 0xffff));
}
BufferOffset Assembler::as_cmpld(Register ra, Register rb)
{
    spew("cmpld\t%3s,%3s", ra.name(), rb.name());
    return writeInst(PPC_cmpld | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmpldi(Register ra, int16_t im)
{
    spew("cmpldi\t%3s,%d", ra.name(), im);
    return writeInst(PPC_cmpldi | ra.code() << 16 | ((uint16_t)im & 0xffff));
}
BufferOffset Assembler::as_cmpw(Register ra, Register rb)
{
    spew("cmpw\t%3s,%3s", ra.name(), rb.name());
    return writeInst(PPC_cmpw | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmpwi(Register ra, int16_t im)
{
    spew("cmpwi\t%3s,%d", ra.name(), im);
    return writeInst(PPC_cmpwi | ra.code() << 16 | ((uint16_t)im & 0xffff));
}

BufferOffset Assembler::as_cmplw(Register ra, Register rb)
{
    spew("cmplw\t%3s,%3s", ra.name(), rb.name());
    return writeInst(PPC_cmplw | ra.code() << 16 | rb.code() << 11);
}
BufferOffset Assembler::as_cmplwi(Register ra, int16_t im)
{
    spew("cmplwi\t%3s,%d", ra.name(), im);
    return writeInst(PPC_cmplwi | ra.code() << 16 | ((uint16_t)im & 0xffff));
}

static uint32_t
AForm(uint32_t op, FloatRegister frt, FloatRegister fra, FloatRegister frb,
        FloatRegister frc, bool rc)
{
    return (op | (frt.encoding() << 21) | (fra.encoding() << 16) | (frb.encoding() << 11) |
            (frc.encoding() << 6) | rc);
}

static uint32_t
XForm(uint32_t op, FloatRegister frt, FloatRegister fra, FloatRegister frb, bool rc)
{
    return (op | (frt.encoding() << 21) | (fra.encoding() << 16) | (frb.encoding() << 11) | rc);
}

static uint32_t
XForm(uint32_t op, FloatRegister frt, Register ra, Register rb, bool rc)
{
    return (op | (frt.encoding() << 21) | (ra.code() << 16) | (rb.code() << 11) | rc);
}

static uint32_t
DForm(uint32_t op, FloatRegister frt, Register ra, int16_t imm)
{
    return (op | (frt.encoding() << 21) | (ra.code() << 16) | ((uint16_t)imm & 0xffff));
}

#define DEF_XFORM(op)   \
    BufferOffset Assembler::as_##op(Register rd, Register ra, Register rb) { \
        spew(#op "\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(InstReg(PPC_##op, rd, ra, rb).encode()); }

#define DEF_XFORM_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra, Register rb) {\
        spew(#op ".\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(InstReg(PPC_##op, rd, ra, rb).encode() | 0x1); }

#define DEF_XFORMS(op)   \
    BufferOffset Assembler::as_##op(Register rd, Register ra, Register rb) { \
        spew(#op "\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(InstReg(PPC_##op, ra, rd, rb).encode()); }

#define DEF_XFORMS_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra, Register rb) {\
        spew(#op ".\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(InstReg(PPC_##op, ra, rd, rb).encode() | 0x1); }

#define DEF_AFORM_C(op)   \
    BufferOffset Assembler::as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rc) { \
        spew(#op "\t%3s,%3s,%3s", rd.name(), ra.name(), rc.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, f0, rc, false)); }

#define DEF_AFORM_C_RC(op) \
    BufferOffset Assembler::as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rc) {\
        spew(#op ".\t%3s,%3s,%3s", rd.name(), ra.name(), rc.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, f0, rc, true)); }

#define DEF_AFORM_B(op)   \
    BufferOffset Assembler::as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rb) { \
        spew(#op "\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, rb, f0, false)); }

#define DEF_AFORM_B_RC(op) \
    BufferOffset Assembler::as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rb) {\
        spew(#op ".\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, rb, f0, true)); }

#define DEF_AFORM(op)   \
    BufferOffset Assembler::as_##op(FloatRegister rd, FloatRegister ra, FloatRegister rc, FloatRegister rb) { \
        spew(#op "\t%3s,%3s,%3s,%3s", rd.name(), ra.name(), rc.name(), rb.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, rb, rc, false)); }

#define DEF_AFORM_RC(op) \
    BufferOffset Assembler::as_##op##_rc(FloatRegister rd, FloatRegister ra, FloatRegister rc, FloatRegister rb) {\
        spew(#op ".\t%3s,%3s,%3s,%3s", rd.name(), ra.name(), rc.name(), rb.name()); \
        return writeInst(AForm(PPC_##op, rd, ra, rb, rc, true)); }

#define DEF_XFORMS_I(op)   \
    BufferOffset Assembler::as_##op(Register rd, Register ra, uint8_t sh) { \
        spew(#op "\t%3s,%3s,%d", rd.name(), ra.name(), sh); \
        return writeInst(PPC_##op | ra.code() << 21 | rd.code() << 16 | sh << 11); }

#define DEF_XFORMS_I_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra, uint8_t sh) {\
        spew(#op ".\t%3s,%3s,%d", rd.name(), ra.name(), sh); \
        return writeInst(PPC_##op | ra.code() << 21 | rd.code() << 16 | sh << 11 | 0x1); }

#define DEF_XFORM2(op)   \
    BufferOffset Assembler::as_##op(Register rd, Register ra) { \
        spew(#op "\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(InstReg(PPC_##op, rd, ra, r0).encode()); }

#define DEF_XFORM2_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra) {\
        spew(#op ".\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(InstReg(PPC_##op, rd, ra, r0).encode() | 0x1); }

#define DEF_XFORM2_F(op) \
    BufferOffset Assembler::as_##op(FloatRegister rd, FloatRegister ra) { \
        spew(#op "\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(XForm(PPC_##op, rd, f0, ra, false)); }

#define DEF_XFORM2_F_RC(op) \
    BufferOffset Assembler::as_##op##_rc(FloatRegister rd, FloatRegister ra) {\
        spew(#op ".\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(XForm(PPC_##op, rd, f0, ra, true)); }

#define DEF_XFORM2S(op)   \
    BufferOffset Assembler::as_##op(Register rd, Register ra) { \
        spew(#op "\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(InstReg(PPC_##op, ra, rd, r0).encode()); }

#define DEF_XFORM2S_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra) {\
        spew(#op ".\t%3s,%3s", rd.name(), ra.name()); \
        return writeInst(InstReg(PPC_##op, ra, rd, r0).encode() | 0x1); }

#define DEF_DFORM(op)   \
    BufferOffset Assembler::as_##op(Register ra, Register rs, int16_t im) { \
        spew(#op "\t%3s,%d(%3s)", ra.name(), im, rs.name()); \
        MOZ_ASSERT(rs != r0); \
        return writeInst(InstImm(PPC_##op, ra, rs, im).encode()); }

#define DEF_DFORMS(op)   \
    BufferOffset Assembler::as_##op(Register ra, Register rs, uint16_t im) { \
        spew(#op "\t%3s,%d(%3s)", ra.name(), im, rs.name()); \
        return writeInst(InstImm(PPC_##op, rs, ra, im).encode()); }

#define DEF_DFORM_F(op)   \
    BufferOffset Assembler::as_##op(FloatRegister rt, Register ra, int16_t im) { \
        spew(#op "\t%3s,%d(%3s)", rt.name(), im, ra.name()); \
        MOZ_ASSERT(ra != r0); \
        return writeInst(DForm(PPC_##op, rt, ra, im)); }

#define DEF_MFORM(op) \
    BufferOffset Assembler::as_##op(Register ra, Register rs, Register rb, uint8_t mb, uint8_t me) {\
        spew(#op "\t%3s,%3s,%3s,%d,%d", ra.name(), rs.name(), rb.name(), mb, me); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | rb.code() << 11 | mb << 6 | me << 1); }

#define DEF_MFORM_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register ra, Register rs, Register rb, uint8_t mb, uint8_t me) {\
        spew(#op ".\t%3s,%3s,%3s,%d,%d", ra.name(), rs.name(), rb.name(), mb, me); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | rb.code() << 11 | mb << 6 | me << 1 | 1); }

#define DEF_MFORM_I(op) \
    BufferOffset Assembler::as_##op(Register ra, Register rs, uint8_t sh, uint8_t mb, uint8_t me) {\
        spew(#op "\t%3s,%3s,%d,%d,%d", ra.name(), rs.name(), sh, mb, me); \
        MOZ_ASSERT(sh < 32); \
        MOZ_ASSERT(mb < 32); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | sh << 11 | mb << 6 | me << 1); }

#define DEF_MFORM_I_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register ra, Register rs, uint8_t sh, uint8_t mb, uint8_t me) {\
        spew(#op ".\t%3s,%3s,%d,%d,%d", ra.name(), rs.name(), sh, mb, me); \
        MOZ_ASSERT(sh < 32); \
        MOZ_ASSERT(mb < 32); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | sh << 11 | mb << 6 | me << 1 | 1); }

#define DEF_MDSFORM(op) \
    BufferOffset Assembler::as_##op(Register ra, Register rs, Register rb, uint8_t mb) {\
        spew(#op "\t%3s,%3s,%3s,%d", ra.name(), rs.name(), rb.name(), mb); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | rb.code() << 11 | mb << 6); }

#define DEF_MDSFORM_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register ra, Register rs, Register rb, uint8_t mb) {\
        spew(#op ".\t%3s,%3s,%3s,%d", ra.name(), rs.name(), rb.name(), mb); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | rb.code() << 11 | mb << 6); }

/* NOTHING documents these encodings well, not OPPCC, not even the 3.1 ISA book. */
#define DEF_MDFORM(op) \
    BufferOffset Assembler::as_##op(Register ra, Register rs, uint8_t sh, uint8_t mb) {\
        spew(#op "\t%3s,%3s,%d,%d", ra.name(), rs.name(), sh, mb); \
        MOZ_ASSERT(sh < 64); MOZ_ASSERT(mb < 64); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | ((sh & 0x1f) << 11) | ((mb & 0x1f) << 6) | (mb & 0x20) | ((sh & 0x20) >> 4)); }

#define DEF_MDFORM_RC(op) \
    BufferOffset Assembler::as_##op##_rc(Register ra, Register rs, uint8_t sh, uint8_t mb) {\
        spew(#op ".\t%3s,%3s,%d,%d", ra.name(), rs.name(), sh, mb); \
        MOZ_ASSERT(sh < 64); MOZ_ASSERT(mb < 64); \
        return writeInst(PPC_##op | rs.code() << 21 | ra.code() << 16 | ((sh & 0x1f) << 11) | ((mb & 0x1f) << 6) | (mb & 0x20) | ((sh & 0x20) >> 4) | 0x01); }

DEF_MFORM(rlwnm)
DEF_MFORM_I(rlwinm)
DEF_MFORM_I_RC(rlwinm)
DEF_MFORM_I(rlwimi)
DEF_XFORMS_I(srawi)

DEF_MDSFORM(rldcl)
//DEF_MDSFORM_RC(rldcl)
//DEF_MDSFORM(rldcr)
DEF_MDFORM(rldicl)
DEF_MDFORM_RC(rldicl)
DEF_MDFORM(rldicr)
DEF_MDFORM_RC(rldicr)
DEF_MDFORM(rldimi)
//DEF_MDFORM_RC(rldimi)
BufferOffset Assembler::as_sradi(Register rd, Register rs, int sh)
{
    spew("sradi\t%3s,%3s,%d", rd.name(), rs.name(), sh);
    return writeInst(PPC_sradi | rd.code() << 16 | rs.code() << 21 |
            (sh & 0x1f) << 11 | (sh & 0x20) >> 4);
}

#define DEF_ALU2(op)    DEF_XFORM(op) DEF_XFORM_RC(op)

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

#define DEF_ALU2_NORC(op)   DEF_XFORM(op)
DEF_ALU2_NORC(modsd)
DEF_ALU2_NORC(modud)
DEF_ALU2_NORC(modsw)
DEF_ALU2_NORC(moduw)
#undef DEF_ALU2_NORC

#define DEF_ALUI(op)    \
    BufferOffset Assembler::as_##op(Register rd, Register ra, int16_t im) { \
        spew(#op "\t%3s,%3s,%d", rd.name(), ra.name(), im); \
        return writeInst(InstImm(PPC_##op, rd, ra, im).encode()); }\
    BufferOffset Assembler::as_##op##_rc(Register rd, Register ra, int16_t im) { \
        spew(#op ".\t%3s,%3s,%d", rd.name(), ra.name(), im); \
        return writeInst(InstImm(PPC_##op, rd, ra, im).encode() | 0x1); }
// mscdfr0
BufferOffset Assembler::as_addi(Register rd, Register ra, int16_t im, bool actually_li) {
#if DEBUG
    if (actually_li) {
        spew("li\t%3s,%d", rd.name(), im);
    } else {
        MOZ_ASSERT(ra != r0); // Because that would be li
        spew("addi\t%3s,%3s,%d", rd.name(), ra.name(), im);
    }
#endif
    return writeInst(InstImm(PPC_addi, rd, ra, im).encode());
}
BufferOffset Assembler::as_addis(Register rd, Register ra, int16_t im, bool actually_lis) {
#if DEBUG
    if (actually_lis) {
        spew("lis\t%3s,%d", rd.name(), im);
    } else {
        MOZ_ASSERT(ra != r0); // Because that would be lis
        spew("addis\t%3s,%3s,%d", rd.name(), ra.name(), im);
    }
#endif
    return writeInst(InstImm(PPC_addis, rd, ra, im).encode());
}

DEF_ALUI(addic)
// NB: mulli is usually strength-reduced, since it can take up to five
// cycles in the worst case. See xs_sr_mulli.
DEF_ALUI(mulli)
DEF_ALUI(subfic)
#undef DEF_ALUI

#define DEF_ALUE(op) DEF_XFORM2(op) DEF_XFORM2_RC(op)
DEF_ALUE(addme)
DEF_ALUE(addze)
DEF_ALUE(subfze)
#undef DEF_ALUE

#define DEF_ALUE(op) DEF_XFORM2S(op) DEF_XFORM2S_RC(op)
DEF_ALUE(cntlzw) // NB: In this case, rd = ra and ra = rs, but no biggie here.
DEF_ALUE(cntlzd) // NB: In this case, rd = ra and ra = rs, but no biggie here.
DEF_ALUE(cnttzd) // NB: In this case, rd = ra and ra = rs, but no biggie here.
DEF_ALUE(cnttzw) // NB: In this case, rd = ra and ra = rs, but no biggie here.
#undef DEF_ALUE

DEF_XFORM2S(popcntd)
DEF_XFORM2S(popcntw)

#define DEF_BITALU2(op) DEF_XFORMS(op) DEF_XFORMS_RC(op)
DEF_BITALU2(andc)
DEF_BITALU2(nand)
DEF_BITALU2(nor)
DEF_BITALU2(slw)
DEF_BITALU2(srw)
DEF_BITALU2(sraw)
DEF_BITALU2(sld)
DEF_BITALU2(srd)
DEF_BITALU2(srad)
DEF_BITALU2(and)
//DEF_BITALU2(or)
    BufferOffset Assembler::as_or(Register rd, Register ra, Register rb) {
        spew("or\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name());
        MOZ_ASSERT(!(rd == ra && ra == rb));
        return writeInst(InstReg(PPC_or, ra, rd, rb).encode()); }

    BufferOffset Assembler::as_or_rc(Register rd, Register ra, Register rb) {
        spew("or.\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name());
        return writeInst(InstReg(PPC_or, ra, rd, rb).encode() | 0x1); }

DEF_BITALU2(xor)
#undef DEF_BITALU2

// No Rc bit for these.
#define DEF_BITALUI(op) DEF_DFORMS(op)
DEF_BITALUI(ori)
DEF_BITALUI(oris)
DEF_BITALUI(xori)
DEF_BITALUI(xoris)
#undef DEF_BITALUI
// Implied Rc bit for these.
    BufferOffset Assembler::as_andi_rc(Register ra, Register rs, uint16_t im) {
        spew("andi.\t%3s,%3s,%d", ra.name(), rs.name(), im);
        return writeInst(InstImm(PPC_andi, rs, ra, im).encode()); }

    BufferOffset Assembler::as_andis_rc(Register ra, Register rs, uint16_t im) {
        spew("andis.\t%3s,%3s,%d", ra.name(), rs.name(), im);
        return writeInst(InstImm(PPC_andis, rs, ra, im).encode()); }
        
#define DEF_ALUEXT(op) DEF_XFORM2S(op) DEF_XFORM2S_RC(op)
DEF_ALUEXT(extsb)
DEF_ALUEXT(extsh)
DEF_ALUEXT(extsw)
#undef DEF_ALUEXT

#define DEF_MEMd(op) DEF_DFORM(op)
DEF_MEMd(lbz)
DEF_MEMd(lha)
DEF_MEMd(lhz)
    BufferOffset Assembler::as_lwa(Register ra, Register rs, int16_t im) {
        spew("lwa\t%3s,%d(%3s)", ra.name(), im, rs.name());
        MOZ_ASSERT(rs != r0);
        MOZ_ASSERT(!(im & 0x03));
        return writeInst(InstImm(PPC_lwa, ra, rs, im).encode()); }

DEF_MEMd(lwz)
//DEF_MEMd(ld)
// Assert if the two LSBs of ld's immediate are set, since this assembles
// to a different instruction. (E.g., if we want ldu, we should ask for it.)
    BufferOffset Assembler::as_ld(Register ra, Register rs, int16_t im) {
        spew("ld\t%3s,%d(%3s)", ra.name(), im, rs.name());
        MOZ_ASSERT(rs != r0);
        MOZ_ASSERT(!(im & 0x03));
        return writeInst(InstImm(PPC_ld, ra, rs, im).encode()); }

DEF_MEMd(stb)
DEF_MEMd(stw)
DEF_MEMd(stwu)
DEF_MEMd(sth)
//DEF_MEMd(std)
    BufferOffset Assembler::as_std(Register ra, Register rs, int16_t im) {
        spew("std\t%3s,%d(%3s)", ra.name(), im, rs.name());
        MOZ_ASSERT(rs != r0);
        MOZ_ASSERT(!(im & 0x03));
        return writeInst(InstImm(PPC_std, ra, rs, im).encode()); }
DEF_MEMd(stdu)
#undef DEF_MEMd

#define DEF_MEMx(op) DEF_XFORM(op)
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

BufferOffset Assembler::as_isel(Register rt, Register ra, Register rb, uint16_t bc, CRegisterID cr)
{
    MOZ_ASSERT(ra != r0); // mscdfr0, but see below, because sometimes we want this
    return as_isel0(rt, ra, rb, bc, cr);
}

// This variant allows ra to be r0, because sometimes we feel like a zero.
// Sometimes you don't. Almond Joy's got nuts, Mounds don't.
BufferOffset Assembler::as_isel0(Register rt, Register ra, Register rb, uint16_t bc, CRegisterID cr)
{
    spew("isel\t%3s,%3s,%3s,cr%d:0x%02x", rt.name(), ra.name(), rb.name(), cr, bc);
    // Only bits that can be directly tested for in the CR are valid.
    // The upper nybble of the condition contains the CR bit.
    MOZ_ASSERT((bc < 0x40) && ((bc & 0x0f) == 0x0c));
    uint16_t nbc = (bc >> 4) + (cr << 2);
    return writeInst(PPC_isel | rt.code() << 21 | ra.code() << 16 | rb.code() << 11 | nbc << 6);
}

// FPR operations and load-stores.
BufferOffset Assembler::as_fcmpo(CRegisterID cr, FloatRegister ra, FloatRegister rb)
{
    spew("fcmpo\t%d,%3s,%3s", cr, ra.name(), rb.name());
    return writeInst(PPC_fcmpo | cr << 23 | ra.encoding() << 16 | rb.encoding() << 11);
}

BufferOffset Assembler::as_fcmpo(FloatRegister ra, FloatRegister rb)
{
    return as_fcmpo(cr0, ra, rb);
}

BufferOffset Assembler::as_fcmpu(CRegisterID cr, FloatRegister ra, FloatRegister rb)
{
    spew("fcmpu\t%d,%3s,%3s", cr, ra.name(), rb.name());
    return writeInst(PPC_fcmpu | cr << 23 | ra.encoding() << 16 | rb.encoding() << 11);
}

BufferOffset Assembler::as_fcmpu(FloatRegister ra, FloatRegister rb)
{
    return as_fcmpu(cr0, ra, rb);
}

#define DEF_FPUAC(op) DEF_AFORM_C(op) DEF_AFORM_C_RC(op)
DEF_FPUAC(fmul)
DEF_FPUAC(fmuls)
#undef DEF_FPUAC

#define DEF_FPUAB(op) DEF_AFORM_B(op) DEF_AFORM_B_RC(op)
DEF_FPUAB(fadd)
DEF_FPUAB(fdiv)
DEF_FPUAB(fsub)
DEF_FPUAB(fadds)
DEF_FPUAB(fdivs)
DEF_FPUAB(fsubs)
DEF_FPUAB(fcpsgn)
DEF_FPUAB(fmrgew) /* rc form invalid */
#undef DEF_FPUAB

#define DEF_FPUDS(op) DEF_XFORM2_F(op) DEF_XFORM2_F_RC(op)
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
#define DEF_FPUACB(op) DEF_AFORM(op) DEF_AFORM_RC(op)
DEF_FPUACB(fmadd)
DEF_FPUACB(fnmsub)
DEF_FPUACB(fsel)
#undef DEF_FPUACB

#define DEF_FMEMd(op) DEF_DFORM_F(op)
DEF_FMEMd(lfd)
DEF_FMEMd(lfs)
DEF_FMEMd(stfd)
DEF_FMEMd(stfs)
DEF_FMEMd(stfdu)
DEF_FMEMd(stfsu)
#undef DEF_FMEMd

#define DEF_FMEMx(op) \
    BufferOffset Assembler::as_##op(FloatRegister rd, Register ra, Register rb) { \
        spew(#op "\t%3s,%3s,%3s", rd.name(), ra.name(), rb.name()); \
        return writeInst(XForm(PPC_##op, rd, ra, rb, false)); }
DEF_FMEMx(lfdx)
DEF_FMEMx(lfsx)
DEF_FMEMx(lfiwax)
DEF_FMEMx(stfiwx)
DEF_FMEMx(stfdx)
DEF_FMEMx(stfsx)
#undef DEF_FMEMx

BufferOffset Assembler::as_mtfsb0(uint8_t bt)
{
    spew("mtfsb0\t%d", bt);
    return writeInst(PPC_mtfsb0 | (uint32_t)bt << 21);
}

BufferOffset Assembler::as_mtfsb1(uint8_t bt)
{
    spew("mtfsb1\t%d", bt);
    return writeInst(PPC_mtfsb1 | (uint32_t)bt << 21);
}

BufferOffset Assembler::as_mtfsfi(uint8_t fi, uint8_t imm)
{
    spew("mtfsfi\t%d,%d", fi, imm);
    return writeInst(PPC_mtfsfi | fi << 23 | imm << 12);
}

BufferOffset Assembler::as_mcrf(CRegisterID bt, CRegisterID bs)
{
    spew("mcrf\t%d,%d", bt, bs);
    return writeInst(PPC_mcrf | (uint32_t)bt << 23 | (uint32_t)bs << 18);
}

BufferOffset Assembler::as_mcrfs(CRegisterID bf, uint8_t bfa)
{
    spew("mcrfs\t%d,%d", bf, bfa);
    return writeInst(PPC_mcrfs | (uint32_t)bf << 23 | (uint32_t)bfa << 18);
}

// VSX
// Currently only supported for FPRs.
// No RC forms for these (least significant bit sets vector or FPR).
    BufferOffset Assembler::as_mfvsrd(Register ra, FloatRegister xs) {
        spew("mfvsrd\t%3s,%3s", ra.name(), xs.name());
        return writeInst(XForm(PPC_mfvsrd, xs, ra, r0, false));
    }
    BufferOffset Assembler::as_mtvsrd(FloatRegister xt, Register ra) {
        spew("mtvsrd\t%3s,%3s", xt.name(), ra.name());
        // Yes, same operand order (see PowerISA v3.1 page 121)
        return writeInst(XForm(PPC_mtvsrd, xt, ra, r0, false));
    }
    BufferOffset Assembler::as_mtvsrws(FloatRegister xt, Register ra) {
        spew("mtvsrws\t%3s,%3s", xt.name(), ra.name());
        return writeInst(XForm(PPC_mtvsrws, xt, ra, r0, false));
    }
    BufferOffset Assembler::as_mtvsrwz(FloatRegister xt, Register ra) {
        spew("mtvsrwz\t%3s,%3s", xt.name(), ra.name());
        return writeInst(XForm(PPC_mtvsrwz, xt, ra, r0, false));
    }
    BufferOffset Assembler::as_xxbrd(FloatRegister xt, FloatRegister xb) {
        spew("xxbrd\t%3s,%3s", xt.name(), xb.name());
        return writeInst(XForm(PPC_xxbrd, xt, f0, xb, false));
    }
    BufferOffset Assembler::as_xscvdpsp(FloatRegister xt, FloatRegister xb) {
        spew("xscvdpsp\t%3s,%3s", xt.name(), xb.name());
        return writeInst(XForm(PPC_xscvdpsp, xt, f0, xb, false));
    }
    BufferOffset Assembler::as_xscvspdp(FloatRegister xt, FloatRegister xb) {
        spew("xscvspdp\t%3s,%3s", xt.name(), xb.name());
        return writeInst(XForm(PPC_xscvspdp, xt, f0, xb, false));
    }
    BufferOffset Assembler::as_xscvdpspn(FloatRegister xt, FloatRegister xb) {
        spew("xscvdpspn\t%3s,%3s", xt.name(), xb.name());
        return writeInst(XForm(PPC_xscvdpspn, xt, f0, xb, false));
    }
    BufferOffset Assembler::as_xscvspdpn(FloatRegister xt, FloatRegister xb) {
        spew("xscvspdpn\t%3s,%3s", xt.name(), xb.name());
        return writeInst(XForm(PPC_xscvspdpn, xt, f0, xb, false));
    }
    BufferOffset Assembler::as_xxlxor(FloatRegister xt, FloatRegister xa, FloatRegister xb) {
        spew("xxlxor\t%3s,%3s,%3s", xt.name(), xa.name(), xb.name());
        return writeInst(XForm(PPC_xxlxor, xt, xa, xb, false));
    }

    BufferOffset Assembler::as_addpcis(Register rt, uint16_t im) {
        spew("addpcis\t%s,%d", rt.name(), im);
        MOZ_ASSERT(im == 0); // not implemented for anything other than lnia

        return writeInst(PPC_addpcis | (rt.code() << 21));
    }

// Conveniences and generally accepted alternate mnemonics.
// XXX: change these to xs_
BufferOffset Assembler::xs_trap()
{
    spew("trap @ %08x", currentOffset());
    return writeInst(PPC_trap);
}
// trap with metadata encoded as register
BufferOffset Assembler::xs_trap_tagged(TrapTag tag) {
    uint32_t tv = PPC_trap | ((uint8_t)tag << 16) | ((uint8_t)tag << 11);
    spew("trap @ %08x ; MARK %d %08x", currentOffset(), (uint8_t)tag, tv);
    return writeInst(tv);
}
Assembler::TrapTag InstImm::traptag() {
    // Extract a tag from a tagged trap instruction.
    uint8_t r = ((data & 0x001f0000) >> 16);
    MOZ_ASSERT(isOpcode(PPC_tw)); // not a trap
    MOZ_ASSERT(r == ((data & 0x0000f800) >> 11)); // probably not a tagged trap
    return (Assembler::TrapTag)(r & 0xfe); // mask bit 0
}

BufferOffset Assembler::xs_mr(Register rd, Register ra)
{
    return as_or(rd, ra, ra);
}

BufferOffset Assembler::x_beq(CRegisterID cr, int16_t off, LikelyBit lkb, LinkBit lb)
{
    return as_bc(off, Equal, cr, lkb, lb);
}

BufferOffset Assembler::x_bne(CRegisterID cr, int16_t off, LikelyBit lkb, LinkBit lb)
{
    return as_bc(off, NotEqual, cr, lkb, lb);
}

BufferOffset Assembler::xs_bdnz(int16_t off, LikelyBit lkb, LinkBit lb)
{
    spew("bdnz .+%d", off);
    MOZ_ASSERT(!(off & 0x03));
    return writeInst(PPC_bc | (0x10 << 21) | (off & 0xfffc) | lkb << 21 | lb);
}

// Emit specialized bcl form to avoid tainting branch history.
// Link bit is implied; this is meaningless without it.
BufferOffset Assembler::xs_bcl_always(int16_t off, LikelyBit lkb)
{
    spew("bcl 20,4*cr7+so,.+%d", off);
    MOZ_ASSERT(!(off & 0x03));
    return writeInst(PPC_bc | (20 << 21) | (31 << 16) | (off & 0xfffc) | 0x01);
}

BufferOffset Assembler::xs_mtctr(Register ra)
{
    return as_mtspr(ctr, ra);
}

BufferOffset Assembler::xs_mtlr(Register ra)
{
    return as_mtspr(lr_spr, ra);
}

BufferOffset Assembler::xs_mflr(Register rd)
{
    return as_mfspr(rd, lr_spr);
}

BufferOffset Assembler::xs_mtcr(Register rs)
{
    return as_mtcrf(0xff, rs);
}

BufferOffset Assembler::xs_mfxer(Register ra)
{
    return as_mfspr(ra, xer);
}

BufferOffset Assembler::xs_mtxer(Register ra)
{
    return as_mtspr(xer, ra);
}

BufferOffset Assembler::x_bit_value(Register rd, Register rs, unsigned bit)
{
    return as_rlwinm(rd, rs, bit + 1, 31, 31);
}

BufferOffset Assembler::x_slwi(Register rd, Register rs, int n)
{
    return as_rlwinm(rd, rs, n, 0, 31 - n);
}

BufferOffset Assembler::x_sldi(Register rd, Register rs, int n)
{
    return as_rldicr(rd, rs, n, 63 - n);
}

BufferOffset Assembler::x_srwi(Register rd, Register rs, int n)
{
    return as_rlwinm(rd, rs, 32 - n, n, 31);
}

BufferOffset Assembler::x_srdi(Register rd, Register rs, int n)
{
    return as_rldicl(rd, rs, 64 - n, n);
}

BufferOffset Assembler::x_subi(Register rd, Register ra, int16_t im)
{
    return as_addi(rd, ra, -im);
}

BufferOffset Assembler::xs_li(Register rd, int16_t im)
{
    return as_addi(rd, r0, im, true /* actually_li */);
}

BufferOffset Assembler::xs_lis(Register rd, int16_t im)
{
    return as_addis(rd, r0, im, true /* actually_lis */);
}

BufferOffset Assembler::x_not(Register rd, Register ra)
{
    return as_nor(rd, ra, ra);
}

BufferOffset Assembler::xs_sr_mulli(Register rd, Register ra, int16_t im)
{
    // XXX: expand this
    if (im == -1) {
        return as_neg(rd, ra);
    }
    if (im == 0) {
        return xs_li(rd, 0);
    }
    if (im == 1) {
        if (ra != rd) {
            return xs_mr(rd, ra);
        }
        return BufferOffset(currentOffset());
    }
    if (im == 2) {
        return as_add(rd, ra, ra);
    }
    if (im == 3 && rd != ra) {
        as_add(rd, ra, ra);
        return as_add(rd, rd, ra);
    }
    // XXX: convert 2^x to bit shift

    return as_mulli(rd, ra, im);
}

// Traps
BufferOffset Assembler::as_tw(uint8_t to, Register ra, Register rb)
{
    return writeInst(PPC_tw | (uint32_t)to << 21 | ra.code() << 16 |
            rb.code() << 11);
}

BufferOffset Assembler::as_twi(uint8_t to, Register ra, int16_t si)
{
    return writeInst(PPC_twi | (uint32_t)to << 21 | ra.code() << 16 | si);
}

BufferOffset Assembler::as_stop()
{
    spew("stop!!!\n");
    return writeInst(PPC_stop);
}
