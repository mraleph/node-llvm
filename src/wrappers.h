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

#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <node.h>

#include <cassert>

class WrapperBase {
 public:
  typedef void* (*CtorCallback) (const v8::Arguments& args);

  WrapperBase(WrapperBase* parent, v8::InvocationCallback ctor_callback)
      : parent_(parent), ctor_callback_(ctor_callback) {
  }

  v8::Handle<v8::FunctionTemplate> Template() {
    if (template_.IsEmpty()) Init();
    return template_;
  }

  v8::Handle<v8::Function> Constructor() {
    if (ctor_.IsEmpty()) {
      ctor_ = v8::Persistent<v8::Function>::New(Template()->GetFunction());
      // Emulate inheritance of static members.
      if (parent_) ctor_->SetPrototype(parent_->Constructor());
    }
    return ctor_;
  }

  v8::Handle<v8::ObjectTemplate> Prototype() {
    return Template()->PrototypeTemplate();
  }

  bool Is(v8::Handle<v8::Value> value) {
    return template_->HasInstance(value);
  }

 protected:
  void Init() {
    template_ =
        v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(ctor_callback_));
    template_->InstanceTemplate()->SetInternalFieldCount(1);
    if (parent_ != NULL) template_->Inherit(parent_->Template());
  }

  WrapperBase* parent_;
  v8::Persistent<v8::FunctionTemplate> template_;
  v8::Persistent<v8::Function> ctor_;

  v8::InvocationCallback ctor_callback_;
};


template<typename T>
class WrapperTypedBase : public WrapperBase {
 public:
  WrapperTypedBase(WrapperBase* parent, v8::InvocationCallback ctor_callback)
      : WrapperBase(parent, ctor_callback) {
  }

  v8::Handle<v8::Value> Wrap(const T* val) {
    return Wrap(const_cast<T*>(val));
  }

  v8::Handle<v8::Value> Wrap(T* val) {
    if (val == NULL) {
      return v8::Null();
    }

    v8::HandleScope scope;
    v8::Handle<v8::Value> args[] = { v8::External::New(val) };
    return scope.Close(ctor_->NewInstance(1, args));
  }

  T* Unwrap(v8::Handle<v8::Value> value) {
    if (value->IsNull()) {
      return NULL;
    }

    assert(template_->HasInstance(value));
    return static_cast<T*>(v8::Handle<v8::Object>::Cast(value)->GetPointerFromInternalField(0));
  }
};


inline void* DummyCtorCallback(const v8::Arguments& args) { return NULL; }


template<typename T, WrapperBase::CtorCallback ctor = &DummyCtorCallback>
class Wrapper : public WrapperTypedBase<T> {
 public:
  Wrapper() : WrapperTypedBase<T>(NULL, &Ctor) { }
  Wrapper(WrapperBase& parent) : WrapperTypedBase<T>(&parent, &Ctor) { }

 private:
  static void Dtor(v8::Persistent<v8::Value> obj, void* ptr) {
    obj.Dispose();
    obj.Clear();
    // TODO(vegorov): some types (e.g. Passes) have "external" ownership after
    // they were added somewhere so automatic deletion does not work.
    // delete static_cast<T*>(ptr);
  }

  static v8::Handle<v8::Value> Ctor(const v8::Arguments& args) {
    // This constructor can be called as a wrapping constructor.
    if (args.Length() == 1 && args[0]->IsExternal()) {
      args.This()->SetPointerInInternalField(0, v8::External::Unwrap(args[0]));
      return args.This();
    } else {
      void* obj = ctor(args);
      if (obj == NULL) return v8::Undefined();  // Exception should be pending.
      args.This()->SetPointerInInternalField(0, obj);
      v8::Persistent<v8::Value> weak = v8::Persistent<v8::Value>::New(args.This());
      weak.MakeWeak(obj, Dtor);
      weak.MarkIndependent();
      return args.This();
    }
  }
};


template<typename T>
class Wrapper<T, &DummyCtorCallback> : public WrapperTypedBase<T> {
 public:
  Wrapper() : WrapperTypedBase<T>(NULL, &Ctor) { }
  Wrapper(WrapperBase& parent) : WrapperTypedBase<T>(&parent, &Ctor) { }

 private:
  static v8::Handle<v8::Value> Ctor(const v8::Arguments& args) {
    // This constructor can be called as a wrapping constructor.
    if (args.Length() == 1 && args[0]->IsExternal()) {
      args.This()->SetPointerInInternalField(0, v8::External::Unwrap(args[0]));
      return args.This();
    } else {
      return v8::ThrowException(v8::Exception::Error(v8::String::New("illegal invocation!")));
    }
  }
};


#endif
