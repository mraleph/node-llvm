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

exports = module.exports = new (function utils() {})();

var util = require('util');

var libclang = require('libclang');
var Type = libclang.Type;
var Cursor = libclang.Cursor;

String.prototype.format = function () {
  "use strict";
  var args = [this];
  Array.prototype.push.apply(args, arguments);
  return util.format.apply(util, args);
};

Cursor.prototype.toString = function () {
  return "libclang.cursor {%s#%d}".format(this.spelling(), this.kind());
};

Type.prototype.toString = function () {
  return "libclang.type {%s#%d}".format(this.spelling(), this.kind());
};

// Return immediate children of the given cursor.
var children = exports.children = function (cursor) {
  var arr = [];
  cursor.visit(function (child) { arr.push(child); return Cursor.VisitContinue; });
  return arr;
};

exports.pathTo = function (e) {
  var path = [];
  for (var parent = e.parent(); !parent.isNull(); parent = parent.parent()) {
    path.push(parent.spelling());
  }
  if (path[path.length - 1] === '' || path[path.length - 1] === 'src/bindings.cc') path.pop();
  path = path.reverse();
  path.push(e.spelling());
  return path;
};

exports.cxxname = function (e) {
  var path = [e.spelling()];
  for (var parent = e.parent(); !parent.parent().isNull(); parent = parent.parent()) {
    if (parent.kind() === Cursor.EnumDecl) continue;
    path.push(parent.spelling());
  }
  return path.reverse().join('::');
};

// For a parameter of variable declaration which type is
// template-based try guessing first template's type argument.
exports.guessFirstTemplateArgument = function (decl) {
  var c = children(decl.canonical());
  if (c.length < 2 || c[0].kind() !== Cursor.TemplateRef) return null;

  for (var i = 1, l = c.length; i < l; i++) {
    switch (c[i].kind()) {
    case Cursor.NamespaceRef:
      continue;
    case Cursor.TypeRef:
    case Cursor.TemplateRef:
      return c[i];
    default:
      return null;
    }
  }

  return null;
};

// Try counting arguments which do not have default initializers.
// Unfortunately libclang does not have any simple API for that
// so we try some hand waving heuristic.
exports.guessRequiredArgs = function (method) {
  var no_result = method.type().result().kind === Type.Void;
  var count = 0;
  method.visit(function (cursor) {
    switch (cursor.kind()) {
    case Cursor.ParmDecl:
      var hasDefault = false;
      cursor.visit(function (cursor) {
        var kind = cursor.kind();
        if (kind !== Cursor.TypeRef &&
            kind !== Cursor.TemplateRef &&
            kind !== Cursor.NamespaceRef &&
            kind !== Cursor.ParmDecl) {
          hasDefault = true;
          return Cursor.VisitBreak;
        }
        return Cursor.VisitContinue;
      });

      if (!hasDefault) {
        count++;
        return Cursor.VisitContinue;
      }
      break;

    case Cursor.TypeRef:
    case Cursor.TemplateRef:
      if (no_result) count++;
      return Cursor.VisitContinue;

    case Cursor.FirstAttr:
    case Cursor.UnexposedAttr:
      return Cursor.VisitContinue;
    }

    return Cursor.VisitBreak;
  });

  return count;
};
