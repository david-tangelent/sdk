// Copyright (c) 2021, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"
#if defined(TARGET_ARCH_RISCV32) || defined(TARGET_ARCH_RISCV64)

#include "vm/dart_entry.h"
#include "vm/isolate.h"
#include "vm/native_entry.h"
#include "vm/native_entry_test.h"
#include "vm/object.h"
#include "vm/runtime_entry.h"
#include "vm/stub_code.h"
#include "vm/symbols.h"
#include "vm/unit_test.h"

#define __ assembler->

namespace dart {

static Function* CreateFunction(const char* name) {
  const String& class_name =
      String::Handle(Symbols::New(Thread::Current(), "ownerClass"));
  const Script& script = Script::Handle();
  const Library& lib = Library::Handle(Library::New(class_name));
  const Class& owner_class = Class::Handle(
      Class::New(lib, class_name, script, TokenPosition::kNoSource));
  const String& function_name =
      String::ZoneHandle(Symbols::New(Thread::Current(), name));
  const FunctionType& signature = FunctionType::ZoneHandle(FunctionType::New());
  Function& function = Function::ZoneHandle(Function::New(
      signature, function_name, UntaggedFunction::kRegularFunction, true, false,
      false, false, false, owner_class, TokenPosition::kNoSource));
  return &function;
}

// Test calls to stub code which calls into the runtime.
static void GenerateCallToCallRuntimeStub(compiler::Assembler* assembler,
                                          int length) {
  const int argc = 2;
  const Smi& smi_length = Smi::ZoneHandle(Smi::New(length));
  __ EnterDartFrame(0);
  __ PushObject(Object::null_object());  // Push Null obj for return value.
  __ PushObject(smi_length);             // Push argument 1: length.
  __ PushObject(Object::null_object());  // Push argument 2: type arguments.
  ASSERT(kAllocateArrayRuntimeEntry.argument_count() == argc);
  __ CallRuntime(kAllocateArrayRuntimeEntry, argc);
  __ addi(SP, SP, argc * kWordSize);
  __ PopRegister(A0);  // Pop return value from return slot.
  __ LeaveDartFrame();
  __ ret();
}

ISOLATE_UNIT_TEST_CASE(CallRuntimeStubCode) {
  extern const Function& RegisterFakeFunction(const char* name,
                                              const Code& code);
  const int length = 10;
  const char* kName = "Test_CallRuntimeStubCode";
  compiler::ObjectPoolBuilder object_pool_builder;
  compiler::Assembler assembler(&object_pool_builder);
  GenerateCallToCallRuntimeStub(&assembler, length);
  SafepointWriteRwLocker ml(thread, thread->isolate_group()->program_lock());
  const Code& code = Code::Handle(Code::FinalizeCodeAndNotify(
      *CreateFunction("Test_CallRuntimeStubCode"), nullptr, &assembler,
      Code::PoolAttachment::kAttachPool));
  const Function& function = RegisterFakeFunction(kName, code);
  Array& result = Array::Handle();
  result ^= DartEntry::InvokeFunction(function, Object::empty_array());
  EXPECT_EQ(length, result.Length());
}

// Test calls to stub code which calls into a leaf runtime entry.
static void GenerateCallToCallLeafRuntimeStub(compiler::Assembler* assembler,
                                              const char* str_value,
                                              intptr_t lhs_index_value,
                                              intptr_t rhs_index_value,
                                              intptr_t length_value) {
  const String& str = String::ZoneHandle(String::New(str_value, Heap::kOld));
  const Smi& lhs_index = Smi::ZoneHandle(Smi::New(lhs_index_value));
  const Smi& rhs_index = Smi::ZoneHandle(Smi::New(rhs_index_value));
  const Smi& length = Smi::ZoneHandle(Smi::New(length_value));
  __ EnterDartFrame(0);
  __ ReserveAlignedFrameSpace(0);
  __ LoadObject(A0, str);
  __ LoadObject(A1, lhs_index);
  __ LoadObject(A2, rhs_index);
  __ LoadObject(A3, length);
  __ CallRuntime(kCaseInsensitiveCompareUCS2RuntimeEntry, 4);
  __ LeaveDartFrame();
  __ ret();  // Return value is in A0.
}

ISOLATE_UNIT_TEST_CASE(CallLeafRuntimeStubCode) {
  extern const Function& RegisterFakeFunction(const char* name,
                                              const Code& code);
  const char* str_value = "abAB";
  intptr_t lhs_index_value = 0;
  intptr_t rhs_index_value = 2;
  intptr_t length_value = 2;
  const char* kName = "Test_CallLeafRuntimeStubCode";
  compiler::ObjectPoolBuilder object_pool_builder;
  compiler::Assembler assembler(&object_pool_builder);
  GenerateCallToCallLeafRuntimeStub(&assembler, str_value, lhs_index_value,
                                    rhs_index_value, length_value);
  SafepointWriteRwLocker ml(thread, thread->isolate_group()->program_lock());
  const Code& code = Code::Handle(Code::FinalizeCodeAndNotify(
      *CreateFunction("Test_CallLeafRuntimeStubCode"), nullptr, &assembler,
      Code::PoolAttachment::kAttachPool));
  const Function& function = RegisterFakeFunction(kName, code);
  Instance& result = Instance::Handle();
  result ^= DartEntry::InvokeFunction(function, Object::empty_array());
  EXPECT_EQ(Bool::True().ptr(), result.ptr());
}

}  // namespace dart

#endif  // defined TARGET_ARCH_RISCV
