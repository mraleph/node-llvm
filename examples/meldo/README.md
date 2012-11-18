Meldo is a simple module that abstracts away LLVM related boilerplate and
provides direct access to some V8 heap structures.

Currently it is extremely primitive and undocumented. It was written for
[moe.js](https://code.google.com/p/moe-js/).

Here is a small example of "melding" a function that multiplies two floating
point arguments:

    var meldo = new Meldo;
    with (meldo) {
      var x = unboxNumber(arg(0));  // fetch arg #0 and unbox it
      var y = unboxNumber(arg(1));  // fetch arg #1 and unbox it
      var result = fmul(x, y);      // multiply two double values
      ret(boxNumber(result));       // box result and return it
    }
    var func = meldo.meld();

Meldo is very low-level and requires deep understanding of V8 innards.