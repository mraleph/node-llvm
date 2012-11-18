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

#ifndef BINDINGS_HELPERS_H
#define BINDINGS_HELPERS_H

#define THROW_ERROR(str) (v8::ThrowException(v8::Exception::Error(v8::String::New(str))))

#define BOOL_TO_V8(val) v8::Boolean::New((val))
#define BOOL_FROM_V8(val) ((val)->BooleanValue())
#define IS_BOOL(val) val->IsBoolean()

#define DOUBLE_TO_V8(val) v8::Number::New((val))
#define DOUBLE_FROM_V8(val) ((val)->NumberValue())
#define IS_DOUBLE(val) ((val)->IsNumber())

// TODO: marshaling of big (long/long long) types is completely borked
#define UINT_TO_V8(val) v8::Integer::NewFromUnsigned(static_cast<uint32_t>(val))
#define UINT_FROM_V8(val) ((val)->Uint32Value())
#define IS_UINT(val) ((val)->IsUint32())

#define ULONG_TO_V8(val) UINT_TO_V8(val)
#define ULONG_FROM_V8(val) static_cast<unsigned long>(UINT_FROM_V8(val))
#define IS_ULONG(val) IS_UINT(val)

#define ULONGLONG_TO_V8(val) UINT_TO_V8(val)
#define ULONGLONG_FROM_V8(val) static_cast<unsigned long long>(UINT_FROM_V8(val))
#define IS_ULONGLONG(val) IS_UINT(val)

#define INT_TO_V8(val) v8::Integer::New(static_cast<int32_t>(val))
#define INT_FROM_V8(val) ((val)->Int32Value())
#define IS_INT(val) ((val)->IsInt32())

#define LONG_TO_V8(val) INT_TO_V8(val)
#define LONG_FROM_V8(val) static_cast<long>(INT_FROM_V8(val))
#define IS_LONG(val) IS_INT(val)

#define LONGLONG_TO_V8(val) INT_TO_V8(val)
#define LONGLONG_FROM_V8(val) static_cast<long long>(INT_FROM_V8(val))
#define IS_LONGLONG(val) IS_INT(val)

#define IS_TWINE(val) ((val)->IsString())
#define TWINE_FROM_V8(val) (*v8::String::AsciiValue(val))

#define IS_STRINGREF(val) ((val)->IsString())
#define STRINGREF_FROM_V8(val) (*v8::String::AsciiValue(val))
#define STRINGREF_TO_V8(val) StringRefToV8(val)

inline v8::Handle<v8::String> StringRefToV8(const llvm::StringRef& sref) {
  return v8::String::New(sref.data(), sref.size());
}

#define IS_STDSTRING(val) ((val)->IsString())
#define STDSTRING_FROM_V8(val) (std::string(*v8::String::AsciiValue(val)))
#define STDSTRING_TO_V8(val) STDStringToV8(val)

inline v8::Handle<v8::String> STDStringToV8(const std::string& str) {
  return v8::String::New(str.c_str());
}

#define ENUM_FROM_V8(e, val) static_cast<e>((val)->Int32Value())
#define ENUM_TO_V8(e, val) v8::Integer::New(val)
#define IS_ENUM(val) ((val)->IsInt32())

#define VOID_TO_V8(val) ((val), v8::Undefined())

#define IS_ARRAYREF(val) ((val)->IsArray())

template<typename T>
inline std::vector<T*> ArrayRefFromV8(v8::Handle<v8::Value> val, WrapperTypedBase<T>& w) {
  v8::HandleScope scope;
  v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(val);
  std::vector<T*> v;
  for (uint32_t i = 0, len = arr->Length(); i < len; i++) {
    v.push_back(static_cast<T*>(w.Unwrap(arr->Get(i))));
  }
  return v;
}

template<typename T>
struct Primitive { };

template<>
struct Primitive<unsigned> {
  static unsigned fromV8(v8::Handle<v8::Value> val) { return val->Uint32Value(); }
};

template<typename T>
inline std::vector<T> ArrayRefFromV8(v8::Handle<v8::Value> val) {
  v8::HandleScope scope;
  v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(val);
  std::vector<T> v;
  for (uint32_t i = 0, len = arr->Length(); i < len; i++) {
    v.push_back(Primitive<T>::fromV8(arr->Get(i)));
  }
  return v;
}

template<typename NativeT, typename WrapperT>
inline v8::Handle<v8::Array> IPListToV8(llvm::iplist<NativeT>& list,
                                        WrapperTypedBase<WrapperT>& w) {
  v8::HandleScope scope;
  uint32_t size = list.size();
  uint32_t index = 0;
  v8::Handle<v8::Array> arr = v8::Array::New(size);
  for (typename llvm::iplist<NativeT>::iterator i = list.begin(); index < size; ++i, ++index) {
    arr->Set(index, w.Wrap(i));
  }
  return scope.Close(arr);
}

#define IPLIST_TO_V8(Wrapper, WrapperT, NativeT, val) \
  IPListToV8<NativeT, WrapperT>((val), (Wrapper))

#define BIND_INSTANCE_METHOD(W, Name, Func)                             \
  (W).Prototype()->Set(v8::String::New(#Name), v8::FunctionTemplate::New(&Func))

#define BIND_STATIC_METHOD(W, Name, Func)                             \
  (W).Template()->Set(v8::String::New(#Name), v8::FunctionTemplate::New(&Func))

#define SET_CONSTANT(O, Name, Value) \
  (O)->Set(v8::String::New(#Name), v8::Integer::New(Value))

#define SET_FUNCTION(O, Name, Func) \
  (O)->Set(v8::String::New(#Name), v8::FunctionTemplate::New(&Func)->GetFunction())

#define BIND_CONST(W, Name, Value) \
  SET_CONSTANT(W.Template(), Name, Value)

#endif
