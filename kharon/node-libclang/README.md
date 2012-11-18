Simple libclang bindings for node.

Usage example:

    var libclang = require('libclang');
    var Cursor = libclang.Cursor;

    var tu = libclang.Parse(['-x',
                             'c++',
                             inputPath,
                             '-I' + NODE_INCLUDE_DIR]);

    tu.cursor().visit(function (cursor) {
      switch (cursor.kind()) {
        case Cursor.VarDecl: // Do something with variable declaration.
        case Cursor.FunctionDecl: // Do something else with function declaration.
      }
      return Cursor.VisitContinue;
    });


