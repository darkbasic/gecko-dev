/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ppc64le_SharedICHelpers_ppc64le_inl_h
#define jit_ppc64le_SharedICHelpers_ppc64le_inl_h

#include "jit/SharedICHelpers.h"

#include "jit/MacroAssembler-inl.h"

namespace js {
namespace jit {

inline void
EmitBaselineTailCallVM(TrampolinePtr target, MacroAssembler& masm, uint32_t argSize)
{
    Register scratch = R2.scratchReg();

    // Compute frame size.
    masm.movePtr(BaselineFrameReg, scratch);
    masm.addPtr(Imm32(BaselineFrame::FramePointerOffset), scratch);
    masm.subPtr(BaselineStackReg, scratch);

#ifdef DEBUG
    // Store frame size without VMFunction arguments for debug assertions.
    masm.subPtr(Imm32(argSize), scratch);
    Address frameSizeAddr(BaselineFrameReg,
            BaselineFrame::reverseOffsetOfDebugFrameSize());
    masm.store32(scratch, frameSizeAddr);
    masm.addPtr(Imm32(argSize), scratch);
#endif

    // Push frame descriptor and perform the tail call.
    // ICTailCallReg (LR) already contains the return address (as we
    // keep it there through the stub calls), but the VMWrapper code being
    // called expects the return address to also be pushed on the stack.
    masm.makeFrameDescriptor(scratch, FrameType::BaselineJS, ExitFrameLayout::Size());
    masm.subPtr(Imm32(sizeof(CommonFrameLayout)), StackPointer);
    // Keep the tail call register current (i.e., don't just use r0).
    masm.xs_mflr(ICTailCallReg);
    masm.storePtr(scratch, Address(StackPointer, CommonFrameLayout::offsetOfDescriptor()));
    masm.storePtr(ICTailCallReg, Address(StackPointer, CommonFrameLayout::offsetOfReturnAddress()));

    masm.jump(target);
}

/*
inline void
EmitIonTailCallVM(TrampolinePtr target, MacroAssembler& masm, uint32_t stackSize)
{
    Register scratch = R2.scratchReg();

    masm.loadPtr(Address(sp, stackSize), scratch);
    masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), scratch);
    masm.addPtr(Imm32(stackSize + JitStubFrameLayout::Size() - sizeof(intptr_t)), scratch);

    // Push frame descriptor and return address, perform the tail call.
    masm.makeFrameDescriptor(scratch, FrameType::IonJS, ExitFrameLayout::Size());
    masm.xs_mflr(ScratchRegister);
    masm.push(scratch);
    masm.push(ScratchRegister);
    masm.jump(target);
}
*/

inline void
EmitBaselineCreateStubFrameDescriptor(MacroAssembler& masm, Register reg, uint32_t headerSize)
{
    // Compute stub frame size. We have to add two pointers: the stub reg and
    // previous frame pointer pushed by EmitEnterStubFrame.
    masm.as_addi(reg, BaselineFrameReg, sizeof(intptr_t)*2);
    masm.subPtr(BaselineStackReg, reg);

    masm.makeFrameDescriptor(reg, FrameType::BaselineStub, headerSize);
}

inline void
EmitBaselineCallVM(TrampolinePtr target, MacroAssembler& masm)
{
    Register scratch = R2.scratchReg();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
    masm.push(scratch);
    masm.call(target);
}

inline void
EmitBaselineEnterStubFrame(MacroAssembler& masm, Register scratch)
{
    // Compute frame size.
    masm.as_addi(scratch, BaselineFrameReg, BaselineFrame::FramePointerOffset);
    masm.subPtr(BaselineStackReg, scratch);

#ifdef DEBUG
    Address frameSizeAddr(BaselineFrameReg,
            BaselineFrame::reverseOffsetOfDebugFrameSize());
    masm.store32(scratch, frameSizeAddr);
#endif

    // Note: when making changes here, don't forget to update
    // BaselineStubFrame if needed.

    // Push frame descriptor and return address.
    masm.makeFrameDescriptor(scratch, FrameType::BaselineJS, BaselineStubFrameLayout::Size());
    // Keep the tail call register current (i.e., don't just use r0).
    masm.xs_mflr(ICTailCallReg);
    masm.subPtr(Imm32(STUB_FRAME_SIZE), StackPointer);
    masm.storePtr(scratch, Address(StackPointer, offsetof(BaselineStubFrame, descriptor)));
    masm.storePtr(ICTailCallReg, Address(StackPointer,
                                      offsetof(BaselineStubFrame, returnAddress)));

    // Save old frame pointer, stack pointer and stub reg.
    masm.storePtr(ICStubReg, Address(StackPointer,
                                           offsetof(BaselineStubFrame, savedStub)));
    masm.storePtr(BaselineFrameReg, Address(StackPointer,
                                            offsetof(BaselineStubFrame, savedFrame)));
    masm.movePtr(BaselineStackReg, BaselineFrameReg);

    // Stack should remain aligned.
    masm.assertStackAlignment(sizeof(Value), 0);
}

} // namespace jit
} // namespace js

#endif /* jit_ppc64le_SharedICHelpers_ppc64le_inl_h */
