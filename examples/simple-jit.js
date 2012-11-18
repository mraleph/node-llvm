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

var assert = require('assert');
var llvm = require('../build/Release/llvm');

llvm.InitializeNativeTarget();

var module = new llvm.Module('Test Module');

var ee = new llvm.EngineBuilder(module).setEngineKind(llvm.EngineKind.JIT).create();

assert(ee !== null, "failed to create execution engine");

var fpm = new llvm.FunctionPassManager(module);

fpm.add(new llvm.TargetData(ee.getTargetData()));
fpm.add(llvm.createBasicAliasAnalysisPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createReassociatePass());
fpm.add(llvm.createGVNPass());
fpm.add(llvm.createCFGSimplificationPass());
fpm.doInitialization();

var abort_sig = llvm.FunctionType.get(llvm.Type.getVoidTy(), [], false);
var abort = llvm.Function.Create(abort_sig, llvm.Function.ExternalLinkage, "abort", module);

var void_ptr = llvm.Type.getInt1PtrTy();  // LLVM does not permit pointer to void.
var int32_t = llvm.Type.getInt32Ty();
var v8capi_arg = llvm.Function.Create(llvm.FunctionType.get(void_ptr, [void_ptr, int32_t], false),
                                     llvm.Function.ExternalLinkage,
                                     "v8capi_arg",
                                     module);

var ft = llvm.FunctionType.get(void_ptr, [void_ptr], false);

var f = llvm.Function.Create(ft, llvm.Function.ExternalLinkage, "foo", module);

var args = f.getArgumentList();

args.forEach(function (arg, idx) {
  arg.setName("arg_" + idx);
});

var builder = new llvm.IRBuilder();

var bb = llvm.BasicBlock.Create("entry", f);

builder.SetInsertPoint(bb);

var result = builder.CreateCall(v8capi_arg, [args[0], builder.getInt32(1)]);

builder.CreateRet(result);

fpm.run(f);

f.dump();


var pointer = ee.getPointerToFunction(f);

console.log("%s", pointer instanceof llvm.FunctionPointer);

console.log("%s", pointer.toJSFunction()("arg#1", { toString: function () { return "hoooray!" } }, "arg#3"));

module.dump();
