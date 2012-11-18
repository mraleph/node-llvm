// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <node.h>

#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/PassManager.h"
#include "llvm/InlineAsm.h"
#include "llvm/Intrinsics.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/TargetSelect.h"

#include "wrappers.h"
#include "bindings-helpers.h"

inline void* MakeIRBuilder(const v8::Arguments& args) {
  return new llvm::IRBuilder<> (llvm::getGlobalContext());
}

inline void* MakeModule(const v8::Arguments& args) {
  if (args.Length() != 1 && !args[0]->IsString()) {
    THROW_ERROR("Module constructor expected 1 string argument");
    return NULL;
  }
  v8::String::AsciiValue name(args[0]);
  return new llvm::Module(*name, llvm::getGlobalContext());
}

Wrapper<llvm::IRBuilderBase> IRBuilderBase;
Wrapper<llvm::IRBuilder<>, &MakeIRBuilder> IRBuilder(IRBuilderBase);
Wrapper<llvm::Module, &MakeModule> Module;

Wrapper<llvm::Type> Type;
Wrapper<llvm::FunctionType> FunctionType(Type);
Wrapper<llvm::ArrayType> ArrayType(Type);
Wrapper<llvm::StructType> StructType(Type);
Wrapper<llvm::Value> Value;
Wrapper<llvm::GlobalValue> GlobalValue(Value);
Wrapper<llvm::Function> Function(GlobalValue);
Wrapper<llvm::GlobalVariable> GlobalVariable(GlobalValue);
Wrapper<llvm::BasicBlock> BasicBlock(Value);
Wrapper<llvm::Argument> Argument(Value);
Wrapper<llvm::InlineAsm> InlineAsm(Value);
Wrapper<llvm::PHINode> PHINode(Value);

Wrapper<llvm::Constant> Constant(Value);
Wrapper<llvm::ConstantInt> ConstantInt(Constant);
Wrapper<llvm::ConstantFP> ConstantFP(Constant);

inline void* MakeEngineBuilder(const v8::Arguments& args) {
  if (args.Length() != 1 || !Module.Is(args[0])) {
    THROW_ERROR("expected 1 argument: Module");
    return NULL;
  }

  // TODO engine takes ownership over module if llvm::EngineBuilder::create is successful
  return new llvm::EngineBuilder(Module.Unwrap(args[0]));
}


Wrapper<llvm::EngineBuilder, &MakeEngineBuilder> EngineBuilder;

// TODO while it has no constructor it actually has a virtual destructor
Wrapper<llvm::ExecutionEngine> ExecutionEngine;


inline void* MakeFunctionPassManager(const v8::Arguments& args) {
  if (args.Length() != 1 || !Module.Is(args[0])) {
    THROW_ERROR("expected 1 argument: Module");
    return NULL;
  }

  return new llvm::FunctionPassManager(Module.Unwrap(args[0]));
}


Wrapper<llvm::FunctionPassManager, &MakeFunctionPassManager> FunctionPassManager;
Wrapper<llvm::Pass> Pass;

void* MakeTargetData(const v8::Arguments& args);

Wrapper<llvm::TargetData, &MakeTargetData> TargetData(Pass);

void* MakeTargetData(const v8::Arguments& args) {
  if (args.Length() != 1 || !TargetData.Is(args[0])) {
    THROW_ERROR("expected 1 argument: TargetData");
    return NULL;
  }

  return new llvm::TargetData(*TargetData.Unwrap(args[0]));
}


static v8::Handle<v8::Value> EngineBuilder_create (const v8::Arguments& args) {
  if (args.Length() != 0) return THROW_ERROR("illegal number of arguments");
  std::string errstr;
  llvm::ExecutionEngine* ee = EngineBuilder.Unwrap(args.This())->setErrorStr(&errstr).create();
  return ee != NULL ? ExecutionEngine.Wrap(ee) : THROW_ERROR(errstr.c_str());
}


// TODO: teach it to manage function lifetime properly
namespace util {
class FunctionPointer {
 public:
  FunctionPointer(void* ptr) : ptr_(ptr) { }

  v8::Handle<v8::Function> toJSFunction() {
    return v8::FunctionTemplate::New(reinterpret_cast<v8::InvocationCallback>(ptr_))->GetFunction();
  }

 private:
  void* ptr_;
};
}

Wrapper<util::FunctionPointer> FunctionPointer;

static v8::Handle<v8::Value> ExecutionEngine_getPointerToFunction(const v8::Arguments& args) {
  if (args.Length() != 1 || !Function.Is(args[0])) return THROW_ERROR("illegal argument #0: llvm.Function expected");
  return FunctionPointer.Wrap(new util::FunctionPointer(ExecutionEngine.Unwrap(args.This())->getPointerToFunction(Function.Unwrap(args[0]))));
}


static v8::Handle<v8::Value> FunctionPointer_toJSFunction(const v8::Arguments& args) {
  return FunctionPointer.Unwrap(args.This())->toJSFunction();
}
