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
var util = require('util');

exports.Nod = Nod;

function prefixLen(a, b) {
  var l = Math.min(a.length, b.length);
  for (var i = 0; i < l; i++) {
    if (a[i] !== b[i]) {
      return i;
    }
  }
  return l;
}

function Arr(seg, nod) {
  this.seg = seg;
  this.nod = nod;
}

Arr.prototype.split = function (l) {
  assert(l > 0);

  if (l == this.seg.length) {
    return this.nod;
  }

  var prefix = this.seg.slice(0, l);
  var suffix = this.seg.slice(l);
  this.seg = prefix;
  this.nod = new Nod(null, [new Arr(suffix, this.nod)]);

  return this.nod;
};

Arr.prototype.insert = function (seg, val) {
  var pl = prefixLen(this.seg, seg);

  if (pl === 0) {
    return false;
  }

  this.split(pl).insert(seg.slice(pl), val);
  return true;
};

function Nod(val, arr) {
  this.val = val;
  this.arr = arr;
}

Nod.prototype.insert = function (seg, val) {
  if (seg.length === 0) {
    assert(this.val === null);
    this.val = val;
    return;
  }

  for (var i = 0; i < this.arr.length; i++) {
    var arr = this.arr[i];
    if (arr.insert(seg, val)) {
      return;
    }
  }
  this.arr.push(new Arr(seg, new Nod(val, [])));
};

Nod.prototype.toString = function (indent) {
  indent = indent || "";
  return util.format("%s{%s}\n%s", indent, this.val === null ? "" : this.val.toString(),
                     this.arr.map(function (arr) {
                       return util.format("%s[%s] ->\n%s", indent, arr.seg.join(', '), arr.nod.toString(indent + "  "));
                     }).join('\n'));
};
