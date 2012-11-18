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

#define V8CAPI extern "C"
#define ARGS(v) reinterpret_cast<v8::Arguments*>(v)

V8CAPI int v8capi_argc(void* p) {
  return ARGS(p)->Length();
}

V8CAPI void* v8capi_arg(void* p, uint32_t idx) {
  v8::Handle<v8::Value> arg = (*ARGS(p))[idx];
  return reinterpret_cast<void*>(*arg);
}

V8CAPI void* v8capi_new_number(double val) {
  v8::Handle<v8::Value> arg = v8::Number::New(val);
  return reinterpret_cast<void*>(*arg);
}
