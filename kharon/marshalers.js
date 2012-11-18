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

exports = module.exports = new (function marshalers() {})();

//
// Marshaler is an object that knows how to convert certain type of native values
// into V8 value and/or vice versa.  Every marshaler deals with native values of fixed
// type and can provide a number of methods to produce C++ code snippets to perfom conversions:
//     toV8: given an expression that computes native value
//           returns expression that converts computed native object into V8 Value;
//     fromV8: given an expression that computes V8 Value
//             returns expression that converts computed Value into native value;
//     test: given an expression that computes V8 Value
//           returns expression that tests whether given Value can be converted to
//           native value of certain type;
//
// There is also a special kind of marshalers that can synthesize native values from
// nothing instead of converting V8 Values.  Such marshalers provide method called
// |synthesize| instead of |fromV8|.
//

var assert = require('assert');

var utils = require('./utils.js');

var libclang = require('libclang');
var Type = libclang.Type;
var Cursor = libclang.Cursor;

// Dictionary of classes exposed through bindings layer.
var bound_classes = Object.create(null);

// Register given class as exposed through bindings layer.
exports.addBoundClass = function (decl) {
  var usr = decl.usr();
  assert(!(usr in bound_classes));
  bound_classes[usr] = BoundClass(decl);
  return bound_classes[usr];
};

// Helper methods to classify marshalers.
var isSynthetic = exports.isSynthetic = function (m) { return "synthesize" in m; };
exports.canMarshalToV8   = function (m) { return m !== null && ("toV8" in m); };
exports.canMarshalFromV8 = function (m) { return m !== null && ("fromV8" in m || isSynthetic(m)); };

// Find marshaler for a give type and marshaling direction.
exports.marshalType = function (type, direction, paramDecl) {
  var canonical = type.canonical();
  switch (canonical.kind()) {
  case Type.Pointer:
    return marshalClass(pointeeOf(type), direction);
  case Type.LValueReference:
    var pointee = pointeeOf(type);
    switch (pointee.spelling()) {
    case 'LLVMContext':
      return LLVMContext;
    case 'basic_string':
      return STDString;
    case 'Twine':
      return Twine;
    case 'StringRef':
      return StringRef;
    case 'iplist':
      var decl = type.pointee().declaration();
      var elemT = utils.guessFirstTemplateArgument(decl);
      if (elemT === null) return null;
      var elemClass = marshalClass(elemT, direction);
      if (elemClass === null) return null;
      return iplist(elemT, elemClass);
    }

    var pointeeClass = marshalClass(pointee, direction);
    if (pointeeClass !== null) {
      return BoundClassRef(pointee, pointeeClass);
    }

    return null;

  case Type.Record:
    var decl = declOf(type);
    switch (decl.spelling()) {
    case 'StringRef':
      return StringRef;
    case 'ArrayRef':
      var elemT = utils.guessFirstTemplateArgument(paramDecl);
      if (elemT === null) return null;
      var elemClass = marshalClass(elemT, direction);
      if (elemClass === null) return null;
      return ArrayRef(elemT, elemClass);
    }
    return null;

  case Type.Enum:
    return Enum(declOf(type));

  case Type.Void:
  case Type.Bool:
  case Type.UInt:
  case Type.ULong:
  case Type.ULongLong:
  case Type.Int:
  case Type.Long:
  case Type.LongLong:
  case Type.Double:
    return Primitive(canonical.spelling());

  default:
    return null;
  }
};

var BoundClass = marshaler({
  ctor: function (decl) {
    this.decl = decl;
    this.name = decl.spelling();
    this.cxxname = utils.cxxname(decl);
  },
  id:      function () { return this.decl.usr(); },
  display: "class $cxxname",
  toV8:    "$name.Wrap($val)",
  fromV8:  "$name.Unwrap($val)",
  test:    "$name.Is($val)"
});

var BoundClassRef = marshaler({
  ctor: function (actual, clazz) {
    this.actual = actual;
    this.clazz  = clazz;
  },
  id:      function () { return this.actual.usr(); },
  display: function () { return this.clazz.display; },

  toV8:    function (val) { return this.clazz.toV8("&(%s)".format(val)); },
  fromV8:  function (val) { return "*%s".format(this.clazz.fromV8(val)); },
  test:    function (val) { return this.clazz.test(val); }
});

var Primitive = marshaler({
  ctor: function (typename) {
    this.typename = typename;
    this.id       = typename.toUpperCase();
  },
  display: "$typename",
  toV8:    "$id_TO_V8($val)",
  fromV8:  "$id_FROM_V8($val)",
  test:    "IS_$id($val)"
});

var LLVMContext = {
  synthesize: function () {
    return "llvm::getGlobalContext()";
  },
  toString: function () {
    return "llvm::getGlobalContext()";
  }
};

var Twine = marshaler({
  id:      "Twine",
  display: "string",
  fromV8:  "TWINE_FROM_V8($val)",
  test:    "IS_TWINE($val)",
});

var StringRef = marshaler({
  id:      "StringRef",
  display: "string",
  toV8:    "STRINGREF_TO_V8($val)",
  fromV8:  "STRINGREF_FROM_V8($val)",
  test:    "IS_STRINGREF($val)"
});

var STDString = marshaler({
  id:      "STDString",
  display: "string",
  toV8:    "STDSTRING_TO_V8($val)",
  fromV8:  "STDSTRING_FROM_V8($val)",
  test:    "IS_STDSTRING($val)"
});

var iplist = marshaler({
  ctor: function (actual, clazz) {
    this.actual = actual;
    this.clazz  = clazz;
  },
  id:      "${actual.definition().usr()}",
  display: "llvm::iplist<${clazz.cxxname}>",

  toV8: "IPLIST_TO_V8(${clazz.name}, ${clazz.cxxname}, ${actual.display()}, $val)",
  test: "IS_IPLIST($val)",
});

var ArrayRef = marshaler({
  ctor: function (actual, clazz) {
    this.actual = actual;
    this.clazz  = clazz;
  },
  id:     "${actual.definition().usr()}",
  toV8:   "ArrayRefToV8<${actual.display()}>($val, ${clazz.name})",
  fromV8: "ArrayRefFromV8<${actual.display()}>($val, ${clazz.name})",
  test:   "IS_ARRAYREF($val)"
});

var Enum = exports.Enum = marshaler({
  ctor: function (decl) {
    this.decl = decl;
    this.name = decl.spelling();
    this.cxxname = utils.cxxname(decl);
  },
  id:      "$cxxname",
  display: "enum $cxxname",
  toV8:    "ENUM_TO_V8($cxxname, $val)",
  fromV8:  "ENUM_FROM_V8($cxxname, $val)",
  test:    "IS_ENUM($val)"
});

// For a given class find its base class that is in the list of bound classes.
function findBoundBase(decl) {
  if (decl.usr() in bound_classes) return bound_classes[decl.usr()];

  var decl = decl.definition();
  if (decl.kind() === Cursor.ClassDecl) {
    if (decl.usr() in bound_classes) return bound_classes[decl.usr()];

    var inherits = null;
    decl.visit(function (cursor) {
      if (cursor.kind() === Cursor.CXXBaseSpecifier) {
        inherits = findBoundBase(cursor);
      }
      return (inherits !== null) ? Cursor.VisitBreak : Cursor.VisitContinue;
    });

    return inherits;
  }

  return null;
}

// For a given class declaration find its marshaler among list of bound classes.
function findDirectMarshaler(decl) {
  if (decl.usr() in bound_classes) return bound_classes[decl.usr()];

  var decl = decl.definition();
  if (decl.kind() === Cursor.ClassDecl) {
    if (decl.usr() in bound_classes) return bound_classes[decl.usr()];
  }

  return null;
}

// Find marshaler for a class. When marshaling into V8 we can select bound base class
// if the class itself is not bound.
function marshalClass(decl, direction) {
    return (direction === "toV8") ? findBoundBase(decl) : findDirectMarshaler(decl);
}

function pointeeOf(type) { return type.canonical().pointee().declaration().canonical(); }

function declOf(type) { return type.canonical().declaration().canonical(); }

// Create marshaler from a template description.
function marshaler(desc) {
  function ctor() {
    this.id   = null;
    this.name = null;
  }

  function formatTemplate(template, dict) {
    return template.replace(/\$([a-z]+)/g, function (_, name) {
      assert(name in dict);
      return dict[name];
    }).replace(/\$\{([^}]+)\}/g, function (_, expr) {
      return new Function ("obj", "with (obj) { return (%s); }".format(expr)) (dict);
    });
  }

  function installFormatter(name) {
    if (!(name in desc)) return;

    var m = desc[name];
    if (typeof m === 'string') {
      var template = m;
      m = function (val) {
        var dict = Object.create(this);
        dict.val = val;
        return formatTemplate(template, dict);
      };
    }
    assert(typeof m === 'function');
    ctor.prototype[name] = m;
  }

  function initializeField(obj, name) {
    if (typeof desc[name] === 'string') {
      obj[name] = formatTemplate(desc[name], obj);
    } else if (typeof desc[name] === 'function') {
      obj[name] = desc[name].apply(obj);
    }
  }

  installFormatter("toV8");
  installFormatter("fromV8");
  installFormatter("test");

  ctor.prototype.toString = function () {
    return this.display || this.id;
  };

  var cache = Object.create(null);
  function factory() {
    var obj = new ctor();
    if (typeof desc.ctor === 'function') desc.ctor.apply(obj, arguments);
    initializeField(obj, 'id');
    initializeField(obj, 'display');
    return cache[obj.id] || (cache[obj.id] = obj);
  }

  if (typeof desc.ctor === 'function') {
    factory.getInstances = function () {
      return Object.keys(cache).map(function (k) { return cache[k]; });
    };

    return factory;
  }

  return factory();
}
