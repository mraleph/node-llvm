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

var llvm = require('llvm');
var assert = require('assert');

// Initialize LLVM ExecutionEngine and create a module.
llvm.InitializeNativeTarget();

var M = new llvm.Module('Meldo module');
var ee = new llvm.EngineBuilder(M).setEngineKind(llvm.EngineKind.JIT).create();
assert(ee !== null, "failed to create execution engine");

fpm = new llvm.FunctionPassManager(M);
fpm.add(new llvm.TargetData(ee.getTargetData()));
fpm.add(llvm.createCFGSimplificationPass());
fpm.add(llvm.createPromoteMemoryToRegisterPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createScalarReplAggregatesPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createJumpThreadingPass());
fpm.add(llvm.createCFGSimplificationPass());
fpm.add(llvm.createReassociatePass());
fpm.add(llvm.createEarlyCSEPass());
fpm.add(llvm.createLoopRotatePass());
fpm.add(llvm.createLICMPass());
fpm.add(llvm.createLoopUnswitchPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createIndVarSimplifyPass());
fpm.add(llvm.createLoopUnrollPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createGVNPass());
fpm.add(llvm.createSCCPPass());
fpm.add(llvm.createInstructionCombiningPass());
fpm.add(llvm.createJumpThreadingPass());
fpm.add(llvm.createDeadStoreEliminationPass());
fpm.add(llvm.createAggressiveDCEPass());
fpm.add(llvm.createCFGSimplificationPass());
fpm.doInitialization();

// Commonly used types.
var double_ty = llvm.Type.getDoubleTy();
var ptr_ty = llvm.Type.getInt8PtrTy();
var ptr_ptr_ty = ptr_ty.getPointerTo();
var args_ty = llvm.StructType.create([ptr_ty, ptr_ty.getPointerTo(), llvm.Type.getInt32Ty()], "args_ty");

// Register global functions.
var meldo_new_number = llvm.Function.Create(
  llvm.FunctionType.get(ptr_ptr_ty, [double_ty], false),
  llvm.Function.ExternalLinkage,
  "v8capi_new_number",
  M);

var function_id = 0;

module.exports = Meldo;
function Meldo() {
  this.builder = new llvm.IRBuilder();

  this.double_ty = double_ty;
  this.ptr_ty = ptr_ty;
  this.ptr_ptr_ty = ptr_ptr_ty;

  this.func = llvm.Function.Create(
    llvm.FunctionType.get(ptr_ty, [args_ty.getPointerTo()], false),
    llvm.Function.ExternalLinkage,
    "meldo_function_" + (function_id++),
    M);

  var args = this.func.getArgumentList();
  args[0].setName("args");

  this.blockId = 0;

  this.builder.SetInsertPoint(this.block());

  this.args_ptr = this.builder.CreateLoad(this.builder.CreateStructGEP(args[0], 1));
}

Meldo.prototype.block = function () {
  return llvm.BasicBlock.Create("B" + (this.blockId++), this.func);
};

Meldo.prototype.debugbreak = function () {
  this.builder.CreateCall(
    llvm.Intrinsic.getDeclaration(M, llvm.Intrinsic.x86_int),
    [llvm.ConstantInt.get(llvm.Type.getInt8Ty(), 3)]
  );
};

Meldo.prototype.meld = function () {
  fpm.run(this.func);
  return ee.getPointerToFunction(this.func).toJSFunction();
};

Meldo.prototype.dump = function () {
  this.func.dump();
};

// Load argument with the given index.
Meldo.prototype.arg = function (idx) {
  return this.builder.CreateLoad(
    this.builder.CreateGEP(this.args_ptr, llvm.ConstantInt.getSigned(llvm.Type.getInt8Ty(), -idx | 0)));
};

Meldo.prototype.untag = function (obj) {
  return this.builder.CreateBitCast(
    this.builder.CreateGEP(obj, [llvm.ConstantInt.getSigned(llvm.Type.getInt8Ty(), -1)]),
    this.ptr_ptr_ty);
};

Meldo.prototype.fieldptr = function (obj, idx, offs) {
  obj = this.untag(obj);

  if (typeof offs === "number") {
    offs = this.builder.getInt32(offs | 0);
    obj = this.builder.CreateGEP(obj, offs);
  }

  if (typeof idx === "number") {
    idx = this.builder.getInt32(idx | 0);
  }

  assert(typeof idx === "object");

  return this.builder.CreateGEP(obj, idx);
};

Meldo.prototype.load = function (ptr) {
  return this.builder.CreateLoad(ptr);
};

Meldo.prototype.elements = function (obj) {
  return this.load(this.fieldptr(obj, 2));
};

Meldo.prototype.property = function (obj, idx) {
  return this.load(this.fieldptr(obj, idx, 3));
};

Meldo.prototype.elementptr = function (obj, idx) {
  return this.fieldptr(obj, idx, 2);
};

Meldo.prototype.element = function (obj, idx) {
  return this.load(this.elementptr(obj, idx));
};

Meldo.prototype.setelement = function (obj, idx, val) {
  return this.store(val, this.elementptr(obj, idx));
};

Meldo.prototype.boxNumber = function (value) {
  return this.load(this.builder.CreateCall(meldo_new_number, [value]));
};

Meldo.prototype.unboxNumber = function (obj) {
  var builder = this.builder;

  var is_smi = this.block();
  var is_heapnumber = this.block();
  var join = this.block();

  var int64obj = builder.CreatePtrToInt(obj, builder.getInt64Ty());

  builder.CreateCondBr(
    builder.CreateICmpEQ(builder.CreateAnd(int64obj, builder.getInt64(1)), builder.getInt64(0)),
    is_smi,
    is_heapnumber);

  builder.SetInsertPoint(is_smi);
  var smi_val = builder.CreateSIToFP(
    builder.CreateIntCast(builder.CreateAShr(int64obj, 32), builder.getInt32Ty(), true),
      builder.getDoubleTy());
  builder.CreateBr(join);

  builder.SetInsertPoint(is_heapnumber);
  // TODO(vegorov) check heap number map
  var heapnumber_val = builder.CreateLoad(builder.CreateGEP(
    builder.CreateBitCast(this.untag(obj), this.double_ty.getPointerTo()),
    builder.getInt32(1)
  ));
  builder.CreateBr(join);

  builder.SetInsertPoint(join);
  var phi = builder.CreatePHI(this.double_ty, 2);
  phi.addIncoming(smi_val, is_smi);
  phi.addIncoming(heapnumber_val, is_heapnumber);
  return phi;
};

Meldo.prototype.unboxInteger32 = function (val, check) {
  var builder = this.builder;

  var int32 = builder.CreateFPToSI(val, builder.getInt32Ty());
  check(builder.CreateFCmpOEQ(builder.CreateSIToFP(int32, this.double_ty), val));
  return int32;
};

Meldo.prototype.not = function (value) {
  return this.builder.CreateNot(value);
};

Meldo.prototype.currentBlock = function () {
  return this.builder.GetInsertBlock();
};

Meldo.prototype.setCurrentBlock = function (block) {
  return this.builder.SetInsertPoint(block);
};

function forward(from, to) {
  Meldo.prototype[from] = function () {
    return this.builder[to].apply(this.builder, arguments);
  };
}

forward("icmpeq", "CreateICmpEQ");
forward("fcmpolt", "CreateFCmpOLT");
forward("fmul", "CreateFMul");
forward("fadd", "CreateFAdd");
forward("store", "CreateStore");
forward("branch", "CreateBr");
forward("phi", "CreatePHI")

Meldo.prototype.if_ = function (cond, build_then, build_else) {
  var builder = this.builder;

  var trueblock = this.block();
  var falseblock = this.block();
  var join = this.block();

  builder.CreateCondBr(cond, trueblock, falseblock);

  builder.SetInsertPoint(trueblock);
  if (typeof build_then === "function") {
    build_then(trueblock, falseblock, join);
  } else {
    builder.CreateBr(join);
  }

  builder.SetInsertPoint(falseblock);
  if (typeof build_else === "function") {
    build_else(trueblock, falseblock, join);
  } else {
    builder.CreateBr(join);
  }

  builder.SetInsertPoint(join);
};

Meldo.prototype.ret = function (value) {
  if (typeof value === "undefined") {
    value = this.builder.CreateIntToPtr(this.builder.getInt64(0), this.ptr_ty);
  }
  this.builder.CreateRet(value);
};

Meldo.prototype.literal = function (value) {
  assert(typeof value === "number");  // Only numbers are supported for now.
  return llvm.ConstantFP.get(this.double_ty, value);
};

